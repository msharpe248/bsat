/* Generated application-shaped histories with independent domain oracles.
   BMC: a 16-bit counter optionally increments at each step.
   Configuration: eight variants per stage with local compatibility constraints. */
#include "bsat.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
typedef struct {bsat *s;unsigned vars,clauses,queries;const char *kind;double cpu_limit;unsigned conflict_limit;} History;
static const char *certificate_directory;
static unsigned random_state=20260908;
static unsigned next(void){random_state=random_state*1664525u+1013904223u;return random_state;}
static void clause(History *h,const int *a,size_t n) {
    for(size_t i=0;i<n;++i){unsigned v=(unsigned)(a[i]<0?-a[i]:a[i]);if(v>h->vars)h->vars=v;}
    assert(bsat_add_clause(h->s,a,n));++h->clauses;
}
static void query_limits(History *h,double cpu,unsigned conflicts) {
    assert(bsat_set_query_limits(h->s,cpu,conflicts,0));
    h->cpu_limit=cpu;h->conflict_limit=conflicts;
}
static int query(History *h,const int *a,size_t n,int expected) {
    int r=bsat_solve(h->s,a,n);assert(!bsat_error(h->s));assert(r==0 || r==expected);
    bsat_stats_v1 s;assert(bsat_get_stats(h->s,&s,sizeof s));
    printf("{\"kind\":\"%s\",\"query\":%u,\"vars\":%u,\"clauses\":%u,\"result\":%d,\"oracle\":%d,\"conflicts\":%llu,\"decisions\":%llu,\"propagations\":%llu,\"reused\":%llu,\"owned_bytes\":%llu,\"cpu_seconds\":%.9f,\"cpu_limit\":%.9f,\"conflict_limit\":%u}\n",
        h->kind,h->queries++,h->vars,h->clauses,r,expected,(unsigned long long)s.conflicts,
        (unsigned long long)s.decisions,(unsigned long long)s.propagations,
        (unsigned long long)s.reused_preparations,(unsigned long long)s.owned_capacity_bytes,s.cpu_seconds,h->cpu_limit,h->conflict_limit);
    if(certificate_directory && r && h->vars>=4000 &&
       (!strcmp(h->kind,"bmc-counter") || r==20 || (h->queries-1)%16==0)) {
        char folder[4096],cnf[4096],proof[4096],model[4096];
        int len=snprintf(folder,sizeof folder,"%s/%s-%u",certificate_directory,h->kind,h->queries-1);
        assert(len>0 && (size_t)len+16<sizeof folder);assert(!mkdir(folder,0700));
        snprintf(cnf,sizeof cnf,"%s/input.cnf",folder);snprintf(proof,sizeof proof,"%s/proof.drat",folder);snprintf(model,sizeof model,"%s/model.txt",folder);
        assert(bsat_export_query(h->s,cnf,proof));FILE *f=fopen(model,"w");assert(f);
        fprintf(f,"s %s\n",r==10?"SATISFIABLE":"UNSATISFIABLE");
        if(r==10){fputs("v ",f);for(unsigned v=1;v<=h->vars;++v)fprintf(f,"%d ",bsat_value(h->s,(int)v));fputs("0\n",f);}assert(!fclose(f));
    }
    if(r==10)for(size_t i=0;i<n;++i)assert(bsat_value(h->s,a[i])==a[i]);
    return r;
}
static void xor_gate(History *h,int x,int c,int y) {
    int a[][3]={{-x,-c,-y},{x,c,-y},{x,-c,y},{-x,c,y}};
    for(int i=0;i<4;++i)clause(h,a[i],3);
}
static void and_gate(History *h,int x,int c,int z) {
    int a[]={-x,-c,z},b[]={x,-z},d[]={c,-z};clause(h,a,3);clause(h,b,2);clause(h,d,2);
}
static int cancelled(void *p){return *(int *)p;}
static unsigned counter_value(History *h,int *bits) {
    unsigned value=0;for(unsigned b=0;b<16;++b)if(bsat_value(h->s,bits[b])>0)value|=1u<<b;return value;
}
static void bmc(unsigned flags) {
    History h={.s=bsat_create(1,flags),.kind="bmc-counter"};assert(h.s);
    query_limits(&h,0.2,2000);
    int state[129][16],enable[129]={0};unsigned variable=0;
    for(int b=0;b<16;++b){state[0][b]=(int)++variable;int c=-state[0][b];clause(&h,&c,1);}
    for(unsigned depth=1;depth<=128;++depth) {
        enable[depth]=(int)++variable;int carry=enable[depth];
        for(int b=0;b<16;++b) {
            int x=state[depth-1][b],y=state[depth][b]=(int)++variable;xor_gate(&h,x,carry,y);
            if(b<15){int z=(int)++variable;and_gate(&h,x,carry,z);carry=z;}
        }
        if(depth%8)continue;
        unsigned targets[]={0,depth/2,depth,depth+1};
        for(unsigned t=0;t<4;++t) {
            int assumptions[16];for(unsigned b=0;b<16;++b)assumptions[b]=(targets[t]>>b)&1?state[depth][b]:-state[depth][b];
            int r=query(&h,assumptions,16,targets[t]<=depth?10:20);
            if(r==10) {
                assert(counter_value(&h,state[0])==0);
                for(unsigned step=1;step<=depth;++step)
                    assert(counter_value(&h,state[step])==counter_value(&h,state[step-1])+(bsat_value(h.s,enable[step])>0));
                assert(counter_value(&h,state[depth])==targets[t]);
            }
        }
        if(depth==64) {
            int stop=1;bsat_set_terminate(h.s,&stop,cancelled);assert(query(&h,NULL,0,10)==0);
            /* This assertion tests recovery, not completion within 0.2 CPU
               seconds on every sanitizer/platform. Keep a deterministic work
               bound; the outer history runner also has a wall timeout. */
            query_limits(&h,0,1000000);
            stop=0;assert(query(&h,NULL,0,10)==10);bsat_set_terminate(h.s,NULL,NULL);
            query_limits(&h,0.2,2000);
        }
    }
    assert(h.vars==4112);bsat_destroy(h.s);
}
static int feature(unsigned group,unsigned variant){return (int)(group*8+variant+1);}
static int config_oracle(unsigned groups,const unsigned char *allowed,const int *a,size_t n) {
    unsigned char domains[512];memcpy(domains,allowed,groups);
    for(size_t i=0;i<n;++i) {
        unsigned v=(unsigned)(a[i]<0?-a[i]:a[i])-1,g=v/8,bit=1u<<(v%8);
        if(a[i]>0)domains[g]&=(unsigned char)bit;else domains[g]&=(unsigned char)~bit;
    }
    unsigned reachable=domains[0];
    for(unsigned g=1;g<groups;++g)reachable=((reachable|((reachable<<1)|(reachable>>7)))&255)&domains[g];
    return reachable?10:20;
}
static void configuration(unsigned flags) {
    History h={.s=bsat_create(1,flags),.kind="configuration-chain"};assert(h.s);
    query_limits(&h,0.2,2000);unsigned char allowed[512];memset(allowed,255,sizeof allowed);
    unsigned groups=0;
    for(unsigned batch=0;batch<8;++batch) {
        for(unsigned added=0;added<64;++added,++groups) {
            int row[8];for(unsigned v=0;v<8;++v)row[v]=feature(groups,v);clause(&h,row,8);
            for(unsigned v=0;v<8;++v)for(unsigned w=0;w<v;++w){int c[]={-row[v],-row[w]};clause(&h,c,2);}
            if(groups)for(unsigned v=0;v<8;++v)for(unsigned w=0;w<8;++w)if(w!=v && w!=(v+1)%8) {
                int c[]={-feature(groups-1,v),-feature(groups,w)};clause(&h,c,2);
            }
        }
        for(unsigned q=0;q<64;++q) {
            int a[8];size_t n=1+next()%8;
            for(size_t i=0;i<n;++i){unsigned g=next()%groups,v=(next()>>8)%8;a[i]=feature(g,v);if(next()&256)a[i]=-a[i];}
            int expected=config_oracle(groups,allowed,a,n),r=query(&h,a,n,expected);
            if(r==10) {
                unsigned previous=0;
                for(unsigned g=0;g<groups;++g) {
                    unsigned count=0,chosen=0;for(unsigned v=0;v<8;++v)if(bsat_value(h.s,feature(g,v))>0){++count;chosen=v;}
                    assert(count==1 && (allowed[g]&(1u<<chosen)));if(g)assert(chosen==previous || chosen==(previous+1)%8);previous=chosen;
                }
            }
        }
        /* Permanent changes preserve the all-zero witness while excluding a variant. */
        unsigned g=next()%groups,v=1+(next()>>8)%7;int unit=-feature(g,v);clause(&h,&unit,1);allowed[g]&=(unsigned char)~(1u<<v);
    }
    assert(h.vars==4096);bsat_destroy(h.s);
}
int main(int argc,char **argv) {
    assert(argc==3 || argc==4);unsigned flags=(unsigned)strtoul(argv[2],NULL,10);assert(flags<=3);
    if(argc==4){assert(flags&BSAT_CERTIFICATES);certificate_directory=argv[3];}
    if(!strcmp(argv[1],"bmc"))bmc(flags);else {assert(!strcmp(argv[1],"configuration"));configuration(flags);}return 0;
}
