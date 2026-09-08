#include "solver.h"
#include <assert.h>
#include <stdio.h>

static Solver *instance(unsigned profile) {
    SolverOpts o=default_opts();o.reuse_learnts=true;o.probing=false;
    o.congruence=profile&1;o.equiv=profile&2;
    Solver *s=solver_new_with_opts(&o);assert(s);
    for(unsigned i=0;i<3;++i)assert(solver_new_var(s));
    return s;
}
static void congruence_history(void) {
    Solver *s=instance(1);assert(solver_new_var(s)==4);
    for(Var out=3;out<=4;++out) {
        Lit x[]={mkLit(out,true),mkLit(1,false)};
        Lit y[]={mkLit(out,true),mkLit(2,false)};
        Lit z[]={mkLit(out,false),mkLit(1,true),mkLit(2,true)};
        assert(solver_add_clause(s,x,2));assert(solver_add_clause(s,y,2));
        assert(solver_add_clause(s,z,3));
    }
    assert(solver_solve(s)==TRUE && s->stats.congruence_merges && !s->elim);
    for(unsigned bits=0;bits<16;++bits) {
        Lit a[4];for(unsigned j=0;j<4;++j)a[j]=mkLit(j+1,!(bits&(1u<<j)));
        bool and_value=(bits&3)==3;
        bool expected=((bool)(bits&4)==and_value) && ((bool)(bits&8)==and_value);
        uint64_t before=s->reused_solves;
        assert(solver_solve_with_assumptions(s,a,4)==(expected?TRUE:FALSE));
        assert(s->reused_solves==before+1 && s->input_clauses==6);
        if(expected)assert(solver_check_model(s));
    }
    assert(solver_solve(s)==TRUE && solver_check_model(s));solver_free(s);
}
int main(void) {
    congruence_history();
    for(unsigned profile=0;profile<4;++profile) {
        Solver *s=instance(profile);
        Lit c[]={mkLit(1,false),mkLit(2,false),mkLit(3,false)};
        assert(solver_add_clause(s,c,3));
        assert(solver_solve(s)==TRUE && !s->elim);
        uint64_t before=s->reused_solves;
        Lit a[]={mkLit(1,true),mkLit(2,true),mkLit(3,true)};
        assert(solver_solve_with_assumptions(s,a,3)==FALSE);
        assert(solver_solve(s)==TRUE && solver_check_model(s));
        assert(s->reused_solves==before+2);
        for(unsigned i=0;i<3;++i){solver_add_clause(s,a+i,1);assert(!s->error);}
        assert(solver_solve_with_assumptions(s,c,1)==FALSE);
        assert(solver_solve(s)==FALSE); /* permanent contradiction survives */
        solver_free(s);

        s=instance(profile);
        assert(solver_solve(s)==TRUE);
        solver_add_clause(s,NULL,0);assert(!s->error);
        assert(solver_solve_with_assumptions(s,c,1)==FALSE);
        assert(solver_solve(s)==FALSE);solver_free(s);
    }
    Solver *s=instance(2);
    Lit eq1[]={mkLit(1,true),mkLit(2,false)};
    Lit eq2[]={mkLit(1,false),mkLit(2,true)};
    assert(solver_add_clause(s,eq1,2));assert(solver_add_clause(s,eq2,2));
    assert(solver_solve(s)==TRUE && s->elim);
    uint64_t before=s->reused_solves;Lit a=mkLit(1,false);
    assert(solver_solve_with_assumptions(s,&a,1)==TRUE);
    assert(s->reused_solves==before && solver_check_model(s));
    assert(solver_model_value(s,1)==TRUE && solver_model_value(s,2)==TRUE);
    solver_free(s);
    puts("PASS: conditional UNSAT reuse, compatible preprocessing, permanent root contradiction and empty-clause persistence");
}
