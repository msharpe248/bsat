#include "../include/solver.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static Solver *fixture(unsigned n) {
    Solver *s=solver_new();assert(s);
    Lit *padding=malloc(2*n*sizeof(Lit)),*live=malloc(n*sizeof(Lit));assert(padding && live);
    for(unsigned i=0;i<2*n;++i) padding[i]=mkLit(1,false);
    for(unsigned i=0;i<n;++i) {assert(solver_new_var(s));live[i]=mkLit(i+1,false);}
    CRef dead=arena_alloc(s->arena,padding,2*n,true);assert(dead!=INVALID_CLAUSE);
    arena_delete(s->arena,dead);assert(solver_add_clause(s,live,n));free(padding);free(live);
    CLAUSE_HEADER(s->arena,s->clauses[0])->search=63;
    s->work=23;return s;
}
int main(void) {
    for(unsigned budget=0;budget<=72;++budget) {
        Solver *s=fixture(65);Arena *old=s->arena;size_t bytes=old->size*sizeof(uint32_t);
        uint32_t *copy=malloc(bytes);assert(copy);memcpy(copy,old->memory,bytes);
        CRef cr=s->clauses[0];s->work_limit=s->work+budget;
        solver_collect_garbage(s);
        // Two header visits plus 69 words in the complete live record.
        if(budget<=71) {
            assert(s->arena==old && !s->garbage_collections && s->clauses[0]==cr);
            assert(!memcmp(old->memory,copy,bytes) && s->work==s->work_limit);
        } else {
            assert(s->garbage_collections==1 && s->work==23+71);
            assert(s->clauses[0]==1 && CLAUSE_HEADER(s->arena,1)->search==63);
        }
        free(copy);s->work_limit=0;s->interrupted=false;
        solver_collect_garbage(s);assert(s->garbage_collections==1);
        for(Var v=1;v<=s->num_vars;++v) s->values[v]=TRUE;
        assert(solver_check_model(s));solver_free(s);
    }
    unsigned limits[]={1,1023,1024,1025,2048,4096,4102,4103,4104};
    for(unsigned i=0;i<sizeof limits/sizeof *limits;++i) {
        Solver *s=fixture(4097);Arena *old=s->arena;
        size_t bytes=old->size*sizeof(uint32_t);void *copy=malloc(bytes);assert(copy);
        memcpy(copy,old->memory,bytes);s->work_limit=s->work+limits[i];
        solver_collect_garbage(s);
        if(limits[i]<=4103) {
            assert(s->arena==old && !s->garbage_collections && !memcmp(copy,old->memory,bytes));
            assert(s->work==s->work_limit);
        } else assert(s->garbage_collections==1 && s->work==23+4103);
        free(copy);solver_free(s);
    }
    Solver *s=fixture(65);Arena *old=s->arena;
    s->opts.max_time=0.001;s->stats.start_time=solver_cpu_time()-1;
    s->clock_initialized=true;s->clock_work=s->work;s->clock_polls=0;
    uint64_t clocks=s->stats.clock_checks;
    solver_collect_garbage(s);
    assert(s->interrupted && s->arena==old && !s->garbage_collections);
    assert(s->work==23 && s->stats.clock_checks==clocks+1);
    solver_free(s);
    puts("PASS: 82 GC work cutoffs, exact rollback/retry and cached expired deadline");
}
