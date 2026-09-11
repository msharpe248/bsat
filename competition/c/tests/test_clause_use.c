#include "solver.h"
#include <assert.h>
#include <stdio.h>

#ifdef BSAT_SEARCH_DIAGNOSTICS
static CRef
learned(Solver *s, unsigned size)
{
    Lit lits[] = {fromDimacs(1), fromDimacs(2), fromDimacs(3)};
    CRef cr = arena_alloc(s->arena, lits, size, true);
    assert(cr != INVALID_CLAUSE);
    s->learnts = realloc(s->learnts, (s->num_learnts + 1) * sizeof(CRef));
    assert(s->learnts);
    s->learnts[s->num_learnts++] = cr;
    s->learnts_size = s->num_learnts;
    set_clause_lbd(s->arena, cr, 8);
    CRef w = size == 2 ? arena_binary_watch_ref(cr) : cr;
    watch_add(s->watches, lits[0], w, lits[1]);
    watch_add(s->watches, lits[1], w, lits[0]);
    return cr;
}

static Solver *
make(void)
{
    SolverOpts o = default_opts();
    o.accounting = true;
    o.reduce_fraction = 0;
    o.probing = false;
    Solver *s = solver_new_with_opts(&o);
    assert(s);
    for (int i = 1; i <= 3; i++)
        assert(solver_new_var(s) == (Var)i);
    return s;
}

static void
binary_use(void)
{
    Solver *s = make();
    CRef cr = learned(s, 2);
    s->values[1] = FALSE;
    s->trail[s->trail_size++] = (Trail){fromDimacs(-1)};
    assert(solver_propagate(s) == INVALID_CLAUSE);
    assert(s->values[2] == TRUE && CLAUSE_HEADER(s->arena, cr)->units == 1);
    assert(s->accounting.use_binary_units == 1);
    solver_reduce_db(s);
    assert(s->accounting.use_kept == 1);
    assert(CLAUSE_HEADER(s->arena, cr)->recent_use == 0);
    solver_free(s);
}

static void
partitions(void)
{
    Solver *s = make();
    CRef a = learned(s, 3), b = learned(s, 3), c = learned(s, 2);
    ClauseHeader *h = CLAUSE_HEADER(s->arena, a);
    h->analyses = 1;
    h->recent_use = 2;
    h->scans = 9;
    CLAUSE_HEADER(s->arena, b)->scans = 4;
    CLAUSE_HEADER(s->arena, c)->units = 1;
    CLAUSE_HEADER(s->arena, c)->recent_use = 1;
    s->accounting.diagnostic_conflicts = 100;
    solver_reduce_db(s);
    assert(s->accounting.use_reduction_visits == 3 && s->accounting.use_deleted == 2 &&
           s->accounting.use_kept == 1);
    assert(s->accounting.use_deleted_recent_analysis == 1 &&
           s->accounting.use_deleted_recent_high_lbd == 1);
    assert(s->accounting.use_deleted_never == 1 && s->accounting.use_deleted_never_scans == 4);
    assert(s->accounting.use_deleted_scans == 13 && s->accounting.use_deleted_age_sum == 200);
    assert(s->garbage_collections == 1 && s->num_learnts == 1);
    h = CLAUSE_HEADER(s->arena, s->learnts[0]);
    assert(h->units == 1 && !h->recent_use);
    solver_reduce_db(s);
    assert(s->accounting.use_reduction_visits == 4);
    solver_free(s);
}
#endif
int
main(void)
{
#ifdef BSAT_SEARCH_DIAGNOSTICS
    binary_use();
    partitions();
    puts("PASS: learned binary reuse, reduction partitions, age, expiry and GC metadata");
#else
    puts("SKIP: compile BSAT_SEARCH_DIAGNOSTICS for clause-use counters");
#endif
}
