#include "solver.h"
#include "ipasir.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>

static size_t calls, fail_at;
static bool failed;
static bool reject(void) {
    if (++calls != fail_at) return false;
    failed = true; errno = ENOMEM; return true;
}
void *fault_malloc(size_t n) { return reject() ? NULL : malloc(n); }
void *fault_calloc(size_t n, size_t z) { return reject() ? NULL : calloc(n,z); }
void *fault_realloc(void *p, size_t n) { return reject() ? NULL : realloc(p,n); }

/* Exclude every assignment to six variables, or leave only all-true.
   Independent closed-form oracle; inspect the original clauses for SAT. */
static size_t attempt(unsigned profile, bool sat, size_t cutoff) {
    calls=0;fail_at=cutoff;failed=false;
    SolverOpts o=default_opts();
    o.reduce_interval=2;o.restart_first=2;o.luby_unit=2;
    o.probing=profile==1;o.elim=profile==2;o.bce=profile==2;
    o.equiv=profile==3;o.congruence=profile==3;
    o.chrono=profile==4;o.chrono_levels=0;o.vmtf=profile==4;
    o.reuse_learnts=profile==6;
    o.lrb=profile==5;o.rephase=profile==5;
    o.factor=profile==7;
    Solver *s=solver_new_with_opts(&o);
    if (!s) { assert(failed); return calls; }
    for (unsigned v=0;v<8;++v) if (!solver_new_var(s)) goto done;
    if(profile==7) {
        for(Var a=1;a<=3;++a)for(Var b=4;b<=6;++b) {
            Lit c[]={mkLit(a,false),mkLit(b,false)};solver_add_clause(s,c,2);
            if(s->error || s->watches->failed)goto done;
        }
        if(!sat) {
            Lit a[]={mkLit(1,true),mkLit(2,true),mkLit(3,true)};
            Lit b[]={mkLit(4,true),mkLit(5,true),mkLit(6,true)};
            solver_add_clause(s,a,3);solver_add_clause(s,b,3);
            if(s->error || s->watches->failed)goto done;
        }
    } else for (unsigned bits=0;bits<64-(unsigned)sat;++bits) {
        Lit c[6];for(unsigned v=0;v<6;++v)c[v]=mkLit(v+1,(bits>>v)&1);
        solver_add_clause(s,c,6);
        if(s->error || s->watches->failed) goto done;
    }
    { Lit a[]={mkLit(7,true),mkLit(8,false)},b[]={mkLit(7,false),mkLit(8,true)};
      solver_add_clause(s,a,2);solver_add_clause(s,b,2); }
    for(unsigned repeat=0;repeat<2;++repeat) {
        if(profile==6 && repeat==1)
            while(s->num_vars<130)if(!solver_new_var(s))goto done;
        lbool r=solver_solve(s);
        if(s->error || s->watches->failed) {assert(r==UNDEF);break;}
        assert(r==(sat?TRUE:FALSE));
        if(sat) {
            assert(solver_check_model(s));
            if(profile!=7)for(unsigned v=1;v<=6;++v)assert(solver_model_value(s,v)==TRUE);
        }
        if(profile==6 && sat && repeat==0) {
            Lit assumption=mkLit(1,true);
            r=solver_solve_with_assumptions(s,&assumption,1);
            if(s->error || s->watches->failed){assert(r==UNDEF);break;}
            assert(r==FALSE); /* Next growth/solve must clear only conditional UNSAT. */
        }
    }
done:
    if(s->error || s->watches->failed) {
        bool error=s->error,watch=s->watches->failed;
        lbool r=solver_solve(s);
        if(r!=UNDEF)fprintf(stderr,"conclusive after failure: error=%d watch=%d result=%d solved=%d\n",error,watch,r,s->has_solved);
        assert(r==UNDEF);
    }
    solver_free(s);
    return calls;
}
static void receive(void *state,int *clause) { (void)state;while(*clause)++clause; }
static size_t ipasir_attempt(bool sat,size_t cutoff) {
    calls=0;fail_at=cutoff;failed=false;
    void *s=ipasir_init();if(!s){assert(failed);return calls;}
    ipasir_set_learn(s,NULL,6,receive);
    for(unsigned bits=0;bits<64-(unsigned)sat;++bits) {
        for(unsigned v=0;v<6;++v)ipasir_add(s,(bits>>v)&1?-(int)(v+1):(int)(v+1));
        ipasir_add(s,0);
    }
    for(unsigned repeat=0;repeat<2;++repeat) {
        int r=ipasir_solve(s);assert(r==(failed?0:sat?10:20));
        if(sat && r==10)for(int v=1;v<=6;++v)assert(ipasir_val(s,v)==v);
        ipasir_assume(s,7); /* Growth and assumption buffer allocations. */
    }
    ipasir_release(s);return calls;
}
int main(void) {
    size_t injected=0;
    for(unsigned p=0;p<8;++p)for(unsigned sat=0;sat<2;++sat) {
        size_t count=attempt(p,sat,0);
        for(size_t i=1;i<=count;++i) {
            fprintf(stderr,"fault profile=%u sat=%u allocation=%zu/%zu\n",p,sat,i,count);
            attempt(p,sat,i);assert(failed);++injected;
        }
    }
    for(unsigned sat=0;sat<2;++sat) {
        size_t count=ipasir_attempt(sat,0);
        for(size_t i=1;i<=count;++i){ipasir_attempt(sat,i);assert(failed);++injected;}
    }
    printf("PASS: %zu single-allocation failures across 18 profiles/formulas and repeated solves\n",injected);
}
