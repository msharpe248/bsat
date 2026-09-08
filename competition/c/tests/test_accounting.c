#include "solver.h"
#include "dimacs.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
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
