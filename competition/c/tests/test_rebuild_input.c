#include "solver.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    SolverOpts o=default_opts();o.probing=false;o.accounting=true;
    Solver *s=solver_new_with_opts(&o);assert(s);
    for(unsigned v=0;v<128;++v)assert(solver_new_var(s));
    for(unsigned i=0;i<2048;++i) {
        Lit c[]={mkLit(1+i%128,false),mkLit(1+(i+1)%128,false),mkLit(1+(i+2)%128,false)};
        assert(solver_add_clause(s,c,3));
    }
    Lit *input=s->input;size_t capacity=s->input_capacity,size=s->input_size;
    for(unsigned i=0;i<20;++i) {
        assert(solver_solve(s)==TRUE && solver_check_model(s));
        assert(s->input==input && s->input_capacity==capacity && s->input_size==size);
        assert(s->input_clauses==2048);
    }
    assert(s->accounting.rebuild_overlap_peak>0);solver_free(s);
    WatchManager *w=watch_init(1);assert(w);
    for(unsigned i=0;i<100;++i)watch_add(w,mkLit(1,false),INVALID_CLAUSE,mkLit(1,true));
    assert(!w->failed && w->lists[2].size==100);
    for(unsigned i=0;i<100;++i)assert(w->lists[2].watches[i].blocker==mkLit(1,true));
    watch_free(w);puts("PASS: rebuild input ownership and repeated original-model validation; watch growth retains entries");
}
