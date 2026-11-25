#include "bsat.h"
#include "solver.h"
#include "pb_enc.h"
#include "queries.h"
#include "lrat.h"
#include <math.h>
#include <string.h>
#include <sys/types.h>
struct bsat {
    Solver *core; int result; bool started; bsat_stats_v1 stats;
    FILE *journal; Lit *query; size_t query_count; off_t journal_end;
    double wall_limit, wall_start; uint64_t capacity_limit;
    unsigned resource_polls; int service_hit; bool active;
    uint32_t threads;
    char *proof_path_copy;
    char *lrat_path_copy;
    void *user_state; int (*user_terminate)(void *);
};
static double wall_time(bsat *s) {
    struct timespec t;
    if(clock_gettime(CLOCK_MONOTONIC,&t)){s->core->error=true;return 0;}
    return (double)t.tv_sec+(double)t.tv_nsec*1e-9;
}
static bool service_exhausted(bsat *s,bool force) {
    if(s->service_hit)return true;
    if(!force && ++s->resource_polls<1024)return false;
    s->resource_polls=0;
    if(s->active && s->wall_limit && wall_time(s)-s->wall_start>=s->wall_limit)
        s->service_hit=BSAT_SERVICE_WALL;
    if(s->capacity_limit && solver_memory(s->core).total>s->capacity_limit)
        s->service_hit=BSAT_SERVICE_CAPACITY;
    return s->service_hit!=0;
}
static int terminate_service(void *state) {
    bsat *s=state;
    if(s->user_terminate && s->user_terminate(s->user_state))return 1;
    return service_exhausted(s,false);
}
static void install_terminate(bsat *s) {
    solver_set_terminate(s->core,s,
        s->user_terminate || s->wall_limit || s->capacity_limit ? terminate_service : NULL);
}
static void begin_service(bsat *s) {
    s->service_hit=0;s->resource_polls=1024;s->active=true;
    if(s->wall_limit)s->wall_start=wall_time(s);
}
uint32_t bsat_abi_version(void) { return BSAT_ABI_VERSION; }
bsat *bsat_create(uint32_t abi,uint32_t flags) {
    if(abi!=BSAT_ABI_VERSION || (flags & ~(BSAT_REUSE_LEARNTS|BSAT_CERTIFICATES|BSAT_CERTIFIED_PROBING)) ||
       ((flags&BSAT_CERTIFIED_PROBING) && !(flags&BSAT_CERTIFICATES)))return NULL;
    bsat *s=calloc(1,sizeof *s);if(!s)return NULL;
    SolverOpts o=default_opts();o.reuse_learnts=(flags&BSAT_REUSE_LEARNTS)!=0;
    o.probe_on_change=o.reuse_learnts;
    if(flags&BSAT_CERTIFICATES) {
        o.restart_assumptions=true;
        o.assumption_lbd=true; /* Score fixed query levels without dropping literals. */
        /* The journal contains RUP additions only. Keep variable namespace and
           original formula intact; do not enable equisatisfiable transforms. */
        o.probing=(flags&BSAT_CERTIFIED_PROBING)!=0;
        o.equiv=false;o.congruence=false;o.elim=false;o.bce=false;
        o.factor=false;o.inprocess=false;o.local_search=false;o.binary_proof=true;
        s->journal=tmpfile();if(!s->journal){free(s);return NULL;}
    }
    s->core=solver_new_with_opts(&o);
    if(!s->core){if(s->journal)fclose(s->journal);free(s);return NULL;}
    s->core->proof_journal=s->journal;return s;
}
void bsat_destroy(bsat *s) {
    if(s){
        solver_free(s->core);
        if(s->journal)fclose(s->journal);
        free(s->query);
        free(s->proof_path_copy);
        free(s->lrat_path_copy);
        free(s);
    }
}
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
int bsat_set_service_limits(bsat *s,double wall,uint64_t capacity) {
    if(bsat_error(s))return 0;
    if(!isfinite(wall) || wall<0){s->core->error=true;return 0;}
    s->wall_limit=wall;s->capacity_limit=capacity;install_terminate(s);return 1;
}
int bsat_service_limit_hit(const bsat *s) {return s?s->service_hit:BSAT_SERVICE_NONE;}
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
    while(s->core->num_vars<largest) {
        uint32_t capacity=s->core->var_capacity;
        if(!solver_new_var(s->core))goto bad;
        if(s->capacity_limit && capacity!=s->core->var_capacity && service_exhausted(s,true))goto bad;
    }
    Lit *out=count?malloc(count*sizeof *out):NULL;
    if(count&&!out)goto bad;
    for(size_t i=0;i<count;++i)out[i]=fromDimacs(lits[i]);
    return out;
bad:s->core->error=true;return NULL;
}
int bsat_add_clause(bsat *s,const int *lits,size_t count) {
    if(bsat_error(s))return 0;
    s->service_hit=0;
    if(service_exhausted(s,true)){s->core->error=true;return 0;}
    s->started=true;s->result=BSAT_UNKNOWN;free(s->query);s->query=NULL;s->query_count=0;
    Lit *a=translate(s,lits,count,true);if(bsat_error(s)){free(a);return 0;}
    solver_add_clause(s->core,a,(uint32_t)count);free(a);
    if(service_exhausted(s,true))s->core->error=true;
    return !bsat_error(s);
}
int bsat_solve(bsat *s,const int *assumptions,size_t count) {
    if(bsat_error(s))return BSAT_UNKNOWN;
    begin_service(s);
    s->started=true;s->result=BSAT_UNKNOWN;free(s->query);s->query=NULL;s->query_count=0;
    Lit *a=translate(s,assumptions,count,false);if(bsat_error(s)){free(a);s->active=false;return BSAT_UNKNOWN;}
    double start=solver_cpu_time();
    lbool r=(s->threads>1 && !count && !s->journal && !s->core->proof_file && !s->core->lrat)
        ? solver_solve_parallel(s->core,s->threads)
        : solver_solve_with_assumptions(s->core,a,(uint32_t)count);
    if(s->journal) {
        s->query=a;s->query_count=count;s->journal_end=ftello(s->journal);
        if(s->journal_end<0){s->core->error=true;r=UNDEF;}
    } else free(a);
    uint64_t owned=solver_memory(s->core).total;
    /* Snapshot collection and journal positioning are part of the solve call.
       Recheck its deadline/callback before exposing a conclusive facade result. */
    if(solver_budget_exhausted_now(s->core) || service_exhausted(s,true)) {
        r=UNDEF;s->core->result=UNDEF;s->core->interrupted=true;
    }
    s->active=false;
    s->result=r==TRUE?BSAT_SAT:r==FALSE?BSAT_UNSAT:BSAT_UNKNOWN;
    bool searched=s->core->stats.start_time>=start;
    s->stats=(bsat_stats_v1){.version=1,.result=s->result,
        .conflicts=searched?s->core->stats.conflicts:0,.decisions=searched?s->core->stats.decisions:0,
        .propagations=searched?s->core->stats.propagations:0,.reused_preparations=s->core->reused_solves,
        .cpu_seconds=solver_cpu_time()-start,.owned_capacity_bytes=owned};
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
    if(s){s->user_state=state;s->user_terminate=callback;install_terminate(s);}
}

int bsat_get_journal_bytes(const bsat *s,uint64_t *bytes) {
    if(!s || !s->journal || !bytes)return 0;
    *bytes=s->core->journal_bytes;return 1;
}
int bsat_set_journal_limit(bsat *s,uint64_t bytes) {
    if(bsat_error(s) || !s->journal || (bytes && bytes<s->core->journal_bytes))return 0;
    s->core->journal_limit=bytes;return 1;
}
int bsat_checkpoint(bsat *s) {
    if(bsat_error(s))return 0;
    begin_service(s);
    s->result=BSAT_UNKNOWN;memset(&s->stats,0,sizeof s->stats);
    free(s->query);s->query=NULL;s->query_count=0;
    FILE *next=s->journal?tmpfile():NULL;
    if(s->journal && !next){s->core->error=true;s->active=false;return 0;}
    if(!solver_reset_learning(s->core)){if(next)fclose(next);s->active=false;return 0;}
    if(s->journal) {
        int failed=fclose(s->journal);
        s->journal=next;s->core->proof_journal=next;
        s->core->journal_bytes=0;s->journal_end=0;
        if(failed){s->core->error=true;s->active=false;return 0;}
    }
    bool exhausted=service_exhausted(s,true);s->active=false;
    return !exhausted && !bsat_error(s);
}

int bsat_export_query(bsat *s,const char *cnf_path,const char *proof_path) {
    if(bsat_error(s) || !s->journal || (s->result!=10 && s->result!=20) ||
       !cnf_path || !proof_path || !strcmp(cnf_path,proof_path))return 0;
    FILE *cnf=fopen(cnf_path,"wbx");if(!cnf)return 0;
    FILE *proof=fopen(proof_path,"wbx");if(!proof){fclose(cnf);return 0;}
    bool ok=fprintf(cnf,"p cnf %u %zu\n",s->core->num_vars,
                    (size_t)s->core->input_clauses+s->query_count)>0;
    for(size_t i=0;ok && i<s->core->input_size;++i) {
        Lit lit=s->core->input[i];
        ok=lit?fprintf(cnf,"%d ",toDimacs(lit))>0:fputs("0\n",cnf)>=0;
    }
    for(size_t i=0;ok && i<s->query_count;++i)ok=fprintf(cnf,"%d 0\n",toDimacs(s->query[i]))>0;
    if(fclose(cnf))ok=false;
    if(fseeko(s->journal,0,SEEK_SET)){s->core->error=true;ok=false;}
    unsigned char buffer[65536];off_t left=s->journal_end;
    while(ok && left>0) {
        size_t n=left>(off_t)sizeof buffer?sizeof buffer:(size_t)left;
        if(fread(buffer,1,n,s->journal)!=n){s->core->error=true;ok=false;break;}
        if(fwrite(buffer,1,n,proof)!=n){ok=false;break;}left-=(off_t)n;
    }
    /* Restore append position even when an output write failed. */
    if(fseeko(s->journal,s->journal_end,SEEK_SET)){s->core->error=true;ok=false;}
    if(ok && s->result==20 && fwrite("a\0",1,2,proof)!=2)ok=false;
    if(fclose(proof))ok=false;
    return ok;
}

int bsat_freeze(bsat *s, int lit) {
    if (bsat_error(s) || !lit) return 0;
    int64_t x = lit; uint64_t v = x < 0 ? (uint64_t)-x : (uint64_t)x;
    if (!v || v > MAX_VARS) { s->core->error = true; return 0; }
    while (s->core->num_vars < (Var)v) {
        if (!solver_new_var(s->core)) return 0;
    }
    solver_freeze(s->core, (Var)v);
    return !bsat_error(s);
}

int bsat_melt(bsat *s, int lit) {
    if (bsat_error(s) || !lit) return 0;
    int64_t x = lit; uint64_t v = x < 0 ? (uint64_t)-x : (uint64_t)x;
    if (!v || v > s->core->num_vars) return 0;
    solver_melt(s->core, (Var)v);
    return !bsat_error(s);
}

int bsat_is_frozen(const bsat *s, int lit) {
    if (bsat_error(s) || !lit) return 0;
    int64_t x = lit; uint64_t v = x < 0 ? (uint64_t)-x : (uint64_t)x;
    if (!v || v > s->core->num_vars) return 0;
    return solver_is_frozen(s->core, (Var)v) ? 1 : 0;
}

int bsat_set_threads(bsat *s, uint32_t threads) {
    if (bsat_error(s) || s->started || threads == 0 || threads > 64) return 0;
    if (threads > 1 && (s->journal || s->core->proof_file || s->core->lrat)) return 0;
    s->threads = threads;
    return 1;
}

int bsat_set_proof(bsat *s, const char *path, int binary) {
    if (bsat_error(s) || s->started || !path || s->journal || s->threads > 1 ||
        s->core->proof_file || s->core->lrat) return 0;
    size_t len = strlen(path) + 1;
    char *copy = malloc(len);
    if (!copy) { s->core->error = true; return 0; }
    memcpy(copy, path, len);
    FILE *fp = fopen(copy, binary ? "wb" : "w");
    if (!fp) { free(copy); return 0; }
    s->proof_path_copy = copy;
    s->core->proof_file = fp;
    s->core->opts.proof_path = copy;
    s->core->opts.binary_proof = (binary != 0);
    return 1;
}

int bsat_set_lrat(bsat *s, const char *path, int binary) {
    if (bsat_error(s) || s->started || !path || s->journal || s->threads > 1 ||
        s->core->proof_file || s->core->lrat) return 0;
    size_t len = strlen(path) + 1;
    char *copy = malloc(len);
    if (!copy) { s->core->error = true; return 0; }
    memcpy(copy, path, len);
    LratState *lr = lrat_create(copy, binary != 0);
    if (!lr) { free(copy); return 0; }
    s->lrat_path_copy = copy;
    s->core->lrat = lr;
    s->core->opts.lrat_path = copy;
    s->core->opts.binary_lrat = (binary != 0);
    s->core->opts.sbva = false;
    s->core->opts.sym = false;
    s->core->opts.elim = false;
    s->core->opts.bce = false;
    s->core->opts.congruence = false;
    s->core->opts.equiv = false;
    s->core->opts.factor = false;
    s->core->opts.probing = false;
    return 1;
}

int bsat_add_xor(bsat *s, const int *lits, size_t count, int rhs) {
    if (bsat_error(s)) return 0;
    s->service_hit = 0;
    s->started = true; s->result = BSAT_UNKNOWN;
    Lit *a = translate(s, lits, count, true);
    if (bsat_error(s)) { free(a); return 0; }
    bool ok = pb_add_xor_clause(s->core, a, (uint32_t)count, rhs != 0);
    free(a);
    return ok && !bsat_error(s);
}

int bsat_add_atmost(bsat *s, const int *lits, size_t count, uint32_t k) {
    if (bsat_error(s)) return 0;
    s->service_hit = 0;
    s->started = true; s->result = BSAT_UNKNOWN;
    Lit *a = translate(s, lits, count, true);
    if (bsat_error(s)) { free(a); return 0; }
    bool ok = pb_add_atmost_sinz(s->core, a, (uint32_t)count, k);
    free(a);
    return ok && !bsat_error(s);
}

int bsat_add_pb_le(bsat *s, const int *lits, const uint64_t *weights,
                   size_t count, uint64_t rhs) {
    if (bsat_error(s) || (count && !weights)) return 0;
    s->service_hit = 0;
    s->started = true; s->result = BSAT_UNKNOWN;
    Lit *a = translate(s, lits, count, true);
    if (bsat_error(s)) { free(a); return 0; }
    bool ok = pb_add_pb_le(s->core, a, weights, (uint32_t)count, rhs);
    free(a);
    return ok && !bsat_error(s);
}

int bsat_backbone(bsat *s, int *out_lits, size_t cap, size_t *out_count) {
    if (bsat_error(s)) return BSAT_UNKNOWN;
    BackboneResult br = bsat_compute_backbone(s->core, s->core->num_vars, &s->core->opts);
    if (out_count) *out_count = br.size;
    if (out_lits && cap > 0 && br.lits) {
        size_t n = br.size < cap ? br.size : cap;
        for (size_t i = 0; i < n; ++i) out_lits[i] = toDimacs(br.lits[i]);
    }
    int rc = br.status == TRUE ? BSAT_SAT : (br.status == FALSE ? BSAT_UNSAT : BSAT_UNKNOWN);
    bsat_backbone_result_free(&br);
    return rc;
}

int bsat_mus(bsat *s, uint32_t *out_clause_ids, size_t cap, size_t *out_count) {
    if (bsat_error(s)) return BSAT_UNKNOWN;
    MusResult mr = bsat_compute_mus(s->core, s->core->num_vars, &s->core->opts);
    if (out_count) *out_count = mr.size;
    if (out_clause_ids && cap > 0 && mr.clause_ids) {
        size_t n = mr.size < cap ? mr.size : cap;
        memcpy(out_clause_ids, mr.clause_ids, n * sizeof(uint32_t));
    }
    int rc = mr.status == FALSE ? BSAT_UNSAT : (mr.status == TRUE ? BSAT_SAT : BSAT_UNKNOWN);
    bsat_mus_result_free(&mr);
    return rc;
}
