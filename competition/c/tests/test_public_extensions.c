/*********************************************************************
 * Unit Tests for Extended Public ABI (bsat.h):
 *   freeze/melt, threads, proof/lrat, add_xor/atmost/pb_le,
 *   backbone, and MUS
 *********************************************************************/

#include "../include/bsat.h"
#include "../include/lrat.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void test_freeze_and_threads(void) {
    bsat *s = bsat_create(BSAT_ABI_VERSION, 0);
    assert(s);
    assert(bsat_set_threads(s, 4) == 1);
    assert(bsat_set_threads(s, 0) == 0);
    assert(bsat_freeze(s, 1) == 1);
    assert(bsat_is_frozen(s, 1) == 1);
    assert(bsat_is_frozen(s, -1) == 1);
    assert(bsat_melt(s, 1) == 1);
    assert(bsat_is_frozen(s, 1) == 0);
    assert(bsat_freeze(s, 2) == 1);

    int c1[] = {1, 2}, c2[] = {-1, 3}, c3[] = {-2, 3};
    assert(bsat_add_clause(s, c1, 2) == 1);
    assert(bsat_add_clause(s, c2, 2) == 1);
    assert(bsat_add_clause(s, c3, 2) == 1);
    assert(bsat_solve(s, NULL, 0) == BSAT_SAT);
    assert(bsat_value(s, 3) == 3);
    bsat_destroy(s);
}

static void test_xor_atmost_pb_via_public_api(void) {
    /* XOR UNSAT via bsat_add_xor */
    {
        bsat *s = bsat_create(BSAT_ABI_VERSION, 0);
        int x1[] = {1, 2, 3}, x2[] = {1, 2}, x3[] = {3};
        assert(bsat_add_xor(s, x1, 3, 1) == 1);
        assert(bsat_add_xor(s, x2, 2, 1) == 1);
        assert(bsat_add_xor(s, x3, 1, 1) == 1);
        assert(bsat_solve(s, NULL, 0) == BSAT_UNSAT);
        bsat_destroy(s);
    }

    /* AtMost-2 + force 3 true -> UNSAT; AtMost-2 + force 2 true -> SAT */
    {
        bsat *s = bsat_create(BSAT_ABI_VERSION, BSAT_REUSE_LEARNTS);
        int lits[] = {1, 2, 3, 4};
        assert(bsat_add_atmost(s, lits, 4, 2) == 1);
        int a_sat[] = {1, 2};
        int a_unsat[] = {1, 2, 3};
        assert(bsat_solve(s, a_sat, 2) == BSAT_SAT);
        assert(bsat_value(s, 3) == -3 && bsat_value(s, 4) == -4);
        assert(bsat_solve(s, a_unsat, 3) == BSAT_UNSAT);
        bsat_destroy(s);
    }

    /* Weighted PB: 3*x1 + 5*x2 + 2*x3 <= 6 with x1=1, x2=1 -> UNSAT; x1=1, x3=1 -> SAT */
    {
        bsat *s = bsat_create(BSAT_ABI_VERSION, BSAT_REUSE_LEARNTS);
        int lits[] = {1, 2, 3};
        uint64_t w[] = {3, 5, 2};
        assert(bsat_add_pb_le(s, lits, w, 3, 6) == 1);
        int a1[] = {1, 2};
        int a2[] = {1, 3};
        assert(bsat_solve(s, a1, 2) == BSAT_UNSAT);
        assert(bsat_solve(s, a2, 2) == BSAT_SAT);
        assert(bsat_value(s, 2) == -2);
        bsat_destroy(s);
    }
}

static void test_backbone_and_mus_via_public_api(void) {
    /* Backbone: x1 & (~x1 | x2) & (x3 | x4) => backbone = {1, 2} */
    {
        bsat *s = bsat_create(BSAT_ABI_VERSION, 0);
        int c1[] = {1}, c2[] = {-1, 2}, c3[] = {3, 4};
        assert(bsat_add_clause(s, c1, 1) == 1);
        assert(bsat_add_clause(s, c2, 2) == 1);
        assert(bsat_add_clause(s, c3, 2) == 1);
        int bb[8]; size_t cnt = 0;
        assert(bsat_backbone(s, bb, 8, &cnt) == BSAT_SAT);
        assert(cnt == 2 && bb[0] == 1 && bb[1] == 2);
        bsat_destroy(s);
    }

    /* MUS: clauses {0: (1), 1: (-1|2), 2: (-2), 3: (3|4)} => MUS = {0, 1, 2} */
    {
        bsat *s = bsat_create(BSAT_ABI_VERSION, 0);
        int c0[] = {1}, c1[] = {-1, 2}, c2[] = {-2}, c3[] = {3, 4};
        assert(bsat_add_clause(s, c0, 1) == 1);
        assert(bsat_add_clause(s, c1, 2) == 1);
        assert(bsat_add_clause(s, c2, 1) == 1);
        assert(bsat_add_clause(s, c3, 2) == 1);
        uint32_t mus[8]; size_t cnt = 0;
        assert(bsat_mus(s, mus, 8, &cnt) == BSAT_UNSAT);
        assert(cnt == 3 && mus[0] == 0 && mus[1] == 1 && mus[2] == 2);
        bsat_destroy(s);
    }
}

static void test_lrat_via_public_api(void) {
    char cnf_path[128], lrat_path[128];
    snprintf(cnf_path, sizeof cnf_path, "/tmp/bsat_pub_lrat_%d.cnf", (int)getpid());
    snprintf(lrat_path, sizeof lrat_path, "/tmp/bsat_pub_lrat_%d.blrat", (int)getpid());

    FILE *f = fopen(cnf_path, "w");
    assert(f);
    fputs("p cnf 3 5\n1 2 0\n-1 2 0\n1 -2 0\n-1 -2 3 0\n-3 0\n", f);
    fclose(f);

    bsat *s = bsat_create(BSAT_ABI_VERSION, 0);
    assert(s);
    assert(bsat_set_lrat(s, lrat_path, 1) == 1);
    int c0[] = {1, 2}, c1[] = {-1, 2}, c2[] = {1, -2}, c3[] = {-1, -2, 3}, c4[] = {-3};
    assert(bsat_add_clause(s, c0, 2) == 1);
    assert(bsat_add_clause(s, c1, 2) == 1);
    assert(bsat_add_clause(s, c2, 2) == 1);
    assert(bsat_add_clause(s, c3, 3) == 1);
    assert(bsat_add_clause(s, c4, 1) == 1);
    assert(bsat_solve(s, NULL, 0) == BSAT_UNSAT);
    bsat_destroy(s);

    assert(lrat_verify_file(cnf_path, lrat_path, true));
    unlink(cnf_path);
    unlink(lrat_path);
}

int main(void) {
    test_freeze_and_threads();
    test_xor_atmost_pb_via_public_api();
    test_backbone_and_mus_via_public_api();
    test_lrat_via_public_api();
    puts("PASS: Extended public ABI (freeze/melt, threads, XOR/PB, backbone, MUS, native LRAT)");
    return 0;
}
