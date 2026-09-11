#include "solver.h"
#include <assert.h>
#include <stdio.h>

int
main(void)
{
    SolverOpts o = default_opts();

    o.accounting = true;
    o.congruence = true;
    o.probing = false;
    o.equiv = false;
    o.congruence_budget = 1000000;
    Solver *s = solver_new_with_opts(&o);

    assert(s);
    assert(solver_new_var(s) == 1 && solver_new_var(s) == 2);
    Lit pair[] = {mkLit(1, false), mkLit(2, false)};

    for (unsigned i = 0; i < 2000; ++i)
        assert(solver_add_clause(s, pair, 2));
    assert(solver_solve(s) == TRUE && solver_check_model(s));
    assert(s->stats.congruence_gates == 0);
    /* Binary-only indexing needs no ITE join table. Keep a concrete memory
       ceiling that the old unconditional allocation exceeded (about 192 KiB). */
    assert(s->accounting.congruence_temporary_peak > 0);
    assert(s->accounting.congruence_temporary_peak < 100000);
    solver_free(s);
    puts("PASS: binary-only congruence stays under 100 KB temporary storage");
}
