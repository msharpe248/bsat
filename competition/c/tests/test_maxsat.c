/*********************************************************************
 * Unit Tests for Core-Guided Weighted Partial MaxSAT (RC2 / OLL)
 *********************************************************************/

#include "../include/maxsat.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t lcg(uint32_t *s) {
    *s = (*s) * 1664525u + 1013904223u;
    return *s;
}

static bool brute_force_maxsat(const MaxSatInstance *inst, uint64_t *out_opt) {
    assert(inst->num_vars <= 12);
    bool found = false;
    uint64_t best = UINT64_MAX;
    uint32_t total = 1u << inst->num_vars;
    lbool m[13] = {UNDEF};
    for (uint32_t mask = 0; mask < total; ++mask) {
        for (uint32_t v = 1; v <= inst->num_vars; ++v) {
            m[v] = ((mask >> (v - 1)) & 1u) ? TRUE : FALSE;
        }
        uint64_t cost = 0;
        if (maxsat_verify_model(inst, m, &cost)) {
            if (!found || cost < best) { found = true; best = cost; }
        }
    }
    if (out_opt) *out_opt = best;
    return found;
}

static void test_wcnf_formats_and_edge_cases(void) {
    SolverOpts o = default_opts();
    o.quiet = true;

    /* 1. Classic WCNF with top=100 (hard clauses + weighted softs). */
    {
        const char *wcnf =
            "c Classic weighted partial MaxSAT\n"
            "p wcnf 3 6 100\n"
            "100 -1 -2 0\n"
            "100 -2 -3 0\n"
            "100 -1 -3 0\n"
            "5 1 0\n"
            "7 2 0\n"
            "3 3 0\n";
        /* At most one of {1,2,3} can be true -> pick 2 (weight 7), falsify 1(5) and 3(3) => cost 8! */
        MaxSatInstance inst;
        assert(maxsat_parse_string(&inst, wcnf));
        assert(inst.num_vars == 3 && inst.num_hard == 3 && inst.num_soft == 3);
        MaxSatResult r = maxsat_solve(&inst, &o);
        assert(r.status == TRUE && r.optimum && r.cost == 8);
        uint64_t vc = 0;
        assert(maxsat_verify_model(&inst, r.model, &vc) && vc == 8);
        maxsat_result_free(&r);
        maxsat_instance_free(&inst);
    }

    /* 2. Modern MaxSAT Evaluation 2022+ headerless format ("h ..." + "<w> ..."). */
    {
        const char *mse =
            "c MSE 2022+ format\n"
            "h 1 2 0\n"
            "h -1 -2 0\n"
            "10 -1 3 0\n"
            "4 -2 3 0\n"
            "20 -3 0\n";
        /* x1 != x2. If x3=0 (keeps 20): either x1=1 (falsifies -1|3 cost 10) or x2=1 (falsifies -2|3 cost 4) -> opt=4! */
        MaxSatInstance inst;
        assert(maxsat_parse_string(&inst, mse));
        MaxSatResult r = maxsat_solve(&inst, &o);
        assert(r.status == TRUE && r.optimum && r.cost == 4);
        uint64_t vc = 0;
        assert(maxsat_verify_model(&inst, r.model, &vc) && vc == 4);
        maxsat_result_free(&r);
        maxsat_instance_free(&inst);
    }

    /* 3. Unsatisfiable hard clauses -> FALSE. */
    {
        const char *unsat_hard =
            "h 1 0\n"
            "h -1 0\n"
            "5 2 0\n";
        MaxSatInstance inst;
        assert(maxsat_parse_string(&inst, unsat_hard));
        MaxSatResult r = maxsat_solve(&inst, &o);
        assert(r.status == FALSE);
        maxsat_result_free(&r);
        maxsat_instance_free(&inst);
    }

    /* 4. Zero-cost SAT instance + empty soft clause base cost. */
    {
        const char *zero_and_empty =
            "h 1 2 0\n"
            "5 1 0\n"
            "3 2 0\n"
            "11 0\n"; /* empty soft clause => mandatory cost 11 */
        MaxSatInstance inst;
        assert(maxsat_parse_string(&inst, zero_and_empty));
        MaxSatResult r = maxsat_solve(&inst, &o);
        assert(r.status == TRUE && r.optimum && r.cost == 11);
        uint64_t vc = 0;
        assert(maxsat_verify_model(&inst, r.model, &vc) && vc == 11);
        maxsat_result_free(&r);
        maxsat_instance_free(&inst);
    }

    /* 5. Duplicate unit softs + overlapping cores forcing OLL weight splitting across totalizer bounds. */
    {
        MaxSatInstance inst;
        maxsat_instance_init(&inst);
        /* Hard: at most 1 of {1,2,3,4} is true. Soft: each var positive with distinct weights + duplicates! */
        for (Var a = 1; a <= 4; ++a) {
            for (Var b = a + 1; b <= 4; ++b) {
                Lit pair[2] = {mkLit(a, true), mkLit(b, true)};
                assert(maxsat_add_clause(&inst, pair, 2, 0));
            }
        }
        Lit u1 = mkLit(1, false), u2 = mkLit(2, false), u3 = mkLit(3, false), u4 = mkLit(4, false);
        assert(maxsat_add_clause(&inst, &u1, 1, 7));
        assert(maxsat_add_clause(&inst, &u1, 1, 5)); /* duplicate unit => weight 12 */
        assert(maxsat_add_clause(&inst, &u2, 1, 10));
        assert(maxsat_add_clause(&inst, &u3, 1, 8));
        assert(maxsat_add_clause(&inst, &u4, 1, 3));
        /* Also soft clauses on pairs to trigger overlapping multi-literal cores! */
        Lit p12[2] = {mkLit(1, false), mkLit(2, false)};
        Lit p34[2] = {mkLit(3, false), mkLit(4, false)};
        assert(maxsat_add_clause(&inst, p12, 2, 6));
        assert(maxsat_add_clause(&inst, p34, 2, 9));

        uint64_t bf_opt = 0;
        assert(brute_force_maxsat(&inst, &bf_opt));
        MaxSatResult r = maxsat_solve(&inst, &o);
        assert(r.status == TRUE && r.optimum && r.cost == bf_opt);
        uint64_t vc = 0;
        assert(maxsat_verify_model(&inst, r.model, &vc) && vc == bf_opt);
        maxsat_result_free(&r);
        maxsat_instance_free(&inst);
    }
}

static void test_randomized_oracle_maxsat(void) {
    SolverOpts o = default_opts();
    o.quiet = true;
    uint32_t rng = 0xDEADBEEFu;

    /* 60 randomized Weighted Partial MaxSAT instances (8..10 vars, overlapping cores,
     * mixed unit/binary/ternary softs, huge stratified weights, hard clauses) vs brute-force! */
    for (int trial = 0; trial < 60; ++trial) {
        MaxSatInstance inst;
        maxsat_instance_init(&inst);
        uint32_t nv = 7u + (lcg(&rng) % 4u); /* 7..10 vars */
        inst.num_vars = nv;

        /* 0..8 hard clauses */
        uint32_t nh = lcg(&rng) % 9u;
        for (uint32_t i = 0; i < nh; ++i) {
            uint32_t k = 2u + (lcg(&rng) % 2u);
            Lit cl[3];
            for (uint32_t j = 0; j < k; ++j) {
                cl[j] = mkLit(1u + (lcg(&rng) % nv), (lcg(&rng) & 1u) != 0);
            }
            assert(maxsat_add_clause(&inst, cl, k, 0));
        }

        /* 12..28 soft clauses (mix of unit, binary, ternary) with stratified & varied weights */
        uint32_t ns = 12u + (lcg(&rng) % 17u);
        for (uint32_t i = 0; i < ns; ++i) {
            uint32_t k = 1u + (lcg(&rng) % 3u);
            Lit cl[3];
            for (uint32_t j = 0; j < k; ++j) {
                cl[j] = mkLit(1u + (lcg(&rng) % nv), (lcg(&rng) & 1u) != 0);
            }
            uint64_t w;
            if (trial < 15) w = 1; /* Unweighted MaxSAT */
            else if (trial < 35) w = 1ULL + (lcg(&rng) % 15ULL);
            else {
                /* Multi-scale stratified weights (1..1000000) */
                uint64_t scales[4] = {1ULL, 10ULL, 1000ULL, 100000ULL};
                w = scales[lcg(&rng) % 4u] * (1ULL + (lcg(&rng) % 7ULL));
            }
            assert(maxsat_add_clause(&inst, cl, k, w));
        }

        uint64_t bf_opt = 0;
        bool bf_sat = brute_force_maxsat(&inst, &bf_opt);
        MaxSatResult r = maxsat_solve(&inst, &o);
        if (!bf_sat) {
            assert(r.status == FALSE);
        } else {
            assert(r.status == TRUE && r.optimum);
            assert(r.cost == bf_opt);
            uint64_t vc = 0;
            assert(maxsat_verify_model(&inst, r.model, &vc) && vc == bf_opt);
        }
        maxsat_result_free(&r);
        maxsat_instance_free(&inst);
    }
}

/* Pigeonhole MaxSAT: PHP(7,6) with soft at-most-one / pigeon clauses -> exact opt = 1! */
static void test_structured_php_maxsat(void) {
    SolverOpts o = default_opts();
    o.quiet = true;
    MaxSatInstance inst;
    maxsat_instance_init(&inst);
    uint32_t P = 7, H = 6;
    inst.num_vars = P * H;
    /* Hard: each hole holds at most 1 pigeon */
    for (uint32_t h = 0; h < H; ++h) {
        for (uint32_t p1 = 0; p1 < P; ++p1) {
            for (uint32_t p2 = p1 + 1; p2 < P; ++p2) {
                Lit pair[2] = {mkLit(p1 * H + h + 1, true), mkLit(p2 * H + h + 1, true)};
                assert(maxsat_add_clause(&inst, pair, 2, 0));
            }
        }
    }
    /* Soft (weight 10): each pigeon gets at least one hole -> 6 of 7 can succeed, min cost = 10! */
    for (uint32_t p = 0; p < P; ++p) {
        Lit cl[6];
        for (uint32_t h = 0; h < H; ++h) cl[h] = mkLit(p * H + h + 1, false);
        assert(maxsat_add_clause(&inst, cl, H, 10));
    }
    MaxSatResult r = maxsat_solve(&inst, &o);
    assert(r.status == TRUE && r.optimum && r.cost == 10);
    uint64_t vc = 0;
    assert(maxsat_verify_model(&inst, r.model, &vc) && vc == 10);
    maxsat_result_free(&r);
    maxsat_instance_free(&inst);
}

int main(void) {
    test_wcnf_formats_and_edge_cases();
    test_randomized_oracle_maxsat();
    test_structured_php_maxsat();
    puts("PASS: RC2/OLL Weighted Partial MaxSAT (60 randomized oracle + WCNF + PHP)");
    return 0;
}
