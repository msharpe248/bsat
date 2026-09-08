#include "solver.h"
#include <assert.h>
#include <stdio.h>

static Solver *unit(void) {
    SolverOpts opts=default_opts();opts.probing=false;
    Solver *s=solver_new_with_opts(&opts);assert(s);
    opts.probing=true;assert(!s->opts.probing); /* Numeric options copied. */
    assert(solver_new_var(s)==1);
    Lit c=mkLit(1,false);assert(solver_add_clause(s,&c,1));c=neg(c);
    assert(solver_solve(s)==TRUE && solver_model_value(s,1)==TRUE);
    return s;
}
int main(void) {
    solver_free(NULL);assert(solver_model_value(NULL,1)==UNDEF);
    assert(!solver_new_with_opts(NULL));
    Solver *s=unit();Lit a=mkLit(1,true);
    assert(solver_solve_with_assumptions(s,&a,1)==FALSE);
    uint32_t count=0;const Lit *core=solver_conflict(s,&count);
    assert(count==1 && core && core[0]==neg(a));
    a=neg(a); /* The caller's assumption storage remains caller-owned. */
    assert(solver_solve(s)==TRUE && solver_model_value(s,1)==TRUE);
    core=solver_conflict(s,&count);assert(count==0);
    assert(solver_new_var(s)==2);Lit b=mkLit(2,true);
    assert(solver_add_clause(s,&b,1));assert(solver_solve(s)==TRUE);
    assert(solver_model_value(s,2)==FALSE);
    assert(solver_model_value(s,0)==UNDEF && solver_model_value(s,3)==UNDEF);
    solver_free(s);
    for(unsigned failure=0;failure<5;++failure) {
        s=unit();Lit invalid= failure==4 ? LIT_UNDEF : mkLit(2,false);
        if(failure==0)assert(!solver_add_clause(s,NULL,1));
        else if(failure==1)assert(!solver_add_clause(s,&invalid,1));
        else if(failure==2)assert(solver_solve_with_assumptions(s,NULL,1)==UNDEF);
        else assert(solver_solve_with_assumptions(s,&invalid,1)==UNDEF);
        assert(s->error);assert(solver_solve(s)==UNDEF);
        assert(!solver_new_var(s));assert(!solver_add_clause(s,NULL,0));
        solver_free(s);
    }
    s=solver_new();assert(s);assert(!solver_add_clause(s,NULL,0));
    assert(!s->error && solver_solve(s)==FALSE);solver_free(s);
    puts("PASS: API ownership, result/core lifetime, incremental input, invalid arguments and persistent errors");
}
