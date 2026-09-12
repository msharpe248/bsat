#include "solver.h"
#include <assert.h>
#include <stdio.h>

/* Reduction must preserve the short learnt and its watches across collection,
   while longer unprotected clauses remain eligible for deletion. */
static void
check(bool retain)
{
    SolverOpts o = default_opts();

    assert(!o.retain_ternary);
    o.retain_ternary = retain;
    o.reduce_fraction = 0;
    o.max_lbd = 0;
    Solver *s = solver_new_with_opts(&o);

    assert(s);
    for (Var v = 1; v <= 7; ++v) {
        assert(solver_new_var(s) == v);
        Lit unit = mkLit(v, false);

        assert(solver_add_clause(s, &unit, 1));
    }
    Lit padding[128];

    for (unsigned i = 0; i < 128; ++i)
        padding[i] = mkLit(1, false);
    CRef dead = arena_alloc(s->arena, padding, 128, false);

    assert(dead != INVALID_CLAUSE);
    arena_delete(s->arena, dead);
    s->learnts = malloc(2 * sizeof *s->learnts);
    assert(s->learnts);
    s->learnts_size = 2;
    for (unsigned size = 3; size <= 4; ++size) {
        Lit lits[4];
        unsigned first = size == 3 ? 1 : 4;

        for (unsigned i = 0; i < size; ++i)
            lits[i] = mkLit(first + i, false);
        CRef cr = arena_alloc(s->arena, lits, size, true);

        assert(cr != INVALID_CLAUSE);
        set_clause_lbd(s->arena, cr, size);
        s->learnts[s->num_learnts++] = cr;
        watch_add(s->watches, lits[0], cr, lits[1]);
        watch_add(s->watches, lits[1], cr, lits[0]);
    }
    solver_reduce_db(s);
    assert(s->garbage_collections == 1);
    assert(s->num_learnts == (unsigned)retain);
    assert(s->stats.deleted_clauses == 2 - (unsigned)retain);
    for (unsigned v = 1; v <= 7; ++v) {
        WatchList *w = watch_list(s->watches, mkLit(v, false));

        assert(w->size == (unsigned)(retain && v <= 2));
        if (w->size) assert(w->watches[0].cref == s->learnts[0]);
    }
    solver_reduce_db(s);
    assert(s->num_learnts == (unsigned)retain);
    assert(solver_check_model(s));
    solver_free(s);
}

int
main(void)
{
    check(false);
    check(true);
    puts("PASS: ternary retention preserves watches across reduction and collection");
    return 0;
}
