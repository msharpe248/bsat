/* Bounded resolution elimination. A default pivot and one polarity of removed
 * clauses reconstruct models; resolvents are staged before arena mutations. */
#include "../include/solver.h"
#include "../include/reduction_sort.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

void
elim_init(Solver *s)
{
    if (s->elim) return;
    s->elim = calloc(1, sizeof *s->elim);
    if (!s->elim) {
        s->error = true;
        return;
    }
    s->elim->occs_capacity = 2 * (s->num_vars + 1);
    s->elim->elim_capacity = s->num_vars + 1;
    s->elim->occs = calloc(s->elim->occs_capacity, sizeof(OccList));
    s->elim->eliminated = calloc(s->elim->elim_capacity, sizeof(bool));
    if (!s->elim->occs || !s->elim->eliminated) s->error = true;
}

void
elim_free(Solver *s)
{
    ElimState *e = s->elim;

    if (!e) return;
    if (e->occs)
        for (uint32_t i = 0; i < e->occs_capacity; ++i)
            free(e->occs[i].clauses);
    for (uint32_t i = 0; i < e->stack_size; ++i)
        free(e->stack[i].clause);
    free(e->occs);
    free(e->eliminated);
    free(e->stack);
    free(e->resolvent_crefs);
    free(e);
    s->elim = NULL;
}

void
elim_clear_occs(Solver *s)
{
    if (s->elim && s->elim->occs)
        for (uint32_t i = 0; i < s->elim->occs_capacity; ++i)
            s->elim->occs[i].size = 0;
}

void
elim_add_occ(Solver *s, Lit l, CRef cr)
{
    OccList *o = &s->elim->occs[l];

    if (o->size == o->capacity) {
        uint32_t cap = o->capacity ? o->capacity * 2 : 8;
        CRef *p = realloc(o->clauses, cap * sizeof *p);

        if (!p) {
            s->error = true;
            return;
        }
        o->clauses = p;
        o->capacity = cap;
    }
    o->clauses[o->size++] = cr;
}

void
elim_remove_occ(Solver *s, Lit l, CRef cr)
{
    OccList *o = &s->elim->occs[l];

    for (uint32_t i = 0; i < o->size; ++i)
        if (o->clauses[i] == cr) {
            o->clauses[i] = o->clauses[--o->size];
            return;
        }
}

void
elim_build_occs(Solver *s)
{
    elim_init(s);
    if (s->error) return;
    elim_clear_occs(s);
    s->elim->occs_complete = false;
    for (uint32_t i = 0; i < s->num_clauses; ++i) {
        CRef cr = s->clauses[i];

        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        for (uint32_t j = 0; j < CLAUSE_SIZE(s->arena, cr); ++j) {
            s->work++;
            if ((s->work & 1023) == 0 && solver_budget_exhausted(s)) return;
            elim_add_occ(s, CLAUSE_LITS(s->arena, cr)[j], cr);
        }
    }
    s->elim->occs_complete = !s->error;
}

bool
elim_is_tautology(const Lit *a, uint32_t na, const Lit *b, uint32_t nb, Var v)
{
    for (uint32_t i = 0; i < na; ++i)
        if (var(a[i]) != v)
            for (uint32_t j = 0; j < nb; ++j)
                if (var(b[j]) != v && a[i] == neg(b[j])) return true;
    return false;
}

static void
clean_occ(Solver *s, OccList *o)
{
    uint32_t n = 0;

    for (uint32_t i = 0; i < o->size; ++i)
        if (!clause_deleted(s->arena, o->clauses[i])) o->clauses[n++] = o->clauses[i];
    o->size = n;
}

enum { PAIR_STOP = -1, PAIR_TAUTOLOGY, PAIR_OK };

/* Root preprocessing owns the otherwise idle minimizer scratch. Charge literal
   visits and mandatory mark cleanup; never leave marks across a pair or abort. */
static int
resolve_pair(Solver *s, const Lit *a, uint32_t na, const Lit *b, uint32_t nb, Var pivot, Lit *out,
             uint32_t *size)
{
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
                status = PAIR_STOP;
                break;
            }
            Lit l = lits[i];
            Var v = var(l);

            if (v == pivot) continue;
            uint8_t mark = 1 + sign(l);

            if (s->seen[v]) {
                if (s->seen[v] != mark) {
                    status = PAIR_TAUTOLOGY;
                    break;
                }
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

bool
elim_bounded_tautology(Solver *s, const Lit *a, uint32_t na, const Lit *b, uint32_t nb, Var pivot)
{
    return resolve_pair(s, a, na, b, nb, pivot, NULL, NULL) == PAIR_TAUTOLOGY;
}

static CRef
find_ternary(Solver *s, OccList *occs, Lit a, Lit b)
{
    for (uint32_t i = 0; i < occs->size; ++i) {
        ++s->work;
        if ((s->work & 1023) ? solver_budget_exhausted(s) : solver_budget_exhausted_now(s))
            return INVALID_CLAUSE;
        CRef cr = occs->clauses[i];

        if (CLAUSE_SIZE(s->arena, cr) != 3) continue;
        Lit *lits = CLAUSE_LITS(s->arena, cr);
        bool has_a = false, has_b = false;

        for (unsigned j = 0; j < 3; ++j) {
            has_a |= lits[j] == a;
            has_b |= lits[j] == b;
        }
        if (has_a && has_b) return cr;
    }
    return INVALID_CLAUSE;
}

static unsigned
find_gate(Solver *s, Var v, CRef gate[8])
{
    /* Pure elimination needs no definition search, including high-fanout pivots. */
    if (!s->elim->occs[mkLit(v, false)].size || !s->elim->occs[mkLit(v, true)].size) return 0;
    for (unsigned polarity = 0; polarity < 2; ++polarity) {
        OccList *outputs = &s->elim->occs[mkLit(v, polarity)];
        OccList *inputs = &s->elim->occs[mkLit(v, !polarity)];

        for (uint32_t i = 0; i < outputs->size; ++i) {
            if (solver_budget_exhausted(s)) return 0;
            CRef cr = outputs->clauses[i];
            uint32_t size = CLAUSE_SIZE(s->arena, cr);

            if (size < 2 || size > 8) continue;
            Lit *lits = CLAUSE_LITS(s->arena, cr);
            unsigned n = 1;

            gate[0] = cr;
            for (uint32_t j = 0; j < size; ++j) {
                if (var(lits[j]) == v) continue;
                CRef found = INVALID_CLAUSE;

                for (uint32_t k = 0; k < inputs->size; ++k) {
                    ++s->work;
                    if ((s->work & 1023) ? solver_budget_exhausted(s)
                                         : solver_budget_exhausted_now(s))
                        return 0;
                    CRef candidate = inputs->clauses[k];

                    if (CLAUSE_SIZE(s->arena, candidate) != 2) continue;
                    Lit *binary = CLAUSE_LITS(s->arena, candidate);

                    if (binary[0] == neg(lits[j]) || binary[1] == neg(lits[j])) {
                        found = candidate;
                        break;
                    }
                }
                if (found == INVALID_CLAUSE) break;
                gate[n++] = found;
            }
            if (n == size) return n;
        }
    }
    OccList *positive = &s->elim->occs[mkLit(v, false)];
    OccList *negative = &s->elim->occs[mkLit(v, true)];

    for (uint32_t i = 0; i < positive->size; ++i) {
        if (solver_budget_exhausted(s)) return 0;
        CRef cr = positive->clauses[i];

        if (CLAUSE_SIZE(s->arena, cr) != 3) continue;
        Lit other[2], *lits = CLAUSE_LITS(s->arena, cr);
        unsigned n = 0;

        for (unsigned j = 0; j < 3; ++j)
            if (var(lits[j]) != v) other[n++] = lits[j];
        if (n != 2) continue;
        gate[0] = cr;
        gate[1] = find_ternary(s, positive, neg(other[0]), neg(other[1]));
        if (gate[1] == INVALID_CLAUSE) continue;
        gate[2] = find_ternary(s, negative, neg(other[0]), other[1]);
        if (gate[2] == INVALID_CLAUSE) continue;
        gate[3] = find_ternary(s, negative, other[0], neg(other[1]));
        if (gate[3] != INVALID_CLAUSE) return 4;
    }
    return 0;
}

static bool
gate_parent(CRef a, CRef b, const CRef *gate, unsigned size)
{
    /* A recognized gate determines its output for every input assignment.
       Resolving external occurrences against the definition is sufficient;
       pairs of two external occurrences add only redundant resolvents. */
    if (!size) return true;
    for (unsigned i = 0; i < size; ++i)
        if (gate[i] == a || gate[i] == b) return true;
    return false;
}

int
elim_cost(Solver *s, Var v)
{
    if (!s->elim || s->values[v] != UNDEF || s->elim->eliminated[v]) return -1;
    if (solver_budget_exhausted(s)) return -1;
    OccList *p = &s->elim->occs[mkLit(v, false)], *n = &s->elim->occs[mkLit(v, true)];

    clean_occ(s, p);
    clean_occ(s, n);
    if (!p->size && !n->size) return -1;
    if ((!s->opts.retained_elim || (p->size && n->size)) &&
        (p->size > s->opts.elim_max_occ || n->size > s->opts.elim_max_occ))
        return -1;
    uint64_t count = 0;
    CRef gate[8];
    unsigned gate_size = s->opts.retained_elim ? find_gate(s, v, gate) : 0;

    for (uint32_t i = 0; i < p->size; ++i)
        for (uint32_t j = 0; j < n->size; ++j) {
            CRef a = p->clauses[i], b = n->clauses[j];
            if (!gate_parent(a, b, gate, gate_size)) continue;
            uint32_t na = CLAUSE_SIZE(s->arena, a), nb = CLAUSE_SIZE(s->arena, b);
            int result = resolve_pair(s, CLAUSE_LITS(s->arena, a), na, CLAUSE_LITS(s->arena, b), nb,
                                      v, NULL, NULL);

            if (result == PAIR_STOP) return -1;
            if (result == PAIR_OK) ++count;
            if (count > INT_MAX || count > (uint64_t)p->size + n->size + s->opts.elim_grow)
                return -1;
        }
    return (int)count;
}

/* On success the stack owns the allocation; on failure the caller still does. */
static bool
save_owned(Solver *s, Var v, Lit *lits, uint32_t size)
{
    elim_init(s);
    if (s->error) return false;
    ElimState *e = s->elim;

    if (e->stack_size == e->stack_capacity) {
        if (e->stack_capacity > UINT32_MAX / 2) {
            s->error = true;
            return false;
        }
        uint32_t cap = e->stack_capacity ? e->stack_capacity * 2 : 32;
        ElimEntry *p = realloc(e->stack, cap * sizeof *p);

        if (!p) {
            s->error = true;
            return false;
        }
        e->stack = p;
        e->stack_capacity = cap;
    }
    e->stack[e->stack_size++] = (ElimEntry){v, lits, size};
    return true;
}

/* BCE and equivalence callers retain their input buffers: keep copy semantics. */
bool
elim_save(Solver *s, Var v, const Lit *lits, uint32_t size)
{
    if (s->error) return false;
    Lit *copy = malloc((size ? size : 1) * sizeof *copy);

    if (!copy) {
        s->error = true;
        return false;
    }
    if (size) memcpy(copy, lits, size * sizeof *copy);
    if (save_owned(s, v, copy, size)) return true;
    free(copy);
    return false;
}

static bool
staging_exhausted(Solver *s)
{
    return (s->work & 1023) ? solver_budget_exhausted(s) : solver_budget_exhausted_now(s);
}

/* A short clause can subsume another original clause, or remove one
   complementary literal by self-subsuming resolution. Root-satisfied clauses
   are left alone, including every live root reason. */
static void
strengthen_from_clause(Solver *s, CRef source)
{
    uint32_t source_size = CLAUSE_SIZE(s->arena, source);

    if (source_size < 2 || source_size > 8) return;
    Lit clause[8];
    unsigned first = 0, second = 1;

    memcpy(clause, CLAUSE_LITS(s->arena, source), source_size * sizeof *clause);
    if (s->elim->occs[clause[second]].size < s->elim->occs[clause[first]].size) {
        first = 1;
        second = 0;
    }
    for (unsigned i = 2; i < source_size; ++i) {
        if (s->elim->occs[clause[i]].size < s->elim->occs[clause[first]].size) {
            second = first;
            first = i;
        } else if (s->elim->occs[clause[i]].size < s->elim->occs[clause[second]].size)
            second = i;
    }
    for (unsigned side = 0; side < 2; ++side) {
        Lit anchor = clause[side ? second : first];
        uint32_t end = s->elim->occs[anchor].size;

        for (uint32_t i = 0; i < end; ++i) {
            if (s->error || s->result == FALSE || solver_budget_exhausted(s)) return;
            CRef cr = s->elim->occs[anchor].clauses[i];

            if (cr == source || clause_deleted(s->arena, cr)) continue;
            Lit *lits = CLAUSE_LITS(s->arena, cr);
            uint32_t size = CLAUSE_SIZE(s->arena, cr), remove = UINT32_MAX;
            bool satisfied = false, subsumed = false;
            unsigned missing = 0;

            if (size < source_size) continue;
            for (uint32_t j = 0; j < size; ++j) {
                ++s->work;
                if (!(s->work & 1023) && solver_budget_exhausted(s)) return;
                if (lxor(s->values[var(lits[j])], sign(lits[j])) == TRUE) satisfied = true;
            }
            if (satisfied) continue;
            for (uint32_t k = 0; k < source_size; ++k) {
                bool present = false;
                uint32_t complement = UINT32_MAX;

                for (uint32_t j = 0; j < size; ++j) {
                    ++s->work;
                    if (!(s->work & 1023) && solver_budget_exhausted(s)) return;
                    if (lits[j] == clause[k]) present = true;
                    if (lits[j] == neg(clause[k])) complement = j;
                }
                if (present) continue;
                if (++missing > 1 || complement == UINT32_MAX) {
                    missing = 2;
                    break;
                }
                remove = complement;
            }
            if (missing > 1) continue;
            subsumed = !missing;
            if (satisfied) continue;
            if (subsumed) {
                if (!clause_learned(s->arena, source)) solver_delete_clause(s, cr);
                continue;
            }
            if (remove == UINT32_MAX) continue;
            Lit *shorter = malloc((size ? size : 1) * sizeof *shorter);

            if (!shorter) {
                s->error = true;
                return;
            }
            uint32_t n = 0;

            for (uint32_t j = 0; j < size; ++j)
                if (j != remove) shorter[n++] = lits[j];
            proof_add_clause(s, shorter, n);
            uint32_t before = s->num_clauses;

            s->internal_add = true;
            solver_add_clause(s, shorter, n);
            s->internal_add = false;
            free(shorter);
            if (s->error) return;
            for (uint32_t k = before; k < s->num_clauses; ++k) {
                CRef added = s->clauses[k];

                for (uint32_t j = 0; j < CLAUSE_SIZE(s->arena, added); ++j)
                    elim_add_occ(s, CLAUSE_LITS(s->arena, added)[j], added);
            }
            solver_delete_clause(s, cr);
        }
    }
}

bool
elim_eliminate_var(Solver *s, Var v)
{
    int cost = elim_cost(s, v);

    if (cost < 0) return false;
    OccList *p = &s->elim->occs[mkLit(v, false)], *n = &s->elim->occs[mkLit(v, true)];
    CRef gate[8];
    unsigned gate_size = s->opts.retained_elim ? find_gate(s, v, gate) : 0;

    if (solver_budget_exhausted(s)) return false;
    uint32_t count = p->size + n->size;
    CRef *removed = malloc(count * sizeof *removed);
    Lit **res = calloc((size_t)cost + 1, sizeof *res);
    uint32_t *sizes = calloc((size_t)cost + 1, sizeof *sizes);
    Lit *saved = NULL;
    uint32_t saved_size = 0, nr = 0;
    bool eliminated = false;

    if (!removed || !res || !sizes) {
        s->error = true;
        goto done;
    }
    if (p->size) memcpy(removed, p->clauses, p->size * sizeof *removed);
    if (n->size) memcpy(removed + p->size, n->clauses, n->size * sizeof *removed);
    uint64_t positive_words = 0, negative_words = 0;

    for (uint32_t i = 0; i < p->size; ++i)
        positive_words += (uint64_t)CLAUSE_SIZE(s->arena, p->clauses[i]) + 1;
    for (uint32_t i = 0; i < n->size; ++i)
        negative_words += (uint64_t)CLAUSE_SIZE(s->arena, n->clauses[i]) + 1;
    bool save_negative = negative_words < positive_words;
    OccList *kept = save_negative ? n : p;
    uint64_t words = 2 + (save_negative ? negative_words : positive_words);

    if (words > UINT32_MAX || words > SIZE_MAX / sizeof *saved) goto done;
    saved_size = (uint32_t)words;
    saved = malloc(saved_size * sizeof *saved);
    if (!saved) {
        s->error = true;
        goto done;
    }
    // Default satisfies the omitted polarity. A saved clause can override it;
    // all resolvents then guarantee satisfaction of every omitted parent.
    Lit defaults[] = {mkLit(v, !save_negative), 0};
    uint32_t at = 0;

    for (unsigned i = 0; i < 2; ++i) {
        if (solver_budget_exhausted(s)) goto done;
        saved[at++] = defaults[i];
        ++s->work;
        if (staging_exhausted(s)) goto done;
    }
    for (uint32_t i = 0; i < kept->size; ++i) {
        uint32_t z = CLAUSE_SIZE(s->arena, kept->clauses[i]);
        const Lit *lits = CLAUSE_LITS(s->arena, kept->clauses[i]);

        for (uint32_t j = 0; j < z;) {
            if (solver_budget_exhausted(s)) goto done;
            uint32_t chunk = MIN(z - j, 1024u - (uint32_t)(s->work & 1023));

            if (s->work_limit && s->work_limit - s->work < chunk)
                chunk = (uint32_t)(s->work_limit - s->work);
            memcpy(saved + at, lits + j, chunk * sizeof *saved);
            at += chunk;
            j += chunk;
            s->work += chunk;
            if (staging_exhausted(s)) goto done;
        }
        saved[at++] = 0;
        ++s->work;
        if (staging_exhausted(s)) goto done;
    }
    for (uint32_t i = 0; i < p->size; ++i)
        for (uint32_t j = 0; j < n->size; ++j) {
            CRef ca = p->clauses[i], cb = n->clauses[j];
            if (!gate_parent(ca, cb, gate, gate_size)) continue;
            uint32_t na = CLAUSE_SIZE(s->arena, ca), nb = CLAUSE_SIZE(s->arena, cb);
            Lit *a = CLAUSE_LITS(s->arena, ca), *b = CLAUSE_LITS(s->arena, cb);
            Lit *r = malloc(((size_t)na + nb) * sizeof *r);

            if (!r) {
                s->error = true;
                goto done;
            }
            uint32_t z = 0;
            int result = resolve_pair(s, a, na, b, nb, v, r, &z);

            if (result != PAIR_OK) {
                free(r);
                if (result == PAIR_STOP) goto done;
                continue;
            }
            res[nr] = r;
            sizes[nr++] = z;
        }
    // Pure elimination may stage only two words: still check time before commit.
    if (solver_budget_exhausted_now(s)) goto done;
    if (!save_owned(s, v, saved, saved_size)) goto done;
    saved = NULL; // Ownership transferred; cleanup must not free the stack record.
    uint32_t added_start = s->num_clauses;
    for (uint32_t i = 0; i < nr && !s->error && s->result != FALSE; ++i) {
        proof_add_clause(s, res[i], sizes[i]);
        uint32_t before = s->num_clauses;

        s->internal_add = true;
        solver_add_clause(s, res[i], sizes[i]);
        s->internal_add = false;
        uint32_t after = s->num_clauses;

        for (uint32_t k = before; k < after; ++k) {
            CRef cr = s->clauses[k];

            for (uint32_t j = 0; j < CLAUSE_SIZE(s->arena, cr); ++j)
                elim_add_occ(s, CLAUSE_LITS(s->arena, cr)[j], cr);
        }
        s->elim->resolvents_added++;
    }
    if (!s->error) {
        for (uint32_t i = 0; i < count; ++i)
            solver_delete_clause(s, removed[i]);
        s->elim->eliminated[v] = true;
        s->elim->vars_eliminated++;
        s->elim->clauses_removed += count;
        eliminated = true;
        uint32_t added_end = s->num_clauses;

        for (uint32_t i = added_start; s->opts.retained_elim && i < added_end; ++i) {
            if (s->error || s->result == FALSE || solver_budget_exhausted(s)) break;
            CRef cr = s->clauses[i];

            if (!clause_deleted(s->arena, cr) && CLAUSE_SIZE(s->arena, cr) <= 8)
                strengthen_from_clause(s, cr);
        }
    }
done:
    for (uint32_t i = 0; i < nr; ++i)
        free(res[i]);
    free(res);
    free(sizes);
    free(removed);
    free(saved);
    return eliminated;
}

static bool
elimination_sort_exhausted(void *state)
{
    Solver *s = state;

    ++s->work;
    return (s->work & 1023) ? solver_budget_exhausted(s) : solver_budget_exhausted_now(s);
}

uint32_t
elim_preprocess_frozen(Solver *s, const Lit *frozen, uint32_t n_frozen)
{
    if (!s || (n_frozen && !frozen)) {
        if (s) s->error = true;
        return 0;
    }
    if (!s->opts.retained_elim || s->decision_level || s->has_solved) return 0;
    for (uint32_t i = 0; i < n_frozen; ++i) {
        if (solver_budget_exhausted(s)) return 0;
        if (!var(frozen[i]) || var(frozen[i]) > s->num_vars) {
            s->error = true;
            return 0;
        }
    }
    uint32_t end = s->num_clauses;

    for (uint32_t i = 0; i < s->trail_size; ++i) {
        if (solver_budget_exhausted(s)) break;
        Lit unit = s->trail[i].lit;
        Var v = var(unit);

        /* Root assignments without reasons are input units or already logged
           learned units. Earlier elimination passes also clear logged reasons. */
        if (!s->proof_journal || s->proof_file || s->vars[v].reason != INVALID_CLAUSE ||
            s->binary_reasons[v] != LIT_UNDEF)
            proof_add_clause(s, &unit, 1);
        if (s->error) break;
        s->vars[v].reason = INVALID_CLAUSE;
        s->binary_reasons[v] = LIT_UNDEF;
    }
    for (uint32_t i = 0; i < end; ++i) {
        if (s->error || s->result == FALSE || solver_budget_exhausted(s)) break;
        CRef cr = s->clauses[i];

        if (clause_deleted(s->arena, cr)) continue;
        uint32_t size = CLAUSE_SIZE(s->arena, cr), assigned = 0;
        Lit *lits = CLAUSE_LITS(s->arena, cr);
        bool satisfied = false;

        for (uint32_t j = 0; j < size; ++j) {
            ++s->work;
            if (!(s->work & 1023) && solver_budget_exhausted(s)) break;
            lbool value = lxor(s->values[var(lits[j])], sign(lits[j]));

            if (value == TRUE) satisfied = true;
            if (value == FALSE) ++assigned;
        }
        if (solver_budget_exhausted(s)) break;
        if (satisfied) {
            solver_delete_clause(s, cr);
            continue;
        }
        if (!assigned) continue;
        Lit *shorter = malloc((size ? size : 1) * sizeof *shorter);

        if (!shorter) {
            s->error = true;
            break;
        }
        uint32_t n = 0;

        for (uint32_t j = 0; j < size; ++j)
            if (s->values[var(lits[j])] == UNDEF) shorter[n++] = lits[j];
        proof_add_clause(s, shorter, n);
        s->internal_add = true;
        solver_add_clause(s, shorter, n);
        s->internal_add = false;
        free(shorter);
        if (s->error) break;
        solver_delete_clause(s, cr);
    }
    if (solver_propagate(s) != INVALID_CLAUSE) s->result = FALSE;
    elim_build_occs(s);
    uint32_t count = 0;

    if (!s->elim || !s->elim->occs_complete) return 0;
    uint8_t *protected = n_frozen ? calloc((size_t)s->num_vars + 1, 1) : NULL;

    if (n_frozen && !protected) {
        s->error = true;
        return 0;
    }
    for (uint32_t i = 0; i < n_frozen; ++i) {
        if (solver_budget_exhausted(s)) {
            free(protected);
            return 0;
        }
        protected[var(frozen[i])] = 1;
    }
    for (uint32_t i = 0; i < s->num_learnts; ++i) {
        if (s->error || s->result == FALSE || solver_budget_exhausted(s)) break;
        CRef cr = s->learnts[i];

        if (!clause_deleted(s->arena, cr)) strengthen_from_clause(s, cr);
    }
    if (s->error || s->result == FALSE || solver_budget_exhausted(s)) {
        free(protected);
        return 0;
    }
    size_t entries = s->num_vars ? s->num_vars : 1;
    ClauseScore *order =
        entries <= SIZE_MAX / sizeof *order ? malloc(entries * sizeof *order) : NULL;
    uint32_t n = 0;

    if (!order) {
        free(protected);
        s->error = true;
        return 0;
    }
    for (Var v = 1; v <= s->num_vars; ++v) {
        if (solver_budget_exhausted(s)) break;
        ++s->work;
        if (s->values[v] != UNDEF || s->elim->eliminated[v] || (protected && protected[v]))
            continue;
        uint64_t p = s->elim->occs[mkLit(v, false)].size;
        uint64_t q = s->elim->occs[mkLit(v, true)].size;

        uint64_t score = p && q ? 1 + p * q + p + q : 0;

        order[n++] = (ClauseScore){v, (uint32_t)MIN(score, UINT32_MAX),
                                   p && q ? -(double)v : (double)(p + q)};
    }
    if (!bsat_sort_clause_scores_bounded(order, n, elimination_sort_exhausted, s)) {
        free(order);
        free(protected);
        return 0;
    }
    for (uint32_t i = 0; i < n && !s->error && s->result != FALSE; ++i) {
        Var v = order[i].cref;
        if (solver_budget_exhausted(s)) break;
        if ((!protected || !protected[v]) && elim_eliminate_var(s, v)) count++;
        if (solver_propagate(s) != INVALID_CLAUSE) s->result = FALSE;
    }
    free(order);
    free(protected);
    return count;
}

uint32_t
elim_preprocess(Solver *s)
{
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

void
elim_extend_model(Solver *s)
{
    for (Var v = 1; v <= s->num_vars; ++v)
        if (s->values[v] == UNDEF) s->values[v] = TRUE;
    if (!s->elim) return;
    for (uint32_t i = s->elim->stack_size; i > 0;) {
        ElimEntry *e = &s->elim->stack[--i];
        bool satisfied = false;
        Lit pivot = 0;

        for (uint32_t j = 0; j < e->clause_size; ++j) {
            Lit l = e->clause[j];

            if (!l) {
                if (!satisfied && pivot) s->values[e->var] = sign(pivot) ? FALSE : TRUE;
                satisfied = false;
                pivot = 0;
            } else if (var(l) == e->var)
                pivot = l;
            else if (lxor(s->values[var(l)], sign(l)) == TRUE)
                satisfied = true;
        }
    }
}

bool
elim_is_eliminated(const Solver *s, Var v)
{
    return s->elim && v < s->elim->elim_capacity && s->elim->eliminated[v];
}

OccList *
elim_get_occs(Solver *s, Lit l)
{
    return s->elim && l < s->elim->occs_capacity ? &s->elim->occs[l] : NULL;
}

void
elim_print_stats(const Solver *s)
{
    if (s->elim)
        printf("c Eliminated variables: %llu\\n", (unsigned long long)s->elim->vars_eliminated);
}
