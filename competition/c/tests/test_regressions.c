#include "../include/solver.h"
#include "../include/dimacs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static Solver *formula(const char *text) {
    Solver *s=solver_new(); assert(s);
    assert(dimacs_parse_string(s,text)==DIMACS_OK);
    return s;
}
static void parser(void) {
    const char *valid[]={"p cnf 1 1\n0\n", "p cnf 1 2\n1 0 -1 0\n",
                         "p cnf 2 2\n1\n2 0\n-1 0\n", "p cnf 0 0\n",
                         "c header\np cnf 2 2\n1 1 0\n2 -2 0\n"};
    lbool expected[]={FALSE,FALSE,TRUE,TRUE,TRUE};
    for (unsigned i=0;i<5;++i) {
        Solver *s=formula(valid[i]);assert(solver_solve(s)==expected[i]);solver_free(s);
    }
    const char *bad[]={"p cnf 1 1\n1", "p cnf 1 1\n2 0\n", "p cnf 1 2\n1 0\n",
                       "p cnf 999999999999999999999 0\n", "p cnf 1 1\n-2147483648 0\n",
                       "p cnf 1 1\n1x 0\n", "p cnf 1 0\n0\n"};
    for (unsigned i=0;i<sizeof bad/sizeof *bad;++i) {
        Solver *s=solver_new(); assert(dimacs_parse_string(s,bad[i])!=DIMACS_OK);solver_free(s);
    }
    /* Long physical lines have no fixed parser limit. */
    FILE *f=tmpfile();assert(f);fputs("p cnf 1 1\n",f);
    for (unsigned i=0;i<600000;++i) fputs("1 ",f);
    fputs("0\n",f);rewind(f);
    Solver *s=solver_new();assert(dimacs_parse_stream(s,f)==DIMACS_OK);fclose(f);
    assert(solver_solve(s)==TRUE);solver_free(s);
}
static void assumptions(void) {
    Solver *s=formula("p cnf 1 1\n1 0\n");
    Lit a=mkLit(1,true);
    assert(solver_solve_with_assumptions(s,&a,1)==FALSE);
    uint32_t core_size=0;const Lit *core=solver_conflict(s,&core_size);
    assert(core_size==1 && core[0]==neg(a));
    assert(solver_solve(s)==TRUE);
    assert(!solver_add_clause(s,&a,1));
    assert(solver_solve(s)==FALSE);solver_free(s);
    s=formula("p cnf 2 1\n1 2 0\n");
    Lit repeated[300];for(unsigned i=0;i<300;++i) repeated[i]=mkLit(1,true);
    assert(solver_solve_with_assumptions(s,repeated,300)==TRUE);
    assert(solver_model_value(s,2)==TRUE);
    assert(solver_new_var(s)==3);
    Lit unit=mkLit(3,true);assert(solver_add_clause(s,&unit,1));
    assert(solver_solve(s)==TRUE);assert(solver_model_value(s,3)==FALSE);solver_free(s);
}
static unsigned rng=19;
static unsigned next(void) { rng=rng*1664525u+1013904223u;return rng; }
static void api_fuzz(void) {
    for(unsigned trial=0;trial<100;++trial) {
        SolverOpts o=default_opts();o.probing=false;o.elim=trial%2;o.bce=trial%3==0;
        o.reduce_interval=1;o.inprocess=true;o.inprocess_interval=1;
        Solver *s=solver_new_with_opts(&o);assert(s);
        for(unsigned v=0;v<5;++v) assert(solver_new_var(s));
        Lit clauses[16][3];
        for(unsigned c=0;c<16;++c) {
            for(unsigned j=0;j<3;++j) clauses[c][j]=mkLit(1+next()%5,(next()>>16)&1u);
            solver_add_clause(s,clauses[c],3);
        }
        for(unsigned call=0;call<8;++call) {
            Lit a[3];unsigned na=call%4;
            for(unsigned j=0;j<na;++j) a[j]=mkLit(1+next()%5,(next()>>16)&1u);
            bool sat=false;
            for(unsigned bits=0;bits<32;++bits) {
                bool ok=true;
                for(unsigned c=0;c<16;++c) {
                    bool satisfied=false;
                    for(unsigned j=0;j<3;++j) {
                        Lit l=clauses[c][j];
                        if (((bits>>(var(l)-1))&1u)!=sign(l)) satisfied=true;
                    }
                    ok &= satisfied;
                }
                for(unsigned j=0;j<na;++j) if (((bits>>(var(a[j])-1))&1u)==sign(a[j])) ok=false;
                sat |= ok;
            }
            assert(solver_solve_with_assumptions(s,a,na)==(sat?TRUE:FALSE));
            if(sat) assert(solver_check_model(s));
        }
        solver_free(s);
    }
}
static void backtrack(void) {
    Solver *s=solver_new();for(unsigned i=0;i<4;++i) solver_new_var(s);
    assert(solver_decide(s));Var first=var(s->trail[0].lit);
    assert(solver_decide(s));Var second=var(s->trail[1].lit);
    solver_backtrack(s,1);
    assert(s->trail_size==1 && s->values[first]!=UNDEF && s->values[second]==UNDEF);
    solver_backtrack(s,0);assert(s->trail_size==0 && s->values[first]==UNDEF);solver_free(s);
}
static void assignment_growth(void) {
    Solver *s=solver_new();assert(s);
    assert(solver_model_value(s,0)==UNDEF);
    for(unsigned v=1;v<=4;++v) assert(solver_new_var(s)==v);
    Lit unit=mkLit(1,false);assert(solver_add_clause(s,&unit,1));
    Lit binary[]={mkLit(1,true),mkLit(2,true)};
    assert(solver_add_clause(s,binary,2));
    assert(solver_propagate(s)==INVALID_CLAUSE);
    assert(solver_model_value(s,1)==TRUE && solver_model_value(s,2)==FALSE);
    assert(solver_decide(s));Var decision=var(s->trail[s->trail_size-1].lit);
    lbool saved=solver_model_value(s,decision);
    uint32_t old_capacity=s->var_capacity;
    for(Var v=5;v<=4*old_capacity+1;++v) {
        assert(solver_new_var(s)==v && solver_model_value(s,v)==UNDEF);
        assert(solver_model_value(s,1)==TRUE && solver_model_value(s,2)==FALSE);
        assert(solver_model_value(s,decision)==saved);
    }
    assert(s->var_capacity>4*old_capacity);
    solver_backtrack(s,0);
    assert(solver_model_value(s,decision)==UNDEF);
    assert(solver_model_value(s,1)==TRUE && solver_model_value(s,2)==FALSE);
    assert(solver_model_value(s,0)==UNDEF && solver_model_value(s,UINT32_MAX)==UNDEF);
    assert(solver_solve(s)==TRUE && solver_check_model(s));
    Lit assumption=mkLit(decision,false);
    assert(solver_solve_with_assumptions(s,&assumption,1)==TRUE);
    assert(solver_model_value(s,decision)==TRUE && solver_check_model(s));
    solver_free(s);
}
static void reduction_and_gc(void) {
    Solver *s=formula("p cnf 3 1\n1 2 3 0\n");
    s->opts.reduce_fraction=0;s->opts.glue_lbd=2;
    s->learnts=malloc(40*sizeof *s->learnts);assert(s->learnts);s->learnts_size=40;
    Lit lits[]={mkLit(1,false),mkLit(2,false),mkLit(3,false)};
    for(unsigned i=0;i<40;++i) {
        CRef cr=arena_alloc(s->arena,lits,3,true);assert(cr!=INVALID_CLAUSE);
        s->learnts[s->num_learnts++]=cr;set_clause_lbd(s->arena,cr,i==1?2:5);
        watch_add(s->watches,lits[0],cr,lits[1]);watch_add(s->watches,lits[1],cr,lits[0]);
    }
    s->values[1]=TRUE;s->vars[1].level=0;s->vars[1].reason=s->learnts[0];
    s->trail[0]=(Trail){lits[0],0};s->trail_size=1;
    solver_reduce_db(s);
    assert(s->stats.deleted_clauses==38 && s->num_learnts==2 && s->garbage_collections==1);
    assert(s->arena->wasted==0);
    assert(CLAUSE_SIZE(s->arena,s->vars[1].reason)==3);
    assert(clause_lbd(s->arena,s->vars[1].reason)==5);
    assert(solver_solve(s)==TRUE && solver_check_model(s));solver_free(s);
}
static void restart(void) {
    SolverOpts o=default_opts();o.luby_restart=true;o.luby_unit=2;
    Solver *s=solver_new_with_opts(&o);
    s->stats.conflicts=100;s->restart.conflicts_since=1;assert(!solver_should_restart(s));
    s->restart.conflicts_since=2;assert(solver_should_restart(s));assert(s->restart.conflicts_since==0);
    assert(!solver_should_restart(s));solver_free(s);
    o.luby_restart=false;o.glucose_use_ema=false;o.glucose_window_size=2;o.glucose_min_conflicts=1;
    s=solver_new_with_opts(&o);s->restart.conflicts_since=1;s->restart.recent_lbds_count=2;
    s->recent_lbd_sum=20;s->restart.lbd_sum=10;s->restart.lbd_count=1;
    assert(!solver_should_restart(s));s->recent_lbd_sum=26;assert(solver_should_restart(s));solver_free(s);
}
static void proof_export(void) {
    for(unsigned binary=0;binary<2;++binary) {
        char path[]="/tmp/bsat-proof-XXXXXX";
        int fd=mkstemp(path);assert(fd>=0);close(fd);
        SolverOpts o=default_opts();o.proof_path=path;o.binary_proof=binary;
        Solver *s=solver_new_with_opts(&o);assert(s);
        assert(dimacs_parse_string(s,"p cnf 201 4\n200 201 0\n-200 201 0\n200 -201 0\n-200 -201 0\n")==DIMACS_OK);
        assert(solver_solve(s)==FALSE);
        FILE *out=tmpfile();assert(out);
        assert(dimacs_write_proof(s,out));rewind(out);
        assert(fgetc(out)==(binary?'a':'2'));
        if(binary) { assert(fgetc(out)==0x90);assert(fgetc(out)==3); }
        assert(!dimacs_write_proof(s,s->proof_file));
        fclose(out);solver_free(s);assert(!remove(path));
    }
}
int main(void) {
    parser();assumptions();api_fuzz();backtrack();assignment_growth();reduction_and_gc();restart();proof_export();
    puts("PASS: parser, 800 API solves, assumptions, backjump boundaries, reduction/GC, restart regressions");
    return 0;
}
