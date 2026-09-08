#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#include "bsat.h"
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <unistd.h>
int main(void) {
    char directory[]="/tmp/bsat-export-XXXXXX";assert(mkdtemp(directory));
    char cnf[256],proof[256],next_cnf[256],next_proof[256];
    snprintf(cnf,sizeof cnf,"%s/input",directory);snprintf(proof,sizeof proof,"%s/proof",directory);
    snprintf(next_cnf,sizeof next_cnf,"%s/next-input",directory);snprintf(next_proof,sizeof next_proof,"%s/next-proof",directory);
    bsat *s=bsat_create(1,BSAT_CERTIFICATES|BSAT_REUSE_LEARNTS);assert(s);
    for(int v=1;v<=1000;++v)assert(bsat_add_clause(s,&v,1));assert(bsat_solve(s,NULL,0)==10);
    struct rlimit saved,limited;assert(!getrlimit(RLIMIT_FSIZE,&saved));limited=saved;limited.rlim_cur=128;
    struct sigaction old,ignore={0};ignore.sa_handler=SIG_IGN;sigemptyset(&ignore.sa_mask);assert(!sigaction(SIGXFSZ,&ignore,&old));
    assert(!setrlimit(RLIMIT_FSIZE,&limited));assert(!bsat_export_query(s,cnf,proof));
    assert(!setrlimit(RLIMIT_FSIZE,&saved));assert(!sigaction(SIGXFSZ,&old,NULL));
    assert(!bsat_error(s));assert(bsat_value(s,1000)==1000);assert(bsat_export_query(s,next_cnf,next_proof));
    assert(bsat_solve(s,NULL,0)==10);bsat_destroy(s);
    unlink(cnf);unlink(proof);unlink(next_cnf);unlink(next_proof);assert(!rmdir(directory));
    puts("PASS: export flush failure preserves solver and permits a new export");
}
