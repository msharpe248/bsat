#include "bsat.h"
#include "solver.h"
#include <math.h>
struct bsat { Solver *core; int result; bool started; bsat_stats_v1 stats; };
uint32_t bsat_abi_version(void) { return BSAT_ABI_VERSION; }
bsat *bsat_create(uint32_t abi,uint32_t flags) {
    if(abi!=BSAT_ABI_VERSION || (flags & ~BSAT_REUSE_LEARNTS))return NULL;
    bsat *s=calloc(1,sizeof *s);if(!s)return NULL;
    SolverOpts o=default_opts();o.reuse_learnts=(flags&BSAT_REUSE_LEARNTS)!=0;
    s->core=solver_new_with_opts(&o);
    if(!s->core){free(s);return NULL;}return s;
}
void bsat_destroy(bsat *s) {if(s){solver_free(s->core);free(s);}}
int bsat_error(const bsat *s) {return !s || s->core->error || s->core->watches->failed;}
int bsat_set_limits(bsat *s,double cpu,uint32_t conflicts,uint32_t decisions) {
    if(bsat_error(s))return 0;
    if(s->started || !isfinite(cpu) || cpu<0){s->core->error=true;return 0;}
    s->core->opts.max_time=cpu;s->core->opts.max_conflicts=conflicts;s->core->opts.max_decisions=decisions;return 1;
}
int bsat_set_query_limits(bsat *s,double cpu,uint32_t conflicts,uint32_t decisions) {
    if(bsat_error(s))return 0;
    if(!isfinite(cpu) || cpu<0){s->core->error=true;return 0;}
    s->core->opts.max_time=cpu;s->core->opts.max_conflicts=conflicts;s->core->opts.max_decisions=decisions;return 1;
}
int bsat_get_stats(const bsat *s,bsat_stats_v1 *out,size_t size) {
    if(bsat_error(s) || !out || size<sizeof *out || !s->stats.version)return 0;
    *out=s->stats;return 1;
}
static Lit *translate(bsat *s,const int *lits,size_t count,bool grow) {
    if(count>((1u<<28)-1) || (count&&!lits))goto bad;
    Var largest=0;
    for(size_t i=0;i<count;++i) {
        int64_t x=lits[i];uint64_t v=x<0?(uint64_t)-x:(uint64_t)x;
        if(!v || v>MAX_VARS)goto bad;
        if(v>largest)largest=(Var)v;
    }
    if(!grow && largest>s->core->num_vars)goto bad;
    while(s->core->num_vars<largest)if(!solver_new_var(s->core))goto bad;
    Lit *out=count?malloc(count*sizeof *out):NULL;
    if(count&&!out)goto bad;
    for(size_t i=0;i<count;++i)out[i]=fromDimacs(lits[i]);
    return out;
bad:s->core->error=true;return NULL;
}
int bsat_add_clause(bsat *s,const int *lits,size_t count) {
    if(bsat_error(s))return 0;
    s->started=true;s->result=BSAT_UNKNOWN;
    Lit *a=translate(s,lits,count,true);if(bsat_error(s)){free(a);return 0;}
    solver_add_clause(s->core,a,(uint32_t)count);free(a);return !bsat_error(s);
}
int bsat_solve(bsat *s,const int *assumptions,size_t count) {
    if(bsat_error(s))return BSAT_UNKNOWN;
    s->started=true;s->result=BSAT_UNKNOWN;
    Lit *a=translate(s,assumptions,count,false);if(bsat_error(s)){free(a);return BSAT_UNKNOWN;}
    double start=solver_cpu_time();
    lbool r=solver_solve_with_assumptions(s->core,a,(uint32_t)count);free(a);
    s->result=r==TRUE?BSAT_SAT:r==FALSE?BSAT_UNSAT:BSAT_UNKNOWN;
    bool searched=s->core->stats.start_time>=start;
    s->stats=(bsat_stats_v1){.version=1,.result=s->result,
        .conflicts=searched?s->core->stats.conflicts:0,.decisions=searched?s->core->stats.decisions:0,
        .propagations=searched?s->core->stats.propagations:0,.reused_preparations=s->core->reused_solves,
        .cpu_seconds=solver_cpu_time()-start,.owned_capacity_bytes=solver_memory(s->core).total};
    return s->result;
}
int bsat_value(const bsat *s,int lit) {
    if(bsat_error(s)||s->result!=BSAT_SAT)return 0;
    int64_t x=lit;uint64_t v=x<0?(uint64_t)-x:(uint64_t)x;
    if(!v || v>s->core->num_vars)return 0;
    lbool value=solver_model_value(s->core,(Var)v);if(value==UNDEF)return 0;
    return (value==TRUE)==(lit>0)?lit:-lit;
}
int bsat_failed(const bsat *s,int assumption) {
    if(bsat_error(s)||s->result!=BSAT_UNSAT)return 0;
    uint32_t n=0;const Lit *core=solver_conflict(s->core,&n);
    for(uint32_t i=0;i<n;++i)if(toDimacs(neg(core[i]))==assumption)return 1;
    return 0;
}
void bsat_set_terminate(bsat *s,void *state,int (*callback)(void *)) {
    if(s)solver_set_terminate(s->core,state,callback);
}
