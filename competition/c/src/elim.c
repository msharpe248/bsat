/* Bounded resolution elimination. All removed clauses are retained for model
 * reconstruction; resolvents are staged before arena mutations invalidate pointers. */
#include "../include/solver.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

void elim_init(Solver *s) {
    if (s->elim) return;
    s->elim = calloc(1, sizeof *s->elim);
    if (!s->elim) { s->error = true; return; }
    s->elim->occs_capacity = 2 * (s->num_vars + 1);
    s->elim->elim_capacity = s->num_vars + 1;
    s->elim->occs = calloc(s->elim->occs_capacity, sizeof(OccList));
    s->elim->eliminated = calloc(s->elim->elim_capacity, sizeof(bool));
    if (!s->elim->occs || !s->elim->eliminated) s->error = true;
}
void elim_free(Solver *s) {
    ElimState *e = s->elim;
    if (!e) return;
    if (e->occs) for (uint32_t i = 0; i < e->occs_capacity; ++i) free(e->occs[i].clauses);
    for (uint32_t i = 0; i < e->stack_size; ++i) free(e->stack[i].clause);
    free(e->occs); free(e->eliminated); free(e->stack); free(e->resolvent_crefs); free(e);
    s->elim = NULL;
}
void elim_clear_occs(Solver *s) {
    if (s->elim && s->elim->occs)
        for (uint32_t i = 0; i < s->elim->occs_capacity; ++i) s->elim->occs[i].size = 0;
}
void elim_add_occ(Solver *s, Lit l, CRef cr) {
    OccList *o = &s->elim->occs[l];
    if (o->size == o->capacity) {
        uint32_t cap = o->capacity ? o->capacity * 2 : 8;
        CRef *p = realloc(o->clauses, cap * sizeof *p);
        if (!p) { s->error = true; return; }
        o->clauses = p; o->capacity = cap;
    }
    o->clauses[o->size++] = cr;
}
void elim_remove_occ(Solver *s, Lit l, CRef cr) {
    OccList *o = &s->elim->occs[l];
    for (uint32_t i = 0; i < o->size; ++i)
        if (o->clauses[i] == cr) { o->clauses[i] = o->clauses[--o->size]; return; }
}
void elim_build_occs(Solver *s) {
    elim_init(s);
    if (s->error) return;
    elim_clear_occs(s);
    s->elim->occs_complete=false;
    for (uint32_t i = 0; i < s->num_clauses; ++i) {
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        for (uint32_t j = 0; j < CLAUSE_SIZE(s->arena, cr); ++j) {
            s->work++;
            if ((s->work & 1023)==0 && solver_budget_exhausted(s)) return;
            elim_add_occ(s, CLAUSE_LITS(s->arena, cr)[j], cr);
        }
    }
    s->elim->occs_complete=!s->error;
}
bool elim_is_tautology(const Lit *a, uint32_t na, const Lit *b, uint32_t nb, Var v) {
    for (uint32_t i = 0; i < na; ++i) if (var(a[i]) != v)
        for (uint32_t j = 0; j < nb; ++j) if (var(b[j]) != v && a[i] == neg(b[j])) return true;
    return false;
}
static void clean_occ(Solver *s, OccList *o) {
    uint32_t n = 0;
    for (uint32_t i = 0; i < o->size; ++i)
        if (!clause_deleted(s->arena, o->clauses[i])) o->clauses[n++] = o->clauses[i];
    o->size = n;
}
enum { PAIR_STOP = -1, PAIR_TAUTOLOGY, PAIR_OK };

/* Root preprocessing owns the otherwise idle minimizer scratch. Charge literal
   visits and mandatory mark cleanup; never leave marks across a pair or abort. */
static int resolve_pair(Solver *s, const Lit *a, uint32_t na,
                        const Lit *b, uint32_t nb, Var pivot, Lit *out, uint32_t *size) {
    if (solver_budget_exhausted(s)) return PAIR_STOP;
    uint32_t touched = 0, count = 0;
    int status = PAIR_OK;
    for (unsigned part = 0; part < 2 && status == PAIR_OK; ++part) {
        const Lit *lits = part ? b : a;
        uint32_t n = part ? nb : na;
        for (uint32_t i = 0; i < n; ++i) {
            ++s->work;
            if ((s->work_limit && s->work >= s->work_limit) ||
                (!(s->work & 1023) && solver_budget_exhausted(s))) {
                status = PAIR_STOP; break;
            }
            Lit l = lits[i];Var v = var(l);
            if (v == pivot) continue;
            uint8_t mark = 1 + sign(l);
            if (s->seen[v]) {
                if (s->seen[v] != mark) { status = PAIR_TAUTOLOGY; break; }
                continue;
            }
            s->seen[v] = mark;
            s->minimize_touched[touched++] = v;
            if (out) out[count] = l;
            ++count;
        }
    }
    for (uint32_t i = 0; i < touched; ++i) {
        s->seen[s->minimize_touched[i]] = 0;
        ++s->work;
    }
    if (solver_budget_exhausted(s)) status = PAIR_STOP;
    if (size) *size = count;
    return status;
}

int elim_cost(Solver *s, Var v) {
    if (!s->elim || s->values[v] != UNDEF || s->elim->eliminated[v]) return -1;
    if (solver_budget_exhausted(s)) return -1;
    OccList *p = &s->elim->occs[mkLit(v, false)], *n = &s->elim->occs[mkLit(v, true)];
    clean_occ(s, p); clean_occ(s, n);
    if (!p->size && !n->size) return -1;
    if (p->size > s->opts.elim_max_occ || n->size > s->opts.elim_max_occ) return -1;
    uint64_t count = 0;
    for (uint32_t i = 0; i < p->size; ++i) for (uint32_t j = 0; j < n->size; ++j) {
        CRef a = p->clauses[i], b = n->clauses[j];
        uint32_t na = CLAUSE_SIZE(s->arena, a), nb = CLAUSE_SIZE(s->arena, b);
        int result = resolve_pair(s, CLAUSE_LITS(s->arena,a), na,
                                  CLAUSE_LITS(s->arena,b), nb, v, NULL, NULL);
        if (result == PAIR_STOP) return -1;
        if (result == PAIR_OK) ++count;
        if (count > INT_MAX || count > (uint64_t)p->size + n->size + s->opts.elim_grow) return -1;
    }
    return (int)count;
}
/* On success the stack owns the allocation; on failure the caller still does. */
static bool save_owned(Solver *s, Var v, Lit *lits, uint32_t size) {
    elim_init(s);
    if (s->error) return false;
    ElimState *e = s->elim;
    if (e->stack_size == e->stack_capacity) {
        if (e->stack_capacity > UINT32_MAX / 2) { s->error = true; return false; }
        uint32_t cap = e->stack_capacity ? e->stack_capacity * 2 : 32;
        ElimEntry *p = realloc(e->stack, cap * sizeof *p);
        if (!p) { s->error = true; return false; }
        e->stack = p; e->stack_capacity = cap;
    }
    e->stack[e->stack_size++] = (ElimEntry){v, lits, size};
    return true;
}

/* BCE and equivalence callers retain their input buffers: keep copy semantics. */
bool elim_save(Solver *s, Var v, const Lit *lits, uint32_t size) {
    if (s->error) return false;
    Lit *copy = malloc((size ? size : 1) * sizeof *copy);
    if (!copy) { s->error = true; return false; }
    if (size) memcpy(copy, lits, size * sizeof *copy);
    if (save_owned(s, v, copy, size)) return true;
    free(copy);
    return false;
}

static bool staging_exhausted(Solver *s) {
    return (s->work & 1023) ? solver_budget_exhausted(s) : solver_budget_exhausted_now(s);
}

bool elim_eliminate_var(Solver *s, Var v) {
    int cost = elim_cost(s, v);
    if (cost < 0) return false;
    OccList *p = &s->elim->occs[mkLit(v, false)], *n = &s->elim->occs[mkLit(v, true)];
    uint32_t count = p->size + n->size;
    CRef *removed = malloc(count * sizeof *removed);
    Lit **res = calloc((size_t)cost + 1, sizeof *res);
    uint32_t *sizes = calloc((size_t)cost + 1, sizeof *sizes);
    Lit *saved = NULL;
    uint32_t saved_size = 0, nr = 0;
    bool eliminated = false;
    if (!removed || !res || !sizes) { s->error = true; goto done; }
    if (p->size) memcpy(removed, p->clauses, p->size * sizeof *removed);
    if (n->size) memcpy(removed + p->size, n->clauses, n->size * sizeof *removed);
    uint64_t words = 0;
    for (uint32_t i = 0; i < count; ++i) words += (uint64_t)CLAUSE_SIZE(s->arena, removed[i]) + 1;
    if (words > UINT32_MAX || words > SIZE_MAX / sizeof *saved) goto done;
    saved_size = (uint32_t)words;
    saved = malloc(saved_size * sizeof *saved);
    if (!saved) { s->error = true; goto done; }
    uint32_t at = 0;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t z = CLAUSE_SIZE(s->arena, removed[i]);
        const Lit *lits = CLAUSE_LITS(s->arena, removed[i]);
        for (uint32_t j = 0; j < z;) {
            if (solver_budget_exhausted(s)) goto done;
            uint32_t chunk = MIN(z-j, 1024u-(uint32_t)(s->work & 1023));
            if (s->work_limit && s->work_limit-s->work < chunk)
                chunk = (uint32_t)(s->work_limit-s->work);
            memcpy(saved+at, lits+j, chunk*sizeof *saved);
            at += chunk; j += chunk; s->work += chunk;
            if (staging_exhausted(s)) goto done;
        }
        saved[at++] = 0; ++s->work;
        if (staging_exhausted(s)) goto done;
    }
    for (uint32_t i = 0; i < p->size; ++i) for (uint32_t j = 0; j < n->size; ++j) {
        CRef ca = p->clauses[i], cb = n->clauses[j];
        uint32_t na = CLAUSE_SIZE(s->arena, ca), nb = CLAUSE_SIZE(s->arena, cb);
        Lit *a = CLAUSE_LITS(s->arena, ca), *b = CLAUSE_LITS(s->arena, cb);
        Lit *r = malloc(((size_t)na + nb) * sizeof *r);
        if (!r) { s->error = true; goto done; }
        uint32_t z = 0;
        int result = resolve_pair(s, a, na, b, nb, v, r, &z);
        if (result != PAIR_OK) {
            free(r);
            if (result == PAIR_STOP) goto done;
            continue;
        }
        res[nr] = r; sizes[nr++] = z;
    }
    if (!save_owned(s, v, saved, saved_size)) goto done;
    saved = NULL; // Ownership transferred; cleanup must not free the stack record.
    for (uint32_t i = 0; i < nr && !s->error && s->result != FALSE; ++i) {
        proof_add_clause(s, res[i], sizes[i]);
        uint32_t before = s->num_clauses;
        s->internal_add = true;
        solver_add_clause(s, res[i], sizes[i]);
        s->internal_add = false;
        for (uint32_t k = before; k < s->num_clauses; ++k) {
            CRef cr = s->clauses[k];
            for (uint32_t j = 0; j < CLAUSE_SIZE(s->arena, cr); ++j)
                elim_add_occ(s, CLAUSE_LITS(s->arena, cr)[j], cr);
        }
        s->elim->resolvents_added++;
    }
    if (!s->error) {
        for (uint32_t i = 0; i < count; ++i) solver_delete_clause(s, removed[i]);
        s->elim->eliminated[v] = true;
        s->elim->vars_eliminated++; s->elim->clauses_removed += count;
        eliminated = true;
    }
done:
    for (uint32_t i = 0; i < nr; ++i) free(res[i]);
    free(res); free(sizes); free(removed); free(saved);
    return eliminated;
}
uint32_t elim_preprocess(Solver *s) {
    elim_build_occs(s);
    uint32_t count = 0;
    if (!s->elim || !s->elim->occs_complete) return 0;
    for (Var v = 1; v <= s->num_vars && !s->error && s->result != FALSE; ++v) {
        if (solver_budget_exhausted(s)) break;
        if (elim_eliminate_var(s, v)) count++;
        if (solver_propagate(s) != INVALID_CLAUSE) s->result = FALSE;
    }
    return count;
}
void elim_extend_model(Solver *s) {
    for (Var v = 1; v <= s->num_vars; ++v) if (s->values[v] == UNDEF) s->values[v] = TRUE;
    if (!s->elim) return;
    for (uint32_t i = s->elim->stack_size; i > 0;) {
        ElimEntry *e = &s->elim->stack[--i];
        bool satisfied = false;
        Lit pivot = 0;
        for (uint32_t j = 0; j < e->clause_size; ++j) {
            Lit l = e->clause[j];
            if (!l) {
                if (!satisfied && pivot) s->values[e->var] = sign(pivot) ? FALSE : TRUE;
                satisfied = false; pivot = 0;
            } else if (var(l) == e->var) pivot = l;
            else if (lxor(s->values[var(l)], sign(l)) == TRUE) satisfied = true;
        }
    }
}
bool elim_is_eliminated(const Solver *s, Var v) { return s->elim && v < s->elim->elim_capacity && s->elim->eliminated[v]; }
OccList *elim_get_occs(Solver *s, Lit l) { return s->elim && l < s->elim->occs_capacity ? &s->elim->occs[l] : NULL; }
void elim_print_stats(const Solver *s) {
    if (s->elim) printf("c Eliminated variables: %llu\\n", (unsigned long long)s->elim->vars_eliminated);
}
