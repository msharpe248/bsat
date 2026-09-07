#include "../include/solver.h"
#include "../include/dimacs.h"
#include <assert.h>
#include <stdio.h>

static void prefixes(bool queue) {
    SolverOpts o=default_opts();o.reuse_trail=true;o.vmtf=queue;
    Solver *s=solver_new_with_opts(&o);assert(s);
    for(unsigned i=0;i<4;++i) assert(solver_new_var(s));
    assert(solver_restart_level(s)==0);
    Var decisions[3];
    for(unsigned i=0;i<3;++i) {
        assert(solver_decide(s));decisions[i]=var(s->trail[s->trail_lims[i+1]].lit);
    }
    Var next=0;
    for(Var v=1;v<=4;++v) if(s->values[v]==UNDEF) next=v;
    assert(next);
    if(queue) {
        /* Move the available variable above decisions 2/3, below decision 1. */
        solver_vmtf_bump(s,next);solver_vmtf_bump(s,decisions[0]);
    } else {
        s->vars[next].activity=5;
        s->vars[decisions[0]].activity=10;
        s->vars[decisions[1]].activity=5; /* Ties are not retained. */
        s->vars[decisions[2]].activity=9;
    }
    assert(solver_restart_level(s)==1);
    assert(s->decision_level==3 && s->values[next]==UNDEF);
    s->opts.reuse_trail=false;assert(solver_restart_level(s)==0);s->opts.reuse_trail=true;
    s->opts.alternating=true;assert(solver_restart_level(s)==0);s->opts.alternating=false;
    solver_backtrack(s,1);
    assert(s->trail_size==1 && s->values[decisions[0]]!=UNDEF);
    while(solver_decide(s)) {}
    assert(solver_restart_level(s)==s->decision_level);
    solver_backtrack(s,0);
    unsigned assigned=0;
    while(solver_decide(s)) ++assigned;
    assert(assigned==4); /* Peeking must not lose a variable from the order. */
    solver_free(s);
}

static void implications(void) {
    SolverOpts o=default_opts();o.reuse_trail=true;o.probing=false;
    Solver *s=solver_new_with_opts(&o);assert(s);
    assert(dimacs_parse_string(s,"p cnf 5 2\n1 2 0\n1 -2 3 0\n")==DIMACS_OK);
    assert(solver_decide(s) && var(s->trail[0].lit)==1);
    assert(solver_propagate(s)==INVALID_CLAUSE);
    assert(s->trail_size==3 && s->qhead==3);
    CRef reason=s->vars[3].reason;assert(reason!=INVALID_CLAUSE);
    Lit binary_reason=s->binary_reasons[2];assert(binary_reason!=LIT_UNDEF);
    assert(solver_decide(s));
    Var dropped=var(s->trail[3].lit),next=dropped==4?5:4;
    s->vars[1].activity=10;s->vars[dropped].activity=0;s->vars[next].activity=5;
    assert(solver_propagate(s)==INVALID_CLAUSE);
    assert(solver_restart_level(s)==1);
    solver_backtrack(s,1);
    assert(s->trail_size==3 && s->qhead==3 && s->vars[3].reason==reason);
    assert(s->binary_reasons[2]==binary_reason);
    assert(s->values[2]==TRUE && s->values[3]==TRUE && s->values[dropped]==UNDEF);
    while(solver_decide(s)) assert(solver_propagate(s)==INVALID_CLAUSE);
    assert(solver_check_model(s));
    s->interrupted=true;assert(solver_restart_level(s)==0);
    solver_free(s);
}

static void assumptions(void) {
    SolverOpts o=default_opts();o.reuse_trail=true;o.probing=false;
    o.luby_restart=true;o.luby_unit=1;
    Solver *s=solver_new_with_opts(&o);assert(s);
    assert(dimacs_parse_string(s,"p cnf 3 3\n1 2 0\n-1 2 0\n-2 3 0\n")==DIMACS_OK);
    Lit repeated[300];for(unsigned i=0;i<300;++i) repeated[i]=mkLit(3,false);
    assert(solver_solve_with_assumptions(s,repeated,300)==TRUE);
    assert(solver_check_model(s) && !s->stats.reused_levels);
    Lit negative=mkLit(3,true);
    assert(solver_solve_with_assumptions(s,&negative,1)==FALSE);
    assert(solver_solve(s)==TRUE && solver_check_model(s));
    solver_free(s);
}

int main(void) {
    prefixes(false);prefixes(true);implications();assumptions();
    puts("PASS: heap and queue restart prefix boundaries, fallbacks and order recovery");
    return 0;
}
