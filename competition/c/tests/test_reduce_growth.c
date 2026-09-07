#include "../include/solver.h"
#include <assert.h>
#include <stdio.h>

static void schedule(void) {
    SolverOpts o=default_opts();assert(!o.reduce_increment);
    Solver *s=solver_new_with_opts(&o);assert(s);
    for(uint64_t c=1;c<=30000;++c) {
        s->stats.conflicts=c;
        assert(solver_should_reduce(s)==(c%2000==0));
    }
    s->opts.reduce_increment=1000;s->reduce_span=2000;s->reduce_limit=2000;
    const uint64_t expected[]={2000,5000,9000,14000,20000,27000};unsigned at=0;
    for(uint64_t c=1;c<=30000;++c) {
        s->stats.conflicts=c;bool due=at<6 && c==expected[at];
        assert(solver_should_reduce(s)==due);if(due)++at;
    }
    assert(at==6 && s->reduce_limit==35000 && s->reduce_span==8000);
    s->stats.conflicts=UINT64_MAX-10;s->reduce_limit=s->stats.conflicts;
    s->reduce_span=UINT64_MAX-1;
    assert(solver_should_reduce(s));
    assert(s->reduce_span==UINT64_MAX && s->reduce_limit==UINT64_MAX);
    s->stats.conflicts=UINT64_MAX;assert(!solver_should_reduce(s));
    solver_free(s);
}

static void search(bool equiv, bool chrono, bool queue) {
    SolverOpts o=default_opts();o.probing=false;o.equiv=equiv;o.chrono=chrono;
    o.chrono_levels=0;o.vmtf=queue;o.reduce_interval=1;o.reduce_increment=1;
    o.glue_lbd=0;o.max_lbd=1;o.reduce_fraction=0;
    Solver *s=solver_new_with_opts(&o);assert(s);
    for(unsigned v=0;v<10;++v)assert(solver_new_var(s));
    /* Exclude every eight-variable assignment. The independent alias forces
       SCC replacement before a search requiring multiple reductions. */
    for(unsigned bits=0;bits<256;++bits) {
        Lit clause[8];for(unsigned v=0;v<8;++v)clause[v]=mkLit(v+1,(bits>>v)&1);
        assert(solver_add_clause(s,clause,8));
    }
    Lit a[]={mkLit(9,1),mkLit(10,0)},b[]={mkLit(9,0),mkLit(10,1)};
    assert(solver_add_clause(s,a,2) && solver_add_clause(s,b,2));
    for(unsigned repeat=0;repeat<3;++repeat) {
        Lit assumption=mkLit(9,0);
        lbool result=repeat==1?solver_solve_with_assumptions(s,&assumption,1):solver_solve(s);
        assert(result==FALSE && !s->error);
        uint64_t learned=s->stats.learned_clauses, expected=0;
        for(uint64_t limit=1,step=2;limit<=learned;limit+=step,++step)++expected;
        assert(expected>1 && s->stats.reduces==expected);
        assert(s->stats.deleted_clauses>0);
        assert(s->stats.equiv_variables==((equiv && repeat!=1)?1u:0u));
    }
    solver_free(s);
}

int main(void) {
    schedule();
    for(unsigned mask=0;mask<8;++mask)search(mask&1,mask&2,mask&4);
    puts("PASS: fixed/growing reduction boundaries, saturation, actual deletions, SCC, chronology, VMTF and repeated assumption solves");
}
