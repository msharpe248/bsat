/*********************************************************************
 * Unit Tests for AllSAT Enumeration, Exact Backbone & MUS Extraction
 *********************************************************************/

#include "../include/queries.h"
#include "../include/dimacs.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t lcg(uint32_t *st) {
    *st = (*st) * 1664525u + 1013904223u;
    return *st;
}

typedef struct {
    uint64_t seen_mask[256]; /* bitset for <= 8 vars -> up to 256 assignments */
    uint32_t nvars;
    const Var *proj;
    uint32_t nproj;
} EnumCheck;

static bool check_cb(void *ctx, const lbool *model, uint32_t num_vars, uint64_t idx) {
    EnumCheck *ec = (EnumCheck *)ctx;
    (void)idx;
    uint32_t code = 0;
    if (ec->proj && ec->nproj > 0) {
        for (uint32_t i = 0; i < ec->nproj; ++i) {
            assert(ec->proj[i] >= 1 && ec->proj[i] <= num_vars);
            if (model[ec->proj[i]] == TRUE) code |= (1u << i);
        }
    } else {
        for (uint32_t v = 1; v <= num_vars; ++v) {
            if (model[v] == TRUE) code |= (1u << (v - 1));
        }
    }
    assert(code < 256u);
    assert(ec->seen_mask[code] == 0); /* No duplicate (projected) models! */
    ec->seen_mask[code] = 1;
    return true;
}

static bool eval_clause_mask(const Lit *lits, uint32_t sz, uint32_t mask) {
    for (uint32_t i = 0; i < sz; ++i) {
        bool v = ((mask >> (var(lits[i]) - 1)) & 1u) != 0;
        if (sign(lits[i]) ? !v : v) return true;
    }
    return false;
}

static void test_allsat_and_backbone_oracle(void) {
    uint32_t rng = 0x13579BDFu;
    SolverOpts o = default_opts();
    o.quiet = true;

    for (int trial = 0; trial < 35; ++trial) {
        uint32_t nv = 5u + (lcg(&rng) % 3u); /* 5..7 vars */
        uint32_t nc = 4u + (lcg(&rng) % 14u);
        Lit cls[20][3];
        uint32_t csz[20];

        Solver *s = solver_new();
        for (Var v = 1; v <= nv; ++v) assert(solver_new_var(s) == v);
        for (uint32_t c = 0; c < nc; ++c) {
            csz[c] = (c == 0 && trial % 5 == 0) ? 1u : (2u + (lcg(&rng) % 2u));
            for (uint32_t j = 0; j < csz[c]; ++j) {
                cls[c][j] = mkLit(1u + (lcg(&rng) % nv), (lcg(&rng) & 1u) != 0);
            }
            solver_add_clause(s, cls[c], csz[c]);
        }

        /* Brute-force exact model count, projected count on {1,2,3}, and exact backbone! */
        uint32_t total = 1u << nv;
        uint64_t bf_models = 0;
        uint8_t proj_seen[8] = {0};
        uint32_t always_true = (1u << nv) - 1u;
        uint32_t always_false = (1u << nv) - 1u;

        for (uint32_t mask = 0; mask < total; ++mask) {
            bool ok = true;
            for (uint32_t c = 0; c < nc; ++c) {
                if (!eval_clause_mask(cls[c], csz[c], mask)) { ok = false; break; }
            }
            if (ok) {
                bf_models++;
                proj_seen[mask & 7u] = 1;
                always_true &= mask;
                always_false &= ~mask;
            }
        }
        uint64_t bf_proj = 0;
        for (int p = 0; p < 8; ++p) bf_proj += proj_seen[p];

        /* 1. Full AllSAT enumeration */
        EnumCheck ec;
        memset(&ec, 0, sizeof ec);
        ec.nvars = nv;
        AllSatResult ar = bsat_enumerate_models(s, nv, NULL, 0, 0, &o, check_cb, &ec);
        assert(ar.complete);
        assert(ar.count == bf_models);
        assert(ar.status == (bf_models > 0 ? TRUE : FALSE));

        /* 2. Projected AllSAT onto {1, 2, 3} */
        Var proj[3] = {1, 2, 3};
        EnumCheck pec;
        memset(&pec, 0, sizeof pec);
        pec.nvars = nv; pec.proj = proj; pec.nproj = 3;
        AllSatResult par = bsat_enumerate_models(s, nv, proj, 3, 0, &o, check_cb, &pec);
        assert(par.complete);
        assert(par.count == bf_proj);
        assert(par.status == (bf_proj > 0 ? TRUE : FALSE));

        /* 3. Capped AllSAT (--models 3) */
        if (bf_models >= 3) {
            AllSatResult cap_r = bsat_enumerate_models(s, nv, NULL, 0, 3, &o, NULL, NULL);
            assert(cap_r.status == TRUE && cap_r.complete && cap_r.count == 3);
        }

        /* 4. Exact Global Backbone */
        BackboneResult br = bsat_compute_backbone(s, nv, &o);
        if (bf_models == 0) {
            assert(br.status == FALSE && br.complete && br.size == 0);
        } else {
            assert(br.status == TRUE && br.complete);
            uint32_t expected_bb = 0;
            for (Var v = 1; v <= nv; ++v) {
                if ((always_true >> (v - 1)) & 1u) expected_bb++;
                if ((always_false >> (v - 1)) & 1u) expected_bb++;
            }
            assert(br.size == expected_bb);
            for (uint32_t i = 0; i < br.size; ++i) {
                Lit l = br.lits[i];
                Var v = var(l);
                if (!sign(l)) assert(((always_true >> (v - 1)) & 1u) != 0);
                else assert(((always_false >> (v - 1)) & 1u) != 0);
            }
        }
        bsat_backbone_result_free(&br);
        solver_free(s);
    }
}

/* Verify MUS is BOTH unsatisfiable AND irreducibly minimal (dropping ANY 1 clause makes it SAT!). */
static void verify_mus_exact(const Lit (*cls)[4], const uint32_t *csz, uint32_t nc,
                             uint32_t nv, const MusResult *mr) {
    assert(mr->status == FALSE && mr->minimal && mr->size >= 1 && mr->size <= nc);
    /* Check indices are strictly increasing and in range */
    for (uint32_t i = 0; i < mr->size; ++i) {
        assert(mr->clause_ids[i] < nc);
        if (i > 0) assert(mr->clause_ids[i] > mr->clause_ids[i - 1]);
    }
    /* 1. MUS itself must be UNSAT */
    {
        Solver *chk = solver_new();
        for (Var v = 1; v <= nv; ++v) assert(solver_new_var(chk) == v);
        for (uint32_t i = 0; i < mr->size; ++i) {
            uint32_t c = mr->clause_ids[i];
            solver_add_clause(chk, cls[c], csz[c]);
        }
        assert(solver_solve(chk) == FALSE);
        solver_free(chk);
    }
    /* 2. Dropping ANY single clause from MUS must become SAT! */
    for (uint32_t drop = 0; drop < mr->size; ++drop) {
        Solver *chk = solver_new();
        for (Var v = 1; v <= nv; ++v) assert(solver_new_var(chk) == v);
        for (uint32_t i = 0; i < mr->size; ++i) {
            if (i == drop) continue;
            uint32_t c = mr->clause_ids[i];
            solver_add_clause(chk, cls[c], csz[c]);
        }
        assert(solver_solve(chk) == TRUE);
        solver_free(chk);
    }
}

static void test_mus_extraction(void) {
    SolverOpts o = default_opts();
    o.quiet = true;

    /* 1. Crafted UNSAT: PHP(3,2) + disjoint-variable noise clauses -> exact 9-clause MUS {0..8}. */
    {
        const char *cnf =
            "p cnf 9 12\n"
            "1 2 0\n"      /* 0: PHP(3,2) */
            "3 4 0\n"      /* 1 */
            "5 6 0\n"      /* 2 */
            "-1 -3 0\n"    /* 3 */
            "-1 -5 0\n"    /* 4 */
            "-3 -5 0\n"    /* 5 */
            "-2 -4 0\n"    /* 6 */
            "-2 -6 0\n"    /* 7 */
            "-4 -6 0\n"    /* 8 */
            "7 8 0\n"      /* 9: disjoint noise */
            "-7 9 0\n"     /* 10: disjoint noise */
            "8 -9 0\n";    /* 11: disjoint noise */
        Solver *s = solver_new();
        assert(dimacs_parse_string(s, cnf) == DIMACS_OK);
        MusResult mr = bsat_compute_mus(s, 9, &o);
        assert(mr.status == FALSE && mr.minimal && mr.size == 9);
        /* Every PHP(3,2) clause (0..8) is necessary; disjoint noise clauses 9..11 must be dropped! */
        for (uint32_t i = 0; i < 9; ++i) assert(mr.clause_ids[i] == i);
        bsat_mus_result_free(&mr);
        solver_free(s);
    }

    /* 2. Empty clause MUS -> size 1. */
    {
        Solver *s = solver_new();
        assert(dimacs_parse_string(s, "p cnf 2 3\n1 2 0\n0\n-1 -2 0\n") == DIMACS_OK);
        MusResult mr = bsat_compute_mus(s, 2, &o);
        assert(mr.status == FALSE && mr.minimal && mr.size == 1 && mr.clause_ids[0] == 1);
        bsat_mus_result_free(&mr);
        solver_free(s);
    }

    /* 3. 25 randomized UNSAT/SAT formulas: verify every UNSAT MUS is UNSAT & irreducibly minimal! */
    uint32_t rng = 0x98765432u;
    for (int trial = 0; trial < 25; ++trial) {
        uint32_t nv = 6u + (lcg(&rng) % 3u);
        uint32_t nc = 18u + (lcg(&rng) % 20u);
        Lit cls[40][4];
        uint32_t csz[40];
        Solver *s = solver_new();
        for (Var v = 1; v <= nv; ++v) assert(solver_new_var(s) == v);
        for (uint32_t c = 0; c < nc; ++c) {
            csz[c] = (c < 4) ? 2u : (2u + (lcg(&rng) % 2u));
            for (uint32_t j = 0; j < csz[c]; ++j) {
                cls[c][j] = mkLit(1u + (lcg(&rng) % nv), (lcg(&rng) & 1u) != 0);
            }
            solver_add_clause(s, cls[c], csz[c]);
        }
        MusResult mr = bsat_compute_mus(s, nv, &o);
        if (mr.status == FALSE) {
            verify_mus_exact(cls, csz, nc, nv, &mr);
        } else {
            assert(mr.status == TRUE);
            assert(solver_solve(s) == TRUE);
        }
        bsat_mus_result_free(&mr);
        solver_free(s);
    }
}

int main(void) {
    test_allsat_and_backbone_oracle();
    test_mus_extraction();
    puts("PASS: AllSAT/projected enumeration, exact global backbone, and minimal MUS extraction");
    return 0;
}
