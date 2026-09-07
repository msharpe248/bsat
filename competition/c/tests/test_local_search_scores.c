#include "../include/solver.h"
#include "../include/local_search.h"
#include <assert.h>
#include <stdio.h>

static uint32_t truths(const LocalSearchState *ls, uint32_t c) {
    uint32_t n=0;
    for(unsigned i=0;i<ls->clause_sizes[c];++i) {
        Lit lit=ls->clause_lits[c][i];
        n += ls->assignment[var(lit)] != sign(lit);
    }
    return n;
}

static uint32_t unsatisfied(const LocalSearchState *ls) {
    uint32_t n=0;
    for(unsigned c=0;c<ls->num_clauses;++c) n += !truths(ls,c);
    return n;
}

static void verify(LocalSearchState *ls) {
    uint32_t before=unsatisfied(ls);
    assert(ls->num_unsat==before);
    for(unsigned c=0;c<ls->num_clauses;++c) assert(ls->num_true_lits[c]==truths(ls,c));
    for(Var v=1;v<=ls->num_vars;++v) {
        ls->assignment[v]=!ls->assignment[v];
        int32_t delta=(int32_t)unsatisfied(ls)-(int32_t)before;
        ls->assignment[v]=!ls->assignment[v];
        assert(ls->break_count[v]==delta);
    }
}

static unsigned prefixes(Solver *s, bool wide) {
    LocalSearchState *ls=local_search_init(s);assert(ls);
    unsigned checks=0;
    for(unsigned noise=0;noise<3;++noise) for(unsigned seed=1;seed<=(wide?4:8);++seed)
    for(unsigned flips=0;flips<=(wide?16:32);++flips) {
        for(Var v=1;v<=s->num_vars;++v)
            s->vars[v].polarity=wide ? v==s->num_vars && (seed&1) : (seed>>(v-1))&1;
        ls->random_state=seed;ls->flips=0;
        assert(!local_search_run(s,ls,flips,noise*0.5));
        assert(ls->flips==flips);verify(ls);++checks;
    }
    local_search_free(ls);return checks;
}

int main(void) {
    Solver *s=solver_new();assert(s);
    for(unsigned i=0;i<4;++i) assert(solver_new_var(s));
    /* Complete four-variable contradiction plus uneven extra clauses: the
       objective varies, while every walk must remain UNSAT at every prefix. */
    for(unsigned mask=0;mask<16;++mask) {
        Lit clause[4];for(unsigned v=1;v<=4;++v) clause[v-1]=mkLit(v,(mask>>(v-1))&1);
        assert(solver_add_clause(s,clause,4));
    }
    Lit a[]={mkLit(1,false),mkLit(2,false)};
    Lit b[]={mkLit(2,true),mkLit(3,false),mkLit(4,false)};
    Lit c[]={mkLit(1,true),mkLit(3,true)};
    assert(solver_add_clause(s,a,2));assert(solver_add_clause(s,b,3));assert(solver_add_clause(s,c,2));
    unsigned checks=prefixes(s,false);solver_free(s);
    s=solver_new();assert(s);
    for(unsigned i=0;i<64;++i) assert(solver_new_var(s));
    for(unsigned mask=0;mask<4;++mask) {
        Lit clause[]={mkLit(1,mask&1),mkLit(2,(mask>>1)&1)};
        assert(solver_add_clause(s,clause,2));
    }
    Lit wide[64];for(unsigned i=0;i<64;++i) wide[i]=mkLit(i+1,false);
    assert(solver_add_clause(s,wide,64));
    checks+=prefixes(s,true);solver_free(s);
    printf("PASS: %u walk prefixes checked against full score recomputation\n",checks);
    return 0;
}
