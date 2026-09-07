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
static void check(Solver *s) {
    assert(solver_propagate(s)==INVALID_CLAUSE);
    bool okay=solver_simplify(s), sat=false;
    assert(!s->error && !s->interrupted && !s->decision_level && !s->work_limit);
    CRef cr=s->learnts[0];unsigned n=CLAUSE_SIZE(s->arena,cr);
    const Lit *lits=CLAUSE_LITS(s->arena,cr);
    assert(rup(s,lits,n));
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
static uint32_t state=20261112;
static uint32_t next(void) { state=1664525u*state+1013904223u;return state; }
int main(void) {
    const unsigned budgets[]={0,1,2,3,4,8,16,64,1024};unsigned cases=0;
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
            check(s);solver_free(s);++cases;
        }
    }
    for (unsigned signs=0;signs<16;++signs) for (unsigned kind=0;kind<5;++kind) {
        Lit c[4];for(Var v=1;v<=4;++v)c[v-1]=mkLit(v,(signs>>(v-1))&1u);
        Solver *s=fixture(c,4,10000);
        if (kind==0) {
            Lit a[]={c[0],c[1],mkLit(6,false)},b[]={c[0],c[1],mkLit(6,true)};
            assert(solver_add_clause(s,a,3) && solver_add_clause(s,b,3));
        } else if (kind==1) {
            Lit a[]={c[0],neg(c[1])};assert(solver_add_clause(s,a,2));
        } else if (kind==4) {
            Lit a[]={c[0],c[2]};assert(solver_add_clause(s,a,2));
        } else {
            Lit unit=kind==2?c[0]:neg(c[0]);assert(solver_add_clause(s,&unit,1));
        }
        assert(solver_propagate(s)==INVALID_CLAUSE);
        assert(solver_simplify(s));
        unsigned n=CLAUSE_SIZE(s->arena,s->learnts[0]);
        assert(n==((kind==0 || kind==4)?2:kind==2?1:3));
        check(s);solver_free(s);++cases;
    }
    assert(cases==4688);
    puts("PASS: 4688 vivification truth-table/RUP, budget, signed implication, root and subsequent solve cases");
}
