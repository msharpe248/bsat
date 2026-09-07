/* Standalone WalkSAT driver for model-verified quality/throughput comparisons. */
#include "../include/solver.h"
#include "../include/local_search.h"
#include "../include/dimacs.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static bool number(const char *text, uint32_t *out) {
    char *end;errno=0;
    if (!*text || *text=='-') return false;
    unsigned long n=strtoul(text,&end,10);
    if (errno || *end || n>UINT32_MAX) return false;
    *out=(uint32_t)n;return true;
}

int main(int argc, char **argv) {
    uint32_t flips,seed;
    if (argc!=4 || !number(argv[2],&flips) || !number(argv[3],&seed)) {
        fprintf(stderr,"usage: %s input.cnf max_flips seed\n",argv[0]);return 2;
    }
    SolverOpts o=default_opts();o.seed=seed;
    Solver *s=solver_new_with_opts(&o);if (!s) return 1;
    if (dimacs_parse_file(s,argv[1])!=DIMACS_OK) { solver_free(s);return 1; }
    LocalSearchState *ls=local_search_init(s);
    if (!ls) { solver_free(s);return 1; }
    double start=(double)clock()/CLOCKS_PER_SEC;
    bool found=local_search_run(s,ls,flips,0.5);
    double elapsed=(double)clock()/CLOCKS_PER_SEC-start;
    if (found) {
        local_search_copy_solution(s,ls);
        if (!solver_check_model(s)) { local_search_free(ls);solver_free(s);return 1; }
    }
    printf("c Walk CPU time : %.6f\nc Walk flips : %llu\nc Walk unsatisfied : %u\n",
           elapsed,(unsigned long long)ls->flips,ls->num_unsat);
    puts(found ? "s SATISFIABLE" : "s UNKNOWN");
    if (found) {
        printf("v");
        for (Var v=1;v<=s->num_vars;++v) printf(" %d",s->values[v]==TRUE?(int)v:-(int)v);
        puts(" 0");
    }
    local_search_free(ls);solver_free(s);return found?10:0;
}
