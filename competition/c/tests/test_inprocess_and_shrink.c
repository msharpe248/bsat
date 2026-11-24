/* Unit tests for block-level all-UIP shrinking, 3-state minimization,
 * OTFS, 3-tier DB, and the full inprocessing pipeline (SCC, SSR, gate BVE,
 * O(n) prefix vivification, 64-bit SAT sweeping, WalkSAT phase seeding). */
#include "../include/solver.h"
#include "../include/inprocess.h"
#include "../include/dimacs.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static uint32_t rng_state = 0x20261123u;
static uint32_t next_rand(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static bool eval_cnf(unsigned bits, const Lit *clauses, const uint32_t *sizes, uint32_t nclauses) {
    uint32_t off = 0;
    for (uint32_t i = 0; i < nclauses; ++i) {
        bool sat = false;
        for (uint32_t j = 0; j < sizes[i]; ++j) {
            Lit l = clauses[off + j];
            bool val = ((bits >> (var(l) - 1)) & 1u) != 0;
            if (val != sign(l)) { sat = true; break; }
        }
        if (!sat) return false;
        off += sizes[i];
    }
    return true;
}

static void test_block_shrinking(void) {
    SolverOpts o = default_opts();
    o.probing = false; o.inprocess = false; o.chrono = false; o.alternating = false;
    Solver *s = solver_new_with_opts(&o); assert(s);
    for (unsigned v = 1; v <= 6; ++v) assert(solver_new_var(s) == v);

    /* Level 1 decision: x1 = true. Implies x2 = true, x3 = true, x4 = true via binary/ternary chain.
     * Block {~x3, ~x4} at level 1 should shrink to single UIP {~x2}! */
    Lit c1[] = {fromDimacs(-1), fromDimacs(2)};
    Lit c2[] = {fromDimacs(-2), fromDimacs(3)};
    Lit c3[] = {fromDimacs(-2), fromDimacs(4)};
    assert(solver_add_clause(s, c1, 2));
    assert(solver_add_clause(s, c2, 2));
    assert(solver_add_clause(s, c3, 2));

    s->decision_level = 1;
    s->trail_lims[1] = s->trail_size;
    s->values[1] = TRUE; s->vars[1].level = 1; s->vars[1].reason = INVALID_CLAUSE;
    s->vars[1].trail_pos = s->trail_size; s->trail[s->trail_size++] = (Trail){fromDimacs(1)};
    assert(solver_propagate(s) == INVALID_CLAUSE);
    assert(s->values[2] == TRUE && s->values[3] == TRUE && s->values[4] == TRUE);

    /* Level 2 decision: x5 = true. Learned clause: (~x5 | ~x3 | ~x4) -> shrinks to (~x5 | ~x2)! */
    s->decision_level = 2;
    s->trail_lims[2] = s->trail_size;
    s->values[5] = TRUE; s->vars[5].level = 2; s->vars[5].reason = INVALID_CLAUSE;
    s->vars[5].trail_pos = s->trail_size; s->trail[s->trail_size++] = (Trail){fromDimacs(5)};
    assert(solver_propagate(s) == INVALID_CLAUSE);

    Lit learnt[] = {fromDimacs(-5), fromDimacs(-3), fromDimacs(-4)};
    uint32_t n = 3;
    uint32_t removed = solver_shrink_clause(s, learnt, &n);
    assert(removed == 1 && n == 2);
    assert(learnt[0] == fromDimacs(-5) && learnt[1] == fromDimacs(-2));
    for (Var v = 1; v <= 6; ++v) assert(!s->seen[v]);
    solver_free(s);
}

static void test_gate_bve_and_reconstruction(void) {
    /* Circuit with AND gate (x4 <-> x1 & x2), XOR gate (x5 <-> x3 ^ x4),
     * ITE gate (x6 <-> ite(x1, x5, x2)), plus non-gate constraints! */
    const char *cnf =
        "p cnf 6 14\n"
        /* x4 <-> x1 & x2 */
        "-4 1 0\n-4 2 0\n4 -1 -2 0\n"
        /* x5 <-> x3 ^ x4 */
        "-5 3 4 0\n-5 -3 -4 0\n5 -3 4 0\n5 3 -4 0\n"
        /* x6 <-> ite(x1, x5, x2) */
        "-1 -5 6 0\n-1 5 -6 0\n1 -2 6 0\n1 2 -6 0\n"
        /* non-gate constraints forcing a unique/constrained solution */
        "6 0\n3 4 0\n-2 5 0\n";
    Solver *s = solver_new(); assert(s);
    assert(dimacs_parse_string(s, cnf) == DIMACS_OK);
    assert(solver_solve(s) == TRUE);
    assert(solver_check_model(s));
    assert(s->elim && s->elim->vars_eliminated > 0);
    solver_free(s);
}

static void test_exhaustive_random_inprocessing(void) {
    unsigned sat_cnt = 0, unsat_cnt = 0;
    for (unsigned trial = 0; trial < 256; ++trial) {
        unsigned nvars = 5 + (next_rand() % 4); /* 5..8 vars */
        unsigned nclauses = 12 + (next_rand() % 28);
        Lit clauses[40 * 4];
        uint32_t sizes[40];
        uint32_t off = 0;
        for (unsigned c = 0; c < nclauses; ++c) {
            unsigned sz = 2 + (next_rand() % 3);
            sizes[c] = sz;
            for (unsigned j = 0; j < sz; ++j) {
                clauses[off + j] = mkLit(1 + (next_rand() % nvars), next_rand() & 1u);
            }
            off += sz;
        }
        bool expected = false;
        for (unsigned bits = 0; bits < (1u << nvars); ++bits) {
            if (eval_cnf(bits, clauses, sizes, nclauses)) { expected = true; break; }
        }
        SolverOpts o = default_opts();
        o.inprocess = true;
        o.inprocess_interval = 1;
        o.reduce_interval = 4;
        o.reduce_increment = 2;
        Solver *s = solver_new_with_opts(&o); assert(s);
        for (unsigned v = 1; v <= nvars; ++v) assert(solver_new_var(s) == v);
        off = 0;
        for (unsigned c = 0; c < nclauses; ++c) {
            solver_add_clause(s, clauses + off, sizes[c]);
            assert(!s->error);
            off += sizes[c];
        }
        lbool ans = solver_solve(s);
        assert(!s->error);
        assert(ans == (expected ? TRUE : FALSE));
        if (expected) { assert(solver_check_model(s)); sat_cnt++; }
        else { unsat_cnt++; }
        solver_free(s);
    }
    printf("PASS: block shrinking, gate BVE reconstruction, and 256 exhaustive inprocessing solves (%u SAT, %u UNSAT)\n",
           sat_cnt, unsat_cnt);
}

int main(void) {
    test_block_shrinking();
    test_gate_bve_and_reconstruction();
    test_exhaustive_random_inprocessing();
    return 0;
}
