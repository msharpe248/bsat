/*********************************************************************
 * BSAT C Solver - Arena Memory Allocator Tests
 *
 * Tests for arena.c: clause memory management, allocation, deletion,
 * garbage collection, and statistics.
 *********************************************************************/

#include "../include/arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// Required for linking (normally defined in main.c)
bool g_verbose = false;

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

void test_arena_creation() {
    TEST("Arena creation and destruction");

    Arena* arena = arena_init(1024);
    if (arena == NULL) {
        FAIL("Failed to create arena");
    }

    if (arena->memory == NULL) {
        FAIL("Arena memory not allocated");
    }

    if (arena->capacity == 0) {
        FAIL("Arena capacity should be > 0");
    }

    if (arena->size != 1) {
        FAIL("New arena should have size 1 (index 0 reserved as invalid)");
    }

    arena_free(arena);
    PASS();
}

void test_single_clause_alloc() {
    TEST("Single clause allocation");

    Arena* arena = arena_init(1024);

    // Allocate a clause: (x1 ∨ x2 ∨ x3)
    Lit lits[3] = {mkLit(1, false), mkLit(2, false), mkLit(3, false)};
    CRef cref = arena_alloc(arena, lits, 3, false);

    if (cref == INVALID_CLAUSE) {
        FAIL("Allocation failed");
    }

    // Verify clause size
    if (CLAUSE_SIZE(arena, cref) != 3) {
        FAIL("Clause size mismatch");
    }

    // Verify clause literals
    Lit* stored = CLAUSE_LITS(arena, cref);
    for (int i = 0; i < 3; i++) {
        if (stored[i] != lits[i]) {
            FAIL("Literal mismatch");
        }
    }

    // Verify it's not marked as learned
    if (clause_learned(arena, cref)) {
        FAIL("Clause should not be learned");
    }

    // Verify it's not deleted
    if (clause_deleted(arena, cref)) {
        FAIL("Clause should not be deleted");
    }

    arena_free(arena);
    PASS();
}

void test_learned_clause() {
    TEST("Learned clause allocation");

    Arena* arena = arena_init(1024);

    // Allocate a learned clause
    Lit lits[2] = {mkLit(1, false), mkLit(2, true)};
    CRef cref = arena_alloc(arena, lits, 2, true);  // learned=true

    if (cref == INVALID_CLAUSE) {
        FAIL("Allocation failed");
    }

    // Verify it's marked as learned
    if (!clause_learned(arena, cref)) {
        FAIL("Clause should be learned");
    }

    arena_free(arena);
    PASS();
}

void test_multiple_clauses() {
    TEST("Multiple clause allocations");

    Arena* arena = arena_init(1024);

    // Allocate multiple clauses of different sizes
    Lit lits1[1] = {mkLit(1, false)};
    Lit lits2[2] = {mkLit(2, false), mkLit(3, true)};
    Lit lits3[3] = {mkLit(4, false), mkLit(5, false), mkLit(6, true)};

    CRef cref1 = arena_alloc(arena, lits1, 1, false);
    CRef cref2 = arena_alloc(arena, lits2, 2, false);
    CRef cref3 = arena_alloc(arena, lits3, 3, true);

    if (cref1 == INVALID_CLAUSE || cref2 == INVALID_CLAUSE || cref3 == INVALID_CLAUSE) {
        FAIL("One or more allocations failed");
    }

    // Verify all clauses have correct sizes
    if (CLAUSE_SIZE(arena, cref1) != 1) FAIL("Clause 1 size wrong");
    if (CLAUSE_SIZE(arena, cref2) != 2) FAIL("Clause 2 size wrong");
    if (CLAUSE_SIZE(arena, cref3) != 3) FAIL("Clause 3 size wrong");

    // Verify learned flag
    if (clause_learned(arena, cref1)) FAIL("Clause 1 should not be learned");
    if (clause_learned(arena, cref2)) FAIL("Clause 2 should not be learned");
    if (!clause_learned(arena, cref3)) FAIL("Clause 3 should be learned");

    arena_free(arena);
    PASS();
}

void test_lbd_operations() {
    TEST("LBD (Literal Block Distance) operations");

    Arena* arena = arena_init(1024);

    Lit lits[2] = {mkLit(1, false), mkLit(2, false)};
    CRef cref = arena_alloc(arena, lits, 2, true);

    // Initially LBD should be 0
    if (clause_lbd(arena, cref) != 0) {
        FAIL("Initial LBD should be 0");
    }

    // Set LBD
    set_clause_lbd(arena, cref, 5);
    if (clause_lbd(arena, cref) != 5) {
        FAIL("LBD should be 5 after setting");
    }

    // Update LBD
    set_clause_lbd(arena, cref, 2);
    if (clause_lbd(arena, cref) != 2) {
        FAIL("LBD should be 2 after update");
    }

    arena_free(arena);
    PASS();
}

void test_activity_operations() {
    TEST("Clause activity operations");

    Arena* arena = arena_init(1024);

    Lit lits[2] = {mkLit(1, false), mkLit(2, false)};
    CRef cref = arena_alloc(arena, lits, 2, true);

    // Initially activity should be 0
    if (clause_activity(arena, cref) != 0.0f) {
        FAIL("Initial activity should be 0.0");
    }

    // Bump activity
    bump_clause_activity(arena, cref, 1.5f);
    if (clause_activity(arena, cref) < 1.4f || clause_activity(arena, cref) > 1.6f) {
        FAIL("Activity should be ~1.5 after bump");
    }

    // Bump again
    bump_clause_activity(arena, cref, 2.5f);
    if (clause_activity(arena, cref) < 3.9f || clause_activity(arena, cref) > 4.1f) {
        FAIL("Activity should be ~4.0 after second bump");
    }

    arena_free(arena);
    PASS();
}

void test_clause_deletion() {
    TEST("Clause deletion");

    Arena* arena = arena_init(1024);

    Lit lits[2] = {mkLit(1, false), mkLit(2, false)};
    CRef cref = arena_alloc(arena, lits, 2, false);

    // Initially not deleted
    if (clause_deleted(arena, cref)) {
        FAIL("Clause should not be deleted initially");
    }

    // Mark as deleted
    arena_delete(arena, cref);
    if (!clause_deleted(arena, cref)) {
        FAIL("Clause should be deleted after arena_delete");
    }

    arena_free(arena);
    PASS();
}

void test_arena_stats() {
    TEST("Arena statistics");

    Arena* arena = arena_init(1024);

    // Initially has 1 word reserved (index 0)
    ArenaStats stats = arena_stats(arena);
    if (stats.used_bytes != sizeof(uint32_t)) {
        FAIL("Initial used_bytes should be sizeof(uint32_t) (index 0 reserved)");
    }

    // Allocate a clause
    Lit lits[3] = {mkLit(1, false), mkLit(2, false), mkLit(3, false)};
    arena_alloc(arena, lits, 3, false);

    // Check stats updated
    stats = arena_stats(arena);
    if (stats.used_bytes == 0) {
        FAIL("used_bytes should be > 0 after allocation");
    }

    if (stats.total_bytes == 0) {
        FAIL("total_bytes should be > 0");
    }

    arena_free(arena);
    PASS();
}

void test_empty_clause() {
    TEST("Empty clause handling");

    Arena* arena = arena_init(1024);

    // Try to allocate empty clause
    CRef cref = arena_alloc(arena, NULL, 0, false);

    if (cref == INVALID_CLAUSE) {
        FAIL("Should be able to allocate empty clause");
    }

    if (CLAUSE_SIZE(arena, cref) != 0) {
        FAIL("Empty clause should have size 0");
    }

    arena_free(arena);
    PASS();
}

void test_large_clause() {
    TEST("Large clause allocation");

    Arena* arena = arena_init(1024);

    // Allocate a large clause (100 literals)
    Lit lits[100];
    for (int i = 0; i < 100; i++) {
        lits[i] = mkLit(i + 1, i % 2 == 0);
    }

    CRef cref = arena_alloc(arena, lits, 100, false);

    if (cref == INVALID_CLAUSE) {
        FAIL("Failed to allocate large clause");
    }

    if (CLAUSE_SIZE(arena, cref) != 100) {
        FAIL("Large clause size mismatch");
    }

    // Verify all literals stored correctly
    Lit* stored = CLAUSE_LITS(arena, cref);
    for (int i = 0; i < 100; i++) {
        if (stored[i] != lits[i]) {
            FAIL("Literal mismatch in large clause");
        }
    }

    arena_free(arena);
    PASS();
}

void test_arena_growth() {
    TEST("Arena automatic growth");

    // Start with small arena
    Arena* arena = arena_init(16);
    size_t initial_capacity = arena->capacity;

    // Allocate many clauses to force growth
    for (int i = 0; i < 100; i++) {
        Lit lits[3] = {mkLit(1, false), mkLit(2, false), mkLit(3, false)};
        CRef cref = arena_alloc(arena, lits, 3, false);
        if (cref == INVALID_CLAUSE) {
            FAIL("Allocation failed during growth");
        }
    }

    // Capacity should have grown
    if (arena->capacity <= initial_capacity) {
        FAIL("Arena should have grown");
    }

    arena_free(arena);
    PASS();
}

/*********************************************************************
 * Main Test Runner
 *********************************************************************/

int main(void) {
    printf("========================================\n");
    printf("BSAT Arena Allocator Unit Tests\n");
    printf("========================================\n\n");

    // Basic tests
    test_arena_creation();
    test_single_clause_alloc();
    test_learned_clause();
    test_multiple_clauses();

    // Feature tests
    test_lbd_operations();
    test_activity_operations();
    test_clause_deletion();
    test_arena_stats();

    // Edge cases
    test_empty_clause();
    test_large_clause();
    test_arena_growth();

    printf("\n========================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("========================================\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
