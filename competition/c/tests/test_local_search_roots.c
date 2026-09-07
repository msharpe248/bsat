#include "../include/solver.h"
#include "../include/local_search.h"
#include "../include/dimacs.h"
#include <assert.h>
#include <stdio.h>

static void saved_phases(void) {
    SolverOpts o=default_opts();o.phase_saving=false;
    Solver *s=solver_new_with_opts(&o);assert(s);
    assert(dimacs_parse_string(s,"p cnf 3 3\n1 0\n-1 2 0\n-2 3 0\n")==DIMACS_OK);
    assert(solver_propagate(s)==INVALID_CLAUSE);
    LocalSearchState *ls=local_search_init(s);assert(ls);
    assert(local_search_run(s,ls,0,0.0));
    assert(ls->flips==0);
    for(Var v=1;v<=3;++v) assert(ls->assignment[v] && s->values[v]==TRUE);
    local_search_free(ls);solver_free(s);
}

static void random_walk(void) {
    Solver *s=solver_new();assert(s);
    assert(dimacs_parse_string(s,"p cnf 3 2\n1 0\n-1 2 3 0\n")==DIMACS_OK);
    assert(solver_propagate(s)==INVALID_CLAUSE);
    LocalSearchState *ls=local_search_init(s);assert(ls);
    for(unsigned seed=1;seed<=64;++seed) {
        ls->random_state=seed;
        uint64_t flips=ls->flips;
        assert(local_search_run(s,ls,1,1.0));
        assert(ls->assignment[1] && (ls->assignment[2] || ls->assignment[3]));
        assert(ls->flips==flips+1);
    }
    local_search_free(ls);solver_free(s);
}

static void greedy_walk(void) {
    Solver *s=solver_new();assert(s);
    assert(dimacs_parse_string(s,"p cnf 5 6\n1 0\n-1 2 3 0\n-2 4 0\n-2 -4 0\n-3 5 0\n-3 -5 0\n")==DIMACS_OK);
    assert(solver_propagate(s)==INVALID_CLAUSE);
    LocalSearchState *ls=local_search_init(s);assert(ls);
    assert(!local_search_run(s,ls,1,0.0));
    assert(ls->assignment[1] && ls->assignment[2] && ls->flips==1);
    local_search_free(ls);solver_free(s);
}

static void no_root_trace(void) {
    Solver *s=solver_new();assert(s);
    assert(dimacs_parse_string(s,"p cnf 2 1\n1 2 0\n")==DIMACS_OK);
    assert(solver_decide(s)); /* A non-root assignment must remain movable. */
    LocalSearchState *ls=local_search_init(s);assert(ls);
    for(unsigned seed=1;seed<=64;++seed) {
        ls->random_state=seed;
        uint32_t expected=seed;
        (void)bsat_random(&expected); /* Unsatisfied-clause choice. */
        (void)bsat_random(&expected); /* Noise decision. */
        Var chosen=1+bsat_random(&expected)%2;
        assert(local_search_run(s,ls,1,1.0));
        assert(ls->random_state==expected && ls->assignment[chosen]);
        assert(!ls->assignment[3-chosen]);
    }
    local_search_free(ls);solver_free(s);
}

static void blocked_root(void) {
    Solver *s=solver_new();assert(s);
    assert(dimacs_parse_string(s,"p cnf 2 1\n1 2 0\n")==DIMACS_OK);
    for(Var v=1;v<=2;++v) { s->values[v]=FALSE;s->vars[v].level=0; }
    LocalSearchState *ls=local_search_init(s);assert(ls);
    for(unsigned noise=0;noise<=1;++noise) {
        assert(!local_search_run(s,ls,100,noise));
        assert(!ls->flips && s->result==UNDEF);
    }
    local_search_free(ls);solver_free(s);
}

static Solver *pigeonhole(bool queue, bool walk) {
    SolverOpts o=default_opts();o.vmtf=queue;o.reuse_trail=true;o.probing=false;
    o.local_search=walk;o.ls_interval=1;o.ls_max_flips=0;
    Solver *s=solver_new_with_opts(&o);assert(s);
    for(unsigned i=0;i<20;++i) assert(solver_new_var(s));
    for(unsigned p=0;p<5;++p) {
        Lit clause[4];for(unsigned h=0;h<4;++h) clause[h]=mkLit(1+4*p+h,false);
        assert(solver_add_clause(s,clause,4));
    }
    for(unsigned h=0;h<4;++h) for(unsigned p=0;p<5;++p) for(unsigned q=p+1;q<5;++q) {
        Lit clause[]={mkLit(1+4*p+h,true),mkLit(1+4*q+h,true)};
        assert(solver_add_clause(s,clause,2));
    }
    assert(solver_solve(s)==FALSE);
    return s;
}

static void failed_walk_preserves_search(void) {
    for(unsigned queue=0;queue<2;++queue) {
        Solver *base=pigeonhole(queue,false),*walk=pigeonhole(queue,true);
        assert(walk->local_search.calls && !walk->local_search.successes);
        assert(base->stats.conflicts==walk->stats.conflicts);
        assert(base->stats.decisions==walk->stats.decisions);
        assert(base->stats.propagations==walk->stats.propagations);
        assert(base->stats.learned_literals==walk->stats.learned_literals);
        assert(base->stats.restarts==walk->stats.restarts);
        assert(base->work==walk->work);
        solver_free(base);solver_free(walk);
    }
}

int main(void) {
    saved_phases();random_walk();greedy_walk();no_root_trace();blocked_root();failed_walk_preserves_search();
    puts("PASS: root values override phases and remain fixed during random walks");
    return 0;
}
