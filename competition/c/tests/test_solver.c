/*********************************************************************
 * BSAT C Solver - Core Solver Unit Tests
 *
 * Tests for solver creation, clause addition, solving, and statistics.
 *********************************************************************/

#include "../include/solver.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// Test counter
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        printf("Testing %s... ", name); \
        tests_run++; \
    } while (0)

#define PASS() \
    do { \
        printf("✅ PASS\n"); \
        tests_passed++; \
    } while (0)

#define FAIL(msg) \
    do { \
        printf("❌ FAIL: %s\n", msg); \
        exit(1); \
    } while (0)

/*********************************************************************
 * Test Cases
 *********************************************************************/

void test_solver_creation() {
    TEST("Solver creation and destruction");

    Solver* s = solver_new();
    if (s == NULL) {
        FAIL("Failed to create solver");
    }

    if (s->num_vars != 0) {
        FAIL("New solver should have 0 variables");
    }

    if (s->num_clauses != 0) {
        FAIL("New solver should have 0 clauses");
    }

    solver_free(s);
    PASS();
}

void test_add_variables() {
    TEST("Add variables");

    Solver* s = solver_new();

    // Reserve variables
    for (Var v = 1; v <= 10; v++) {
        solver_new_var(s);
    }

    if (s->num_vars != 10) {
        FAIL("Should have 10 variables");
    }

    solver_free(s);
    PASS();
}

void test_add_clause() {
    TEST("Add clause");

    Solver* s = solver_new();

    // Create variables
    Var x1 = solver_new_var(s);
    Var x2 = solver_new_var(s);

    // Add clause: (x1 ∨ x2)
    Lit lits[2];
    lits[0] = mkLit(x1, false);
    lits[1] = mkLit(x2, false);

    bool added = solver_add_clause(s, lits, 2);
    if (!added) {
        FAIL("Failed to add clause");
    }

    if (s->num_clauses == 0) {
        FAIL("Should have at least 1 clause");
    }

    solver_free(s);
    PASS();
}

void test_empty_formula_sat() {
    TEST("Empty formula is SAT");

    Solver* s = solver_new();

    lbool result = solver_solve(s);
    if (result != TRUE) {
        FAIL("Empty formula should be SAT");
    }

    solver_free(s);
    PASS();
}

void test_single_unit_clause() {
    TEST("Single unit clause");

    Solver* s = solver_new();

    Var x = solver_new_var(s);

    // Add clause: (x)
    Lit lits[1];
    lits[0] = mkLit(x, false);
    solver_add_clause(s, lits, 1);

    lbool result = solver_solve(s);
    if (result != TRUE) {
        FAIL("Single positive unit should be SAT");
    }

    // Check solution
    if (solver_model_value(s, x) != TRUE) {
        FAIL("Variable should be true in solution");
    }

    solver_free(s);
    PASS();
}

void test_contradiction() {
    TEST("Contradiction: (x) ∧ (~x)");

    Solver* s = solver_new();

    Var x = solver_new_var(s);

    // Add clause: (x)
    Lit lits1[1];
    lits1[0] = mkLit(x, false);
    solver_add_clause(s, lits1, 1);

    // Add clause: (~x)
    Lit lits2[1];
    lits2[0] = mkLit(x, true);
    solver_add_clause(s, lits2, 1);

    lbool result = solver_solve(s);
    if (result != FALSE) {
        FAIL("Contradiction should be UNSAT");
    }

    solver_free(s);
    PASS();
}

void test_simple_sat() {
    TEST("Simple SAT: (x ∨ y) ∧ (~x ∨ z)");

    Solver* s = solver_new();

    Var x = solver_new_var(s);
    Var y = solver_new_var(s);
    Var z = solver_new_var(s);

    // Add clause: (x ∨ y)
    Lit lits1[2];
    lits1[0] = mkLit(x, false);
    lits1[1] = mkLit(y, false);
    solver_add_clause(s, lits1, 2);

    // Add clause: (~x ∨ z)
    Lit lits2[2];
    lits2[0] = mkLit(x, true);
    lits2[1] = mkLit(z, false);
    solver_add_clause(s, lits2, 2);

    lbool result = solver_solve(s);
    if (result != TRUE) {
        FAIL("Should be SAT");
    }

    solver_free(s);
    PASS();
}

void test_statistics_tracking() {
    TEST("Statistics tracking");

    Solver* s = solver_new();

    Var x = solver_new_var(s);

    // Add clause: (x)
    Lit lits[1];
    lits[0] = mkLit(x, false);
    solver_add_clause(s, lits, 1);

    lbool result = solver_solve(s);
    if (result != TRUE) {
        FAIL("Should be SAT");
    }

    // Check that statistics were tracked
    // (Exact values depend on solver implementation, just verify structure exists)
    if (s->stats.decisions < 0) {
        FAIL("Statistics should be non-negative");
    }

    solver_free(s);
    PASS();
}

void test_conflict_limit() {
    TEST("Conflict limit");

    SolverOpts opts = default_opts();
    opts.max_conflicts = 1;  // Very low limit

    Solver* s = solver_new_with_opts(&opts);

    // Add a hard problem (will likely hit conflict limit)
    for (int i = 0; i < 5; i++) {
        solver_new_var(s);
    }

    // Add many random clauses
    for (int i = 0; i < 20; i++) {
        Lit lits[3];
        lits[0] = mkLit(1, i % 2);
        lits[1] = mkLit(2, (i + 1) % 2);
        lits[2] = mkLit(3, (i + 2) % 2);
        solver_add_clause(s, lits, 3);
    }

    lbool result = solver_solve(s);

    // Result could be SAT, UNSAT, or UNKNOWN (if limit hit)
    // Just verify solver doesn't crash
    (void)result;

    solver_free(s);
    PASS();
}

void test_assumptions() {
    TEST("Solving with assumptions");

    Solver* s = solver_new();

    Var x = solver_new_var(s);
    Var y = solver_new_var(s);

    // Add clause: (x ∨ y)
    Lit lits[2];
    lits[0] = mkLit(x, false);
    lits[1] = mkLit(y, false);
    solver_add_clause(s, lits, 2);

    // Solve with assumption: ~x
    Lit assumptions[1];
    assumptions[0] = mkLit(x, true);

    lbool result = solver_solve_with_assumptions(s, assumptions, 1);

    if (result != TRUE) {
        FAIL("Should be SAT with assumption ~x");
    }

    // y must be true to satisfy (x ∨ y) with x=false
    if (solver_model_value(s, y) != TRUE) {
        FAIL("y should be true in solution");
    }

    solver_free(s);
    PASS();
}

void test_multiple_solves() {
    TEST("Multiple solve calls");

    Solver* s = solver_new();

    Var x = solver_new_var(s);

    // Add clause: (x)
    Lit lits[1];
    lits[0] = mkLit(x, false);
    solver_add_clause(s, lits, 1);

    // First solve
    lbool result1 = solver_solve(s);
    if (result1 != TRUE) {
        FAIL("First solve should be SAT");
    }

    // Second solve (should give same result)
    lbool result2 = solver_solve(s);
    if (result2 != TRUE) {
        FAIL("Second solve should also be SAT");
    }

    solver_free(s);
    PASS();
}

/*********************************************************************
 * Main Test Runner
 *********************************************************************/

int main() {
    printf("========================================\n");
    printf("BSAT Core Solver Unit Tests\n");
    printf("========================================\n\n");

    // Basic tests
    test_solver_creation();
    test_add_variables();
    test_add_clause();

    // Solving tests
    test_empty_formula_sat();
    test_single_unit_clause();
    test_contradiction();
    test_simple_sat();

    // Advanced tests
    test_statistics_tracking();
    test_conflict_limit();
    test_assumptions();
    test_multiple_solves();

    printf("\n========================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("========================================\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
