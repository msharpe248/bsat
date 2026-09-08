#include "solver.h"
#include <assert.h>
#include <stdio.h>
static int cancel_once(void *state) {unsigned *n=state;return ++*n==3;}
static void add_module(Solver *s,Var selector,Var first) {
    const unsigned pigeons=8,holes=7;
    while(s->num_vars<first+pigeons*holes-1)assert(solver_new_var(s));
    for(unsigned p=0;p<pigeons;++p) {
        Lit c[8];c[0]=mkLit(selector,true);
        for(unsigned h=0;h<holes;++h)c[h+1]=mkLit(first+p*holes+h,false);
        assert(solver_add_clause(s,c,8));
    }
    for(unsigned h=0;h<holes;++h)for(unsigned p=0;p<pigeons;++p)for(unsigned q=p+1;q<pigeons;++q) {
        Lit c[]={mkLit(selector,true),mkLit(first+p*holes+h,true),mkLit(first+q*holes+h,true)};
        assert(solver_add_clause(s,c,3));
    }
}
/* Independent scan of the caller's permanent formula through DIMACS-like
   integer truth values, plus the active assumption. No solver_check_model. */
static void check_sat(Solver *s) {
    bool clause=false;
    for(size_t i=0;i<s->input_size;++i) {
        int lit=toDimacs(s->input[i]);
        if(!lit){assert(clause);clause=false;}
        else {lbool v=solver_model_value(s,(Var)abs(lit));assert(v!=UNDEF);clause|=(v==TRUE)==(lit>0);}
    }
}
int main(void) {
    unsigned queries=0,unknown=0;uint64_t conflicts=0,reductions=0,gc=0;
    for(unsigned profile=0;profile<4;++profile) {
        SolverOpts o=default_opts();o.reuse_learnts=true;o.probing=false;
        o.max_conflicts=127;o.reduce_interval=32;o.restart_first=16;o.luby_unit=16;
        o.chrono=profile&1;o.chrono_levels=0;o.vmtf=profile&2;o.alternating=profile&2;
        Solver *s=solver_new_with_opts(&o);assert(s);
        for(unsigned module=0;module<8;++module) {
            Var selector=solver_new_var(s);assert(selector);Var first=selector+1;
            add_module(s,selector,first);Lit active=mkLit(selector,false);
            if(module%3==0) {
                unsigned polls=0;solver_set_terminate(s,&polls,cancel_once);
                assert(solver_solve_with_assumptions(s,&active,1)==UNDEF && !s->error);
                solver_set_terminate(s,NULL,NULL);
            }
            lbool result;unsigned slices=0;
            do {
                /* Repeated tiny budgets need not make progress under reductions.
                   Exercise retries, then allow a longer contiguous search. */
                s->opts.max_conflicts=slices<8?127:32768;
                result=solver_solve_with_assumptions(s,&active,1);++queries;
                assert(!s->error && ++slices<2048);unknown+=result==UNDEF;
                conflicts+=s->stats.conflicts;reductions+=s->stats.reduces;gc+=s->garbage_collections>0;
            } while(result==UNDEF);
            /* Pigeonhole principle proves every activated module UNSAT. */
            assert(result==FALSE);
            uint32_t n=0;const Lit *core=solver_conflict(s,&n);assert(n==1&&core[0]==neg(active));
            slices=0;
            do {s->opts.max_conflicts=slices<8?127:32768;
                result=solver_solve(s);++queries;assert(!s->error&&++slices<2048);unknown+=result==UNDEF;
                conflicts+=s->stats.conflicts;reductions+=s->stats.reduces;gc+=s->garbage_collections>0;
            } while(result==UNDEF);
            /* All selectors false is always a base-formula witness. */
            assert(result==TRUE);check_sat(s);
        }
        solver_free(s);
    }
    assert(unknown && conflicts>1000 && reductions && gc);
    printf("PASS: %u hard incremental queries, %u budget slices, %llu conflicts, %llu reductions, %llu queries observing GC\n",queries,unknown,(unsigned long long)conflicts,(unsigned long long)reductions,(unsigned long long)gc);
}
