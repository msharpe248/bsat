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

static void test_sbva_arbitrary_length_sat_and_unsat(void) {
    /* Construct a structured 4-SAT biclique (A x B) where |A| = 5, |B| = 6
     * (30 4-clauses of the form (a_i | b_j1 | b_j2 | b_j3)), which naive 2/3-clause
     * BVA cannot factor, and verify SBVA compresses it cleanly with SAT model check
     * and UNSAT equivalence! */
    SolverOpts opts = default_opts();
    opts.sbva = true;
    Solver *s = solver_new_with_opts(&opts);
    const uint32_t NA = 5, NB = 18;
    for (uint32_t i = 0; i < NA + NB; ++i) solver_new_var(s);
    /* 5 literals a_1..a_5 crossed with 6 distinct 3-literal clauses -> 30 4-clauses */
    for (uint32_t a = 1; a <= NA; ++a) {
        for (uint32_t b = 0; b < 6; ++b) {
            Lit c[4] = {
                mkLit(a, false),
                mkLit(NA + b * 3 + 1, false),
                mkLit(NA + b * 3 + 2, false),
                mkLit(NA + b * 3 + 3, false)
            };
            solver_add_clause(s, c, 4);
        }
    }
    /* Add extra clauses so num_clauses >= 64 */
    for (uint32_t b = 0; b < 6; ++b) {
        for (uint32_t a = 1; a <= NA; ++a) {
            Lit c[4] = {
                mkLit(a, true),
                mkLit(NA + b * 3 + 1, true),
                mkLit(NA + b * 3 + 2, false),
                mkLit(NA + b * 3 + 3, true)
            };
            solver_add_clause(s, c, 4);
        }
    }
    for (uint32_t i = 1; i <= 6; ++i) {
        Lit c[2] = {mkLit(i, false), mkLit(i + 1, true)};
        solver_add_clause(s, c, 2);
    }
    uint32_t orig_nv = s->num_vars;
    assert(solver_sbva(s, 5000000ULL, 64, 2));
    assert(s->factor_original_vars == orig_nv);
    assert(s->num_vars > orig_nv);
    lbool r = solver_solve(s);
    assert(r == TRUE);
    assert(solver_check_model(s));

    /* Now force all a_i = FALSE AND all b_{0,1..3} = FALSE -> UNSAT! */
    for (uint32_t a = 1; a <= NA; ++a) {
        Lit u = mkLit(a, true);
        solver_add_clause(s, &u, 1);
    }
    for (uint32_t k = 1; k <= 3; ++k) {
        Lit u = mkLit(NA + k, true);
        solver_add_clause(s, &u, 1);
    }
    assert(solver_solve(s) == FALSE);
    solver_free(s);
}

static void add_chained_xor(Solver *s, const Var *vars, uint32_t len, uint8_t rhs) {
    /* Encode x_0 ^ x_1 ^ ... ^ x_{len-1} = rhs as a Tseitin chain of 3-var XORs! */
    assert(len >= 3);
    if (len <= 6) {
        add_xor_clauses(s, vars, len, rhs);
        return;
    }
    Var cur = vars[0];
    for (uint32_t i = 1; i + 2 < len; ++i) {
        Var t = solver_new_var(s);
        Var tri[3] = {cur, vars[i], t};
        add_xor_clauses(s, tri, 3, 0); /* cur ^ vars[i] = t */
        cur = t;
    }
    Var last[3] = {cur, vars[len - 2], vars[len - 1]};
    add_xor_clauses(s, last, 3, rhs);
}

static void test_gauss_chain_stitching_and_subsumed_xor(void) {
    /* 1. Tseitin-chained 16-variable XOR system (UNSAT in 0 conflicts via chain stitching + RREF!). */
    SolverOpts opts = default_opts();
    opts.inprocess = true;
    Solver *s = solver_new_with_opts(&opts);
    const uint32_t N = 16;
    Var base[16];
    for (uint32_t i = 0; i < N; ++i) base[i] = solver_new_var(s);
    /* Two contradictory 16-variable Tseitin-chained XORs over the SAME 16 variables! */
    add_chained_xor(s, base, N, 0);
    add_chained_xor(s, base, N, 1);
    lbool r = solver_solve(s);
    assert(r == FALSE);
    assert(s->stats.conflicts == 0);
    solver_free(s);

    /* 2. Subsumed/strengthened 4-var XOR recovery:
     *    Encode x1^x2^x3^x4 = 0 AND x1^x2^x3^x4 = 1, EXCEPT replace clause (x1|x2|x3|x4)
     *    in the second XOR by the stronger binary clause (x1|x2)! */
    Solver *s2 = solver_new_with_opts(&opts);
    Var vs[4];
    for (uint32_t i = 0; i < 4; ++i) vs[i] = solver_new_var(s2);
    add_xor_clauses(s2, vs, 4, 0); /* forbids all odd masks */
    /* Add all even masks EXCEPT mask 0 (all positive: x1|x2|x3|x4), replaced by binary (x1|x2)! */
    Lit sub[2] = {mkLit(vs[0], false), mkLit(vs[1], false)};
    solver_add_clause(s2, sub, 2);
    for (uint32_t m = 1; m < 16; ++m) {
        if ((__builtin_popcount(m) & 1) == 0) {
            Lit cl[4];
            for (uint32_t i = 0; i < 4; ++i) cl[i] = mkLit(vs[i], ((m >> i) & 1) != 0);
            solver_add_clause(s2, cl, 4);
        }
    }
    /* Add a dummy 3-XOR so n_ge3 >= 2 triggers Gauss extraction. */
    Var v5 = solver_new_var(s2);
    Var tri[3] = {vs[0], vs[1], v5};
    add_xor_clauses(s2, tri, 3, 0);
    assert(solver_solve(s2) == FALSE);
    assert(s2->stats.conflicts == 0);
    solver_free(s2);
}

static void test_nary_congruence_and_miter_sweep(void) {
    /* 1. Two 5-input AND gates with identical inputs (z1 = a1&a2&a3&a4&a5,
     *    z2 = a1&a2&a3&a4&a5) plus miter constraint z1 != z2 -> UNSAT in 0 conflicts
     *    via N-ary AND congruence + Tarjan SCC (100% RUP-certified!). */
    SolverOpts opts = default_opts();
    opts.inprocess = true;
    Solver *s = solver_new_with_opts(&opts);
    Var a[5];
    for (int i = 0; i < 5; ++i) a[i] = solver_new_var(s);
    Var z1 = solver_new_var(s), z2 = solver_new_var(s);
    Var outs[2] = {z1, z2};
    for (int g = 0; g < 2; ++g) {
        Lit big[6];
        big[0] = mkLit(outs[g], false);
        for (int i = 0; i < 5; ++i) {
            big[i + 1] = mkLit(a[i], true);
            Lit b[2] = {mkLit(outs[g], true), mkLit(a[i], false)};
            solver_add_clause(s, b, 2);
        }
        solver_add_clause(s, big, 6);
    }
    /* Assert z1 != z2: (z1 | z2) and (~z1 | ~z2). */
    Lit d1[2] = {mkLit(z1, false), mkLit(z2, false)};
    Lit d2[2] = {mkLit(z1, true), mkLit(z2, true)};
    solver_add_clause(s, d1, 2);
    solver_add_clause(s, d2, 2);
    assert(solver_solve(s) == FALSE);
    assert(s->stats.conflicts == 0);
    solver_free(s);
}

static void test_parallel_portfolio_sat_and_unsat(void) {
    /* 1. SAT mixed 3-SAT solved with 4 parallel clause-sharing threads! */
    SolverOpts opts = default_opts();
    opts.quiet = true;
    opts.stats = false;
    Solver *s = solver_new_with_opts(&opts);
    const uint32_t N = 50;
    for (uint32_t i = 0; i < N; ++i) solver_new_var(s);
    uint32_t st = 0xCAFEBABEu;
    uint8_t sol[51];
    for (uint32_t v = 1; v <= N; ++v) sol[v] = (uint8_t)(rng_next(&st) & 1u);
    for (uint32_t c = 0; c < 200; ++c) {
        Lit cl[3];
        bool sat = false;
        for (uint32_t j = 0; j < 3; ++j) {
            Var v = 1u + (rng_next(&st) % N);
            bool neg_s = (rng_next(&st) & 1u) != 0;
            cl[j] = mkLit(v, neg_s);
            if ((sol[v] != 0) == !neg_s) sat = true;
        }
        if (!sat) cl[0] = neg(cl[0]);
        solver_add_clause(s, cl, 3);
    }
    lbool r = solver_solve_parallel(s, 4);
    assert(r == TRUE);
    assert(solver_check_model(s));
    solver_free(s);

    /* 2. UNSAT Pigeonhole PHP(7, 6) solved with 4 parallel clause-sharing threads! */
    Solver *su = solver_new_with_opts(&opts);
    const uint32_t H = 6, P = 7;
    for (uint32_t i = 0; i < P * H; ++i) solver_new_var(su);
    for (uint32_t p = 0; p < P; ++p) {
        Lit alo[6];
        for (uint32_t h = 0; h < H; ++h) alo[h] = mkLit(p * H + h + 1, false);
        solver_add_clause(su, alo, H);
    }
    for (uint32_t h = 0; h < H; ++h) {
        for (uint32_t p1 = 0; p1 < P; ++p1) {
            for (uint32_t p2 = p1 + 1; p2 < P; ++p2) {
                Lit pair[2] = {mkLit(p1 * H + h + 1, true), mkLit(p2 * H + h + 1, true)};
                solver_add_clause(su, pair, 2);
            }
        }
    }
    assert(solver_solve_parallel(su, 4) == FALSE);
    solver_free(su);
}

static void test_symmetry_breaking_php_and_sat(void) {
    /* 1. PHP(15, 14) UNSAT: exponential for resolution, solved near-instantly via
     *    BreakID-style exact row-interchange lex-leader symmetry breaking! */
    SolverOpts opts = default_opts();
    opts.sym = true;
    Solver *su = solver_new_with_opts(&opts);
    const uint32_t H = 14, P = 15;
    for (uint32_t i = 0; i < P * H; ++i) solver_new_var(su);
    for (uint32_t p = 0; p < P; ++p) {
        Lit alo[14];
        for (uint32_t h = 0; h < H; ++h) alo[h] = mkLit(p * H + h + 1, false);
        solver_add_clause(su, alo, H);
    }
    for (uint32_t h = 0; h < H; ++h) {
        for (uint32_t p1 = 0; p1 < P; ++p1) {
            for (uint32_t p2 = p1 + 1; p2 < P; ++p2) {
                Lit pair[2] = {mkLit(p1 * H + h + 1, true), mkLit(p2 * H + h + 1, true)};
                solver_add_clause(su, pair, 2);
            }
        }
    }
    assert(solver_solve(su) == FALSE);
    assert(su->stats.conflicts < 500);
    solver_free(su);

    /* 2. PHP(10, 10) SAT: symmetric matrix must remain SAT with a 100% valid model! */
    Solver *ss = solver_new_with_opts(&opts);
    const uint32_t N = 10;
    for (uint32_t i = 0; i < N * N; ++i) solver_new_var(ss);
    for (uint32_t p = 0; p < N; ++p) {
        Lit alo[10];
        for (uint32_t h = 0; h < N; ++h) alo[h] = mkLit(p * N + h + 1, false);
        solver_add_clause(ss, alo, N);
    }
    for (uint32_t h = 0; h < N; ++h) {
        for (uint32_t p1 = 0; p1 < N; ++p1) {
            for (uint32_t p2 = p1 + 1; p2 < N; ++p2) {
                Lit pair[2] = {mkLit(p1 * N + h + 1, true), mkLit(p2 * N + h + 1, true)};
                solver_add_clause(ss, pair, 2);
            }
        }
    }
    assert(solver_solve(ss) == TRUE);
    assert(solver_check_model(ss));
    solver_free(ss);

    /* 3. Regression: clauses containing a level-0 FALSE literal remain active and
     *    must NOT be skipped during symmetry automorphism verification!
     *    Here u=FALSE reduces (u | a | ~b) to (a | ~b). Combined with (a | b) and
     *    (~a | ~b), the unique model is a=TRUE, b=FALSE. Skipping (u | a | ~b) would
     *    falsely think a<->b is a symmetry and emit (~a | b), turning SAT into UNSAT! */
    Solver *sr = solver_new_with_opts(&opts);
    Var u = solver_new_var(sr);
    Var a = solver_new_var(sr);
    Var b = solver_new_var(sr);
    Var pad1 = solver_new_var(sr);
    Var pad2 = solver_new_var(sr);
    Lit lu = mkLit(u, true);
    solver_add_clause(sr, &lu, 1); /* u = FALSE at level 0 */
    Lit asym[3] = {mkLit(u, false), mkLit(a, false), mkLit(b, true)};
    solver_add_clause(sr, asym, 3); /* reduces to (a | ~b) */
    Lit neq1[2] = {mkLit(a, false), mkLit(b, false)};
    Lit neq2[2] = {mkLit(a, true), mkLit(b, true)};
    solver_add_clause(sr, neq1, 2);
    solver_add_clause(sr, neq2, 2);
    /* Pad with symmetric clauses so live_cnt >= 8 triggers solver_break_symmetries. */
    for (int k = 0; k < 2; ++k) {
        Lit p1[3] = {mkLit(a, k != 0), mkLit(b, k != 0), mkLit(pad1, false)};
        Lit p2[3] = {mkLit(a, k != 0), mkLit(b, k != 0), mkLit(pad2, false)};
        Lit p3[3] = {mkLit(a, k != 0), mkLit(b, k != 0), mkLit(pad1, true)};
        solver_add_clause(sr, p1, 3);
        solver_add_clause(sr, p2, 3);
        solver_add_clause(sr, p3, 3);
    }
    assert(solver_solve(sr) == TRUE);
    assert(solver_check_model(sr));
    assert(solver_model_value(sr, a) == TRUE && solver_model_value(sr, b) == FALSE);
    solver_free(sr);
}

static void test_frozen_variable_inprocessing(void) {
    /* Build an AND-gate chain with frozen interface variables (a, b, out) and
     * unfrozen internal Tseitin helpers (t1, t2), run inprocessing, and verify:
     *  - frozen vars are never eliminated or substituted away;
     *  - unfrozen internal helpers ARE eliminated by BVE;
     *  - incremental assumptions on frozen vars produce exact SAT/UNSAT answers. */
    SolverOpts opts = default_opts();
    opts.inprocess = true;
    Solver *s = solver_new_with_opts(&opts);
    Var a = solver_new_var(s);
    Var b = solver_new_var(s);
    Var t1 = solver_new_var(s);
    Var t2 = solver_new_var(s);
    Var out = solver_new_var(s);

    /* t1 <-> (a & b) */
    Lit g1[3] = {mkLit(t1, false), mkLit(a, true), mkLit(b, true)};
    Lit g2[2] = {mkLit(t1, true), mkLit(a, false)};
    Lit g3[2] = {mkLit(t1, true), mkLit(b, false)};
    solver_add_clause(s, g1, 3);
    solver_add_clause(s, g2, 2);
    solver_add_clause(s, g3, 2);
    /* t2 <-> t1 */
    Lit e1[2] = {mkLit(t1, true), mkLit(t2, false)};
    Lit e2[2] = {mkLit(t2, true), mkLit(t1, false)};
    solver_add_clause(s, e1, 2);
    solver_add_clause(s, e2, 2);
    /* out <-> t2 */
    Lit e3[2] = {mkLit(t2, true), mkLit(out, false)};
    Lit e4[2] = {mkLit(out, true), mkLit(t2, false)};
    solver_add_clause(s, e3, 2);
    solver_add_clause(s, e4, 2);

    solver_freeze(s, a);
    solver_freeze(s, b);
    solver_freeze(s, out);
    assert(solver_is_frozen(s, a) && solver_is_frozen(s, b) && solver_is_frozen(s, out));
    assert(!solver_is_frozen(s, t1) && !solver_is_frozen(s, t2));

    assert(solver_inprocess(s, true));
    /* Frozen interface vars must remain live, while internal helpers get simplified away! */
    assert(!elim_is_eliminated(s, a));
    assert(!elim_is_eliminated(s, b));
    assert(!elim_is_eliminated(s, out));
    assert(elim_is_eliminated(s, t1) || elim_is_eliminated(s, t2));

    /* Assumptions on frozen vars: (a=T, b=T, out=F) -> UNSAT; (a=T, b=T, out=T) -> SAT. */
    Lit bad[3] = {mkLit(a, false), mkLit(b, false), mkLit(out, true)};
    assert(solver_solve_with_assumptions(s, bad, 3) == FALSE);
    /* Verify frozen status persists across incremental rebuilds! */
    assert(solver_is_frozen(s, a) && solver_is_frozen(s, b) && solver_is_frozen(s, out));
    Lit good[3] = {mkLit(a, false), mkLit(b, false), mkLit(out, false)};
    assert(solver_solve_with_assumptions(s, good, 3) == TRUE);
    assert(solver_check_model(s));

    solver_melt(s, a);
    assert(!solver_is_frozen(s, a));
    solver_free(s);
}

static void add_maj3_gate(Solver *s, Lit out, Lit a, Lit b, Lit c) {
    Lit c1[3] = {neg(out), a, b}, c2[3] = {out, neg(a), neg(b)};
    Lit c3[3] = {neg(out), a, c}, c4[3] = {out, neg(a), neg(c)};
    Lit c5[3] = {neg(out), b, c}, c6[3] = {out, neg(b), neg(c)};
    solver_add_clause(s, c1, 3); solver_add_clause(s, c2, 3);
    solver_add_clause(s, c3, 3); solver_add_clause(s, c4, 3);
    solver_add_clause(s, c5, 3); solver_add_clause(s, c6, 3);
}

static void add_xor3_gate(Solver *s, Lit out, Lit a, Lit b, Lit c) {
    Lit lits[4] = {out, a, b, c};
    for (unsigned m = 0; m < 16u; ++m) {
        unsigned pop = ((m >> 0) & 1u) + ((m >> 1) & 1u) + ((m >> 2) & 1u) + ((m >> 3) & 1u);
        if ((pop & 1u) == 0u) continue;
        Lit cl[4];
        for (unsigned j = 0; j < 4u; ++j) cl[j] = ((m >> j) & 1u) ? neg(lits[j]) : lits[j];
        solver_add_clause(s, cl, 4);
    }
}

static void test_full_adder_maj3_xor3_congruence(void) {
    /* Two parallel 2-stage full-adder chains (a, b, cin -> sum, carry -> sum2, carry2)
     * with miter asserting carry2_1 != carry2_2 (must collapse to UNSAT in 0 conflicts!). */
    SolverOpts opts = default_opts();
    opts.inprocess = true;
    Solver *s = solver_new_with_opts(&opts);
    Var a = solver_new_var(s), b = solver_new_var(s), cin = solver_new_var(s), d = solver_new_var(s);
    Var sum1 = solver_new_var(s), car1 = solver_new_var(s);
    Var sum2 = solver_new_var(s), car2 = solver_new_var(s);
    Var out1 = solver_new_var(s), out2 = solver_new_var(s);

    /* Stage 1: two identical full adders on (a, b, cin). */
    add_xor3_gate(s, mkLit(sum1, false), mkLit(a, false), mkLit(b, false), mkLit(cin, false));
    add_maj3_gate(s, mkLit(car1, false), mkLit(a, false), mkLit(b, false), mkLit(cin, false));
    add_xor3_gate(s, mkLit(sum2, false), mkLit(a, false), mkLit(b, false), mkLit(cin, false));
    add_maj3_gate(s, mkLit(car2, false), mkLit(a, false), mkLit(b, false), mkLit(cin, false));

    /* Stage 2: downstream MAJ3 consuming Stage 1 outputs -> requires iterative fixpoint! */
    add_maj3_gate(s, mkLit(out1, false), mkLit(sum1, false), mkLit(car1, false), mkLit(d, false));
    add_maj3_gate(s, mkLit(out2, false), mkLit(sum2, false), mkLit(car2, false), mkLit(d, false));

    /* Miter: out1 != out2 (UNSAT!). */
    Lit m1[2] = {mkLit(out1, false), mkLit(out2, false)};
    Lit m2[2] = {mkLit(out1, true), mkLit(out2, true)};
    solver_add_clause(s, m1, 2);
    solver_add_clause(s, m2, 2);

    assert(solver_solve(s) == FALSE);
    assert(s->stats.conflicts == 0);
    solver_free(s);
}

static void test_semantic_definition_bve(void) {
    /* Non-syntactic semantic definition for v:
     *   G_pos = {(v | a), (v | b)}
     *   G_neg = {(~v | ~a | c), (~v | ~b | ~c)}
     * Note G_pos' U G_neg' = {a, b, (~a|c), (~b|~c)} is UNSAT, yet G_pos x G_neg
     * contains the non-tautological resolvent (a | ~b | ~c) and (b | ~a | c)!
     * Add extra non-gate clauses R_pos and R_neg so plain BVE exceeds grow_bound,
     * forcing semantic definition extraction; verify exact SAT model under all
     * critical assignments (including a=0, b=1, c=1 where v would conflict if GxG were dropped!). */
    for (int mode = 0; mode < 2; ++mode) {
        SolverOpts opts = default_opts();
        opts.inprocess = true;
        Solver *s = solver_new_with_opts(&opts);
        Var v = solver_new_var(s);
        Var a = solver_new_var(s), b = solver_new_var(s), c = solver_new_var(s);
        Var r1 = solver_new_var(s), r2 = solver_new_var(s), r3 = solver_new_var(s);
        Var s1 = solver_new_var(s), s2 = solver_new_var(s), s3 = solver_new_var(s);

        Lit gp1[2] = {mkLit(v, false), mkLit(a, false)};
        Lit gp2[2] = {mkLit(v, false), mkLit(b, false)};
        Lit gn1[3] = {mkLit(v, true), mkLit(a, true), mkLit(c, false)};
        Lit gn2[3] = {mkLit(v, true), mkLit(b, true), mkLit(c, true)};
        solver_add_clause(s, gp1, 2);
        solver_add_clause(s, gp2, 2);
        solver_add_clause(s, gn1, 3);
        solver_add_clause(s, gn2, 3);

        Lit rp1[2] = {mkLit(v, false), mkLit(r1, false)};
        Lit rp2[2] = {mkLit(v, false), mkLit(r2, false)};
        Lit rp3[2] = {mkLit(v, false), mkLit(r3, false)};
        Lit rn1[2] = {mkLit(v, true), mkLit(s1, false)};
        Lit rn2[2] = {mkLit(v, true), mkLit(s2, false)};
        Lit rn3[2] = {mkLit(v, true), mkLit(s3, false)};
        solver_add_clause(s, rp1, 2);
        solver_add_clause(s, rp2, 2);
        solver_add_clause(s, rp3, 2);
        solver_add_clause(s, rn1, 2);
        solver_add_clause(s, rn2, 2);
        solver_add_clause(s, rn3, 2);

        if (mode == 0) {
            /* Force a=0, b=1, c=1: MUST be UNSAT because gp1 forces v=1 and gn2 forces v=0! */
            Lit u1 = mkLit(a, true), u2 = mkLit(b, false), u3 = mkLit(c, false);
            solver_add_clause(s, &u1, 1);
            solver_add_clause(s, &u2, 1);
            solver_add_clause(s, &u3, 1);
            assert(solver_solve(s) == FALSE);
        } else {
            /* Force a=0, b=0, c=1: SAT with v=1 and s1=s2=s3=1! */
            Lit u1 = mkLit(a, true), u2 = mkLit(b, true), u3 = mkLit(c, false);
            solver_add_clause(s, &u1, 1);
            solver_add_clause(s, &u2, 1);
            solver_add_clause(s, &u3, 1);
            assert(solver_solve(s) == TRUE);
            assert(solver_check_model(s));
            assert(solver_model_value(s, v) == TRUE);
        }
        solver_free(s);
    }
}

static void test_symmetry_breaking_drat_proof(void) {
    /* PHP(7, 6) has 7*6 = 42 vars and 7 + 6*21 = 133 clauses (>= 64),
     * so --proof mode triggers O(n) single-pair peeling + failed-literal RUP
     * and solves in 0 conflicts with 0 residual auxiliary variables! */
    for (int binary = 0; binary <= 1; ++binary) {
        SolverOpts opts = default_opts();
        opts.inprocess = true;
        opts.sym = true;
        opts.binary_proof = (binary != 0);
        Solver *s = solver_new_with_opts(&opts);
        assert(s != NULL);
        FILE *pf = tmpfile();
        assert(pf != NULL);
        s->proof_file = pf;

        const uint32_t H = 6, P = 7;
        for (uint32_t i = 0; i < P * H; ++i) solver_new_var(s);
        for (uint32_t p = 0; p < P; ++p) {
            Lit alo[6];
            for (uint32_t h = 0; h < H; ++h) alo[h] = mkLit(p * H + h + 1, false);
            solver_add_clause(s, alo, H);
        }
        for (uint32_t h = 0; h < H; ++h) {
            for (uint32_t p1 = 0; p1 < P; ++p1) {
                for (uint32_t p2 = p1 + 1; p2 < P; ++p2) {
                    Lit pair[2] = {mkLit(p1 * H + h + 1, true), mkLit(p2 * H + h + 1, true)};
                    solver_add_clause(s, pair, 2);
                }
            }
        }
        lbool r = solver_solve(s);
        assert(r == FALSE);
        assert(!s->error);
        assert(s->stats.conflicts == 0);
        assert(s->num_vars == P * H); /* zero residual aux vars */
        fflush(pf);
        assert(ftell(pf) > 0);
        s->proof_file = NULL;
        fclose(pf);
        solver_free(s);
    }
}

static void test_gauss_rref_drat_proof(void) {
    /* 16-variable contradictory Tseitin-chained XOR system (>= 64 clauses) in --proof mode
     * (both ASCII and binary DRAT): root GF(2) RREF emits a linear XOR-chain DRAT proof
     * and solves UNSAT in 0 conflicts with 0 residual auxiliary variables! */
    for (int binary = 0; binary <= 1; ++binary) {
        SolverOpts opts = default_opts();
        opts.inprocess = true;
        opts.binary_proof = (binary != 0);
        Solver *s = solver_new_with_opts(&opts);
        assert(s != NULL);
        FILE *pf = tmpfile();
        assert(pf != NULL);
        s->proof_file = pf;

        const uint32_t N = 16;
        Var base[16];
        for (uint32_t i = 0; i < N; ++i) base[i] = solver_new_var(s);
        add_chained_xor(s, base, N, 0);
        add_chained_xor(s, base, N, 1);
        uint32_t orig_nv = s->num_vars;
        assert(s->num_clauses >= 64u);

        lbool r = solver_solve(s);
        assert(r == FALSE);
        assert(!s->error);
        assert(s->stats.conflicts == 0);
        assert(s->num_vars == orig_nv); /* zero residual aux vars */
        fflush(pf);
        assert(ftell(pf) > 0);
        s->proof_file = NULL;
        fclose(pf);
        solver_free(s);
    }
}

int main(void) {
    test_gauss_mixed_sat_and_unsat();
    test_gauss_chain_stitching_and_subsumed_xor();
    test_gauss_rref_drat_proof();
    test_nary_congruence_and_miter_sweep();
    test_full_adder_maj3_xor3_congruence();
    test_semantic_definition_bve();
    test_amo_cliques_sat_and_unsat();
    test_sbva_arbitrary_length_sat_and_unsat();
    test_parallel_portfolio_sat_and_unsat();
    test_symmetry_breaking_php_and_sat();
    test_symmetry_breaking_drat_proof();
    test_frozen_variable_inprocessing();
    test_lit_values_external_mutation_safety();
    printf("PASS: test_gauss_and_amo\n");
    return 0;
}
