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
static void lbd_state(void) {
    for(unsigned mask=0;mask<8;++mask) {
        SolverOpts o=default_opts();o.equiv=(mask&1)!=0;o.vmtf=(mask&2)!=0;
        o.chrono=(mask&4)!=0;o.chrono_levels=0;o.probing=false;o.max_conflicts=1;
        Solver *s=solver_new_with_opts(&o);assert(s);
        /* Parents 5 and 6 precede child 2 in either decision order. Their
           first conflict learns (5 or 6), with two distinct decision levels.
           The independent 3/4 alias forces the SCC replacement path. */
        assert(dimacs_parse_string(s,"p cnf 6 4\n-3 4 0\n3 -4 0\n5 6 2 0\n5 6 -2 0\n")==DIMACS_OK);
        assert(solver_solve(s)==UNDEF && !s->error);
        assert(s->stats.equiv_variables==(o.equiv?1u:0u) && s->num_learnts==1);
        assert(clause_lbd(s->arena,s->learnts[0])==2 && s->stats.max_lbd==2);
        assert(s->level_seen && s->levels_capacity>s->num_vars);
        for(uint32_t i=0;i<s->levels_capacity;++i)assert(!s->level_seen[i]);
        s->opts.max_conflicts=0;
        assert(solver_solve(s)==TRUE && solver_check_model(s));
        Lit assumptions[]={mkLit(5,1),mkLit(6,1)};
        assert(solver_solve_with_assumptions(s,assumptions,2)==FALSE && !s->error);
        assert(solver_solve(s)==TRUE && solver_check_model(s));
        assert(s->level_seen && s->levels_capacity>s->num_vars);
        solver_free(s);
    }
    puts("PASS: nonzero LBD and scratch ownership across SCC replacement, assumptions and repeated solves");
}
int main(void) {
    lbd_state();
    signed_classes();contradiction();root_binaries();probing_fact();long_cycle();budget_and_composition();
    puts("PASS: signed SCCs, contradiction, 10000-node cycle, budget rollback, reconstruction and API reuse");
    return 0;
}
