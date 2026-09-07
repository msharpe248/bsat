/* Check vivification against original-CNF truth tables and an independent RUP
 * checker. Neither oracle uses watches, solver assignments or reason graphs. */
#include "../include/solver.h"
#include <assert.h>
#include <stdio.h>

static bool lit_value(unsigned bits, Lit l) {
    return ((bits >> (var(l)-1)) & 1u) != sign(l);
}
static bool model(const Solver *s, unsigned bits) {
    bool sat = false;
    for (size_t i=0; i<s->input_size; ++i) {
        if (s->input[i]) sat |= lit_value(bits,s->input[i]);
        else { if (!sat) return false; sat=false; }
    }
    return true;
}
static bool rup(const Solver *s, const Lit *lits, unsigned n) {
    int values[7]={0};
    for (unsigned i=0; i<n; ++i) {
        int value=sign(lits[i]) ? 1 : -1;
        if (values[var(lits[i])] == -value) return true;
        values[var(lits[i])] = value;
    }
    bool changed;
    do {
        changed=false; bool satisfied=false; unsigned undef=0; Lit unit=0;
        for (size_t i=0; i<s->input_size; ++i) {
            Lit l=s->input[i];
            if (l) {
                int value=values[var(l)];
                if (!value) { ++undef;unit=l; }
                else if ((value>0) != sign(l)) satisfied=true;
            } else {
                if (!satisfied) {
                    if (!undef) return true;
                    if (undef==1) { values[var(unit)]=sign(unit)?-1:1;changed=true; }
                }
                satisfied=false;undef=0;
            }
        }
    } while (changed);
    return false;
}
static Solver *fixture(const Lit *lits, unsigned n, unsigned budget) {
    SolverOpts o=default_opts();o.inprocess=true;o.inprocess_interval=1;
    o.preprocess_budget=budget;o.probing=false;
    Solver *s=solver_new_with_opts(&o);assert(s);
    for (unsigned v=0;v<6;++v) assert(solver_new_var(s));
    assert(solver_add_clause(s,lits,n));
    CRef cr=arena_alloc(s->arena,lits,n,true);assert(cr!=INVALID_CLAUSE);
    s->learnts=malloc(sizeof *s->learnts);assert(s->learnts);
    s->learnts[0]=cr;s->num_learnts=s->learnts_size=1;
    watch_add(s->watches,lits[0],cr,lits[1]);
    watch_add(s->watches,lits[1],cr,lits[0]);
    s->stats.conflicts=1;
    return s;
}
static void check(Solver *s, bool exact) {
    assert(solver_propagate(s)==INVALID_CLAUSE);
    Lit expected[6];unsigned expected_n=0;
    if (exact) {
        CRef before=s->learnts[0];expected_n=CLAUSE_SIZE(s->arena,before);
        memcpy(expected,CLAUSE_LITS(s->arena,before),expected_n*sizeof *expected);
        for (unsigned i=0;i<expected_n;) {
            Lit trial[6];unsigned out=0;
            for(unsigned j=0;j<expected_n;++j)if(j!=i)trial[out++]=expected[j];
            if(rup(s,trial,out)) {memcpy(expected,trial,out*sizeof *expected);expected_n=out;}
            else ++i;
        }
    }
    bool okay=solver_simplify(s), sat=false;
    assert(!s->error && !s->interrupted && !s->decision_level && !s->work_limit);
    CRef cr=s->learnts[0];unsigned n=CLAUSE_SIZE(s->arena,cr);
    const Lit *lits=CLAUSE_LITS(s->arena,cr);
    assert(rup(s,lits,n));
    if(exact) {
        assert(n==expected_n);
        for(unsigned i=0;i<n;++i) {
            bool found=false;for(unsigned j=0;j<n;++j)found |= lits[i]==expected[j];
            assert(found);
        }
    }
    for (unsigned bits=0;bits<64;++bits) if (model(s,bits)) {
        sat=true;bool clause=false;
        for (unsigned i=0;i<n;++i) clause |= lit_value(bits,lits[i]);
        assert(clause && okay);
        for (Var v=1;v<=6;++v) if (s->values[v]!=UNDEF)
            assert(((bits>>(v-1))&1u)==(s->values[v]==TRUE));
    }
    assert(solver_solve(s)==(sat?TRUE:FALSE));
    if (sat) assert(solver_check_model(s));
}
/* Stop inside a long watch scan, then retry. Only root units may survive the
 * temporary trials, even when propagation stops before reaching a fixed point. */
static void interrupted_scan(void) {
    const unsigned budgets[]={1,1023,1024,1025,2047,2048,4096,8192};
    for(unsigned b=0;b<sizeof budgets/sizeof *budgets;++b) {
        SolverOpts o=default_opts();o.inprocess=true;o.inprocess_interval=1;
        o.preprocess_budget=budgets[b];o.probing=false;
        Solver *s=solver_new_with_opts(&o);assert(s);
        for(unsigned v=0;v<4100;++v)assert(solver_new_var(s));
        Lit c[]={mkLit(1,false),mkLit(2,false),mkLit(3,false)};
        assert(solver_add_clause(s,c,3));
        Lit *long_clause=malloc(4098*sizeof *long_clause);assert(long_clause);
        long_clause[0]=mkLit(2,false);
        for(unsigned v=4;v<=4100;++v)long_clause[v-3]=mkLit(v,false);
        assert(solver_add_clause(s,long_clause,4098));free(long_clause);
        for(unsigned v=4;v<4100;++v) {Lit unit=mkLit(v,true);assert(solver_add_clause(s,&unit,1));}
        CRef cr=arena_alloc(s->arena,c,3,true);assert(cr!=INVALID_CLAUSE);
        s->learnts=malloc(sizeof *s->learnts);assert(s->learnts);
        s->learnts[0]=cr;s->num_learnts=s->learnts_size=1;
        watch_add(s->watches,c[0],cr,c[1]);watch_add(s->watches,c[1],cr,c[0]);
        assert(solver_propagate(s)==INVALID_CLAUSE);
        unsigned root=s->trail_size;s->work=0;s->stats.conflicts=1;
        assert(solver_simplify(s));
        assert(!s->error && !s->interrupted && !s->decision_level && !s->work_limit);
        assert(s->trail_size==root && s->qhead==root && s->learnts[0]==cr);
        for(Var v=1;v<=3;++v)assert(s->values[v]==UNDEF);
        assert(s->values[4100]==UNDEF);
        for(Var v=4;v<4100;++v)assert(s->values[v]==FALSE);
        s->opts.preprocess_budget=1000000;s->stats.conflicts++;
        assert(solver_simplify(s) && s->learnts[0]==cr);
        assert(solver_solve(s)==TRUE && solver_check_model(s));solver_free(s);
    }
    puts("PASS: 8 long-scan vivification cutoffs, temporary-trail rollback and retry");
}
/* One successful deletion followed by irreducible literals. Resource stops
 * between failed trials must not install a clause with a pending omission. */
static void partial_deletion(void) {
    unsigned cases=0;
    for(unsigned signs=0;signs<16;++signs)for(unsigned budget=0;budget<=128;++budget) {
        Lit c[4];for(Var v=1;v<=4;++v)c[v-1]=mkLit(v,(signs>>(v-1))&1u);
        Solver *s=fixture(c,4,budget);
        Lit a[]={c[1],c[2],c[3],mkLit(6,false)};
        Lit b[]={c[1],c[2],c[3],mkLit(6,true)};
        assert(solver_add_clause(s,a,4) && solver_add_clause(s,b,4));
        check(s,false);solver_free(s);++cases;
    }
    assert(cases==2064);
    puts("PASS: 2064 signed partial-deletion and failed-trial cutoff cases");
}
static uint32_t state=20261112;
static uint32_t next(void) { state=1664525u*state+1013904223u;return state; }
int main(void) {
    const unsigned budgets[]={0,1,2,3,4,8,16,64,1048576};unsigned cases=0;
    for (unsigned sample=0;sample<512;++sample) {
        Lit original[5],extras[8][3];
        for (Var v=1;v<=5;++v) original[v-1]=mkLit(v,(next()>>16)&1u);
        for (unsigned k=0;k<8;++k) {
            unsigned first=next()%6;
            for (unsigned j=0;j<3;++j) extras[k][j]=mkLit(1+(first+j)%6,(next()>>16)&1u);
        }
        for (unsigned b=0;b<sizeof budgets/sizeof *budgets;++b) {
            Solver *s=fixture(original,5,budgets[b]);
            for (unsigned k=0;k<8;++k) assert(solver_add_clause(s,extras[k],2+(k&1u)));
            check(s,budgets[b]==1048576);solver_free(s);++cases;
        }
    }
    for (unsigned signs=0;signs<16;++signs) for (unsigned kind=0;kind<6;++kind) {
        Lit c[4];for(Var v=1;v<=4;++v)c[v-1]=mkLit(v,(signs>>(v-1))&1u);
        Solver *s=fixture(c,4,10000);
        if (kind==0) {
            Lit a[]={c[0],c[1],mkLit(6,false)},b[]={c[0],c[1],mkLit(6,true)};
            assert(solver_add_clause(s,a,3) && solver_add_clause(s,b,3));
        } else if (kind==1) {
            Lit a[]={c[0],neg(c[1])};assert(solver_add_clause(s,a,2));
        } else if (kind==4) {
            Lit a[]={c[0],c[2]};assert(solver_add_clause(s,a,2));
        } else if (kind==5) {
            Lit a[]={c[1],c[2]};assert(solver_add_clause(s,a,2));
        } else {
            Lit unit=kind==2?c[0]:neg(c[0]);assert(solver_add_clause(s,&unit,1));
        }
        assert(solver_propagate(s)==INVALID_CLAUSE);
        assert(solver_simplify(s));
        unsigned n=CLAUSE_SIZE(s->arena,s->learnts[0]);
        assert(n==((kind==0 || kind>=4)?2:kind==2?1:3));
        check(s,false);solver_free(s);++cases;
    }
    partial_deletion();
    interrupted_scan();
    assert(cases==4704);
    puts("PASS: 4704 vivification truth-table/RUP, budget, signed implication, root, 512 exact deletion-oracle and subsequent solve cases");
}
