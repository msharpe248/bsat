#include "../include/solver.h"
#include "../include/local_search.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Reference walk deliberately rescans the formula and recomputes scores. */
static unsigned
count_unsat(const LocalSearchState *ls, const bool *values)
{
    unsigned total = 0;

    for (unsigned c = 0; c < ls->num_clauses; ++c) {
        bool sat = false;

        for (unsigned j = 0; j < ls->clause_sizes[c]; ++j) {
            Lit l = ls->clause_lits[c][j];

            sat |= values[var(l)] != sign(l);
        }
        total += !sat;
    }
    return total;
}

static void
reference(const LocalSearchState *ls, bool *values, uint32_t *rng, unsigned flips, double noise)
{
    for (unsigned f = 0; f < flips; ++f) {
        unsigned before = count_unsat(ls, values);

        assert(before);
        unsigned rank = bsat_random(rng) % before, c;

        for (c = 0; c < ls->num_clauses; ++c) {
            bool sat = false;

            for (unsigned j = 0; j < ls->clause_sizes[c]; ++j) {
                Lit l = ls->clause_lits[c][j];

                sat |= values[var(l)] != sign(l);
            }
            if (!sat && !rank--) break;
        }
        assert(c < ls->num_clauses);
        Var chosen = 0;

        if (bsat_random(rng) / 4294967296.0 < noise) {
            chosen = var(ls->clause_lits[c][bsat_random(rng) % ls->clause_sizes[c]]);
        } else {
            int best = 0;

            for (unsigned j = 0; j < ls->clause_sizes[c]; ++j) {
                Var v = var(ls->clause_lits[c][j]);

                values[v] = !values[v];
                int delta = (int)count_unsat(ls, values) - (int)before;

                values[v] = !values[v];
                if (!chosen || delta < best) {
                    chosen = v;
                    best = delta;
                }
            }
        }
        values[chosen] = !values[chosen];
    }
}

int
main(void)
{
    const unsigned sizes[] = {4, 7, 8, 9, 255, 256, 257, 1023, 1024, 1025};
    unsigned checks = 0;

    for (unsigned k = 0; k < sizeof sizes / sizeof *sizes; ++k) {
        Solver *s = solver_new();

        assert(s);
        for (unsigned v = 0; v < 12; ++v)
            assert(solver_new_var(s));
        /* Contradictory core prevents an early successful return. */
        for (unsigned c = 0; c < 4; ++c) {
            Lit lits[] = {mkLit(1, c & 1), mkLit(2, (c >> 1) & 1)};

            assert(solver_add_clause(s, lits, 2));
        }
        uint32_t gen = 12345;

        for (unsigned c = 4; c < sizes[k]; ++c) {
            unsigned v = 1 + bsat_random(&gen) % 12;
            Lit lits[] = {mkLit(v, bsat_random(&gen) & 1), mkLit(v % 12 + 1, bsat_random(&gen) & 1),
                          mkLit((v + 1) % 12 + 1, bsat_random(&gen) & 1)};

            assert(solver_add_clause(s, lits, 3));
        }
        LocalSearchState *ls = local_search_init(s);

        assert(ls);
        assert(ls->num_clauses == sizes[k]);
        for (unsigned seed = 0; seed < 4; ++seed)
            for (unsigned noise = 0; noise < 3; ++noise)
                for (unsigned flips = 0; flips <= 16; ++flips) {
                    bool values[13] = {false};

                    for (Var v = 1; v <= 12; ++v)
                        values[v] = s->vars[v].polarity = (seed >> ((v - 1) % 2)) & 1;
                    uint32_t rng = seed;

                    reference(ls, values, &rng, flips, noise * 0.5);
                    ls->random_state = seed;
                    ls->flips = 0;
                    assert(!local_search_run(s, ls, flips, noise * 0.5));
                    assert(ls->flips == flips && ls->random_state == rng);
                    assert(!memcmp(values + 1, ls->assignment + 1, 12 * sizeof(bool)));
                    assert(ls->num_unsat == count_unsat(ls, values));
                    assert(ls->unsat_tree[0] == 0);
                    for (size_t i = 1; i <= ls->num_clauses; ++i) {
                        unsigned expected = 0;

                        for (size_t j = i - (i & -i); j < i; ++j)
                            expected += !ls->num_true_lits[j];
                        assert(ls->unsat_tree[i] == expected);
                    }
                    ++checks;
                }
        local_search_free(ls);
        solver_free(s);
    }
    printf("PASS: %u walk prefixes match scan oracle across tree boundaries\n", checks);
    return 0;
}
