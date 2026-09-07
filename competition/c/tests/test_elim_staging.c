#include "../include/solver.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static Solver *pure(unsigned n) {
    Solver *s=solver_new();assert(s);
    Lit *lits=malloc(n*sizeof *lits);assert(lits);
    for(unsigned i=0;i<n;++i) {assert(solver_new_var(s));lits[i]=mkLit(i+1,false);}
    assert(solver_add_clause(s,lits,n));free(lits);
    elim_build_occs(s);assert(!s->error && s->elim->occs_complete);
    return s;
}
static void unchanged(Solver *s) {
    assert(!s->error && !s->elim->vars_eliminated && !s->elim->stack_size);
    assert(s->num_clauses==1 && !clause_deleted(s->arena,s->clauses[0]));
    for(Var v=1;v<=s->num_vars;++v) assert(s->values[v]==UNDEF && !s->seen[v]);
}
static void pure_deadline(void) {
    Solver *s=pure(5000);
    s->opts.max_time=0.001;s->stats.start_time=(double)clock()/CLOCKS_PER_SEC-1.0;
    s->clock_initialized=true;s->clock_polls=0;
    s->clock_work=s->work;s->clock_minimize=s->stats.minimize_inspections;
    uint64_t before=s->work,clocks=s->stats.clock_checks;
    assert(!elim_eliminate_var(s,1));unchanged(s);
    assert(s->interrupted && s->stats.clock_checks==clocks+1);
    assert(s->work>before && s->work-before<=1024);
    solver_free(s);
}
static void cutoffs(void) {
    for(unsigned budget=0;budget<=67;++budget) {
        Solver *s=pure(65);uint64_t start=s->work;
        s->work_limit=start+budget;
        bool ok=elim_eliminate_var(s,1);
        if(budget<=2) {assert(!ok);unchanged(s);assert(s->work==s->work_limit);}
        else {
            assert(ok && s->work-start==2 && s->elim->stack_size==1);
            for(Var v=1;v<=s->num_vars;++v) s->values[v]=FALSE;
            elim_extend_model(s);assert(s->values[1]==TRUE && solver_check_model(s));
        }
        solver_free(s);
    }
    Solver *s=pure(65);s->interrupted=true;
    assert(!elim_eliminate_var(s,1));unchanged(s);solver_free(s);
}
static void copy_contract(void) {
    Solver *s=solver_new();assert(s);assert(solver_new_var(s));assert(solver_new_var(s));
    Lit witness[]={mkLit(1,false),mkLit(2,true),0};
    assert(elim_save(s,1,witness,3));witness[0]=neg(witness[0]);
    assert(s->elim->stack[0].clause!=witness && s->elim->stack[0].clause[0]==mkLit(1,false));
    assert(elim_save(s,1,NULL,0));assert(s->elim->stack[1].clause_size==0);
    solver_free(s);
}
static void stack_growth(void) {
    Solver *s=solver_new();assert(s);
    for(unsigned i=0;i<96;++i) assert(solver_new_var(s));
    for(Var v=1;v<=96;v+=2) {Lit c[]={mkLit(v,false),mkLit(v+1,false)};assert(solver_add_clause(s,c,2));}
    elim_build_occs(s);
    for(Var v=1;v<=96;v+=2) assert(elim_eliminate_var(s,v));
    assert(s->elim->stack_size==48 && s->elim->stack_capacity>=48);
    for(unsigned i=0;i<48;++i) {
        ElimEntry *e=&s->elim->stack[i];
        assert(e->var==2*i+1 && e->clause_size==2);
        assert(e->clause[0]==mkLit(e->var,false) && !e->clause[1]);
        for(unsigned j=0;j<i;++j) assert(e->clause!=s->elim->stack[j].clause);
    }
    for(Var v=1;v<=96;++v) s->values[v]=FALSE;
    elim_extend_model(s);assert(solver_check_model(s));solver_free(s);
}
int main(void) {
    pure_deadline();cutoffs();copy_contract();stack_growth();
    puts("PASS: pure staging deadline, 68 work boundaries, copy contract and owned-record growth");
}
