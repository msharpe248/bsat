#include "solver.h"
#include "ipasir.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>

static size_t calls, fail_at;
static bool failed;

static bool
reject(void)
{
    if (++calls != fail_at) return false;
    failed = true;
    errno = ENOMEM;
    return true;
}

void *
fault_malloc(size_t n)
{
    return reject() ? NULL : malloc(n);
}

void *
fault_calloc(size_t n, size_t z)
{
    return reject() ? NULL : calloc(n, z);
}

void *
fault_realloc(void *p, size_t n)
{
    return reject() ? NULL : realloc(p, n);
}

/* Exclude every assignment to six variables, or leave only all-true.
   Independent closed-form oracle; inspect the original clauses for SAT. */
static size_t
attempt(unsigned profile, bool sat, size_t cutoff)
{
    calls = 0;
    fail_at = cutoff;
    failed = false;
    SolverOpts o = default_opts();

    o.reduce_interval = 2;
    o.restart_first = 2;
    o.luby_unit = 2;
    o.probing = profile == 1;
    o.elim = profile == 2;
    o.bce = profile == 2;
    o.equiv = profile == 3;
    o.congruence = profile == 3;
    o.chrono = profile == 4;
    o.chrono_levels = 0;
    o.vmtf = profile == 4;
    o.reuse_learnts = profile == 6 || profile == 8;
    o.retained_elim = profile == 8;
    if (profile == 8) o.elim_max_occ = 100;
    o.lrb = profile == 5;
    o.rephase = profile == 5;
    o.factor = profile == 7;
    Solver *s = solver_new_with_opts(&o);
    FILE *journal = NULL;

    if (!s) {
        assert(failed);
        return calls;
    }
    if (profile == 8) {
        journal = tmpfile();
        assert(journal);
        s->proof_journal = journal;
    }
    for (unsigned v = 0; v < 8; ++v)
        if (!solver_new_var(s)) goto done;
    if (profile == 7) {
        for (Var a = 1; a <= 3; ++a)
            for (Var b = 4; b <= 6; ++b) {
                Lit c[] = {mkLit(a, false), mkLit(b, false)};

                solver_add_clause(s, c, 2);
                if (s->error || s->watches->failed) goto done;
            }
        if (!sat) {
            Lit a[] = {mkLit(1, true), mkLit(2, true), mkLit(3, true)};
            Lit b[] = {mkLit(4, true), mkLit(5, true), mkLit(6, true)};

            solver_add_clause(s, a, 3);
            solver_add_clause(s, b, 3);
            if (s->error || s->watches->failed) goto done;
        }
    } else
        for (unsigned bits = 0; bits < 64 - (unsigned)sat; ++bits) {
            Lit c[6];

            for (unsigned v = 0; v < 6; ++v)
                c[v] = mkLit(v + 1, (bits >> v) & 1);
            solver_add_clause(s, c, 6);
            if (s->error || s->watches->failed) goto done;
        }
    {
        Lit a[] = {mkLit(7, true), mkLit(8, false)}, b[] = {mkLit(7, false), mkLit(8, true)};

        solver_add_clause(s, a, 2);
        solver_add_clause(s, b, 2);
    }
    if (profile == 8) {
        Lit frozen = mkLit(7, false);

        elim_preprocess_frozen(s, &frozen, 1);
        if (s->error || s->watches->failed) goto done;
    }
    for (unsigned repeat = 0; repeat < 2; ++repeat) {
        if ((profile == 6 || profile == 8) && repeat == 1)
            while (s->num_vars < 130)
                if (!solver_new_var(s)) goto done;
        lbool r = solver_solve(s);

        if (s->error || s->watches->failed) {
            assert(r == UNDEF);
            break;
        }
        assert(r == (sat ? TRUE : FALSE));
        if (sat) {
            assert(solver_check_model(s));
            if (profile != 7)
                for (unsigned v = 1; v <= 6; ++v)
                    assert(solver_model_value(s, v) == TRUE);
        }
        if ((profile == 6 || profile == 8) && sat && repeat == 0) {
            Lit assumption = mkLit(1, true);

            r = solver_solve_with_assumptions(s, &assumption, 1);
            if (s->error || s->watches->failed) {
                assert(r == UNDEF);
                break;
            }
            assert(r == FALSE); /* Next growth/solve must clear only conditional UNSAT. */
        }
    }
done:
    if (s->error || s->watches->failed) {
        bool error = s->error, watch = s->watches->failed;
        lbool r = solver_solve(s);

        if (r != UNDEF)
            fprintf(stderr, "conclusive after failure: error=%d watch=%d result=%d solved=%d\n",
                    error, watch, r, s->has_solved);
        assert(r == UNDEF);
    }
    solver_free(s);
    if (journal) assert(!fclose(journal));
    return calls;
}

static void
receive(void *state, int *clause)
{
    (void)state;
    while (*clause)
        ++clause;
}

static size_t
ipasir_attempt(bool sat, size_t cutoff)
{
    calls = 0;
    fail_at = cutoff;
    failed = false;
    void *s = ipasir_init();

    if (!s) {
        assert(failed);
        return calls;
    }
    ipasir_set_learn(s, NULL, 6, receive);
    for (unsigned bits = 0; bits < 64 - (unsigned)sat; ++bits) {
        for (unsigned v = 0; v < 6; ++v)
            ipasir_add(s, (bits >> v) & 1 ? -(int)(v + 1) : (int)(v + 1));
        ipasir_add(s, 0);
    }
    for (unsigned repeat = 0; repeat < 2; ++repeat) {
        int r = ipasir_solve(s);

        assert(r == (failed ? 0 : sat ? 10 : 20));
        if (sat && r == 10)
            for (int v = 1; v <= 6; ++v)
                assert(ipasir_val(s, v) == v);
        ipasir_assume(s, 7); /* Growth and assumption buffer allocations. */
    }
    ipasir_release(s);
    return calls;
}

static size_t
certificate_attempt(uint32_t flags, size_t cutoff)
{
    calls = 0;
    fail_at = cutoff;
    failed = false;
    bsat *s = bsat_create(1, flags);

    if (!s) {
        assert(failed);
        return calls;
    }
    int a[] = {1, 2}, b[] = {-1, 2}, unit = -2;

    bsat_add_clause(s, a, 2);
    bsat_add_clause(s, b, 2);
    for (unsigned i = 0; i < 4; ++i) {
        int r = bsat_solve(s, i & 1 ? NULL : &unit, i & 1 ? 0 : 1);

        assert(r == (failed ? 0 : i & 1 ? 10 : 20));
        if (i == 1) {
            int completed = bsat_checkpoint(s);

            assert(completed == !failed);
        }
    }
    bsat_destroy(s);
    return calls;
}
#ifdef BSAT_CERTIFIED_SSR
static size_t
ssr_attempt(size_t cutoff)
{
    fail_at = 0;
    calls = 0;
    failed = false;
    SolverOpts o = default_opts();
    o.probing = false;
    o.assumption_lbd = true;
    Solver *s = solver_new_with_opts(&o);
    assert(s);
    for (unsigned v = 0; v < 202; ++v)
        assert(solver_new_var(s));
    Lit source[] = {mkLit(1, true), mkLit(2, false)};
    assert(solver_add_clause(s, source, 2));
    for (unsigned v = 3; v <= 202; ++v) {
        Lit target[] = {mkLit(1, false), mkLit(2, false), mkLit(v, false)};
        assert(solver_add_clause(s, target, 3));
    }
    FILE *journal = tmpfile();
    assert(journal);
    s->proof_journal = journal;
    s->opts.binary_proof = true;
    s->work_limit = 1000000;
    calls = 0;
    fail_at = cutoff;
    failed = false;
    bool okay = solver_certified_ssr(s);
    size_t count = calls;
    fail_at = 0;
    if (failed)
        assert(!okay && s->error);
    else
        assert(okay && s->ssr_strengthened == 200);
    s->work_limit = 0;
    lbool result = solver_solve(s);
    if (failed)
        assert(result == UNDEF);
    else
        assert(result == TRUE && solver_check_model(s));
    solver_free(s);
    fclose(journal);
    return count;
}
#endif
int
main(void)
{
    size_t injected = 0;

#ifdef BSAT_CERTIFIED_SSR
    size_t ssr_count = ssr_attempt(0);
    assert(ssr_count > 0);
    for (size_t i = 1; i <= ssr_count; ++i) {
        ssr_attempt(i);
        assert(failed);
        ++injected;
    }
    printf("PASS: %zu SSR replacement allocation failures preserve UNKNOWN/error\n", ssr_count);
#endif
    for (unsigned p = 0; p < 9; ++p)
        for (unsigned sat = 0; sat < 2; ++sat) {
            size_t count = attempt(p, sat, 0);

            for (size_t i = 1; i <= count; ++i) {
                fprintf(stderr, "fault profile=%u sat=%u allocation=%zu/%zu\n", p, sat, i, count);
                attempt(p, sat, i);
                assert(failed);
                ++injected;
            }
        }
    for (unsigned sat = 0; sat < 2; ++sat) {
        size_t count = ipasir_attempt(sat, 0);

        for (size_t i = 1; i <= count; ++i) {
            ipasir_attempt(sat, i);
            assert(failed);
            ++injected;
        }
    }
    const uint32_t certificate_flags[] = {2, 3, 6, 7, 11, 15, 19, 23, 27, 31};

    for (unsigned mode = 0; mode < sizeof certificate_flags / sizeof *certificate_flags; ++mode) {
        size_t count = certificate_attempt(certificate_flags[mode], 0);

        for (size_t i = 1; i <= count; ++i) {
            certificate_attempt(certificate_flags[mode], i);
            assert(failed);
            ++injected;
        }
    }
    printf("PASS: %zu single-allocation failures across 30 profiles/formulas and repeated solves\n",
           injected);
}
