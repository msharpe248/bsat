#include "../include/solver.h"
#include "../include/dimacs.h"
#include <assert.h>
#include <stdio.h>

static Solver *formula(const char *text) {
    SolverOpts o=default_opts();o.equiv=true;o.probing=false;
    Solver *s=solver_new_with_opts(&o);assert(s);
    assert(dimacs_parse_string(s,text)==DIMACS_OK);return s;
}
static void signed_classes(void) {
    for(unsigned invert=0;invert<2;++invert) {
        Solver *s=formula(invert ? "p cnf 3 3\n1 2 0\n-1 -2 0\n2 3 0\n" :
                                  "p cnf 3 3\n-1 2 0\n1 -2 0\n2 3 0\n");
        assert(solver_solve(s)==TRUE && solver_check_model(s));
        assert(s->stats.equiv_variables==1 && s->num_clauses==1);
        assert((solver_model_value(s,1)!=solver_model_value(s,2))==invert);
        /* Assumptions use the original input, including substituted variables. */
        Lit a=mkLit(2,false);assert(solver_solve_with_assumptions(s,&a,1)==TRUE);
        assert(solver_model_value(s,2)==TRUE && solver_check_model(s));
        assert(!s->stats.equiv_variables);
        assert(solver_solve(s)==TRUE && s->stats.equiv_variables==1);
        assert(solver_new_var(s)==4);
        Lit unit=mkLit(4,true);assert(solver_add_clause(s,&unit,1));
        assert(solver_solve(s)==TRUE && solver_model_value(s,4)==FALSE);
        solver_free(s);
    }
}
static void contradiction(void) {
    Solver *s=formula("p cnf 2 4\n1 2 0\n-1 2 0\n1 -2 0\n-1 -2 0\n");
    assert(solver_solve(s)==FALSE && s->stats.equiv_conflicts==1);
    assert(s->stats.decisions==0);solver_free(s);
}
static void root_binaries(void) {
    Solver *s=formula("p cnf 4 4\n-4 0\n-1 2 4 0\n1 -2 4 0\n2 3 0\n");
    assert(solver_solve(s)==TRUE && solver_check_model(s));
    assert(s->stats.equiv_binaries==2 && s->stats.equiv_variables==1);
    assert(solver_model_value(s,1)==solver_model_value(s,2));solver_free(s);
    s=formula("p cnf 4 4\n4 0\n-1 2 4 0\n1 -2 4 0\n2 3 0\n");
    assert(solver_solve(s)==TRUE && solver_check_model(s));
    assert(!s->stats.equiv_binaries && !s->stats.equiv_variables);solver_free(s);
}
static void probing_fact(void) {
    Solver *s=formula("p cnf 5 5\n1 2 0\n1 -2 0\n-3 4 0\n3 -4 0\n4 5 0\n");
    s->opts.probing=true;
    assert(solver_solve(s)==TRUE && solver_check_model(s));
    assert(s->stats.equiv_variables==1 && solver_model_value(s,1)==TRUE);
    solver_free(s);
}
static void long_cycle(void) {
    for(unsigned signed_cycle=0;signed_cycle<2;++signed_cycle) {
    Solver *s=formula("p cnf 0 0\n");const unsigned n=10000;
    s->opts.equiv_budget=10000000;
    for(unsigned v=1;v<=n;++v) assert(solver_new_var(s)==v);
    for(unsigned v=1;v<=n;++v) {
        unsigned next=v==n?1:v+1;
        Lit a=mkLit(v,signed_cycle && v%2), b=mkLit(next,signed_cycle && next%2);
        Lit c[]={neg(a),b};
        assert(solver_add_clause(s,c,2));
    }
    assert(solver_solve(s)==TRUE && solver_check_model(s));
    assert(s->stats.equiv_variables==n-1 && s->num_clauses==0);
    for(unsigned v=2;v<=n;++v)
        assert((solver_model_value(s,v)==solver_model_value(s,1))==(!signed_cycle || v%2));
    solver_free(s);
    }
}
static void budget_and_composition(void) {
    const char *text="p cnf 4 4\n1 2 0\n-1 -2 0\n2 3 0\n-3 4 0\n";
    for(unsigned budget=0;budget<150;++budget) {
        Solver *s=formula(text);s->opts.equiv_budget=budget;
        assert(solver_solve(s)==TRUE && solver_check_model(s));
        assert(s->stats.equiv_work<=budget);solver_free(s);
    }
    for(unsigned budget=0;budget<150;++budget) {
        Solver *s=formula("p cnf 5 5\n1 2 0\n1 -2 0\n-3 4 0\n3 -4 0\n4 5 0\n");
        s->opts.probing=true;s->opts.equiv_budget=budget;
        assert(solver_solve(s)==TRUE && solver_check_model(s));
        assert(solver_model_value(s,1)==TRUE && s->stats.equiv_work<=budget);solver_free(s);
    }
    Solver *s=formula(text);s->opts.elim=true;s->opts.bce=true;
    assert(solver_solve(s)==TRUE && solver_check_model(s));
    assert(s->stats.equiv_variables==1);solver_free(s);
}
int main(void) {
    signed_classes();contradiction();root_binaries();probing_fact();long_cycle();budget_and_composition();
    puts("PASS: signed SCCs, contradiction, 10000-node cycle, budget rollback, reconstruction and API reuse");
    return 0;
}
