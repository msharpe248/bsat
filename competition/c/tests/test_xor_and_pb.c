/*********************************************************************
 * Unit Tests for XOR-CNF, Cardinality Encoders, GTE/Adder PB, & OPB
 *********************************************************************/

#include "../include/solver.h"
#include "../include/dimacs.h"
#include "../include/pb_enc.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t lcg_next(uint32_t *st) {
    *st = (*st) * 1664525u + 1013904223u;
    return *st;
}

static void test_xor_truth_tables(void) {
    /* Verify pb_add_xor_clause against exhaustive truth tables for k = 1..7 (including Tseitin splits). */
    for (uint32_t k = 1; k <= 7; ++k) {
        for (int rhs = 0; rhs <= 1; ++rhs) {
            Solver *s = solver_new();
            assert(s);
            for (uint32_t v = 1; v <= k; ++v) assert(solver_new_var(s) == v);
            Lit lits[8];
            for (uint32_t i = 0; i < k; ++i) {
                /* Mix positive and negative literals. */
                lits[i] = mkLit(i + 1, (i % 3) == 1);
            }
            assert(pb_add_xor_clause(s, lits, k, rhs != 0));
            /* Check all 2^k assignments via assumptions. */
            uint32_t total = 1u << k;
            for (uint32_t mask = 0; mask < total; ++mask) {
                Lit assumps[8];
                uint32_t parity = 0;
                for (uint32_t i = 0; i < k; ++i) {
                    bool val = ((mask >> i) & 1u) != 0;
                    assumps[i] = mkLit(i + 1, !val);
                    bool lit_true = sign(lits[i]) ? !val : val;
                    parity ^= lit_true ? 1u : 0u;
                }
                lbool r = solver_solve_with_assumptions(s, assumps, k);
                assert(r == (parity == (uint32_t)rhs ? TRUE : FALSE));
            }
            solver_free(s);
        }
    }

    /* Duplicate cancellation: x1 XOR x1 == 1 is UNSAT, x1 XOR x1 == 0 is SAT. */
    {
        Solver *s = solver_new();
        assert(solver_new_var(s) == 1);
        Lit dup[2] = {mkLit(1, false), mkLit(1, false)};
        assert(pb_add_xor_clause(s, dup, 2, true));
        assert(solver_solve(s) == FALSE);
        solver_free(s);
    }
    {
        Solver *s = solver_new();
        assert(solver_new_var(s) == 1);
        Lit dup[2] = {mkLit(1, false), mkLit(1, true)}; /* x XOR ~x == 1 (tautology) */
        assert(pb_add_xor_clause(s, dup, 2, true));
        assert(solver_solve(s) == TRUE);
        solver_free(s);
    }

    /* Native XOR-CNF DIMACS parser with long 12-var XOR chain -> gauss.c solves instantly! */
    {
        const char *xor_unsat =
            "c 12-variable inconsistent XOR system\n"
            "p cnf 12 3\n"
            "x 1 2 3 4 5 6 7 8 9 10 11 12 0\n"
            "x 1 2 3 4 5 6 0\n"
            "x 7 8 9 10 11 12 0\n";
        /* eq1: sum(1..12) = 1; eq2: sum(1..6) = 1; eq3: sum(7..12) = 1 => 1+1 = 0 != 1 -> UNSAT! */
        Solver *s = solver_new();
        uint32_t hv = 0;
        assert(dimacs_parse_string_xor(s, xor_unsat, &hv) == DIMACS_OK);
        assert(hv == 12);
        assert(solver_solve(s) == FALSE);
        solver_free(s);
    }
    {
        const char *xor_sat =
            "p cnf 12 3\n"
            "x 1 2 3 4 5 6 7 8 9 10 11 12 0\n"
            "x -1 2 3 4 5 6 0\n"
            "x 7 8 9 10 11 12 0\n";
        /* eq2 has -1 => sum(1..6) = 0, eq3: sum(7..12) = 1 => sum(1..12) = 1 -> SAT! */
        Solver *s = solver_new();
        uint32_t hv = 0;
        assert(dimacs_parse_string_xor(s, xor_sat, &hv) == DIMACS_OK);
        assert(hv == 12);
        assert(solver_solve(s) == TRUE);
        assert(solver_check_model(s));
        /* Verify all 3 XOR equations directly on the model. */
        uint32_t p1 = 0, p2 = 0, p3 = 0;
        for (Var v = 1; v <= 12; ++v) {
            uint32_t b = (solver_model_value(s, v) == TRUE) ? 1u : 0u;
            p1 ^= b;
            if (v <= 6) p2 ^= (v == 1 ? (1u ^ b) : b);
            else p3 ^= b;
        }
        assert(p1 == 1 && p2 == 1 && p3 == 1);
        solver_free(s);
    }
    /* Strict parser must still reject 'x'. */
    {
        Solver *s = solver_new();
        assert(dimacs_parse_string(s, "p cnf 2 1\nx 1 2 0\n") == DIMACS_ERROR_FORMAT);
        solver_free(s);
    }
}

static void test_sinz_and_totalizer(void) {
    /* Exhaustive truth-table test for Sinz AtMost-k across n = 1..8, k = 0..n. */
    for (uint32_t n = 1; n <= 8; ++n) {
        for (uint32_t k = 0; k <= n; ++k) {
            Solver *s = solver_new();
            for (uint32_t v = 1; v <= n; ++v) assert(solver_new_var(s) == v);
            Lit lits[8];
            for (uint32_t i = 0; i < n; ++i) lits[i] = mkLit(i + 1, (i & 1u) != 0);
            assert(pb_add_atmost_sinz(s, lits, n, k));
            uint32_t total = 1u << n;
            for (uint32_t mask = 0; mask < total; ++mask) {
                Lit assumps[8];
                uint32_t cnt = 0;
                for (uint32_t i = 0; i < n; ++i) {
                    bool val = ((mask >> i) & 1u) != 0;
                    assumps[i] = mkLit(i + 1, !val);
                    bool lt = sign(lits[i]) ? !val : val;
                    if (lt) cnt++;
                }
                lbool r = solver_solve_with_assumptions(s, assumps, n);
                assert(r == (cnt <= k ? TRUE : FALSE));
            }
            solver_free(s);
        }
    }

    /* Incremental Totalizer bound tightening under reuse_learnts = true. */
    for (uint32_t n = 1; n <= 8; ++n) {
        SolverOpts o = default_opts();
        o.reuse_learnts = true;
        o.inprocess = false;
        Solver *s = solver_new_with_opts(&o);
        for (uint32_t v = 1; v <= n; ++v) assert(solver_new_var(s) == v);
        Lit lits[8];
        for (uint32_t i = 0; i < n; ++i) lits[i] = mkLit(i + 1, false);
        PbTotalizer *tot = pb_totalizer_create(s, lits, n, 0);
        assert(tot);
        for (uint32_t ub = 0; ub < n; ++ub) {
            assert(pb_totalizer_increase_bound(tot, s, ub));
            Lit bound_lit = neg(pb_totalizer_out(tot, ub));
            assert(bound_lit != LIT_UNDEF);
            uint32_t total = 1u << n;
            for (uint32_t mask = 0; mask < total; ++mask) {
                Lit assumps[9];
                uint32_t cnt = 0;
                for (uint32_t i = 0; i < n; ++i) {
                    bool val = ((mask >> i) & 1u) != 0;
                    assumps[i] = mkLit(i + 1, !val);
                    if (val) cnt++;
                }
                assumps[n] = bound_lit;
                lbool r = solver_solve_with_assumptions(s, assumps, n + 1);
                assert(r == (cnt <= ub ? TRUE : FALSE));
            }
        }
        pb_totalizer_free(tot);
        solver_free(s);
    }
}

static void test_weighted_pb_gte_and_adder(void) {
    uint32_t rng = 0x12345678u;
    /* 20 random small-weight instances (GTE) + 10 large-weight instances (forced binary adder). */
    for (int trial = 0; trial < 30; ++trial) {
        bool large = (trial >= 20);
        uint32_t n = large ? 12u : (3u + (lcg_next(&rng) % 5u));
        uint32_t nvars = large ? 12u : n;
        Lit lits[16];
        uint64_t weights[16];
        uint64_t sum_w = 0;
        for (uint32_t i = 0; i < n; ++i) {
            Var v = large ? (i + 1) : (1u + (lcg_next(&rng) % nvars));
            bool neg_f = (lcg_next(&rng) & 1u) != 0;
            lits[i] = mkLit(v, neg_f);
            weights[i] = large
                ? (1000000000ULL + ((uint64_t)lcg_next(&rng) % 999999999ULL))
                : (1ULL + (lcg_next(&rng) % 11ULL));
            sum_w += weights[i];
        }
        uint64_t rhs = (lcg_next(&rng) % 100u) * (sum_w / 100ULL + 1ULL);
        if (trial == 0) rhs = 0;

        Solver *s = solver_new();
        for (uint32_t v = 1; v <= nvars; ++v) assert(solver_new_var(s) == v);
        assert(pb_add_pb_le(s, lits, weights, n, rhs));

        if (!large) {
            /* Exhaustive truth-table verification for every 2^nvars assignment! */
            uint32_t total = 1u << nvars;
            for (uint32_t mask = 0; mask < total; ++mask) {
                Lit assumps[8];
                uint64_t lhs = 0;
                for (uint32_t v = 1; v <= nvars; ++v) {
                    bool val = ((mask >> (v - 1)) & 1u) != 0;
                    assumps[v - 1] = mkLit(v, !val);
                }
                for (uint32_t i = 0; i < n; ++i) {
                    bool val = ((mask >> (var(lits[i]) - 1)) & 1u) != 0;
                    bool lt = sign(lits[i]) ? !val : val;
                    if (lt) lhs += weights[i];
                }
                lbool r = solver_solve_with_assumptions(s, assumps, nvars);
                assert(r == (lhs <= rhs ? TRUE : FALSE));
            }
        } else {
            /* Sample 40 assignments including boundary cases on 12-var large-weight PB. */
            for (int q = 0; q < 40; ++q) {
                uint32_t mask = lcg_next(&rng) & ((1u << nvars) - 1u);
                Lit assumps[12];
                uint64_t lhs = 0;
                for (uint32_t v = 1; v <= nvars; ++v) {
                    bool val = ((mask >> (v - 1)) & 1u) != 0;
                    assumps[v - 1] = mkLit(v, !val);
                }
                for (uint32_t i = 0; i < n; ++i) {
                    bool val = ((mask >> (var(lits[i]) - 1)) & 1u) != 0;
                    bool lt = sign(lits[i]) ? !val : val;
                    if (lt) lhs += weights[i];
                }
                lbool r = solver_solve_with_assumptions(s, assumps, nvars);
                assert(r == (lhs <= rhs ? TRUE : FALSE));
            }
        }
        solver_free(s);
    }
}

static void test_opb_parser_and_optimization(void) {
    /* 1. Handcrafted OPB optimization with negative coefficients, ~x, >=, <=, =, comments. */
    const char *opb =
        "* #variable= 5 #constraint= 3\n"
        "min: +5 x1 -3 x2 +4 ~x3 +2 x4 +6 x5 ;\n"
        "+2 x1 +3 x2 +1 x3 +2 x4 >= 4 ;\n"
        "+1 x1 +1 x2 +1 x5 <= 2 ;\n"
        "+1 x3 +1 x4 = 1 ;\n";
    PbInstance inst;
    assert(pb_parse_opb_string(&inst, opb));
    assert(inst.num_vars == 5 && inst.num_constraints == 3 && inst.has_objective);

    /* Brute-force exact optimum over all 32 assignments. */
    bool bf_sat = false;
    int64_t bf_best = 0;
    for (uint32_t mask = 0; mask < 32; ++mask) {
        lbool m[6] = {UNDEF};
        for (uint32_t v = 1; v <= 5; ++v) m[v] = ((mask >> (v - 1)) & 1u) ? TRUE : FALSE;
        int64_t obj = 0;
        if (pb_verify_model(&inst, m, &obj)) {
            if (!bf_sat || obj < bf_best) { bf_sat = true; bf_best = obj; }
        }
    }
    assert(bf_sat);

    SolverOpts o = default_opts();
    o.quiet = true;
    PbSolveResult res = pb_solve_opb(&inst, &o);
    assert(res.status == TRUE && res.optimum);
    assert(res.best_obj == bf_best);
    int64_t chk_obj = 0;
    assert(pb_verify_model(&inst, res.best_model, &chk_obj) && chk_obj == bf_best);
    pb_solve_result_free(&res);
    pb_instance_free(&inst);

    /* 2. 20 randomized 7-variable OPB optimization instances vs brute-force oracle. */
    uint32_t rng = 0xCAFEBABEu;
    char buf[4096];
    for (int t = 0; t < 20; ++t) {
        size_t pos = 0;
        pos += (size_t)snprintf(buf + pos, sizeof buf - pos, "min:");
        for (int v = 1; v <= 7; ++v) {
            int c = (int)(lcg_next(&rng) % 13u) - 6;
            if (c == 0) c = 1;
            pos += (size_t)snprintf(buf + pos, sizeof buf - pos, " %+d %sx%d",
                                    c, (lcg_next(&rng) & 1u) ? "~" : "", v);
        }
        pos += (size_t)snprintf(buf + pos, sizeof buf - pos, " ;\n");
        int nc = 3 + (int)(lcg_next(&rng) % 4u);
        for (int ci = 0; ci < nc; ++ci) {
            for (int v = 1; v <= 7; ++v) {
                if ((lcg_next(&rng) % 3u) == 0) continue;
                int c = (int)(lcg_next(&rng) % 9u) - 4;
                if (c == 0) c = 2;
                pos += (size_t)snprintf(buf + pos, sizeof buf - pos, "%+d %sx%d ",
                                        c, (lcg_next(&rng) & 1u) ? "~" : "", v);
            }
            const char *ops[3] = {">=", "<=", "="};
            const char *op = ops[lcg_next(&rng) % (ci == 0 ? 2u : 3u)];
            int rhs = (int)(lcg_next(&rng) % 11u) - 3;
            pos += (size_t)snprintf(buf + pos, sizeof buf - pos, "%s %d ;\n", op, rhs);
        }

        PbInstance rinst;
        assert(pb_parse_opb_string(&rinst, buf));
        bool sat = false;
        int64_t best = 0;
        for (uint32_t mask = 0; mask < (1u << rinst.num_vars); ++mask) {
            lbool m[8] = {UNDEF};
            for (uint32_t v = 1; v <= rinst.num_vars; ++v)
                m[v] = ((mask >> (v - 1)) & 1u) ? TRUE : FALSE;
            int64_t obj = 0;
            if (pb_verify_model(&rinst, m, &obj)) {
                if (!sat || obj < best) { sat = true; best = obj; }
            }
        }
        PbSolveResult rres = pb_solve_opb(&rinst, &o);
        if (!sat) {
            assert(rres.status == FALSE);
        } else {
            assert(rres.status == TRUE && rres.optimum && rres.best_obj == best);
            int64_t vobj = 0;
            assert(pb_verify_model(&rinst, rres.best_model, &vobj) && vobj == best);
        }
        pb_solve_result_free(&rres);
        pb_instance_free(&rinst);
    }

    /* 3. Malformed OPB rejection. */
    {
        PbInstance bad;
        assert(!pb_parse_opb_string(&bad, "min: +1 x1\n")); /* missing ; */
        assert(!pb_parse_opb_string(&bad, "+1 z1 >= 1 ;\n")); /* bad var prefix */
        assert(!pb_parse_opb_string(&bad, "+1 x0 >= 1 ;\n")); /* var 0 invalid */
    }
}

int main(void) {
    test_xor_truth_tables();
    test_sinz_and_totalizer();
    test_weighted_pb_gte_and_adder();
    test_opb_parser_and_optimization();
    puts("PASS: XOR-CNF, Sinz, incremental Totalizer, GTE/Adder PB, and OPB optimization");
    return 0;
}
