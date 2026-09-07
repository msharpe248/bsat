#include "../include/solver.h"
#include "../include/dimacs.h"
#include <assert.h>
#include <stdio.h>

static bool satisfied(const Lit *lits, unsigned n, unsigned assignment) {
    for (unsigned i = 0; i < n; ++i)
        if (((assignment >> (var(lits[i])-1)) & 1) != sign(lits[i])) return true;
    return false;
}

static void resolution(unsigned signs, bool tagged, unsigned budget) {
    SolverOpts o = default_opts();o.binary_minimize = true;o.minimize_budget = budget;
    Solver *s = solver_new_with_opts(&o);assert(s);
    for (unsigned v = 1; v <= 5; ++v) assert(solver_new_var(s) == v);
    Lit original[4];for (unsigned i=0;i<4;++i) original[i]=mkLit(i+1,(signs>>i)&1);
    Lit bins[2][2]={{original[0],neg(original[1])},{original[0],neg(original[2])}};
    for (unsigned i=0;i<2;++i) {
        CRef cr = INVALID_CLAUSE;
        if (tagged) { cr=arena_alloc(s->arena,bins[i],2,true);assert(cr!=INVALID_CLAUSE);cr=arena_binary_watch_ref(cr); }
        watch_add(s->watches,bins[i][0],cr,bins[i][1]);
        watch_add(s->watches,bins[i][1],cr,bins[i][0]);
    }
    // Duplicate hits must remove a literal only once; wrong polarity cannot remove d.
    watch_add(s->watches,original[0],INVALID_CLAUSE,neg(original[1]));
    watch_add(s->watches,original[0],INVALID_CLAUSE,original[3]);
    Lit long_clause[]={original[0],neg(original[3]),mkLit(5,false)};
    CRef long_ref=arena_alloc(s->arena,long_clause,3,false);assert(long_ref!=INVALID_CLAUSE);
    watch_add(s->watches,original[0],long_ref,neg(original[3]));
    watch_add(s->watches,neg(original[3]),long_ref,original[0]);
    Lit deleted[]={original[0],neg(original[3])};
    CRef gone=arena_alloc(s->arena,deleted,2,true);assert(gone!=INVALID_CLAUSE);
    watch_add(s->watches,deleted[0],arena_binary_watch_ref(gone),deleted[1]);
    watch_add(s->watches,deleted[1],arena_binary_watch_ref(gone),deleted[0]);
    solver_delete_clause(s,gone);
    Lit learnt[4];memcpy(learnt,original,sizeof learnt);uint32_t n=4;
    unsigned removed=budget==0?0:budget==1?1:2;
    assert(solver_minimize_binary(s,learnt,&n,4)==removed && n==4-removed);
    assert(learnt[0]==original[0] && learnt[n-1]==original[3]);
    for (unsigned v=1;v<=5;++v) assert(!s->seen[v]);
    for (unsigned a=0;a<32;++a)
        if (satisfied(original,4,a)&&satisfied(bins[0],2,a)&&satisfied(bins[1],2,a))
            assert(satisfied(learnt,n,a));
    for (unsigned mode=0;mode<4;++mode) {
        memcpy(learnt,original,sizeof learnt);n=4;
        s->opts.minimize=mode!=0;s->opts.binary_minimize=mode!=1;s->interrupted=mode==2;
        assert(!solver_minimize_binary(s,learnt,&n,mode==3?7:4) && n==4);
        assert(!memcmp(learnt,original,sizeof learnt));
    }
    solver_free(s);
}

static void unit_result(void) {
    SolverOpts o=default_opts();o.binary_minimize=true;
    Solver *s=solver_new_with_opts(&o);assert(s);
    assert(solver_new_var(s)==1 && solver_new_var(s)==2);
    Lit a=mkLit(1,false),b=mkLit(2,false),learnt[]={a,b};
    watch_add(s->watches,a,INVALID_CLAUSE,neg(b));
    uint32_t n=2;assert(solver_minimize_binary(s,learnt,&n,2)==1 && n==1 && learnt[0]==a);
    assert(!s->seen[1] && !s->seen[2]);solver_free(s);
}
static void integration(void) {
    const char *inputs[] = {
        "p cnf 8 30\n-5 3 -2 0\n4 -6 1 0\n4 3 5 0\n1 2 -4 0\n8 4 -1 0\n-3 -2 0\n-6 -2 4 0\n-6 2 -8 0\n8 1 6 0\n-1 -3 0\n8 -1 -2 0\n3 8 6 0\n1 7 -8 0\n-5 4 3 0\n6 4 3 0\n-5 4 -8 0\n6 2 3 0\n6 8 4 0\n-4 -6 -7 0\n-6 -3 4 0\n-4 1 -6 0\n3 7 -6 0\n-4 3 6 0\n-4 -1 0\n-5 8 -3 0\n5 1 -2 0\n-2 -4 -5 0\n5 -2 0\n4 7 5 0\n-2 -1 5 0\n",
        "p cnf 8 36\n2 6 7 0\n4 -3 5 0\n-2 -8 -5 0\n-3 -6 2 0\n-1 5 -6 0\n-5 2 7 0\n6 -3 0\n-6 -2 0\n2 6 8 0\n-3 7 8 0\n6 -7 0\n8 1 -4 0\n1 -3 0\n8 -6 -2 0\n-7 -6 2 0\n2 -7 3 0\n2 -8 -4 0\n-7 4 0\n8 -6 0\n4 8 5 0\n-2 1 7 0\n8 3 -4 0\n3 -2 8 0\n-1 -5 -2 0\n-7 4 0\n-1 4 3 0\n-5 4 8 0\n-7 4 1 0\n-1 3 7 0\n-1 8 -5 0\n8 -4 5 0\n-3 -6 0\n2 -1 0\n5 4 -6 0\n2 -3 0\n-4 7 -1 0\n"
    };
    for (unsigned i=0;i<2;++i) {
        SolverOpts o=default_opts();o.binary_minimize=true;o.probing=false;o.random_phase=true;
        Solver *s=solver_new_with_opts(&o);assert(s);
        assert(dimacs_parse_string(s,inputs[i])==DIMACS_OK);
        assert(solver_solve(s)==(i?FALSE:TRUE));
        assert(s->stats.binary_minimize_removed>0);
        if (!i) assert(solver_check_model(s));
        for (Var v=1;v<=s->num_vars;++v) assert(!s->seen[v]);
        solver_free(s);
    }
}

int main(void) {
    for (unsigned signs=0;signs<16;++signs)
        for (unsigned kind=0;kind<2;++kind)
            for (unsigned budget=0;budget<3;++budget) resolution(signs,kind,budget==2?100:budget);
    unit_result();integration();
    puts("PASS: signed binary resolution, duplicate/deleted/long watches, budgets, cancellation and truth tables");
    return 0;
}
