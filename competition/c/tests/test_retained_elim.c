#include "../include/solver.h"
#include "../include/dimacs.h"
#include <assert.h>

typedef struct {
    FILE *journal;
    off_t start;
} CancelOnRetention;

static int
cancel_after_retention(void *state)
{
    CancelOnRetention *cancel = state;

    return ftello(cancel->journal) > cancel->start;
}

int
main(void)
{
    SolverOpts opts = default_opts();

    opts.probing = opts.elim = opts.bce = opts.equiv = opts.congruence = false;
    opts.reuse_learnts = opts.retained_elim = true;
    Solver *s = solver_new_with_opts(&opts);
    FILE *journal = tmpfile();

    assert(s && journal);
    s->proof_journal = journal;
    assert(dimacs_parse_string(s, "p cnf 5 5\n1 2 0\n1 -2 0\n"
                                  "-3 4 0\n-3 5 0\n3 -4 -5 0\n") == DIMACS_OK);
    size_t input_size = s->input_size;
    Lit unit = mkLit(1, false), frozen[] = {mkLit(4, false), mkLit(5, false)};

    /* The first two input clauses imply this unit, but do not propagate it. */
    assert(s->values[1] == UNDEF);
    proof_add_clause(s, &unit, 1);
    s->internal_add = true;
    assert(solver_add_clause(s, &unit, 1));
    s->internal_add = false;
    assert(elim_preprocess_frozen(s, frozen, 2) == 1);
    assert(elim_is_eliminated(s, 3));
    assert(s->input_size == input_size);
    assert(solver_solve_with_assumptions(s, frozen, 2) == TRUE);
    assert(s->values[3] == TRUE && solver_check_model(s));

    /* A reconstructed eliminated value is not a globally learned root fact. */
    Lit next[] = {neg(frozen[0]), frozen[1]};

    assert(solver_solve_with_assumptions(s, next, 2) == TRUE);
    assert(!s->error && !s->elim && solver_check_model(s));
    assert(s->values[1] == TRUE && s->values[3] == FALSE);
    assert(s->input_size == input_size);
    CancelOnRetention cancel = {journal, ftello(journal)};

    /* Force a rebuild and cancel after it appends a retained fact. The old
       solver survives, but must account for the shared journal's new bytes. */
    s->opts.inprocess = true;
    s->terminate = cancel_after_retention;
    s->terminate_state = &cancel;
    assert(solver_solve(s) == UNDEF);
    assert(!s->error && s->cancelled);
    assert(ftello(journal) > cancel.start);
    assert(s->journal_bytes == (uint64_t)ftello(journal));
    s->terminate = NULL;
    s->terminate_state = NULL;
    s->opts.inprocess = false;
    uint64_t before_reset = s->journal_bytes;

    /* Checkpointing must not leave unlogged derived units in a fresh ledger. */
    assert(solver_reset_learning(s));
    assert(s->journal_bytes == before_reset);
    assert(s->values[1] == UNDEF && s->input_size == input_size);
    Lit contradicted = neg(unit);

    assert(solver_solve_with_assumptions(s, &contradicted, 1) == FALSE);
    assert(!s->error);
    solver_free(s);
    assert(!fclose(journal));
    puts("PASS: frozen query variables, reconstructed values, root retention and explicit reset");
}
