#include "../examples/recoverable_service.h"
#include <assert.h>
#include <stdio.h>
typedef struct {unsigned calls, stop;} cancellation;
static int stopped(void *p) {cancellation *c=p;return ++c->calls==c->stop;}
int main(void) {
    recoverable_service s;assert(recovery_init(&s,4096,512));
    int a[]={1,2},b[]={-1,2},no=-2,three=3;
    assert(recovery_add(&s,a,2)&&recovery_add(&s,b,2));
    assert(recovery_solve(&s,NULL,0)==BSAT_UNKNOWN);
    s.journal_quota=1;assert(recovery_rebuild(&s));
    assert(recovery_solve(&s,&no,1)==BSAT_UNKNOWN&&!s.worker);
    assert(!recovery_value(&s,2)&&s.used==6&&s.generation==1);
    /* Accepted input remains replayable even while the worker is absent. */
    assert(recovery_add(&s,&three,1));s.journal_quota=512;
    cancellation cancel={0,0};recovery_set_cancel(&s,&cancel,stopped);
    for(unsigned at=1;at<=5;++at) {
        cancel.calls=0;cancel.stop=at;
        assert(!recovery_rebuild(&s)&&!s.worker&&!recovery_value(&s,2));
        assert(s.used==8&&s.generation==1);
    }
    cancel.stop=0;assert(recovery_rebuild(&s)&&s.generation==2);
    assert(recovery_solve(&s,&no,1)==BSAT_UNSAT);
    assert(recovery_solve(&s,NULL,0)==BSAT_SAT);
    assert(recovery_value(&s,2)==2&&recovery_value(&s,3)==3);
    cancel.calls=0;cancel.stop=1;
    assert(recovery_solve(&s,NULL,0)==BSAT_UNKNOWN&&!recovery_value(&s,2));
    cancel.stop=0;assert(recovery_solve(&s,NULL,0)==BSAT_SAT);
    /* Memory/log capacity rejection is atomic; old permanent input is intact. */
    size_t used=s.used;int bad=RECOVERY_MAX_VAR+1;
    assert(!recovery_add(&s,&bad,1)&&s.used==used);
    assert(!recovery_add(&s,a,s.capacity)&&s.used==used);
    s.journal_quota=513;assert(!recovery_rebuild(&s)&&!s.worker&&!recovery_value(&s,2));
    s.journal_quota=512;assert(recovery_rebuild(&s));
    three=-3;assert(recovery_add(&s,&three,1));
    assert(recovery_solve(&s,NULL,0)==BSAT_UNSAT);
    assert(recovery_rebuild(&s)&&recovery_solve(&s,NULL,0)==BSAT_UNSAT);
    recovery_destroy(&s);
    puts("PASS: quota failure, five interrupted replay boundaries, accepted offline additions, quota cap and fresh answers");
}
