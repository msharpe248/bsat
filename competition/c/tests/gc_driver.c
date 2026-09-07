/* Collection-only CPU measurement; generated positive formulas provide models. */
#include "../include/solver.h"
#include "../include/dimacs.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc,char **argv) {
    if(argc!=3) return 2;
    char *end;unsigned long runs=strtoul(argv[2],&end,10);
    if(!*argv[2] || *end || !runs || runs>10000) return 2;
    Solver *s=solver_new();if(!s) return 2;
    if(dimacs_parse_file(s,argv[1])!=DIMACS_OK || !s->num_vars) {solver_free(s);return 2;}
    double cpu=0;size_t maximum_words=0;
    Lit padding[32];for(unsigned i=0;i<32;++i) padding[i]=mkLit(1,false);
    for(unsigned long round=0;round<runs;++round) {
        size_t target=s->arena->size/2;
        while(s->arena->wasted<target) {
            CRef cr=arena_alloc(s->arena,padding,32,true);
            if(cr==INVALID_CLAUSE) {solver_free(s);return 2;}
            arena_delete(s->arena,cr);
        }
        maximum_words=MAX(maximum_words,s->arena->size);
        uint64_t before=s->garbage_collections;
        double start=(double)clock()/CLOCKS_PER_SEC;
        solver_collect_garbage(s);
        cpu+=(double)clock()/CLOCKS_PER_SEC-start;
        if(s->garbage_collections!=before+1 || s->error) {solver_free(s);return 2;}
    }
    for(Var v=1;v<=s->num_vars;++v) s->values[v]=TRUE;
    bool valid=solver_check_model(s);
    printf("c Collection CPU: %.9f\nc Collections: %llu\nc Maximum old arena words: %zu\n",
           cpu,(unsigned long long)s->garbage_collections,maximum_words);
    if(valid) {
        puts("s SATISFIABLE");printf("v");
        for(Var v=1;v<=s->num_vars;++v) printf(" %u",v);
        puts(" 0");
    } else puts("s UNKNOWN");
    solver_free(s);return valid?10:0;
}
