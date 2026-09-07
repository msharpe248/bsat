#include "../include/solver.h"
#include "../include/local_search.h"
#include "../include/dimacs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static Solver *formula(bool feedback, bool saving) {
    SolverOpts o=default_opts();o.ls_save_phases=feedback;o.phase_saving=saving;
    Solver *s=solver_new_with_opts(&o);assert(s);
    assert(dimacs_parse_string(s,"p cnf 5 8\n1 2 0\n1 -2 0\n-1 2 0\n-1 -2 0\n3 4 0\n-3 1 0\n-4 2 0\n5 0\n")==DIMACS_OK);
    assert(solver_propagate(s)==INVALID_CLAUSE);
    for(Var v=1;v<=5;++v)s->vars[v].polarity=false;
    return s;
}

static unsigned objective(const LocalSearchState *ls) {
    unsigned total=0;
    for(unsigned c=0;c<ls->num_clauses;++c) {
        bool sat=false;
        for(unsigned i=0;i<ls->clause_sizes[c];++i) {
            Lit l=ls->clause_lits[c][i];sat|=ls->assignment[var(l)]!=sign(l);
        }
        total+=!sat;
    }
    return total;
}

int main(void) {
    unsigned improved=0,worsened=0,checks=0;
    for(unsigned seed=1;seed<=32;++seed)for(unsigned noise=0;noise<3;++noise) {
        Solver *ref=formula(false,true);LocalSearchState *walk=local_search_init(ref);assert(walk);
        bool expected[6]={0};unsigned best=UINT32_MAX,initial=0;
        for(unsigned limit=0;limit<=32;++limit) {
            walk->random_state=seed;walk->flips=0;
            assert(!local_search_run(ref,walk,limit,noise*0.5));
            unsigned value=objective(walk);
            if(!limit) initial=best=value;
            else if(value<best) {best=value;memcpy(expected,walk->assignment,sizeof expected);}
            for(unsigned saving=0;saving<2;++saving) {
                Solver *s=formula(true,saving);assert(solver_decide(s));
                LocalSearchState *ls=local_search_init(s);assert(ls);
                lbool before[6];memcpy(before,s->values,sizeof before);
                uint32_t trail=s->trail_size,qhead=s->qhead;Level level=s->decision_level;
                VarInfo metadata[6];memcpy(metadata,s->vars,sizeof metadata);
                Lit reasons[6];memcpy(reasons,s->binary_reasons,sizeof reasons);
                Trail before_trail[6];memcpy(before_trail,s->trail,trail*sizeof(Trail));
                ls->random_state=seed;
                assert(!local_search_run(s,ls,limit,noise*0.5));
                assert(ls->flips==limit && ls->random_state==walk->random_state);
                assert(!memcmp(ls->assignment+1,walk->assignment+1,5*sizeof(bool)));
                assert(!memcmp(before,s->values,sizeof before));
                assert(s->trail_size==trail && s->qhead==qhead && s->decision_level==level);
                assert(!memcmp(before_trail,s->trail,trail*sizeof(Trail)));
                for(Var v=1;v<=5;++v) {
                    assert(s->vars[v].level==metadata[v].level);
                    assert(s->vars[v].reason==metadata[v].reason);
                    assert(s->vars[v].trail_pos==metadata[v].trail_pos);
                    assert(s->binary_reasons[v]==reasons[v]);
                }
                assert(s->result==UNDEF && s->vars[5].polarity==false);
                for(Var v=1;v<=4;++v)assert(s->vars[v].polarity==(saving && best<initial ? expected[v] : false));
                ++checks;local_search_free(ls);solver_free(s);
            }
            improved+=best<initial;worsened+=value>best;
        }
        local_search_free(walk);solver_free(ref);
    }
    assert(improved && worsened);
    printf("PASS: %u feedback cases preserve assignment and keep the best phases\n",checks);
    return 0;
}
