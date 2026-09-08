#include "bsat.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static int stop(void *p){return *(int *)p;}
static bsat *formula(void) {
    bsat *s=bsat_create(1,BSAT_REUSE_LEARNTS|BSAT_CERTIFICATES);assert(s);
    int c[]={1,2};assert(bsat_add_clause(s,c,2));assert(bsat_solve(s,NULL,0)==10);return s;
}
static void growing_search(void) {
    bsat *s=bsat_create(1,BSAT_CERTIFICATES|BSAT_REUSE_LEARNTS);assert(s);
    const int holes=6,guard=43;
    for(int p=0;p<7;++p) {
        int c[7];c[0]=-guard;for(int h=0;h<holes;++h)c[h+1]=p*holes+h+1;
        assert(bsat_add_clause(s,c,7));
        for(int q=0;q<p;++q)for(int h=0;h<holes;++h){int d[]={-guard,-(p*holes+h+1),-(q*holes+h+1)};assert(bsat_add_clause(s,d,3));}
    }
    int disabled=-guard;assert(bsat_solve(s,&disabled,1)==10);
    bsat_stats_v1 stats;assert(bsat_get_stats(s,&stats,sizeof stats));
    assert(bsat_set_service_limits(s,0,stats.owned_capacity_bytes+128));
    assert(bsat_solve(s,&guard,1)==0 && !bsat_error(s));
    assert(bsat_service_limit_hit(s)==BSAT_SERVICE_CAPACITY);
    assert(bsat_get_stats(s,&stats,sizeof stats)&&stats.conflicts>0);
    assert(bsat_set_service_limits(s,0,0));assert(bsat_solve(s,&guard,1)==20);
    bsat_destroy(s);
}
int main(void) {
    growing_search();
    bsat *s=formula();int value=bsat_value(s,1);
    assert(bsat_set_service_limits(s,1e-12,0));assert(bsat_value(s,1)==value);
    assert(bsat_solve(s,NULL,0)==0 && !bsat_error(s));
    assert(bsat_service_limit_hit(s)==BSAT_SERVICE_WALL);assert(!bsat_value(s,1));
    assert(!bsat_export_query(s,"/unused/a","/unused/b"));
    assert(bsat_set_service_limits(s,0,0));assert(bsat_solve(s,NULL,0)==10);
    assert(bsat_service_limit_hit(s)==BSAT_SERVICE_NONE);
    assert(bsat_set_service_limits(s,0,1));assert(bsat_solve(s,NULL,0)==0&&!bsat_error(s));
    assert(bsat_service_limit_hit(s)==BSAT_SERVICE_CAPACITY);
    assert(bsat_set_service_limits(s,0,0));assert(bsat_checkpoint(s));assert(bsat_solve(s,NULL,0)==10);
    int cancelled=1;bsat_set_terminate(s,&cancelled,stop);assert(bsat_set_service_limits(s,10,10000000));
    assert(bsat_solve(s,NULL,0)==0&&!bsat_error(s));assert(!bsat_service_limit_hit(s));
    cancelled=0;assert(bsat_solve(s,NULL,0)==10);bsat_set_terminate(s,NULL,NULL);
    assert(bsat_set_service_limits(s,1e-12,0));assert(!bsat_checkpoint(s));
    assert(bsat_service_limit_hit(s)==BSAT_SERVICE_WALL);
    assert(bsat_set_service_limits(s,0,0));assert(bsat_checkpoint(s));assert(bsat_solve(s,NULL,0)==10);
    bsat_destroy(s);
    s=formula();assert(bsat_set_service_limits(s,0,1));int unit=3;
    assert(!bsat_add_clause(s,&unit,1)&&bsat_error(s));assert(bsat_solve(s,NULL,0)==0);bsat_destroy(s);
    for(unsigned i=0;i<3;++i){s=formula();assert(!bsat_set_service_limits(s,i==0?NAN:i==1?INFINITY:-1,0));assert(bsat_error(s));bsat_destroy(s);}
    puts("PASS: cooperative wall/capacity limits, retry, callback composition, checkpoint and input exhaustion");
}
