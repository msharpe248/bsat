#include "solver.h"
#include <assert.h>
#include <stdio.h>

int
main(void)
{
    SolverOpts o = default_opts();

    o.probing = false;
    o.reuse_learnts = true;
    o.binary_proof = true;
    Solver *s = solver_new_with_opts(&o);

    assert(s);
    FILE *journal = tmpfile();

    assert(journal);
    s->proof_journal = journal;
    assert(solver_new_var(s));
    assert(solver_new_var(s));
    Lit a[] = {mkLit(1, false), mkLit(2, false)}, b[] = {mkLit(1, true), mkLit(2, false)},
        assumption = mkLit(2, true);

    solver_add_clause(s, a, 2);
    solver_add_clause(s, b, 2);
    assert(solver_solve_with_assumptions(s, &assumption, 1) == FALSE);
    long end = ftell(journal);

    assert(end > 0);
    proof_delete_clause(s, a, 2);
    assert(ftell(journal) == end);
    rewind(journal);
    int c;

    while ((c = fgetc(journal)) != EOF) {
        assert(c == 'a');
        int literal = fgetc(journal);

        assert(literal > 0); /* No conditional empty clause. */
        while (literal != 0) {
            assert(literal != EOF);
            literal = fgetc(journal);
        }
    }
    assert(!fseek(journal, 0, SEEK_END));
    assert(solver_solve(s) == TRUE);
    assert(s->reused_solves);
    solver_free(s);
    assert(!fclose(journal)); /* Borrowed stream remains open. */
    s = solver_new_with_opts(&o);
    assert(s);
    journal = fopen("/dev/null", "r");
    assert(journal);
    s->proof_journal = journal;
    solver_add_clause(s, NULL, 0);
    assert(solver_solve(s) == UNDEF);
    assert(s->error);
    solver_free(s);
    fclose(journal);
    puts("PASS: conditional journal scope, deletion suppression, borrowing and write failure");
}
