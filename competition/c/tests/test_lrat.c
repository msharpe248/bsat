/*********************************************************************
 * Unit Tests for Native Text & Binary LRAT Proof Emission & Verification
 *********************************************************************/

#include "../include/solver.h"
#include "../include/dimacs.h"
#include "../include/lrat.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static uint32_t lcg(uint32_t *st) {
    *st = (*st) * 1664525u + 1013904223u;
    return *st;
}

static void solve_and_verify_lrat(const char *cnf_text, bool binary, bool chrono,
                                  uint32_t reduce_interval) {
    char cnf_path[128], lrat_path[128];
    snprintf(cnf_path, sizeof cnf_path, "/tmp/bsat_test_lrat_%d.cnf", (int)getpid());
    snprintf(lrat_path, sizeof lrat_path, "/tmp/bsat_test_lrat_%d.lrat", (int)getpid());

    FILE *f = fopen(cnf_path, "w");
    assert(f);
    fputs(cnf_text, f);
    fclose(f);

    SolverOpts opts = default_opts();
    opts.lrat_path = lrat_path;
    opts.binary_lrat = binary;
    opts.chrono = chrono;
    if (reduce_interval > 0) {
        opts.reduce_interval = reduce_interval;
        opts.reduce_increment = 0;
    }

    Solver *s = solver_new_with_opts(&opts);
    assert(s);
    assert(dimacs_parse_file(s, cnf_path) == DIMACS_OK);
    lbool r = solver_solve(s);
    assert(r == FALSE && !s->error);
    assert(lrat_emitted_empty(s->lrat));
    solver_free(s);

    /* Independently verify every addition, deletion, hint chain, and empty clause! */
    assert(lrat_verify_file(cnf_path, lrat_path, binary));

    unlink(cnf_path);
    unlink(lrat_path);
}

static void test_root_and_unit_contradictions(void) {
    /* 1. Empty clause in input (followed by extra clauses to verify ID > M!). */
    const char *empty_in = "p cnf 3 4\n1 2 0\n0\n-1 3 0\n2 -3 0\n";
    /* 2. Direct unit contradiction followed by extra clauses. */
    const char *unit_contra = "p cnf 3 4\n1 0\n-1 0\n2 3 0\n-2 -3 0\n";
    /* 3. Multi-step level-0 propagation chain contradiction + tautology clause in middle. */
    const char *prop_chain =
        "p cnf 5 7\n"
        "1 0\n"
        "2 -2 0\n"    /* tautology -> must still preserve 1-to-1 clause numbering! */
        "-1 2 0\n"
        "-2 3 0\n"
        "-1 -3 4 0\n"
        "-4 5 0\n"
        "-5 -2 0\n";

    for (int bin = 0; bin <= 1; ++bin) {
        solve_and_verify_lrat(empty_in, bin != 0, true, 0);
        solve_and_verify_lrat(unit_contra, bin != 0, true, 0);
        solve_and_verify_lrat(prop_chain, bin != 0, true, 0);
    }
}

static void test_pigeonhole_and_gc_lrat(void) {
    /* PHP(6,5) UNSAT with aggressive clause reduction & arena GC + chronological backtracking! */
    char buf[8192];
    size_t pos = 0;
    uint32_t P = 6, H = 5;
    uint32_t nc = P + H * (P * (P - 1) / 2);
    pos += (size_t)snprintf(buf + pos, sizeof buf - pos, "p cnf %u %u\n", P * H, nc);
    for (uint32_t p = 0; p < P; ++p) {
        for (uint32_t h = 0; h < H; ++h)
            pos += (size_t)snprintf(buf + pos, sizeof buf - pos, "%u ", p * H + h + 1);
        pos += (size_t)snprintf(buf + pos, sizeof buf - pos, "0\n");
    }
    for (uint32_t h = 0; h < H; ++h) {
        for (uint32_t p1 = 0; p1 < P; ++p1) {
            for (uint32_t p2 = p1 + 1; p2 < P; ++p2) {
                pos += (size_t)snprintf(buf + pos, sizeof buf - pos, "-%u -%u 0\n",
                                        p1 * H + h + 1, p2 * H + h + 1);
            }
        }
    }

    for (int bin = 0; bin <= 1; ++bin) {
        for (int chrono = 0; chrono <= 1; ++chrono) {
            /* Force frequent reductions & arena GC (reduce_interval = 30) to stress
             * LRAT deletion buffering + GC CRef relocation! */
            solve_and_verify_lrat(buf, bin != 0, chrono != 0, 30);
        }
    }
}

static void test_random_unsat_lrat(void) {
    /* Generate 25 random UNSAT 3-SAT instances (18 vars, 90 clauses) and verify both
     * text and binary LRAT proofs (exercising 1-UIP, 3-state minimization, chrono, GC). */
    uint32_t rng = 0x517CC1B7u;
    char buf[8192];
    int verified = 0;

    for (int attempt = 0; attempt < 300 && verified < 25; ++attempt) {
        uint32_t nv = 16u + (lcg(&rng) % 5u);
        uint32_t nc = nv * 6u + 15u;
        size_t pos = (size_t)snprintf(buf, sizeof buf, "p cnf %u %u\n", nv, nc);
        for (uint32_t c = 0; c < nc; ++c) {
            int l1 = (int)(1u + ((lcg(&rng) >> 8) % nv)) * (((lcg(&rng) >> 16) & 1u) ? -1 : 1);
            int l2 = (int)(1u + ((lcg(&rng) >> 8) % nv)) * (((lcg(&rng) >> 16) & 1u) ? -1 : 1);
            int l3 = (int)(1u + ((lcg(&rng) >> 8) % nv)) * (((lcg(&rng) >> 16) & 1u) ? -1 : 1);
            pos += (size_t)snprintf(buf + pos, sizeof buf - pos, "%d %d %d 0\n", l1, l2, l3);
        }

        /* Quick pre-check if UNSAT */
        Solver *probe = solver_new();
        assert(dimacs_parse_string(probe, buf) == DIMACS_OK);
        lbool r = solver_solve(probe);
        solver_free(probe);
        if (r != FALSE) continue;

        solve_and_verify_lrat(buf, (verified & 1) != 0, true, 25);
        verified++;
    }
    assert(verified == 25);
}

static void test_verifier_rejects_corrupt_lrat(void) {
    char cnf_path[128], bad_path[128];
    snprintf(cnf_path, sizeof cnf_path, "/tmp/bsat_bad_lrat_%d.cnf", (int)getpid());
    snprintf(bad_path, sizeof bad_path, "/tmp/bsat_bad_lrat_%d.lrat", (int)getpid());
    FILE *f = fopen(cnf_path, "w"); assert(f);
    fputs("p cnf 2 2\n1 2 0\n-1 2 0\n", f);
    fclose(f);
    FILE *p = fopen(bad_path, "w"); assert(p);
    /* Bogus empty clause derivation from a SAT formula! */
    fputs("3 0 1 2 0\n", p);
    fclose(p);
    assert(!lrat_verify_file(cnf_path, bad_path, false));
    unlink(cnf_path);
    unlink(bad_path);
}

int main(void) {
    test_root_and_unit_contradictions();
    test_pigeonhole_and_gc_lrat();
    test_random_unsat_lrat();
    test_verifier_rejects_corrupt_lrat();
    puts("PASS: Native text & binary LRAT emission + independent linear verifier");
    return 0;
}
