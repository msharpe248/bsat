#include "solver.h"
#include <assert.h>
#include <stdio.h>

typedef struct {
    int lits[4];
    unsigned n;
} Clause;

static unsigned
rng(unsigned *state)
{
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    return *state;
}

static bool
oracle(Clause *cs, unsigned count, int *as, unsigned na)
{
    for (unsigned bits = 0; bits < 256; ++bits) {
        bool ok = true;

        for (unsigned i = 0; i < count && ok; ++i) {
            bool sat = false;

            for (unsigned j = 0; j < cs[i].n; ++j) {
                int l = cs[i].lits[j];

                sat |= ((bits >> (abs(l) - 1)) & 1u) == (l > 0);
            }
            ok = sat;
        }
        for (unsigned i = 0; i < na && ok; ++i)
            ok = ((bits >> (abs(as[i]) - 1)) & 1u) == (as[i] > 0);
        if (ok) return true;
    }
    return false;
}

static void
learned_retention(void)
{
    SolverOpts o = default_opts();

    o.reuse_learnts = true;
    o.probing = false;
    Solver *s = solver_new_with_opts(&o);

    assert(s);
    for (unsigned v = 0; v < 8; ++v)
        assert(solver_new_var(s));
    for (unsigned bits = 0; bits < 255; ++bits) {
        Lit c[8];

        for (unsigned j = 0; j < 8; ++j)
            c[j] = mkLit(j + 1, (bits >> j) & 1);
        assert(solver_add_clause(s, c, 8));
    }
    assert(solver_solve(s) == TRUE && solver_check_model(s));
    assert(s->num_learnts > 0);
    unsigned count = s->num_learnts;
    Arena *arena = s->arena;

    assert(solver_new_var(s) == 9);
    assert(s->reused_solves == 1 && s->arena == arena && s->num_learnts == count);
    Lit neg9 = mkLit(9, true);

    assert(solver_add_clause(s, &neg9, 1));
    assert(solver_solve(s) == TRUE && solver_model_value(s, 9) == FALSE);
    Lit bad = mkLit(1, true);

    assert(solver_solve_with_assumptions(s, &bad, 1) == FALSE);
    uint64_t before = s->reused_solves;

    assert(solver_solve(s) == TRUE);
    assert(s->reused_solves == before + 1); /* conditional UNSAT retains entailed clauses */
    solver_free(s);
}

static void
conflict_slices(void)
{
    SolverOpts o = default_opts();

    o.reuse_learnts = true;
    o.probing = false;
    o.max_conflicts = 1;
    Solver *s = solver_new_with_opts(&o);

    assert(s);
    for (unsigned v = 0; v < 9; ++v)
        assert(solver_new_var(s));
    for (unsigned bits = 0; bits < 511; ++bits) {
        Lit c[9];

        for (unsigned j = 0; j < 9; ++j)
            c[j] = mkLit(j + 1, (bits >> j) & 1);
        assert(solver_add_clause(s, c, 9));
    }
    unsigned slices = 0;
    lbool result;

    do {
        result = solver_solve(s);
        assert(!s->error);
        ++slices;
        assert(slices < 1024);
    } while (result == UNDEF);
    assert(result == TRUE && solver_check_model(s) && slices > 1);
    assert(s->reused_solves == slices - 1);
    solver_free(s);
}

static void
probing_revision(void)
{
    SolverOpts o = default_opts();

    o.reuse_learnts = true;
    o.probe_on_change = true;
    Solver *s = solver_new_with_opts(&o);

    assert(s);
    for (unsigned i = 0; i < 3; ++i)
        assert(solver_new_var(s));
    Lit c[] = {mkLit(1, false), mkLit(2, false)};

    assert(solver_add_clause(s, c, 2));
    assert(solver_solve(s) == TRUE && s->stats.probing_calls == 1);
    Lit a = mkLit(1, true);

    assert(solver_solve_with_assumptions(s, &a, 1) == TRUE && !s->stats.probing_calls);
    c[1] = mkLit(3, false);
    assert(solver_add_clause(s, c, 2));
    assert(solver_solve(s) == TRUE && s->stats.probing_calls == 1);
    assert(solver_solve(s) == TRUE && !s->stats.probing_calls);
    solver_free(s);
}

int
main(void)
{
    probing_revision();
    learned_retention();
    conflict_slices();
    unsigned solves = 0;
    uint64_t reused = 0;

    for (unsigned seed = 1; seed <= 128; ++seed) {
        SolverOpts o = default_opts();

        o.reuse_learnts = true;
        o.probing = seed & 1;
        o.probe_on_change = seed & 64;
        o.chrono = seed & 2;
        o.chrono_levels = 0;
        o.vmtf = seed & 4;
        o.lrb = (seed & 8) && !o.vmtf;
        if (seed & 16) o.equiv = true; /* compatible state or reconstruction fallback */
        if (seed & 32) o.congruence = true;
        Solver *s = solver_new_with_opts(&o);

        assert(s);
        for (unsigned v = 0; v < 8; ++v)
            assert(solver_new_var(s));
        Clause cs[32];
        unsigned count = 0, state = seed;

        for (unsigned step = 0; step < 64; ++step) {
            if (count < 32 && rng(&state) % 3 == 0) {
                Clause c = {.n = 2 + rng(&state) % 3};
                Lit lits[4];

                for (unsigned j = 0; j < c.n; ++j) {
                    unsigned x = rng(&state);
                    int v = 1 + x % 8;

                    c.lits[j] = (x & 256) ? -v : v;
                    lits[j] = fromDimacs(c.lits[j]);
                }
                solver_add_clause(s, lits, c.n);
                cs[count++] = c;
                assert(!s->error);
            }
            unsigned na = rng(&state) % 4;
            int as[3];
            Lit assumptions[3];

            for (unsigned j = 0; j < na; ++j) {
                unsigned x = rng(&state);

                as[j] = (1 + (int)(x % 8)) * ((x & 256) ? -1 : 1);
                assumptions[j] = fromDimacs(as[j]);
            }
            bool expected = oracle(cs, count, as, na);
            lbool result = solver_solve_with_assumptions(s, assumptions, na);

            assert(!s->error && result == (expected ? TRUE : FALSE));
            if (result == TRUE) {
                assert(solver_check_model(s));
                for (unsigned j = 0; j < na; ++j)
                    assert(solver_model_value(s, abs(as[j])) == (as[j] > 0 ? TRUE : FALSE));
            }
            ++solves;
        }
        reused += s->reused_solves;
        solver_free(s);
    }
    assert(reused > 1000);
    printf("PASS: %u independent stateful solves, %llu fast preparations, learned retention and "
           "fallback\n",
           solves, (unsigned long long)reused);
}
