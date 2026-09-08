#include "solver.h"
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
    o.lrb=profile==5;o.rephase=profile==5;
    Solver *s=solver_new_with_opts(&o);
    if (!s) { assert(failed); return calls; }
    for (unsigned v=0;v<8;++v) if (!solver_new_var(s)) goto done;
    for (unsigned bits=0;bits<64-(unsigned)sat;++bits) {
        Lit c[6];for(unsigned v=0;v<6;++v)c[v]=mkLit(v+1,(bits>>v)&1);
        solver_add_clause(s,c,6);
        if(s->error || s->watches->failed) goto done;
    }
    { Lit a[]={mkLit(7,true),mkLit(8,false)},b[]={mkLit(7,false),mkLit(8,true)};
      solver_add_clause(s,a,2);solver_add_clause(s,b,2); }
    for(unsigned repeat=0;repeat<2;++repeat) {
        lbool r=solver_solve(s);
        if(s->error || s->watches->failed) {assert(r==UNDEF);break;}
        assert(r==(sat?TRUE:FALSE));
        if(sat) for(unsigned v=1;v<=6;++v)assert(solver_model_value(s,v)==TRUE);
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
int main(void) {
    size_t injected=0;
    for(unsigned p=0;p<6;++p)for(unsigned sat=0;sat<2;++sat) {
        size_t count=attempt(p,sat,0);
        for(size_t i=1;i<=count;++i) {
            fprintf(stderr,"fault profile=%u sat=%u allocation=%zu/%zu\n",p,sat,i,count);
            attempt(p,sat,i);assert(failed);++injected;
        }
    }
    printf("PASS: %zu single-allocation failures across 12 profiles/formulas and repeated solves\n",injected);
}
