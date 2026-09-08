/* Built against the frozen ABI-v1 header and loaded against the new library. */
#include "bsat.h"
#include <assert.h>
int main(void) {
    assert(bsat_abi_version()==1);
    bsat *s=bsat_create(1,BSAT_REUSE_LEARNTS);assert(s);
    assert(bsat_set_limits(s,0,0,0));int c[]={1,2},a[]={-1,-2};
    assert(bsat_add_clause(s,c,2));assert(bsat_solve(s,a,2)==BSAT_UNSAT);
    assert(bsat_failed(s,-1));assert(bsat_solve(s,0,0)==BSAT_SAT);
    assert(bsat_value(s,1)>0 || bsat_value(s,2)>0);
    bsat_set_terminate(s,0,0);assert(!bsat_error(s));bsat_destroy(s);
}
