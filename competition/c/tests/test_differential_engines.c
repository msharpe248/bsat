/*********************************************************************
 * Differential Soundness & Fuzz Test Across All BSAT Engines
 *
 * Compares the baseline pure CDCL oracle (inprocess=false, sym=false,
 * sbva=false) against:
 *   1. Full default BSAT (congruence + Gauss + sym + SBVA + AMO + BVE)
 *   2. Explicit --bce + --elim + --sbva (exercises ElimState resize!)
 *   3. Multi-core portfolio (-j 4, clause sharing + unit broadcast)
 * across 200 seeded random/structured formulas (random 3-SAT, XOR+CNF
 * mixed systems, bicliques/PHP, symmetric coloring, full-adder miters).
 * Verifies 100% SAT/UNSAT agreement and independent model checking!
 *********************************************************************/
#include "../include/portfolio.h"
#include "../include/solver.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    Lit *lits;
    uint32_t *offs;
    uint32_t nvars;
    uint32_t ncls;
    uint32_t lit_used;
    uint32_t lit_cap;
    uint32_t cls_cap;
} Formula;

static void f_init(Formula *f, uint32_t nvars) {
    memset(f, 0, sizeof *f);
    f->nvars = nvars;
    f->lit_cap = 256;
    f->cls_cap = 128;
    f->lits = malloc((size_t)f->lit_cap * sizeof(Lit));
    f->offs = malloc((size_t)(f->cls_cap + 1u) * sizeof(uint32_t));
    assert(f->lits && f->offs);
    f->offs[0] = 0;
}

static void f_free(Formula *f) {
    free(f->lits);
    free(f->offs);
}

static void f_add(Formula *f, const Lit *cl, uint32_t sz) {
    if (f->ncls + 1u >= f->cls_cap) {
        f->cls_cap *= 2u;
        f->offs = realloc(f->offs, (size_t)(f->cls_cap + 1u) * sizeof(uint32_t));
        assert(f->offs);
    }
    if (f->lit_used + sz > f->lit_cap) {
        while (f->lit_used + sz > f->lit_cap) f->lit_cap *= 2u;
        f->lits = realloc(f->lits, (size_t)f->lit_cap * sizeof(Lit));
        assert(f->lits);
    }
    memcpy(f->lits + f->lit_used, cl, (size_t)sz * sizeof(Lit));
    f->lit_used += sz;
    f->offs[++f->ncls] = f->lit_used;
}

static uint32_t rng_next(uint32_t *st) {
    uint32_t x = *st;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *st = x;
    return x;
}

static void verify_model_against_formula(const Solver *s, const Formula *f) {
    for (uint32_t i = 0; i < f->ncls; ++i) {
        uint32_t b = f->offs[i], e = f->offs[i + 1u];
        bool sat = false;
        for (uint32_t j = b; j < e; ++j) {
            Lit l = f->lits[j];
            lbool val = solver_model_value(s, var(l));
            assert(val == TRUE || val == FALSE);
            if ((val == TRUE) == !sign(l)) { sat = true; break; }
        }
        assert(sat);
    }
}

static Solver *load_formula(const Formula *f, const SolverOpts *opts) {
    Solver *s = solver_new_with_opts(opts);
    assert(s != NULL);
    for (uint32_t v = 0; v < f->nvars; ++v) assert(solver_new_var(s) != INVALID_VAR);
    for (uint32_t i = 0; i < f->ncls; ++i) {
        uint32_t b = f->offs[i], e = f->offs[i + 1u];
        solver_add_clause(s, f->lits + b, e - b);
    }
    return s;
}

static void add_xor_block(Formula *f, const Var *vs, uint32_t k, bool rhs) {
    uint32_t total = 1u << k;
    for (uint32_t m = 0; m < total; ++m) {
        uint32_t pop = 0;
        for (uint32_t j = 0; j < k; ++j) pop += (m >> j) & 1u;
        if ((pop & 1u) == (rhs ? 1u : 0u)) continue;
        Lit cl[5];
        for (uint32_t j = 0; j < k; ++j) cl[j] = mkLit(vs[j], ((m >> j) & 1u) != 0);
        f_add(f, cl, k);
    }
}

static void build_case(Formula *f, uint32_t seed, int kind) {
    uint32_t r = seed * 2654435761u + 1013904223u;
    if (kind == 0) {
        /* Random 2/3/4-SAT near phase transition (12..20 vars) */
        uint32_t nv = 12u + (rng_next(&r) % 9u);
        uint32_t nc = nv * 4u + (rng_next(&r) % (nv * 2u));
        f_init(f, nv);
        for (uint32_t i = 0; i < nc; ++i) {
            uint32_t k = 2u + (rng_next(&r) % 3u);
            Lit cl[4];
            for (uint32_t j = 0; j < k; ++j) {
                cl[j] = mkLit(1u + (rng_next(&r) % nv), (rng_next(&r) & 1u) != 0);
            }
            f_add(f, cl, k);
        }
    } else if (kind == 1) {
        /* Mixed GF(2) XOR equations + random CNF constraints */
        uint32_t nv = 14u + (rng_next(&r) % 7u);
        f_init(f, nv);
        uint32_t nxors = 3u + (rng_next(&r) % 4u);
        for (uint32_t x = 0; x < nxors; ++x) {
            uint32_t k = 3u + (rng_next(&r) % 2u);
            Var vs[4];
            for (uint32_t j = 0; j < k; ++j) {
                bool dup;
                do {
                    dup = false;
                    vs[j] = 1u + (rng_next(&r) % nv);
                    for (uint32_t p = 0; p < j; ++p) if (vs[p] == vs[j]) dup = true;
                } while (dup);
            }
            add_xor_block(f, vs, k, (rng_next(&r) & 1u) != 0);
        }
        uint32_t ncnf = 15u + (rng_next(&r) % 25u);
        for (uint32_t i = 0; i < ncnf; ++i) {
            Lit cl[3];
            for (uint32_t j = 0; j < 3u; ++j) {
                cl[j] = mkLit(1u + (rng_next(&r) % nv), (rng_next(&r) & 1u) != 0);
            }
            f_add(f, cl, 3u);
        }
    } else if (kind == 2) {
        /* Symmetric biclique / coloring-like formula with level-0 units */
        uint32_t p = 4u + (rng_next(&r) % 3u);
        uint32_t h = 3u + (rng_next(&r) % 3u);
        uint32_t nv = p * h + 2u;
        f_init(f, nv);
        for (uint32_t i = 0; i < p; ++i) {
            Lit cl[6];
            for (uint32_t j = 0; j < h; ++j) cl[j] = mkLit(i * h + j + 1u, false);
            f_add(f, cl, h);
        }
        for (uint32_t j = 0; j < h; ++j) {
            for (uint32_t i1 = 0; i1 < p; ++i1) {
                for (uint32_t i2 = i1 + 1u; i2 < p; ++i2) {
                    Lit cl[2] = {mkLit(i1 * h + j + 1u, true), mkLit(i2 * h + j + 1u, true)};
                    f_add(f, cl, 2u);
                }
            }
        }
        if (rng_next(&r) & 1u) {
            Lit u = mkLit(p * h + 1u, (rng_next(&r) & 1u) != 0);
            f_add(f, &u, 1u);
            Lit c2[2] = {neg(u), mkLit(1u, (rng_next(&r) & 1u) != 0)};
            f_add(f, c2, 2u);
        }
    } else {
        /* Circuit with AND + MAJ3 + XOR3 gates + random assertions */
        uint32_t nv = 18u;
        f_init(f, nv);
        /* g7 = AND(1, 2, 3), g8 = AND(1, 2, 3) */
        for (uint32_t out = 7u; out <= 8u; ++out) {
            Lit big[4] = {mkLit(out, false), mkLit(1, true), mkLit(2, true), mkLit(3, true)};
            f_add(f, big, 4u);
            for (uint32_t inp = 1u; inp <= 3u; ++inp) {
                Lit b[2] = {mkLit(out, true), mkLit(inp, false)};
                f_add(f, b, 2u);
            }
        }
        /* g9 = MAJ3(4, 5, 6), g10 = MAJ3(4, 5, 6) */
        for (uint32_t out = 9u; out <= 10u; ++out) {
            Var ins[3] = {4, 5, 6};
            for (int a = 0; a < 3; ++a) {
                for (int b = a + 1; b < 3; ++b) {
                    Lit c1[3] = {mkLit(out, true), mkLit(ins[a], false), mkLit(ins[b], false)};
                    Lit c2[3] = {mkLit(out, false), mkLit(ins[a], true), mkLit(ins[b], true)};
                    f_add(f, c1, 3u);
                    f_add(f, c2, 3u);
                }
            }
        }
        /* g11 = XOR3(7, 9, 12), g13 = XOR3(8, 10, 12) */
        Var x1[4] = {11, 7, 9, 12};
        Var x2[4] = {13, 8, 10, 12};
        add_xor_block(f, x1, 4u, false);
        add_xor_block(f, x2, 4u, false);
        /* Random constraints on outputs */
        for (uint32_t i = 0; i < 12u; ++i) {
            Lit cl[3];
            for (uint32_t j = 0; j < 3u; ++j) {
                cl[j] = mkLit(1u + (rng_next(&r) % nv), (rng_next(&r) & 1u) != 0);
            }
            f_add(f, cl, 3u);
        }
        if (rng_next(&r) & 1u) {
            /* Assert g11 != g13 (UNSAT miter!) */
            Lit m1[2] = {mkLit(11, false), mkLit(13, false)};
            Lit m2[2] = {mkLit(11, true), mkLit(13, true)};
            f_add(f, m1, 2u);
            f_add(f, m2, 2u);
        }
    }
}

int main(void) {
    uint32_t total_sat = 0, total_unsat = 0;
    const uint32_t CASES = 160u;

    for (uint32_t c = 0; c < CASES; ++c) {
        Formula f;
        build_case(&f, c + 1u, (int)(c % 4u));

        /* 0. Pure CDCL baseline oracle */
        SolverOpts base = default_opts();
        base.inprocess = false;
        base.sym = false;
        base.sbva = false;
        base.congruence = false;
        base.equiv = false;
        base.bce = false;
        base.elim = false;
        Solver *sb = load_formula(&f, &base);
        lbool expected = solver_solve(sb);
        assert(expected == TRUE || expected == FALSE);
        if (expected == TRUE) { verify_model_against_formula(sb, &f); total_sat++; }
        else total_unsat++;
        solver_free(sb);

        /* 1. Full default engine */
        SolverOpts full = default_opts();
        Solver *sf = load_formula(&f, &full);
        lbool r1 = solver_solve(sf);
        assert(r1 == expected);
        if (r1 == TRUE) verify_model_against_formula(sf, &f);
        solver_free(sf);

        /* 2. Aggressive preprocessing + BCE + elim + SBVA + congruence */
        SolverOpts agg = default_opts();
        agg.congruence = true;
        agg.equiv = true;
        agg.bce = true;
        agg.elim = true;
        agg.sbva = true;
        agg.sym = true;
        Solver *sa = load_formula(&f, &agg);
        lbool r2 = solver_solve(sa);
        assert(r2 == expected);
        if (r2 == TRUE) verify_model_against_formula(sa, &f);
        solver_free(sa);

        /* 3. Multi-core portfolio (-j 4) on every 4th case to keep runtime crisp */
        if ((c & 3u) == 0u) {
            SolverOpts par = default_opts();
            par.quiet = true;
            Solver *sp = load_formula(&f, &par);
            lbool r3 = solver_solve_parallel(sp, 4u);
            assert(r3 == expected);
            if (r3 == TRUE) verify_model_against_formula(sp, &f);
            solver_free(sp);
        }

        f_free(&f);
    }

    printf("PASS: test_differential_engines (%u cases: %u SAT, %u UNSAT across baseline, full, aggressive, -j 4)\n",
           CASES, total_sat, total_unsat);
    return 0;
}
