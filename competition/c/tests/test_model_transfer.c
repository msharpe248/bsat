#include "../include/solver.h"
#include "../include/local_search.h"
#include "../include/dimacs.h"
#include <assert.h>
#include <stdio.h>

static void check_transfer(Solver *s) {
    assert(!s->decision_level && s->trail_size==s->num_vars);
    assert(s->qhead==s->trail_size && !s->rephase.best_prefix_valid);
    for(Var v=1;v<=s->num_vars;++v) {
        assert(s->vars[v].level==0 && s->vars[v].reason==INVALID_CLAUSE);
        assert(s->binary_reasons[v]==LIT_UNDEF && s->vars[v].trail_pos==v-1);
        assert(var(s->trail[v-1].lit)==v);
        assert(lxor(s->values[v],sign(s->trail[v-1].lit))==TRUE);
    }
    assert(solver_check_model(s));
    uint64_t propagated=s->stats.propagations;
    assert(solver_propagate(s)==INVALID_CLAUSE && s->stats.propagations==propagated);
}

static void stale_metadata(void) {
    Solver *s=solver_new();assert(s);
    assert(dimacs_parse_string(s,"p cnf 4 3\n1 2 0\n-1 2 0\n-2 3 0\n")==DIMACS_OK);
    for(Var v=1;v<=4;++v) s->vars[v].polarity=true;
    LocalSearchState *ls=local_search_init(s);assert(ls);
    assert(local_search_run(s,ls,100,0.0));
    for(unsigned cycle=0;cycle<3;++cycle) {
        for(Var v=1;v<=4;++v) {
            s->vars[v].trail_pos=UINT32_MAX;s->vars[v].level=2;
            s->binary_reasons[v]=mkLit(v,true);s->vars[v].reason=s->clauses[0];
        }
        s->decision_level=2;s->trail_size=cycle;s->qhead=cycle;
        s->rephase.best_prefix_valid=true;
        local_search_copy_solution(s,ls);
        check_transfer(s);
    }
    local_search_free(ls);solver_free(s);
}

static void integrated(void) {
    for(unsigned queue=0;queue<2;++queue) {
        SolverOpts o=default_opts();o.local_search=true;o.ls_interval=1;
        o.probing=false;o.vmtf=queue;
        Solver *s=solver_new_with_opts(&o);assert(s);
        assert(dimacs_parse_string(s,"p cnf 2 3\n1 2 0\n1 -2 0\n-1 2 0\n")==DIMACS_OK);
        assert(solver_solve(s)==TRUE && s->local_search.successes==1);
        check_transfer(s);
        Lit a=mkLit(1,true);
        assert(solver_solve_with_assumptions(s,&a,1)==FALSE);
        assert(solver_solve(s)==TRUE && solver_check_model(s));
        assert(!solver_add_clause(s,&a,1) || solver_solve(s)==FALSE);
        solver_free(s);
    }
}

static void success_after_nonroot_backjump(void) {
    SolverOpts o=default_opts();o.local_search=true;o.ls_interval=1;o.probing=false;
    Solver *s=solver_new_with_opts(&o);assert(s);
    /* The first conflict learns (1 | 3), retaining decision 1=false at level 1. */
    assert(dimacs_parse_string(s,"p cnf 3 3\n1 2 3 0\n1 2 -3 0\n1 -2 3 0\n")==DIMACS_OK);
    assert(solver_solve(s)==TRUE && s->stats.conflicts==1);
    assert(s->local_search.calls==1 && s->local_search.successes==1);
    check_transfer(s);
    solver_free(s);
}

int main(void) {
    stale_metadata();integrated();success_after_nonroot_backjump();
    puts("PASS: complete local-search transfer metadata and subsequent API solves");
    return 0;
}
