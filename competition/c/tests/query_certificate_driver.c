#include "bsat.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
static unsigned id;
static const char *directory;
static void path(char *out,size_t size,unsigned query,const char *file) {
    int n=snprintf(out,size,"%s/query-%u/%s",directory,query,file);assert(n>0 && (size_t)n<size);
}
static int stop(void *state){return *(int *)state;}
static void query(bsat *s,int *a,size_t n,int expected,int vars) {
    int r=bsat_solve(s,a,n);assert(r==expected);assert(!bsat_error(s));
    if(r==0){assert(!bsat_export_query(s,"/nonexistent/query","/nonexistent/proof"));return;}
    char folder[4096],cnf[4096],proof[4096],model[4096];unsigned q=id++;
    path(folder,sizeof folder,q,"");assert(!mkdir(folder,0700));
    path(cnf,sizeof cnf,q,"input.cnf");path(proof,sizeof proof,q,"proof.drat");path(model,sizeof model,q,"model.txt");
    bsat_stats_v1 before,after;assert(bsat_get_stats(s,&before,sizeof before));
    assert(bsat_export_query(s,cnf,proof));assert(!bsat_export_query(s,cnf,proof));
    assert(bsat_get_stats(s,&after,sizeof after));assert(!memcmp(&before,&after,sizeof before));
    FILE *f=fopen(model,"w");assert(f);fprintf(f,"s %s\n",r==10?"SATISFIABLE":"UNSATISFIABLE");
    if(r==10){fputs("v ",f);for(int v=1;v<=vars;++v)fprintf(f,"%d ",bsat_value(s,v));fputs("0\n",f);}assert(!fclose(f));
    printf("%u %d %llu %llu\n",q,r,(unsigned long long)before.reused_preparations,(unsigned long long)before.conflicts);
}
int main(int argc,char **argv) {
    assert(argc==2);directory=argv[1];
    for(unsigned reuse=0;reuse<2;++reuse) {
        bsat *s=bsat_create(1,BSAT_CERTIFICATES|(reuse?BSAT_REUSE_LEARNTS:0));assert(s);
        int c[]={1,2},d[]={-1,2};assert(bsat_add_clause(s,c,2));assert(bsat_add_clause(s,d,2));
        for(int i=0;i<8;++i){int a=-2;query(s,&a,1,20,2);query(s,NULL,0,10,2);}
        int both[]={1,-1};query(s,both,2,20,2);query(s,NULL,0,10,2);
        bsat_destroy(s);
    }
    bsat *s=bsat_create(1,BSAT_CERTIFICATES|BSAT_REUSE_LEARNTS);assert(s);
    const int holes=6,pigeons=7,guard=43;
    for(int i=0;i<pigeons;++i) {
        int c[7];c[0]=-guard;for(int j=0;j<holes;++j)c[j+1]=1+i*holes+j;assert(bsat_add_clause(s,c,7));
        for(int k=0;k<i;++k)for(int j=0;j<holes;++j){int d[]={-guard,-(1+i*holes+j),-(1+k*holes+j)};assert(bsat_add_clause(s,d,3));}
    }
    int a=guard,b=-guard;
    assert(bsat_set_query_limits(s,0,1,0));query(s,&a,1,0,guard);
    assert(bsat_set_query_limits(s,0,0,0));query(s,&a,1,20,guard);query(s,&b,1,10,guard);
    for(int i=0;i<4;++i){int dup[]={guard,guard};query(s,dup,2,20,guard);query(s,&b,1,10,guard);}
    int cancel=1;bsat_set_terminate(s,&cancel,stop);query(s,&a,1,0,guard);cancel=0;query(s,&a,1,20,guard);
    int unit=130;assert(bsat_add_clause(s,&unit,1));assert(!bsat_export_query(s,"/nonexistent/query","/nonexistent/proof"));
    query(s,&b,1,10,130);query(s,&a,1,20,130);
    assert(bsat_add_clause(s,&a,1));query(s,NULL,0,20,130);query(s,NULL,0,20,130);
    bsat_destroy(s);return 0;
}
