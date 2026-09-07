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
static void circular_scan_order(void) {
    for (unsigned circular = 0; circular < 2; ++circular)
    for (unsigned cursor = 0; cursor <= 7; ++cursor)
    for (unsigned mask = 0; mask < 16; ++mask) {
        Solver *s = formula("p cnf 6 1\n1 2 3 4 5 6 0\n");
        s->opts.circular = circular;
        CRef cr = s->clauses[0];
        CLAUSE_HEADER(s->arena, cr)->search = cursor;
        /* Establish already-processed false tail literals, then falsify watch 1. */
        for (Var v = 3; v <= 6; ++v) if (!(mask & (1u << (v-3)))) {
            s->values[v] = FALSE;
            s->vars[v].level = 0;
            s->trail[s->trail_size++] = (Trail){mkLit(v, true), 0};
        }
        s->qhead = s->trail_size;
        s->values[1] = FALSE;
        s->vars[1].level = 0;
        s->trail[s->trail_size++] = (Trail){mkLit(1, true), 0};
        unsigned begin = circular && cursor >= 2 && cursor < 6 ? cursor : 2;
        unsigned inspections = 0, expected = 0;
        for (unsigned offset = 0; offset < 4; ++offset) {
            unsigned k = 2 + (begin - 2 + offset) % 4;
            inspections++;
            if (mask & (1u << (k-2))) { expected = k+1; break; }
        }
        uint64_t work = s->work;
        assert(solver_propagate(s) == INVALID_CLAUSE);
        assert(s->work - work == 1 + inspections);
        if (expected) {
            assert(CLAUSE_LITS(s->arena, cr)[1] == mkLit(expected, false));
            assert(s->values[2] == UNDEF);
        } else {
            assert(s->values[2] == TRUE && s->vars[2].reason == cr);
        }
        solver_free(s);
    }
}
static void dynamic_clause_quality(void) {
    for (unsigned initial_lbd = 4; initial_lbd <= 7; ++initial_lbd)
    for (unsigned protect = 0; protect < 2; ++protect)
    for (unsigned enabled = 0; enabled < 2; ++enabled)
    for (unsigned learned = 0; learned < 2; ++learned) {
        Solver *s = formula("p cnf 4 3\n-1 -2 3 4 0\n-1 -2 3 -4 0\n-1 -3 0\n");
        s->opts.dynamic_lbd = enabled;
        s->opts.protect_used = protect;
        /* Normal solve setup allocates independent decision-level marks. */
        s->levels_capacity = 5;
        s->level_seen = calloc(s->levels_capacity, sizeof *s->level_seen);
        assert(s->level_seen);
        CRef a = s->clauses[0], b = s->clauses[1];
        set_clause_lbd(s->arena, a, initial_lbd);
        set_clause_lbd(s->arena, b, initial_lbd);
        if (learned) {
            CLAUSE_HEADER(s->arena, a)->flags |= CLAUSE_LEARNED;
            CLAUSE_HEADER(s->arena, b)->flags |= CLAUSE_LEARNED;
        }
        /* Chosen decisions cause real propagation: 1 implies -3, then 2
           causes the first long clause to imply 4 and the second to conflict. */
        CRef conflict = INVALID_CLAUSE;
        for (Var v = 1; v <= 2; ++v) {
            assert(s->values[v] == UNDEF);
            s->trail_lims[++s->decision_level] = s->trail_size;
            s->values[v] = TRUE;
            s->vars[v].level = s->decision_level;
            s->vars[v].reason = INVALID_CLAUSE;
            s->vars[v].trail_pos = s->trail_size;
            s->trail[s->trail_size++] = (Trail){mkLit(v, false), s->decision_level};
            conflict = solver_propagate(s);
            if (v == 1) assert(conflict == INVALID_CLAUSE && s->values[3] == FALSE);
        }
        assert(conflict == b && s->vars[4].reason == a);
        Lit learnt[4];uint32_t size;Level backjump;
        solver_analyze(s, conflict, learnt, &size, &backjump);
        assert(size > 0 && learnt[0] == mkLit(2, true) && backjump == 1);
        assert(!!(CLAUSE_HEADER(s->arena, a)->flags & CLAUSE_FROZEN) == (protect && learned && (enabled || initial_lbd <= 6)));
        assert(!!(CLAUSE_HEADER(s->arena, b)->flags & CLAUSE_FROZEN) == (protect && learned && (enabled || initial_lbd <= 6)));
        unsigned expected = enabled && learned ? 2 : initial_lbd;
        assert(clause_lbd(s->arena, a) == expected && clause_lbd(s->arena, b) == expected);
        assert(s->stats.lbd_updates == (enabled && learned ? 2u : 0u));
        for (Var v = 1; v <= 4; ++v) assert(!s->seen[v]);
        for (unsigned level = 0; level < s->levels_capacity; ++level) assert(!s->level_seen[level]);
        solver_backtrack(s, 0);
        if (learned) {
            s->learnts = malloc(2 * sizeof *s->learnts);assert(s->learnts);
            s->learnts[0] = a;s->learnts[1] = b;
            s->num_learnts = s->learnts_size = 2;
            s->opts.reduce_fraction = 0;
            solver_reduce_db(s);
            assert(s->num_learnts == (enabled || (protect && initial_lbd <= 6) ? 2u : 0u));
            solver_reduce_db(s);
            assert(s->num_learnts == (enabled ? 2u : 0u));
        }
        solver_free(s);
    }
}
static void used_clause_lifecycle(void) {
    Solver *s = formula("p cnf 3 0\n");
    s->opts.protect_used = true;s->opts.reduce_fraction = 0;
    s->learnts = malloc(32 * sizeof *s->learnts);assert(s->learnts);
    s->learnts_size = 32;
    Lit lits[] = {mkLit(1, false), mkLit(2, false), mkLit(3, false)};
    for (unsigned i = 0; i < 32; ++i) {
        CRef cr = arena_alloc(s->arena, lits, 3, true);assert(cr != INVALID_CLAUSE);
        s->learnts[s->num_learnts++] = cr;
        set_clause_lbd(s->arena, cr, i == 0 ? 6 : 7);
        CLAUSE_HEADER(s->arena, cr)->flags |= CLAUSE_FROZEN;
        if (i >= 2) solver_delete_clause(s, cr);
    }
    solver_collect_garbage(s);assert(s->num_learnts == 2);
    for (unsigned i = 0; i < 2; ++i)
        assert(CLAUSE_HEADER(s->arena, s->learnts[i])->flags & CLAUSE_FROZEN);
    solver_reduce_db(s);assert(s->num_learnts == 1);
    CRef cr = s->learnts[0];assert(clause_lbd(s->arena, cr) == 6);
    assert(!(CLAUSE_HEADER(s->arena, cr)->flags & CLAUSE_FROZEN));
    // A lock retains its clause, but must not postpone consuming the reprieve.
    CLAUSE_HEADER(s->arena, cr)->flags |= CLAUSE_FROZEN;
    s->values[1] = TRUE;s->vars[1].reason = cr;s->vars[1].level = 1;
    s->vars[1].trail_pos = 0;s->trail_lims[1] = 0;s->decision_level = 1;
    s->trail[s->trail_size++] = (Trail){mkLit(1, false), 1};
    solver_reduce_db(s);assert(s->num_learnts == 1);
    assert(!(CLAUSE_HEADER(s->arena, s->learnts[0])->flags & CLAUSE_FROZEN));
    solver_backtrack(s, 0);solver_reduce_db(s);assert(!s->num_learnts);
    assert(solver_solve(s) == TRUE && solver_check_model(s));
    assert(solver_new_var(s) == 4 && s->opts.protect_used && !s->num_learnts);
    solver_free(s);
}
static void clause_normalization_boundaries(void) {
    const unsigned sizes[] = {0, 1, 2, 15, 16, 17, 32, 65};
    for (unsigned test = 0; test < sizeof sizes / sizeof *sizes; ++test) {
        unsigned n = sizes[test];
        for (unsigned kind = 0; kind < 3; ++kind) {
            Solver *s = solver_new();assert(s);
            for (Var v = 1; v <= 65; ++v) assert(solver_new_var(s) == v);
            Lit input[65], original[65];
            for (unsigned i = 0; i < n; ++i)
                input[i] = mkLit(kind ? 1 : n-i, kind == 2 && i == n-1);
            memcpy(original, input, n * sizeof *input);
            bool added = solver_add_clause(s, input, n);
            assert(!memcmp(input, original, n * sizeof *input));
            assert(s->input_clauses == 1 && s->input_size == n+1);
            assert(!memcmp(s->input, original, n * sizeof *input));
            assert(s->input[n] == 0);
            if (!n) {
                assert(!added && solver_solve(s) == FALSE);
            } else {
                assert(added);
                bool tautology = kind == 2 && n > 1;
                assert(s->num_clauses == (tautology ? 0u : 1u));
                if (!tautology) {
                    CRef cr = s->clauses[0];
                    assert(CLAUSE_SIZE(s->arena, cr) == (kind ? 1u : n));
                    if (!kind)
                        for (unsigned i = 0; i < n; ++i)
                            assert(CLAUSE_LITS(s->arena, cr)[i] == mkLit(i+1, false));
                }
                assert(solver_solve(s) == TRUE && solver_check_model(s));
                assert(solver_new_var(s) == 66);
                assert(s->input_size == n+1 && !memcmp(s->input, original, n*sizeof *input));
                assert(solver_solve(s) == TRUE && solver_check_model(s));
            }
            solver_free(s);
        }
    }
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
    parser();assumptions();api_fuzz();backtrack();assignment_growth();circular_scan_order();dynamic_clause_quality();used_clause_lifecycle();clause_normalization_boundaries();reduction_and_gc();restart();proof_export();
    puts("PASS: parser, 800 API solves, assumptions, backjump boundaries, reduction/GC, restart regressions");
    return 0;
}
