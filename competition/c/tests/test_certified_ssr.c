#include "solver.h"
#include "bsat.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef BSAT_CERTIFIED_SSR
static bool
satisfies(const Lit *a, unsigned n, unsigned bits)
{
    for (unsigned i = 0; i < n; ++i)
        if (!!(bits & (1u << (var(a[i]) - 1))) != sign(a[i])) return true;
    return false;
}

static Solver *
pair(unsigned n, unsigned m, unsigned bits, FILE **journal)
{
    SolverOpts o = default_opts();
    o.probing = false;
    o.assumption_lbd = true;
    o.reuse_learnts = true;
    Solver *s = solver_new_with_opts(&o);
    assert(s);
    for (unsigned i = 0; i < n; ++i)
        assert(solver_new_var(s));
    Lit a[6], b[6];
    for (unsigned i = 0; i < n; ++i)
        b[i] = mkLit(i + 1, (bits >> i) & 1);
    a[0] = neg(b[0]);
    for (unsigned i = 1; i < m; ++i)
        a[i] = b[i];
    assert(solver_add_clause(s, a, m) && solver_add_clause(s, b, n));
    *journal = tmpfile();
    assert(*journal);
    s->proof_journal = *journal;
    s->opts.binary_proof = true;
    s->work_limit = 1000000;
    return s;
}

static void
exhaustive(void)
{
    unsigned cases = 0;
    for (unsigned n = 3; n <= 6; ++n)
        for (unsigned m = 2; m <= 4 && m <= n; ++m)
            for (unsigned signs = 0; signs < (1u << n); ++signs) {
                FILE *f;
                Solver *s = pair(n, m, signs, &f);
                Lit input[16];
                size_t len = s->input_size;
                memcpy(input, s->input, len * sizeof *input);
                assert(solver_certified_ssr(s));
                assert(!s->error && s->ssr_strengthened > 0 && s->work <= 1000000);
                assert(s->input_size == len && !memcmp(input, s->input, len * sizeof *input) &&
                       s->num_vars == n);
                /* Force arena compaction after replacement, including tagged binaries. */
                s->work_limit = 0;
                solver_collect_garbage(s);
                assert(s->garbage_collections == 1);
                for (unsigned bits = 0; bits < (1u << n); ++bits) {
                    bool original = true, live = true;
                    size_t start = 0;
                    for (size_t i = 0; i < len; ++i)
                        if (!input[i]) {
                            original &= satisfies(input + start, (unsigned)(i - start), bits);
                            start = i + 1;
                        }
                    for (unsigned i = 0; i < s->num_clauses; ++i)
                        if (!clause_deleted(s->arena, s->clauses[i]))
                            live &= satisfies(CLAUSE_LITS(s->arena, s->clauses[i]),
                                              CLAUSE_SIZE(s->arena, s->clauses[i]), bits);
                    assert(original == live);
                }
                uint64_t work = s->work;
                assert(solver_certified_ssr(s) && s->work == work);
                s->work_limit = 0;
                assert(solver_solve(s) == TRUE && solver_check_model(s));
                assert(solver_reset_learning(s) && !memcmp(input, s->input, len * sizeof *input));
                solver_free(s);
                fclose(f);
                ++cases;
            }
    printf("PASS: %u signed SSR pairs with exhaustive equivalence, immutable input and rebuild\n",
           cases);
}

static int
stop(void *p)
{
    return *(int *)p;
}

static void
boundaries(void)
{
    FILE *f;
    Solver *s = pair(3, 2, 0, &f);
    s->work_limit = 1;
    assert(solver_certified_ssr(s) && !s->ssr_strengthened && s->work <= 1);
    solver_free(s);
    fclose(f);
    s = pair(3, 2, 0, &f);
    int cancel = 1;
    s->terminate = stop;
    s->terminate_state = &cancel;
    assert(solver_certified_ssr(s) && s->interrupted && !s->ssr_strengthened);
    cancel = 0;
    s->interrupted = false;
    s->cancelled = false;
    assert(solver_certified_ssr(s) && s->ssr_strengthened);
    solver_free(s);
    fclose(f);
    s = pair(3, 2, 0, &f);
    s->journal_limit = 1;
    assert(!solver_certified_ssr(s) && s->error && !s->ssr_strengthened);
    assert(!clause_deleted(s->arena, s->clauses[1]));
    solver_free(s);
    fclose(f);
    /* Root-assigned target is left to mandatory propagation, not this pass. */
    s = pair(3, 2, 0, &f);
    Lit unit = mkLit(3, false);
    assert(solver_add_clause(s, &unit, 1));
    assert(solver_certified_ssr(s) && !s->ssr_strengthened);
    solver_free(s);
    fclose(f);
}
#endif
int
main(void)
{
#ifdef BSAT_CERTIFIED_SSR
    exhaustive();
    boundaries();
#else
    puts("SKIP: certified SSR is an experimental compile-time option");
#endif
    /* Future input/assumptions through the public API, in both retained modes. */
    for (unsigned flags = 2; flags <= 7; ++flags)
        if (flags != 4 && flags != 5) {
            bsat *s = bsat_create(1, flags);

            assert(s);
            int a[] = {-1, 2}, b[] = {1, 2, 3};

            assert(bsat_add_clause(s, a, 2) && bsat_add_clause(s, b, 3));
            int both[] = {-2, -3};

            assert(bsat_solve(s, both, 2) == 20 && !bsat_error(s));
            int flip[] = {2, -3};

            assert(bsat_solve(s, flip, 2) == 10);
            assert(bsat_solve(s, NULL, 0) == 10 && bsat_checkpoint(s));
            int c[] = {-2};

            assert(bsat_add_clause(s, c, 1));
            int d[] = {-3};

            assert(bsat_add_clause(s, d, 1));
            assert(bsat_solve(s, NULL, 0) == 20 && !bsat_error(s));
            bsat_destroy(s);
        }
    puts("PASS: certified SSR boundary and future-query/addition semantics");
}
