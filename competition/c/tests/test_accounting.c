#include "solver.h"
#include "dimacs.h"
#include <assert.h>
#include <stdio.h>

static void blocker_denominator(void) {
    Solver *s=solver_new();assert(s);
    for (unsigned v=1;v<=22;++v) assert(solver_new_var(s)==v);
    for (unsigned v=3;v<=22;++v) {
        Lit c[]={fromDimacs(1),fromDimacs(2),mkLit(v,false)};
        CRef cr=arena_alloc(s->arena,c,3,false);assert(cr!=INVALID_CLAUSE);
        watch_add(s->watches,c[0],cr,c[1]);watch_add(s->watches,c[1],cr,c[0]);
    }
    s->values[2]=TRUE;s->trail[s->trail_size++]=(Trail){fromDimacs(2)};
    s->qhead=s->trail_size;
    s->values[1]=FALSE;s->trail[s->trail_size++]=(Trail){fromDimacs(-1)};
    assert(solver_propagate(s)==INVALID_CLAUSE);
    assert(s->stats.propagations==1 && s->watches->skipped==20 && s->watches->visits==20);
    WatchStats stats=watch_stats(s->watches);assert(stats.skip_rate==100.0);
    solver_free(s);
}

static void propagation_counters(void) {
    for (unsigned enabled=0; enabled<2; ++enabled) {
        SolverOpts o=default_opts();o.accounting=enabled;o.probing=false;
        Solver *s=solver_new_with_opts(&o);assert(s);
        assert(dimacs_parse_string(s,"p cnf 6 3\n1 2 3 4 0\n-1 5 0\n-2 6 0\n")==DIMACS_OK);
        Lit assumptions[]={fromDimacs(-1),fromDimacs(-2),fromDimacs(-3)};
        assert(solver_solve_with_assumptions(s,assumptions,3)==TRUE);
        assert(solver_check_model(s) && solver_model_value(s,4)==TRUE);
        bool counters=SEARCH_DIAGNOSTICS(s);
        assert((s->accounting.binary_visits>0)==counters);
        assert((s->accounting.long_visits>0)==counters);
        assert((s->accounting.replacement_scans>0)==counters);
        assert((s->accounting.long_units>0)==counters);
        assert(s->accounting.replacement_scans==s->accounting.scan_size_3+
               s->accounting.scan_size_4_8+s->accounting.scan_size_9_plus);
        assert(s->accounting.blocker_hits+s->accounting.first_hits+
               s->accounting.long_units+s->accounting.long_conflicts+
               s->accounting.replacement_moves==s->accounting.long_visits);
        solver_free(s);
    }
}

int main(void) {
    blocker_denominator();propagation_counters();
    for(unsigned enabled=0;enabled<2;++enabled) {
        SolverOpts o=default_opts();o.accounting=enabled;o.equiv=true;
        Solver *s=solver_new_with_opts(&o);assert(s);
        assert(dimacs_parse_string(s,"p cnf 3 3\n-1 2 0\n1 -2 0\n2 3 0\n")==DIMACS_OK);
        SolverMemory parsed=solver_memory(s);assert(parsed.complete);
        assert(parsed.input>=9*sizeof(Lit) && parsed.watches>0 && parsed.variables>0);
        assert(solver_solve(s)==TRUE);assert(solver_check_model(s));
        assert(s->accounting.calls[ACCOUNT_PARSE]==enabled);
        assert(s->accounting.calls[ACCOUNT_SEARCH]==enabled);
        assert(s->accounting.calls[ACCOUNT_MODEL]==enabled);
        assert(solver_solve(s)==TRUE);
        assert(s->accounting.calls[ACCOUNT_SEARCH]==2*enabled);
        assert(s->accounting.calls[ACCOUNT_PARSE]==enabled);
        for(unsigned i=0;i<ACCOUNT_PHASES;++i) assert(s->accounting.seconds[i]>=0);
        SolverMemory final=solver_memory(s);assert(final.complete && final.total>0);
        s->error=true;assert(!solver_memory(s).complete);
        solver_free(s);
    }
    puts("PASS: accounting survives equivalence/rebuilds and marks incomplete allocation states");
}
