#include "../include/solver.h"
#include "../include/dimacs.h"
#include <assert.h>
#include <stdio.h>

static void assign(Solver *s, Lit lit) {
    Var v = var(lit);
    s->trail_lims[++s->decision_level] = s->trail_size;
    s->values[v] = sign(lit) ? FALSE : TRUE;
    s->vars[v].level = s->decision_level;
    s->vars[v].trail_pos = s->trail_size;
    s->trail[s->trail_size++] = (Trail){lit};
}

static unsigned copies(Solver *s, Lit a, Lit b) {
    WatchList *wl = watch_list(s->watches, a);unsigned n = 0;
    for (unsigned i = 0; i < wl->size; ++i)
        n += (is_binary_watch(wl->watches[i]) || is_arena_binary_watch(wl->watches[i])) && wl->watches[i].blocker == b;
    return n;
}

static void learned(bool queue) {
    SolverOpts o = default_opts();o.probing = false;o.vmtf = queue;
    Solver *s = solver_new_with_opts(&o);assert(s);
    assert(dimacs_parse_string(s, "p cnf 3 2\n-1 -2 3 0\n-1 -2 -3 0\n") == DIMACS_OK);
    Lit pad[] = {fromDimacs(1), fromDimacs(2), fromDimacs(3)};
    for (unsigned i = 0; i < 100; ++i) {
        CRef cr = arena_alloc(s->arena, pad, 3, true);assert(cr != INVALID_CLAUSE);
        arena_delete(s->arena, cr);
    }
    Lit assumptions[] = {fromDimacs(1), fromDimacs(2)};
    assert(solver_solve_with_assumptions(s, assumptions, 2) == FALSE);
    assert(s->num_learnts == 1 && CLAUSE_SIZE(s->arena, s->learnts[0]) == 2);
    assert(copies(s, fromDimacs(-1), fromDimacs(-2)) == 1);
    assert(copies(s, fromDimacs(-2), fromDimacs(-1)) == 1);
    CRef old = s->learnts[0];solver_collect_garbage(s);
    assert(s->garbage_collections && s->learnts[0] != old);
    solver_backtrack(s, 0);
    assign(s, fromDimacs(1));assert(solver_propagate(s) == INVALID_CLAUSE);
    assert(s->values[2] == FALSE && s->vars[2].reason == s->learnts[0]);
    assert(s->binary_reasons[2] == LIT_UNDEF);
    /* Preserve the default minimizer's arena coverage and inspection budget. */
    assign(s, fromDimacs(3));
    Lit minimize[] = {fromDimacs(-3), fromDimacs(-1), fromDimacs(2)};
    uint32_t size = 3;s->opts.minimize_budget = 1;
    assert(solver_minimize_clause(s, minimize, &size) == 0 && size == 3);
    s->opts.minimize_budget = 2;
    assert(solver_minimize_clause(s, minimize, &size) == 1 && size == 2);
    assert(minimize[0] == fromDimacs(-3) && minimize[1] == fromDimacs(-1));
    solver_reduce_db(s);assert(s->num_learnts == 1);
    solver_backtrack(s, 0);
    assign(s, fromDimacs(1));assign(s, fromDimacs(2));
    assert(solver_propagate(s) == s->learnts[0]);
    Lit learnt[4];uint32_t n;Level backtrack;
    solver_analyze(s, s->learnts[0], learnt, &n, &backtrack);
    assert(n == 2 && learnt[0] == fromDimacs(-2) && learnt[1] == fromDimacs(-1));
    assert(backtrack == 1);
    solver_backtrack(s, 0);
    solver_delete_clause(s, s->learnts[0]);
    assert(!copies(s, fromDimacs(-1), fromDimacs(-2)));
    assert(!copies(s, fromDimacs(-2), fromDimacs(-1)));
    assert(solver_solve(s) == TRUE && solver_check_model(s));
    solver_free(s);
}

static void mixed_deletion(void) {
    Solver *s = solver_new();assert(s);assert(solver_new_var(s));assert(solver_new_var(s));
    Lit lits[] = {fromDimacs(1), fromDimacs(2)};
    CRef refs[3];
    for (unsigned k = 0; k < 3; ++k) {
        refs[k] = arena_alloc(s->arena, lits, 2, k != 0);assert(refs[k] != INVALID_CLAUSE);
        CRef watch = k == 0 ? INVALID_CLAUSE : k == 1 ? refs[k] : arena_binary_watch_ref(refs[k]);
        watch_add(s->watches, lits[0], watch, lits[1]);
        watch_add(s->watches, lits[1], watch, lits[0]);
    }
    /* An earlier implicit copy must not hide the explicit watch being deleted. */
    solver_delete_clause(s, refs[1]);
    for (unsigned side = 0; side < 2; ++side) {
        WatchList *wl = watch_list(s->watches, lits[side]);assert(wl->size == 2);
        for (unsigned i = 0; i < wl->size; ++i) assert(is_binary_watch(wl->watches[i]) || (is_arena_binary_watch(wl->watches[i]) && watch_clause(wl->watches[i]) == refs[2]));
    }
    solver_delete_clause(s, refs[2]);
    assert(copies(s, lits[0], lits[1]) == 1 && copies(s, lits[1], lits[0]) == 1);
    solver_delete_clause(s, refs[0]);
    assert(!watch_list(s->watches, lits[0])->size && !watch_list(s->watches, lits[1])->size);
    solver_free(s);
}

static void vivified(void) {
    SolverOpts o = default_opts();o.inprocess = true;o.inprocess_interval = 1;
    Solver *s = solver_new_with_opts(&o);assert(s);
    assert(dimacs_parse_string(s, "p cnf 3 2\n-1 -2 3 0\n-1 -2 -3 0\n") == DIMACS_OK);
    Lit lits[] = {fromDimacs(-1), fromDimacs(-2), fromDimacs(3)};
    CRef cr = arena_alloc(s->arena, lits, 3, true);assert(cr != INVALID_CLAUSE);
    s->learnts = malloc(sizeof *s->learnts);assert(s->learnts);
    s->learnts[0] = cr;s->num_learnts = s->learnts_size = 1;
    watch_add(s->watches, lits[0], cr, lits[1]);
    watch_add(s->watches, lits[1], cr, lits[0]);
    s->stats.conflicts = 1;
    assert(solver_simplify(s));
    assert(clause_deleted(s->arena, cr) && CLAUSE_SIZE(s->arena, s->learnts[0]) == 2);
    assert(copies(s, lits[0], lits[1]) == 1 && copies(s, lits[1], lits[0]) == 1);
    assign(s, fromDimacs(1));assert(solver_propagate(s) == INVALID_CLAUSE);
    assert(s->values[2] == FALSE && s->binary_reasons[2] == LIT_UNDEF);
    solver_free(s);
}

int main(void) {
    assert(sizeof(Watch) == 8);
    Watch edge = {arena_binary_watch_ref(MAX_CLAUSES-1), fromDimacs(1)};
    assert(is_arena_binary_watch(edge) && watch_clause(edge) == MAX_CLAUSES-1);
    assert(!is_binary_watch(edge) && watch_clause(make_binary_watch(fromDimacs(1))) == INVALID_CLAUSE);
    assert(!arena_init((size_t)MAX_CLAUSES+1));
    assert(!arena_init(SIZE_MAX));
    Arena fake = {.size = MAX_CLAUSES-1, .capacity = SIZE_MAX};
    assert(arena_alloc(&fake, NULL, 0, true) == INVALID_CLAUSE);
    assert(!arena_reserve(&fake, (size_t)MAX_CLAUSES+1));
    fake.size = SIZE_MAX;
    assert(arena_alloc(&fake, NULL, 0, true) == INVALID_CLAUSE);
    learned(false);learned(true);mixed_deletion();vivified();
    puts("PASS: compact learned binary propagation, analysis, GC, deletion and rebuilding");
    return 0;
}
