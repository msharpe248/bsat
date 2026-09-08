#include "ipasir.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
typedef struct { int cs[64][8], sizes[64], n, vars, calls; } Formula;
static int truth(const Formula *f,unsigned m) {
    for(int i=0;i<f->n;++i) {
        int sat=0;for(int j=0;j<f->sizes[i];++j){int x=f->cs[i][j],v=x<0?-x:x;sat|=((m>>(v-1))&1)==(unsigned)(x>0);}
        if(!sat)return 0;
    }return 1;
}
static void learned(void *state,int *lits) {
    Formula *f=state;int n=0;while(lits[n]){assert(n<8);++n;}++f->calls;
    for(unsigned m=0;m<(1u<<f->vars);++m)if(truth(f,m)) {
        int sat=0;for(int j=0;j<n;++j){int x=lits[j],v=x<0?-x:x;assert(v<=f->vars);sat|=((m>>(v-1))&1)==(unsigned)(x>0);}assert(sat);
    }
}
static int cancelled(void *state) {return *(int *)state;}
static unsigned rng=714193;
static unsigned next(void){rng=rng*1664525u+1013904223u;return rng;}
int main(void) {
    assert(strstr(ipasir_signature(),"bsat"));unsigned checks=0,total_learned=0;
    for(int trial=0;trial<128;++trial) {
        Formula f={.vars=6};void *s=ipasir_init();assert(s);ipasir_set_learn(s,&f,8,learned);
        /* Assumptions may introduce variables; they disappear after solve. */
        ipasir_assume(s,6);assert(ipasir_solve(s)==10);
        for(int round=0;round<24;++round) {
            int n=2+(int)(next()%3);f.sizes[f.n]=n;
            for(int j=0;j<n;++j){int x=1+(int)(next()%6);if(next()&128)x=-x;f.cs[f.n][j]=x;ipasir_add(s,x);}
            ipasir_add(s,0);++f.n;assert(!ipasir_val(s,1));
            for(int q=0;q<4;++q) {
                int a=1+(int)(next()%6);if(q&1)a=-a;
                ipasir_assume(s,a);assert(!ipasir_failed(s,a));
                int expected=20;for(unsigned m=0;m<64;++m)if(truth(&f,m)&&(((m>>((a<0?-a:a)-1))&1)==(unsigned)(a>0)))expected=10;
                assert(ipasir_solve(s)==expected);++checks;
                if(expected==10){unsigned m=0;for(int v=1;v<=6;++v)if(ipasir_val(s,v)>0)m|=1u<<(v-1);assert(truth(&f,m));assert(ipasir_val(s,a)==a);}
                else if(ipasir_failed(s,a))assert(!ipasir_failed(s,-a));
                int base=20;for(unsigned m=0;m<64;++m)if(truth(&f,m))base=10;
                assert(ipasir_solve(s)==base);++checks;
            }
        }
        total_learned+=f.calls;ipasir_set_learn(s,NULL,-1,NULL);ipasir_release(s);
    }
    assert(total_learned);
    void *s=ipasir_init();int stop=1;ipasir_assume(s,1);ipasir_set_terminate(s,&stop,cancelled);
    assert(ipasir_solve(s)==0);stop=0;ipasir_assume(s,-1);assert(ipasir_solve(s)==10);assert(ipasir_val(s,-1)==-1);
    ipasir_assume(s,1);ipasir_assume(s,-1);assert(ipasir_solve(s)==20);assert(ipasir_failed(s,1)&&ipasir_failed(s,-1));
    assert(ipasir_solve(s)==10);ipasir_add(s,0);assert(ipasir_solve(s)==20);ipasir_release(s);
    s=ipasir_init();ipasir_add(s,INT_MIN);assert(ipasir_solve(s)==0);ipasir_release(s);
    s=ipasir_init();ipasir_add(s,1);assert(ipasir_solve(s)==0);ipasir_release(s);
    printf("PASS: %u IPASIR oracle queries, %u independently entailed callback clauses\n",checks,total_learned);
}
