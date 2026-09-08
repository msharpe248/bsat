#include "solver.h"
#include <assert.h>
#include <stdio.h>
static Solver *formula(unsigned signs,bool unsat,uint64_t budget) {
    SolverOpts o=default_opts();o.factor=true;o.factor_budget=budget;o.probing=false;
    o.chrono=signs&2;o.chrono_levels=0;o.vmtf=signs&4;o.alternating=signs&8;
    o.reuse_learnts=true;Solver *s=solver_new_with_opts(&o);assert(s);
    for(unsigned i=0;i<6;++i)assert(solver_new_var(s));
    Lit v[6];for(unsigned i=0;i<6;++i)v[i]=mkLit(i+1,(signs>>i)&1);
    for(unsigned i=0;i<3;++i)for(unsigned j=3;j<6;++j) {
        Lit c[]={v[i],v[j]};assert(solver_add_clause(s,c,2));
        if(signs&1)assert(solver_add_clause(s,c,2)); /* duplicates are not extra edges */
    }
    if(unsat) {
        Lit a[]={neg(v[0]),neg(v[1]),neg(v[2])},b[]={neg(v[3]),neg(v[4]),neg(v[5])};
        assert(solver_add_clause(s,a,3));assert(solver_add_clause(s,b,3));
    }
    return s;
}
static int cancel(void *p){return ++*(unsigned *)p==5;}
static Solver *ternary(unsigned signs,bool bad,uint64_t budget) {
    SolverOpts o=default_opts();o.factor=true;o.probing=false;o.factor_budget=budget;
    o.chrono=signs&1;o.vmtf=signs&2;o.reuse_learnts=true;
    Solver *s=solver_new_with_opts(&o);assert(s);
    Lit v[9];for(unsigned i=0;i<9;++i){assert(solver_new_var(s));v[i]=mkLit(i+1,(signs>>(i%3))&1);}
    for(unsigned i=0;i<3;++i)for(unsigned j=3;j<9;j+=2) {
        Lit c[]={v[i],v[j],v[j+1]};assert(solver_add_clause(s,c,3));
    }
    if(bad) {
        Lit a[]={neg(v[0]),neg(v[1]),neg(v[2])},b[]={neg(v[3]),neg(v[5]),neg(v[7])};
        assert(solver_add_clause(s,a,3));assert(solver_add_clause(s,b,3));
        for(unsigned j=3;j<9;j+=2) {
            Lit x[]={neg(v[j]),v[j+1]},y[]={v[j],neg(v[j+1])};
            assert(solver_add_clause(s,x,2));assert(solver_add_clause(s,y,2));
        }
    }
    return s;
}
static bool original_value(Solver *s,unsigned bits) {
    bool sat=false;
    for(size_t i=0;i<s->input_size;++i) {
        Lit l=s->input[i];if(!l){if(!sat)return false;sat=false;}
        else sat|=((bits>>(var(l)-1))&1u)!=sign(l);
    }
    return true;
}
static void auxiliary_growth(void) {
    SolverOpts o=default_opts();o.factor=true;o.probing=false;o.vmtf=true;o.chrono=true;
    Solver *s=solver_new_with_opts(&o);assert(s);
    for(unsigned i=0;i<128;++i)assert(solver_new_var(s));
    for(unsigned block=0;block<20;++block)for(unsigned a=0;a<3;++a)for(unsigned b=3;b<6;++b) {
        Lit c[]={mkLit(1+block*6+a,false),mkLit(1+block*6+b,false)};
        assert(solver_add_clause(s,c,2));
    }
    assert(solver_solve(s)==TRUE && !s->error && solver_check_model(s));
    assert(s->stats.factor_variables==20 && s->num_vars==148);
    assert(solver_new_var(s)==129);
    Lit unit=mkLit(129,true);assert(solver_add_clause(s,&unit,1));
    assert(solver_solve(s)==TRUE && solver_model_value(s,129)==FALSE && solver_check_model(s));
    solver_free(s);
}
static void two_columns(void) {
    for(unsigned gain=1;gain<=2;++gain)for(unsigned signs=0;signs<8;++signs)
    for(unsigned bits=0;bits<128;++bits) {
        SolverOpts o=default_opts();o.factor=true;o.probing=false;o.factor_min_gain=gain;
        Solver *s=solver_new_with_opts(&o);assert(s);Lit v[7];
        for(unsigned i=0;i<7;++i){assert(solver_new_var(s));v[i]=mkLit(i+1,(signs>>(i%3))&1);}
        for(unsigned row=0;row<3;++row)for(unsigned col=3;col<7;col+=2){Lit c[]={v[row],v[col],v[col+1]};assert(solver_add_clause(s,c,3));}
        bool expected=original_value(s,bits);
        assert(solver_factor(s)==(gain==1?1u:0u));
        if(gain==1)assert(s->stats.factor_deleted==6 && s->stats.factor_added==5);
        else assert(s->stats.factor_pruned>0);
        Lit a[7];for(unsigned i=0;i<7;++i)a[i]=mkLit(i+1,!((bits>>i)&1));
        assert(solver_solve_with_assumptions(s,a,7)==(expected?TRUE:FALSE));solver_free(s);
    }
}
int main(void) {
    two_columns();
    auxiliary_growth();
    unsigned cases=2048; /* two-column projection and gain-boundary cases */
    for(unsigned signs=0;signs<64;++signs)for(unsigned bad=0;bad<2;++bad)
    for(unsigned cutoff=0;cutoff<20;++cutoff) {
        Solver *s=formula(signs,bad,cutoff==19?100000:cutoff*17);
        assert(solver_solve(s)==(bad?FALSE:TRUE) && !s->error);
        if(!bad)assert(solver_check_model(s));
        if(cutoff==19)assert(s->stats.factor_variables && s->stats.factor_deleted==9);
        /* Fresh user variables must reclaim the private auxiliary namespace. */
        assert(solver_new_var(s)==7);
        assert(solver_solve(s)==(bad?FALSE:TRUE) && !s->error);
        solver_free(s);++cases;
    }
    Solver *s=formula(0,false,100000);unsigned polls=0;
    solver_set_terminate(s,&polls,cancel);assert(solver_solve(s)==UNDEF&&!s->error);
    solver_set_terminate(s,NULL,NULL);assert(solver_solve(s)==TRUE&&solver_check_model(s));
    solver_free(s);
    for(unsigned kind=0;kind<2;++kind)for(unsigned signs=0;signs<8;++signs) {
        unsigned n=kind?9:6;
        for(unsigned bits=0;bits<(1u<<n);++bits) {
            s=kind?ternary(signs,false,100000):formula(signs,false,100000);
            bool expected=original_value(s,bits);
            assert(solver_factor(s)>0 && !s->error);
            Lit assumptions[9];for(unsigned v=0;v<n;++v)assumptions[v]=mkLit(v+1,!((bits>>v)&1));
            assert(solver_solve_with_assumptions(s,assumptions,n)==(expected?TRUE:FALSE));
            solver_free(s);++cases;
        }
        if(kind)for(unsigned cut=0;cut<40;++cut) {
            s=ternary(signs,true,cut==39?100000:cut*17);
            assert(solver_solve(s)==FALSE && !s->error);
            if(cut==39)assert(s->stats.factor_variables>0);
            assert(solver_new_var(s)==10);solver_free(s);++cases;
        }
    }
    printf("PASS: %u factoring projection/cutoff cases, auxiliary growth, namespace rebuilds and cancellation retry\n",cases);
}
