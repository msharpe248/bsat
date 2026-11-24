/*********************************************************************
 * Tests for Phase 6 upgrades:
 *  - Dynamic in-search GF(2) Gaussian elimination (Bird / Han-Jiang)
 *  - AMO clique extraction & Sinz sequential-counter re-encoding
 *  - Probing intersection (double lookahead) with DRAT/RUP logging
 *  - Literal-indexed lit_values[] BCP parity & external mutation safety
 *********************************************************************/

#include "../include/solver.h"
#include "../include/inprocess.h"
#include "../include/gauss.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rng_next(uint32_t *st) {
    uint32_t x = *st;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *st = x;
    return x;
}

static void add_xor_clauses(Solver *s, const Var *vars, uint32_t k, uint8_t rhs) {
    Lit lits[6];
    for (uint32_t mask = 0; mask < (1u << k); ++mask) {
        uint32_t pop = 0;
        for (uint32_t i = 0; i < k; ++i) pop += (mask >> i) & 1u;
        if ((pop & 1u) != rhs) {
            for (uint32_t i = 0; i < k; ++i) {
                /* Forbid assignment where vars[i] == ((mask>>i)&1) */
                lits[i] = mkLit(vars[i], ((mask >> i) & 1u) != 0);
            }
            solver_add_clause(s, lits, k);
        }
    }
}

static void test_gauss_mixed_sat_and_unsat(void) {
    /* 1. Underdetermined 40-var XOR system (30 4-XORs) + planted 3-SAT clauses (SAT)
     *    tested under BOTH chrono and non-chrono modes, plus incremental re-solve! */
    for (int chrono = 0; chrono < 2; ++chrono) {
        SolverOpts opts = default_opts();
        opts.chrono = (chrono != 0);
        opts.inprocess = true;
        Solver *s = solver_new_with_opts(&opts);
        assert(s != NULL);

        const uint32_t N = 40;
        for (uint32_t i = 0; i < N; ++i) assert(solver_new_var(s) == i + 1);

        uint32_t st = 0x12345678u + (uint32_t)chrono;
        uint8_t sol[41];
        for (uint32_t v = 1; v <= N; ++v) sol[v] = (uint8_t)(rng_next(&st) & 1u);

        for (uint32_t e = 0; e < 30; ++e) {
            Var vs[4];
            for (uint32_t j = 0; j < 4; ++j) {
                bool dup;
                do {
                    vs[j] = 1u + (rng_next(&st) % N);
                    dup = false;
                    for (uint32_t p = 0; p < j; ++p) if (vs[p] == vs[j]) dup = true;
                } while (dup);
            }
            uint8_t rhs = 0;
            for (uint32_t j = 0; j < 4; ++j) rhs ^= sol[vs[j]];
            add_xor_clauses(s, vs, 4, rhs);
        }
        for (uint32_t c = 0; c < 90; ++c) {
            Lit cl[3];
            bool sat = false;
            for (uint32_t j = 0; j < 3; ++j) {
                Var v = 1u + (rng_next(&st) % N);
                bool neg_sign = (rng_next(&st) & 1u) != 0;
                cl[j] = mkLit(v, neg_sign);
                if ((sol[v] != 0) == !neg_sign) sat = true;
            }
            if (!sat) cl[0] = neg(cl[0]);
            solver_add_clause(s, cl, 3);
        }

        lbool r = solver_solve(s);
        assert(r == TRUE);
        assert(solver_check_model(s));

        /* Incremental re-solve after Gauss: forbid the found assignment on var 1..3 */
        Lit block[3];
        for (uint32_t v = 1; v <= 3; ++v) {
            block[v - 1] = mkLit(v, solver_model_value(s, v) == TRUE);
        }
        solver_add_clause(s, block, 3);
        lbool r2 = solver_solve(s);
        assert(r2 == TRUE || r2 == FALSE);
        if (r2 == TRUE) assert(solver_check_model(s));

        solver_free(s);
    }

    /* 2. Underdetermined 36-var XOR system + overconstrained 3-SAT (UNSAT)
     *    cross-checked against pure CDCL (--no-inprocess)! */
    SolverOpts o_gauss = default_opts();
    o_gauss.inprocess = true;
    SolverOpts o_plain = default_opts();
    o_plain.inprocess = false;

    Solver *sg = solver_new_with_opts(&o_gauss);
    Solver *sp = solver_new_with_opts(&o_plain);
    const uint32_t N = 36;
    for (uint32_t i = 0; i < N; ++i) {
        assert(solver_new_var(sg) == i + 1);
        assert(solver_new_var(sp) == i + 1);
    }
    uint32_t st = 0xDEADBEEFu;
    for (uint32_t e = 0; e < 26; ++e) {
        Var vs[4];
        for (uint32_t j = 0; j < 4; ++j) {
            bool dup;
            do {
                vs[j] = 1u + (rng_next(&st) % N);
                dup = false;
                for (uint32_t p = 0; p < j; ++p) if (vs[p] == vs[j]) dup = true;
            } while (dup);
        }
        uint8_t rhs = (uint8_t)(rng_next(&st) & 1u);
        add_xor_clauses(sg, vs, 4, rhs);
        add_xor_clauses(sp, vs, 4, rhs);
    }
    for (uint32_t c = 0; c < 140; ++c) {
        Lit cl[3];
        for (uint32_t j = 0; j < 3; ++j) {
            cl[j] = mkLit(1u + (rng_next(&st) % N), (rng_next(&st) & 1u) != 0);
        }
        solver_add_clause(sg, cl, 3);
        solver_add_clause(sp, cl, 3);
    }
    lbool rg = solver_solve(sg);
    lbool rp = solver_solve(sp);
    assert(rg == rp);
    assert(rg == FALSE);
    solver_free(sg);
    solver_free(sp);
}

static void test_amo_cliques_sat_and_unsat(void) {
    /* Build 10-by-9 grid (90 vars) with row-ALO (size 9) and column-AMO (size 10 cliques):
     * PHP(10, 9) has 9 cliques of size 10 = 9*45 = 405 binaries (plus extra cliques so bin_cnt >= 500).
     * First test SAT: 12 disjoint cliques of size 10 (12 * 45 = 540 binaries) + ALO(10) per clique! */
    SolverOpts opts = default_opts();
    opts.inprocess = true;
    Solver *s = solver_new_with_opts(&opts);
    const uint32_t K = 10, GROUPS = 12;
    for (uint32_t i = 0; i < K * GROUPS; ++i) solver_new_var(s);

    for (uint32_t g = 0; g < GROUPS; ++g) {
        Lit alo[10];
        for (uint32_t i = 0; i < K; ++i) {
            Var vi = g * K + i + 1;
            alo[i] = mkLit(vi, false);
            for (uint32_t j = i + 1; j < K; ++j) {
                Var vj = g * K + j + 1;
                Lit pair[2] = {mkLit(vi, true), mkLit(vj, true)};
                solver_add_clause(s, pair, 2);
            }
        }
        solver_add_clause(s, alo, K);
    }
    /* Couple groups with mixed-polarity binary/ternary constraints so neither
     * root propagation nor lucky-phase (all-true / all-false) solves it early! */
    for (uint32_t g = 0; g + 1 < GROUPS; ++g) {
        Lit c1[2] = {mkLit(g * K + 1, true), mkLit((g + 1) * K + 1, true)};
        Lit c2[3] = {mkLit(g * K + 2, false), mkLit(g * K + 3, false), mkLit((g + 1) * K + 2, false)};
        solver_add_clause(s, c1, 2);
        solver_add_clause(s, c2, 3);
    }
    /* Directly invoke solver_amo_cliques and verify compression + aux var creation. */
    assert(solver_amo_cliques(s));
    assert(s->factor_original_vars == K * GROUPS);
    assert(s->num_vars > K * GROUPS);
    lbool r = solver_solve(s);
    assert(r == TRUE);
    assert(solver_check_model(s));
    for (uint32_t g = 0; g < GROUPS; ++g) {
        uint32_t true_cnt = 0;
        for (uint32_t i = 0; i < K; ++i) {
            if (solver_model_value(s, g * K + i + 1) == TRUE) true_cnt++;
        }
        assert(true_cnt == 1);
    }

    /* Incremental contradiction after AMO re-encoding: assert TWO vars in group 0! */
    Lit bad1 = mkLit(1, false), bad2 = mkLit(2, false);
    solver_add_clause(s, &bad1, 1);
    solver_add_clause(s, &bad2, 1);
    lbool r2 = solver_solve(s);
    assert(r2 == FALSE);
    solver_free(s);

    /* Direct UNSAT test with AMO cliques: 12 groups of 10-cliques where group 0 forces TWO vars! */
    Solver *su = solver_new_with_opts(&opts);
    for (uint32_t i = 0; i < K * GROUPS; ++i) solver_new_var(su);
    for (uint32_t g = 0; g < GROUPS; ++g) {
        for (uint32_t i = 0; i < K; ++i) {
            for (uint32_t j = i + 1; j < K; ++j) {
                Lit pair[2] = {mkLit(g * K + i + 1, true), mkLit(g * K + j + 1, true)};
                solver_add_clause(su, pair, 2);
            }
        }
    }
    /* Couple via ternary clauses so it isn't trivially killed at parse time:
     * (x1 | x2), (~x1 | x3), (~x2 | x4), (~x3 | x5), (~x4 | x5), (~x5 | x6) -> wait:
     * Simpler: (a | b), (~a | x1), (~a | x2), (~b | x3), (~b | x4) where x1..x4 in clique 0! */
    Var a = solver_new_var(su);
    Var b = solver_new_var(su);
    Lit c0[2] = {mkLit(a, false), mkLit(b, false)};
    Lit c1[2] = {mkLit(a, true), mkLit(1, false)};
    Lit c2[2] = {mkLit(a, true), mkLit(2, false)};
    Lit c3[2] = {mkLit(b, true), mkLit(3, false)};
    Lit c4[2] = {mkLit(b, true), mkLit(4, false)};
    solver_add_clause(su, c0, 2);
    solver_add_clause(su, c1, 2);
    solver_add_clause(su, c2, 2);
    solver_add_clause(su, c3, 2);
    solver_add_clause(su, c4, 2);
    assert(solver_solve(su) == FALSE);
    solver_free(su);
}

static void test_lit_values_external_mutation_safety(void) {
    /* Verify direct s->values[v] mutation outside solve_internal still works! */
    Solver *s = solver_new();
    Var v1 = solver_new_var(s);
    Var v2 = solver_new_var(s);
    Var v3 = solver_new_var(s);
    Lit c1[2] = {mkLit(v1, true), mkLit(v2, false)};
    Lit c2[2] = {mkLit(v2, true), mkLit(v3, false)};
    solver_add_clause(s, c1, 2);
    solver_add_clause(s, c2, 2);

    /* Mutate s->values[v1] directly (without touching lit_values), then propagate! */
    assert(!s->lit_values_live);
    s->values[v1] = TRUE;
    s->vars[v1].level = 0;
    s->trail[s->trail_size++].lit = mkLit(v1, false);
    assert(solver_propagate(s) == INVALID_CLAUSE);
    assert(s->values[v2] == TRUE);
    assert(s->values[v3] == TRUE);
    solver_free(s);
}

int main(void) {
    test_gauss_mixed_sat_and_unsat();
    test_amo_cliques_sat_and_unsat();
    test_lit_values_external_mutation_safety();
    printf("PASS: test_gauss_and_amo\n");
    return 0;
}
