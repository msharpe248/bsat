#include "../include/solver.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static Solver *
pair(unsigned a, unsigned b)
{
    Solver *s = solver_new();

    assert(s);
    for (unsigned i = 0; i < 4; ++i)
        assert(solver_new_var(s));
    unsigned masks[] = {a, b};

    for (unsigned side = 0; side < 2; ++side) {
        Lit c[4] = {mkLit(1, side)};
        unsigned n = 1, m = masks[side];

        for (Var v = 2; v <= 4; ++v, m /= 3)
            if (m % 3) c[n++] = mkLit(v, m % 3 == 2);
        assert(solver_add_clause(s, c, n));
    }
    elim_build_occs(s);
    assert(!s->error && s->elim->occs_complete);
    return s;
}

static bool
tail(unsigned mask, unsigned bits)
{
    for (unsigned v = 0; v < 3; ++v, mask /= 3)
        if (mask % 3 && (!!(bits & (1u << v)) == (mask % 3 == 1))) return true;
    return false;
}

static void
clean(Solver *s)
{
    for (Var v = 1; v <= s->num_vars; ++v)
        assert(!s->seen[v]);
}

static void
expired_during_pair(void)
{
    const unsigned n = 5000;
    Solver *s = solver_new();

    assert(s);
    Lit *clause = malloc(n * sizeof *clause);

    assert(clause);
    for (unsigned i = 0; i < n; ++i) {
        assert(solver_new_var(s));
        clause[i] = mkLit(i + 1, false);
    }
    assert(solver_add_clause(s, clause, n));
    clause[0] = neg(clause[0]);
    assert(solver_add_clause(s, clause, n));
    free(clause);
    elim_build_occs(s);
    assert(s->elim->occs_complete);
    s->opts.max_time = 0.001;
    s->stats.start_time = solver_cpu_time() - 1.0;
    s->clock_initialized = true;
    s->clock_polls = 0;
    s->clock_work = s->work;
    s->clock_minimize = s->stats.minimize_inspections;
    uint64_t before = s->work, clocks = s->stats.clock_checks;

    assert(!elim_eliminate_var(s, 1));
    assert(s->interrupted && !s->error && !s->elim->stack_size && s->num_clauses == 2);
    assert(s->stats.clock_checks == clocks + 1);
    assert(s->work - before > 1024 && s->work - before <= 4096); // Includes mandatory cleanup.
    clean(s);
    solver_free(s);
}

int
main(void)
{
    unsigned checks = 0;

    // Nonempty tails avoid assigning the pivot through an input unit.
    for (unsigned a = 1; a < 27; ++a)
        for (unsigned b = 1; b < 27; ++b) {
            Solver *s = pair(a, b);

            // Watches can reorder clauses; resolution must not require sorted input.
            for (unsigned c = 0; c < 2; ++c) {
                Lit *l = CLAUSE_LITS(s->arena, s->clauses[c]);
                unsigned n = CLAUSE_SIZE(s->arena, s->clauses[c]);
                Lit t = l[0];

                l[0] = l[1];
                l[1] = t;
                if (n > 3) {
                    t = l[2];
                    l[2] = l[3];
                    l[3] = t;
                }
            }
            bool taut = elim_is_tautology(
                CLAUSE_LITS(s->arena, s->clauses[0]), CLAUSE_SIZE(s->arena, s->clauses[0]),
                CLAUSE_LITS(s->arena, s->clauses[1]), CLAUSE_SIZE(s->arena, s->clauses[1]), 1);

            assert(elim_cost(s, 1) == !taut);
            clean(s);
            assert(elim_eliminate_var(s, 1));
            clean(s);
            assert(s->elim->vars_eliminated == 1 && s->elim->resolvents_added == !taut);
            uint8_t roots[5];

            memcpy(roots, s->values, sizeof roots);
            for (unsigned bits = 0; bits < 8; ++bits) {
                bool expected = tail(a, bits) || tail(b, bits), actual = true;

                for (Var v = 2; v <= 4; ++v) {
                    lbool val = bits & (1u << (v - 2)) ? TRUE : FALSE;

                    if (roots[v] != UNDEF && roots[v] != val) actual = false;
                    s->values[v] = val;
                }
                for (unsigned c = 0; c < s->num_clauses; ++c) {
                    CRef cr = s->clauses[c];

                    if (clause_deleted(s->arena, cr)) continue;
                    bool sat = false;

                    for (unsigned j = 0; j < CLAUSE_SIZE(s->arena, cr); ++j) {
                        Lit l = CLAUSE_LITS(s->arena, cr)[j];

                        assert(var(l) != 1);
                        sat |= lxor(s->values[var(l)], sign(l)) == TRUE;
                    }
                    actual &= sat;
                }
                if (actual != expected)
                    fprintf(stderr,
                            "projection mismatch: a=%u b=%u bits=%u actual=%d expected=%d\n", a, b,
                            bits, actual, expected);
                assert(actual == expected);
                if (actual) {
                    s->values[1] = UNDEF;
                    elim_extend_model(s);
                    assert(solver_check_model(s));
                }
                ++checks;
            }
            solver_free(s);
        }
    // Every work-limit cutoff must leave scratch clean; aborted staging must
    // leave both original clauses and the reconstruction stack untouched.
    Solver *full = pair(13, 13);
    uint64_t start = full->work;

    assert(elim_eliminate_var(full, 1));
    uint64_t needed = full->work - start;

    solver_free(full);
    unsigned aborted = 0, committed = 0;

    for (uint64_t limit = 1; limit <= needed + 1; ++limit) {
        Solver *s = pair(13, 13);

        s->work_limit = s->work + limit;
        bool ok = elim_eliminate_var(s, 1);

        clean(s);
        assert(!s->error);
        if (!ok) {
            ++aborted;
            assert(!s->elim->stack_size && !s->elim->vars_eliminated);
            assert(s->num_clauses == 2);
            for (unsigned i = 0; i < 2; ++i)
                assert(!clause_deleted(s->arena, s->clauses[i]));
        } else {
            ++committed;
            assert(s->elim->vars_eliminated == 1);
        }
        solver_free(s);
    }
    assert(aborted && committed);
    Solver *s = pair(13, 13);

    s->interrupted = true;
    assert(!elim_eliminate_var(s, 1));
    clean(s);
    assert(!s->elim->stack_size);
    solver_free(s);
    expired_during_pair();
    printf("PASS: %u projected truth-table/model checks and all resolution work cutoffs plus an "
           "internal deadline\n",
           checks);
}
