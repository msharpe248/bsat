#include "../include/solver.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static unsigned random_state = 0x918378u;

static unsigned
next(void)
{
    random_state = random_state * 1664525u + 1013904223u;
    return random_state;
}

static void
truth_tables(void)
{
    unsigned fallbacks = 0;

    for (unsigned test = 0; test < 256; ++test) {
        SolverOpts o = default_opts();

        o.probing = test & 1;
        o.elim = test & 2;
        o.bce = test & 4;
        o.equiv = test & 8;
        o.alternating = test & 16;
        Solver *s = solver_new_with_opts(&o);

        assert(s);
        unsigned vars = 2 + next() % 6, count = next() % 32;

        for (unsigned v = 0; v < vars; ++v)
            assert(solver_new_var(s));
        unsigned positive[32] = {0}, negative[32] = {0};

        for (unsigned c = 0; c < count; ++c) {
            Lit lits[5];
            unsigned n = 1 + next() % 5;

            for (unsigned i = 0; i < n; ++i) {
                unsigned v = next() % vars;
                bool sign = (next() >> 16) & 1;

                lits[i] = mkLit(v + 1, sign);
                if (sign)
                    negative[c] |= 1u << v;
                else
                    positive[c] |= 1u << v;
            }
            solver_add_clause(s, lits, n);
            assert(!s->error);
        }
        bool sat = false;

        for (unsigned bits = 0; bits < (1u << vars); ++bits) {
            bool valid = true;

            for (unsigned c = 0; c < count; ++c)
                if (!(positive[c] & bits) && !(negative[c] & ~bits)) valid = false;
            sat |= valid;
        }
        /* A sub-clock slice exercises fresh attempts. Trivial root answers may
           finish within one clock tick and legitimately need only one attempt. */
        for (unsigned repeat = 0; repeat < 2; ++repeat) {
            lbool result = solver_solve_portfolio(s, 1e-12);

            assert(result == (sat ? TRUE : FALSE));
            assert(!s->error && !s->interrupted && s->portfolio_attempts >= 1 &&
                   s->portfolio_attempts <= 2);
            fallbacks += s->portfolio_attempts == 2;
            assert(s->opts.alternating == o.alternating && s->opts.max_time == o.max_time);
            if (sat) assert(solver_check_model(s));
        }
        assert(solver_solve(s) == (sat ? TRUE : FALSE));
        solver_free(s);
    }
    assert(fallbacks > 100);
    printf("PASS: 512 portfolio answers checked against truth tables, %u fresh attempts\n",
           fallbacks);
}

static Solver *
pigeonhole(void)
{
    SolverOpts o = default_opts();

    o.probing = false;
    Solver *s = solver_new_with_opts(&o);

    assert(s);
    for (unsigned v = 0; v < 90; ++v)
        assert(solver_new_var(s));
    for (unsigned p = 0; p < 10; ++p) {
        Lit clause[9];

        for (unsigned h = 0; h < 9; ++h)
            clause[h] = mkLit(p * 9 + h + 1, false);
        assert(solver_add_clause(s, clause, 9));
    }
    for (unsigned h = 0; h < 9; ++h)
        for (unsigned p = 0; p < 10; ++p)
            for (unsigned q = p + 1; q < 10; ++q) {
                Lit clause[] = {mkLit(p * 9 + h + 1, true), mkLit(q * 9 + h + 1, true)};

                assert(solver_add_clause(s, clause, 2));
            }
    return s;
}

static void
limits(void)
{
    for (unsigned mode = 0; mode < 4; ++mode) {
        Solver *s = pigeonhole();

        if (mode & 1)
            s->opts.max_conflicts = 1;
        else
            s->opts.max_decisions = 1;
        assert(solver_solve_portfolio(s, mode & 2 ? 1e-12 : 10) == UNDEF);
        assert(!s->error && s->portfolio_attempts >= 1 && s->portfolio_attempts <= 2);
        if (!(mode & 2)) assert(s->portfolio_attempts == 1);
        if (mode & 1)
            assert(s->stats.conflicts + s->portfolio_first_conflicts <= 1);
        else
            assert(s->stats.decisions + s->portfolio_first_decisions <= 1);
        solver_free(s);
    }
    for (unsigned mode = 0; mode < 3; ++mode) {
        Solver *s = pigeonhole();

        s->opts.max_time = 0.01;
        double start = solver_cpu_time();

        assert(solver_solve_portfolio(s, mode ? 0.001 : 1) == UNDEF);
        double elapsed = solver_cpu_time() - start;

        assert(s->interrupted && !s->error && elapsed >= 0.01 && elapsed < 0.2);
        assert(s->opts.max_time == 0.01);
        if (!mode) assert(s->portfolio_attempts == 1);
        /* Repeated calls must not turn a prior deadline into a permanent stop. */
        s->opts.max_time = 0;
        s->opts.max_conflicts = 1;
        assert(solver_solve_portfolio(s, 1e-12) == UNDEF && !s->error);
        solver_free(s);
    }
    for (unsigned mode = 0; mode < 2; ++mode) {
        Solver *s = pigeonhole();

        if (mode)
            s->opts.max_conflicts = 5000;
        else
            s->opts.max_decisions = 5000;
        assert(solver_solve_portfolio(s, 0.001) == UNDEF && !s->error);
        assert(s->portfolio_attempts == 2);
        uint64_t first = mode ? s->portfolio_first_conflicts : s->portfolio_first_decisions;
        uint64_t last = mode ? s->stats.conflicts : s->stats.decisions;

        assert(first > 0 && first + last == 5000);
        solver_free(s);
    }
    Solver *s = pigeonhole();

    s->opts.max_time = 0.03;
    double start = solver_cpu_time();

    assert(solver_solve_portfolio(s, 0.02) == UNDEF && s->interrupted);
    double elapsed = solver_cpu_time() - start;

    assert(s->portfolio_attempts == 2 && elapsed >= 0.03 && elapsed < 0.045);
    solver_free(s); // Refreshing the full deadline would take at least 0.05 s.
}

static void
certificates(void)
{
    char path[] = "/tmp/bsat-portfolio-test-XXXXXX";
    int fd = mkstemp(path);

    assert(fd >= 0);
    close(fd);
    for (unsigned binary = 0; binary < 2; ++binary) {
        SolverOpts o = default_opts();

        o.proof_path = path;
        o.binary_proof = binary;
        Solver *s = solver_new_with_opts(&o);

        assert(s && !s->error);
        assert(!solver_add_clause(s, NULL, 0));
        assert(solver_solve_portfolio(s, 1e-12) == FALSE);
        solver_free(s);
        FILE *f = fopen(path, "rb");

        assert(f);
        if (binary) {
            assert(fgetc(f) == 'a');
            assert(fgetc(f) == 0);
        } else {
            assert(fgetc(f) == '0');
            assert(fgetc(f) == '\n');
        }
        assert(fgetc(f) == EOF);
        fclose(f); // First attempt's empty clause was discarded.
    }
    /* A first-attempt certificate error must not be hidden by a fresh attempt. */
    SolverOpts o = default_opts();

    o.proof_path = path;
    Solver *failed = solver_new_with_opts(&o);

    assert(failed && failed->proof_file);
    fclose(failed->proof_file);
    failed->proof_file = fopen(path, "r");
    assert(failed->proof_file);
    assert(!solver_add_clause(failed, NULL, 0));
    assert(solver_solve_portfolio(failed, 1e-12) == UNDEF && failed->error);
    assert(failed->portfolio_attempts == 1);
    solver_free(failed);
    /* The old proof cannot be reused if reopening the next proof fails. */
    failed = pigeonhole();
    failed->proof_file = fopen(path, "w");
    assert(failed->proof_file);
    char impossible[128];

    snprintf(impossible, sizeof impossible, "%s/child", path);
    failed->opts.proof_path = impossible; // The parent is a regular file.
    assert(solver_solve_portfolio(failed, 1e-12) == UNDEF && failed->error);
    assert(failed->portfolio_attempts == 1);
    solver_free(failed);
    unlink(path);
    Solver *s = solver_new();

    assert(s);
    s->proof_file = tmpfile();
    assert(s->proof_file);
    assert(solver_solve_portfolio(s, 1e-12) == UNDEF && !s->has_solved);
    assert(solver_solve_portfolio(s, NAN) == UNDEF);
    assert(solver_solve_portfolio(s, 0) == UNDEF);
    solver_free(s);
}

int
main(void)
{
    truth_tables();
    limits();
    certificates();
    puts("PASS: portfolio truth tables, repeated API calls, global limits and proof reset");
    return 0;
}
