/* Build with `make checkpoint-example`. A changing guarded-pigeonhole service:
   queries alternate SAT/UNSAT, with permanent additions and learned-proof bursts.
   An application must retain permanent input to recreate an errored handle. */
#include "bsat.h"
#include "checkpoint_policy.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static double cpu(void) {return (double)clock()/CLOCKS_PER_SEC;}
static int add(bsat *s,const int *c,unsigned n) {return bsat_add_clause(s,c,n);}
int main(int argc,char **argv) {
    unsigned rounds=1024;bool fixed=false;
    if(argc>1) {char *end;unsigned long v=strtoul(argv[1],&end,10);if(!*argv[1] || *end || v<64 || v>100000)return 2;rounds=(unsigned)v;}
    if(argc>2) {if(argc!=3 || argv[2][0]!='f' || argv[2][1])return 2;fixed=true;}
    uint64_t quota=1048576;
    const char *quota_text=getenv("BSAT_EXAMPLE_QUOTA");
    if(quota_text) {
        char *end;unsigned long long parsed=strtoull(quota_text,&end,10);
        if(!*quota_text || *end || quota_text[0]=='-' || parsed<4096 || parsed>1048576)return 2;
        quota=(uint64_t)parsed;
    }
    checkpoint_policy policy;
    uint64_t floor=quota/4<65536?quota/4:65536;
    if(!checkpoint_policy_init(&policy,quota,floor))return 2;
    bsat *s=bsat_create(BSAT_ABI_VERSION,BSAT_REUSE_LEARNTS|BSAT_CERTIFICATES);
    if(!s)return 2;
    int code=2;
    if(!bsat_set_journal_limit(s,policy.quota) || !bsat_set_query_limits(s,5,0,0))goto done;
    unsigned next=1,guard=0,checkpoints=0;uint64_t peak=0;
    double solve_cpu=0,checkpoint_cpu=0;
    /* Four regimes grow independent guarded PHP(5,4), (6,5), (7,6), (6,5).
       Previously introduced guards remain explicitly disabled at each query. */
    int guards[4];unsigned regimes=0;
    for(unsigned q=0;q<rounds;++q) {
        if(q==0 || (regimes<4 && q>=regimes*(rounds/4))) {
            unsigned pigeons=regimes==2?7:regimes==0?5:6,holes=pigeons-1;
            unsigned first=next;guard=first+pigeons*holes;next=guard+1;
            guards[regimes++]=(int)guard;
            for(unsigned p=0;p<pigeons;++p) {
                int c[8];c[0]=-(int)guard;
                for(unsigned h=0;h<holes;++h)c[h+1]=(int)(first+p*holes+h);
                if(!add(s,c,holes+1))goto done;
                for(unsigned other=0;other<p;++other)for(unsigned h=0;h<holes;++h) {
                    int pair[]={-(int)guard,-(int)(first+p*holes+h),-(int)(first+other*holes+h)};
                    if(!add(s,pair,3))goto done;
                }
            }
        }
        uint64_t used;if(!bsat_get_journal_bytes(s,&used))goto done;
        int due=fixed?(used>=8192):checkpoint_policy_due(&policy,used);
        if(due<0) {fprintf(stderr,"Observed burst needs a larger journal quota\n");goto done;}
        if(due) {
            double start=cpu();int okay=bsat_checkpoint(s);double elapsed=cpu()-start;
            if(!okay)goto done; /* Cancelled checkpoints may be retried after clearing cancellation. */
            if(!bsat_get_journal_bytes(s,&used))goto done;
            checkpoint_cpu+=elapsed;++checkpoints;checkpoint_policy_completed(&policy,used,elapsed);
        }
        int assumptions[4];for(unsigned i=0;i<regimes;++i)assumptions[i]=-guards[i];
        assumptions[regimes-1]=(q%2)?-(int)guard:(int)guard;
        int result=bsat_solve(s,assumptions,regimes);
        /* Pigeonhole principle with every pair excluded is an exact domain oracle. */
        if(result!=(q%2?BSAT_SAT:BSAT_UNSAT) || bsat_error(s))goto done;
        bsat_stats_v1 stats;if(!bsat_get_stats(s,&stats,sizeof stats))goto done;
        if(!bsat_get_journal_bytes(s,&used) || used>policy.quota)goto done;
        checkpoint_policy_observe(&policy,used,stats.cpu_seconds);
        solve_cpu+=stats.cpu_seconds;if(used>peak)peak=used;
        printf("{\"query\":%u,\"regime\":%u,\"result\":%d,\"journal\":%llu,\"reserve\":%llu,\"conflicts\":%llu}\n",
               q,regimes,result,(unsigned long long)used,(unsigned long long)checkpoint_policy_reserve(&policy),(unsigned long long)stats.conflicts);
        /* Optional sampled exports for the independent example verifier. */
        const char *directory=getenv("BSAT_EXAMPLE_EXPORT_DIR");
        if(directory && q<4*(rounds/4) && q%(rounds/4)<2) {
            char cnf[1024],proof[1024],model[1024];
            if(snprintf(cnf,sizeof cnf,"%s/%u.cnf",directory,q)>=(int)sizeof cnf ||
               snprintf(proof,sizeof proof,"%s/%u.drat",directory,q)>=(int)sizeof proof ||
               snprintf(model,sizeof model,"%s/%u.model",directory,q)>=(int)sizeof model)goto done;
            if(!bsat_export_query(s,cnf,proof))goto done;
            if(result==BSAT_SAT) {
                FILE *f=fopen(model,"wx");if(!f)goto done;
                fprintf(f,"v ");for(unsigned v=1;v<next;++v)fprintf(f,"%d ",bsat_value(s,(int)v));
                fprintf(f,"0\n");if(fclose(f))goto done;
            }
        }
        /* Consume/export this answer before next iteration's checkpoint. */
    }
    printf("{\"complete\":true,\"fixed\":%s,\"queries\":%u,\"checkpoints\":%u,\"solve_cpu\":%.9f,\"checkpoint_cpu\":%.9f,\"peak_journal\":%llu}\n",
           fixed?"true":"false",rounds,checkpoints,solve_cpu,checkpoint_cpu,(unsigned long long)peak);
    code=0;
done:
    if(code)fprintf(stderr,"Service stopped; error=%d (discard errored handles)\n",bsat_error(s));
    bsat_destroy(s);return code;
}
