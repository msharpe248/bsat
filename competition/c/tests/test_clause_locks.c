#include "../include/solver.h"
#include <assert.h>
#include <stdio.h>

static void protect_reason(unsigned n, unsigned implied) {
    SolverOpts o = default_opts();o.reduce_fraction = 0;o.glue_lbd = 0;
    Solver *s = solver_new_with_opts(&o);assert(s);
    Lit *lits = malloc(n*sizeof *lits);assert(lits);
    for (Var v = 1; v <= n; ++v) { assert(solver_new_var(s) == v);lits[v-1] = mkLit(v, false); }
    // Force relocation of the live reason during collection.
    CRef pad = arena_alloc(s->arena, lits, n, true);assert(pad != INVALID_CLAUSE);
    arena_delete(s->arena, pad);
    CRef cr = arena_alloc(s->arena, lits, n, true);assert(cr != INVALID_CLAUSE);
    set_clause_lbd(s->arena, cr, n);
    s->learnts = malloc(sizeof *s->learnts);assert(s->learnts);
    s->learnts_size = s->num_learnts = 1;s->learnts[0] = cr;
    watch_add(s->watches, lits[0], cr, lits[1]);watch_add(s->watches, lits[1], cr, lits[0]);
    s->decision_level = 1;s->trail_lims[1] = 0;
    for (Var v = 1; v <= n; ++v) if (v != implied) {
        s->values[v] = FALSE;s->vars[v].level = 1;s->vars[v].trail_pos = s->trail_size;
        s->trail[s->trail_size++] = (Trail){mkLit(v,true),1};
    }
    assert(solver_propagate(s) == INVALID_CLAUSE);
    assert(s->values[implied] == TRUE && s->vars[implied].reason == cr);
    assert(var(CLAUSE_LITS(s->arena,cr)[0]) == implied);
    // Replay false watches while the clause is locked, then reduce and relocate.
    s->qhead = 0;assert(solver_propagate(s) == INVALID_CLAUSE);
    solver_reduce_db(s);assert(s->num_learnts == 1 && s->garbage_collections == 1);
    assert(s->vars[implied].reason == s->learnts[0]);
    assert(var(CLAUSE_LITS(s->arena,s->learnts[0])[0]) == implied);
    solver_backtrack(s,0);solver_reduce_db(s);
    assert(!s->num_learnts && s->stats.deleted_clauses == 1);
    free(lits);solver_free(s);
}

int main(void) {
    unsigned sizes[] = {3,32,257};
    for (unsigned k = 0; k < 3; ++k)
        for (unsigned v = 1; v <= sizes[k]; ++v) protect_reason(sizes[k],v);
    puts("PASS: active reason protection, every implied position, replay, GC and backtracking");
    return 0;
}
