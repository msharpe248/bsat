/*********************************************************************
 * BSAT C Solver - Watch Manager Tests
 *
 * Tests for watch.c: two-watched literal scheme, watch list management,
 * adding/removing watches, and blocking literal optimization.
 *********************************************************************/

#include "../include/watch.h"
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

void test_watch_manager_creation() {
    TEST("Watch manager creation and destruction");

    WatchManager* wm = watch_init(10);  // 10 variables
    if (wm == NULL) {
        FAIL("Failed to create watch manager");
    }

    if (wm->lists == NULL) {
        FAIL("Watch lists not allocated");
    }

    if (wm->num_vars != 10) {
        FAIL("Number of variables incorrect");
    }

    // Should have 2 * num_vars lists (positive and negative for each var)
    // Check a few lists are initialized
    for (Var v = 1; v <= 10; v++) {
        WatchList* pos_list = watch_list(wm, mkLit(v, false));
        WatchList* neg_list = watch_list(wm, mkLit(v, true));

        if (pos_list == NULL || neg_list == NULL) {
            FAIL("Watch lists not properly initialized");
        }

        if (pos_list->size != 0) {
            FAIL("New watch list should have size 0");
        }
    }

    watch_free(wm);
    PASS();
}

void test_add_single_watch() {
    TEST("Add single watch");

    Arena* arena = arena_init(1024);
    WatchManager* wm = watch_init(5);

    // Create a clause and add it to arena
    Lit lits[3] = {mkLit(1, false), mkLit(2, false), mkLit(3, true)};
    CRef cref = arena_alloc(arena, lits, 3, false);

    // Add watch for literal x1 watching this clause
    // Use x2 as blocker
    watch_add(wm, mkLit(1, false), cref, mkLit(2, false));

    // Check watch was added
    WatchList* list = watch_list(wm, mkLit(1, false));
    if (list->size != 1) {
        FAIL("Watch list should have 1 watch");
    }

    if (list->watches[0].cref != cref) {
        FAIL("Watch cref doesn't match");
    }

    if (list->watches[0].blocker != mkLit(2, false)) {
        FAIL("Watch blocker doesn't match");
    }

    watch_free(wm);
    arena_free(arena);
    PASS();
}

void test_add_multiple_watches_same_literal() {
    TEST("Add multiple watches to same literal");

    Arena* arena = arena_init(1024);
    WatchManager* wm = watch_init(5);

    // Create multiple clauses
    Lit lits1[2] = {mkLit(1, false), mkLit(2, false)};
    Lit lits2[2] = {mkLit(1, false), mkLit(3, false)};
    Lit lits3[2] = {mkLit(1, false), mkLit(4, false)};

    CRef cref1 = arena_alloc(arena, lits1, 2, false);
    CRef cref2 = arena_alloc(arena, lits2, 2, false);
    CRef cref3 = arena_alloc(arena, lits3, 2, false);

    // All watch literal x1
    watch_add(wm, mkLit(1, false), cref1, mkLit(2, false));
    watch_add(wm, mkLit(1, false), cref2, mkLit(3, false));
    watch_add(wm, mkLit(1, false), cref3, mkLit(4, false));

    // Check all watches added
    WatchList* list = watch_list(wm, mkLit(1, false));
    if (list->size != 3) {
        FAIL("Watch list should have 3 watches");
    }

    watch_free(wm);
    arena_free(arena);
    PASS();
}

void test_add_watches_different_literals() {
    TEST("Add watches to different literals");

    Arena* arena = arena_init(1024);
    WatchManager* wm = watch_init(5);

    // Create a clause
    Lit lits[3] = {mkLit(1, false), mkLit(2, true), mkLit(3, false)};
    CRef cref = arena_alloc(arena, lits, 3, false);

    // Add two watches (standard for 2-watched literal scheme)
    // Watch on x1 and ~x2
    watch_add(wm, mkLit(1, false), cref, mkLit(2, true));
    watch_add(wm, mkLit(2, true), cref, mkLit(1, false));

    // Check both watch lists
    WatchList* list1 = watch_list(wm, mkLit(1, false));
    WatchList* list2 = watch_list(wm, mkLit(2, true));

    if (list1->size != 1) {
        FAIL("List for x1 should have 1 watch");
    }

    if (list2->size != 1) {
        FAIL("List for ~x2 should have 1 watch");
    }

    watch_free(wm);
    arena_free(arena);
    PASS();
}

void test_remove_clause_watches() {
    TEST("Remove all watches for a clause");

    Arena* arena = arena_init(1024);
    WatchManager* wm = watch_init(5);

    // Create a clause with two watches
    Lit lits[3] = {mkLit(1, false), mkLit(2, false), mkLit(3, false)};
    CRef cref = arena_alloc(arena, lits, 3, false);

    // Add two watches
    watch_add(wm, mkLit(1, false), cref, mkLit(2, false));
    watch_add(wm, mkLit(2, false), cref, mkLit(1, false));

    // Verify watches added
    if (watch_list(wm, mkLit(1, false))->size != 1) {
        FAIL("Should have 1 watch before removal");
    }
    if (watch_list(wm, mkLit(2, false))->size != 1) {
        FAIL("Should have 1 watch before removal");
    }

    // Remove all watches for this clause
    watch_remove_clause(wm, arena, cref);

    // Verify watches removed
    if (watch_list(wm, mkLit(1, false))->size != 0) {
        FAIL("Watch should be removed from x1");
    }
    if (watch_list(wm, mkLit(2, false))->size != 0) {
        FAIL("Watch should be removed from x2");
    }

    watch_free(wm);
    arena_free(arena);
    PASS();
}

void test_watch_clear() {
    TEST("Clear all watches");

    Arena* arena = arena_init(1024);
    WatchManager* wm = watch_init(5);

    // Add several watches
    Lit lits[2] = {mkLit(1, false), mkLit(2, false)};
    CRef cref1 = arena_alloc(arena, lits, 2, false);
    CRef cref2 = arena_alloc(arena, lits, 2, false);

    watch_add(wm, mkLit(1, false), cref1, mkLit(2, false));
    watch_add(wm, mkLit(2, false), cref1, mkLit(1, false));
    watch_add(wm, mkLit(1, false), cref2, mkLit(2, false));
    watch_add(wm, mkLit(3, true), cref2, mkLit(1, false));

    // Clear all watches
    watch_clear(wm);

    // Check all lists are empty
    for (Var v = 1; v <= 5; v++) {
        if (watch_list(wm, mkLit(v, false))->size != 0) {
            FAIL("Positive literal watch list should be empty after clear");
        }
        if (watch_list(wm, mkLit(v, true))->size != 0) {
            FAIL("Negative literal watch list should be empty after clear");
        }
    }

    watch_free(wm);
    arena_free(arena);
    PASS();
}

void test_blocker_literal() {
    TEST("Blocking literal optimization");

    Arena* arena = arena_init(1024);
    WatchManager* wm = watch_init(5);

    // Create a clause: (x1 ∨ x2 ∨ x3)
    Lit lits[3] = {mkLit(1, false), mkLit(2, false), mkLit(3, false)};
    CRef cref = arena_alloc(arena, lits, 3, false);

    // Watch on x1 with x2 as blocker
    Lit watched_lit = mkLit(1, false);
    Lit blocker = mkLit(2, false);
    watch_add(wm, watched_lit, cref, blocker);

    // Retrieve the watch
    WatchList* list = watch_list(wm, watched_lit);
    if (list->size != 1) {
        FAIL("Should have 1 watch");
    }

    // Verify blocker is stored correctly
    if (list->watches[0].blocker != blocker) {
        FAIL("Blocker literal not stored correctly");
    }

    // The blocker should be a different literal from the watched literal
    if (list->watches[0].blocker == watched_lit) {
        FAIL("Blocker should be different from watched literal");
    }

    watch_free(wm);
    arena_free(arena);
    PASS();
}

void test_positive_and_negative_literals() {
    TEST("Positive and negative literal watches");

    Arena* arena = arena_init(1024);
    WatchManager* wm = watch_init(5);

    // Create clauses with positive and negative literals
    Lit lits1[2] = {mkLit(1, false), mkLit(2, false)};  // (x1 ∨ x2)
    Lit lits2[2] = {mkLit(1, true), mkLit(2, true)};    // (~x1 ∨ ~x2)

    CRef cref1 = arena_alloc(arena, lits1, 2, false);
    CRef cref2 = arena_alloc(arena, lits2, 2, false);

    // Watch positive x1 for first clause
    watch_add(wm, mkLit(1, false), cref1, mkLit(2, false));

    // Watch negative x1 for second clause
    watch_add(wm, mkLit(1, true), cref2, mkLit(2, true));

    // Check positive literal list
    WatchList* pos_list = watch_list(wm, mkLit(1, false));
    if (pos_list->size != 1) {
        FAIL("Positive x1 should have 1 watch");
    }

    // Check negative literal list
    WatchList* neg_list = watch_list(wm, mkLit(1, true));
    if (neg_list->size != 1) {
        FAIL("Negative x1 should have 1 watch");
    }

    // They should be different watches
    if (pos_list->watches[0].cref == neg_list->watches[0].cref) {
        FAIL("Positive and negative should watch different clauses");
    }

    watch_free(wm);
    arena_free(arena);
    PASS();
}

void test_watch_list_growth() {
    TEST("Watch list automatic growth");

    Arena* arena = arena_init(1024);
    WatchManager* wm = watch_init(2);

    // Add many watches to same literal to force growth
    for (int i = 0; i < 100; i++) {
        Lit lits[2] = {mkLit(1, false), mkLit(2, false)};
        CRef cref = arena_alloc(arena, lits, 2, false);
        watch_add(wm, mkLit(1, false), cref, mkLit(2, false));
    }

    // Check all watches were added
    WatchList* list = watch_list(wm, mkLit(1, false));
    if (list->size != 100) {
        FAIL("Should have 100 watches after adding 100");
    }

    watch_free(wm);
    arena_free(arena);
    PASS();
}

void test_binary_clause_watches(void) {
    TEST("Binary clause (2 literals) watches");

    Arena* arena = arena_init(1024);
    WatchManager* wm = watch_init(5);

    // Create a binary clause: (x1 ∨ x2)
    Lit lits[2] = {mkLit(1, false), mkLit(2, false)};
    CRef cref = arena_alloc(arena, lits, 2, false);

    // Binary clauses still use 2-watched literals
    // Each literal watches the other
    watch_add(wm, mkLit(1, false), cref, mkLit(2, false));
    watch_add(wm, mkLit(2, false), cref, mkLit(1, false));

    // Verify both watches
    if (watch_list(wm, mkLit(1, false))->size != 1) {
        FAIL("x1 should have 1 watch");
    }
    if (watch_list(wm, mkLit(2, false))->size != 1) {
        FAIL("x2 should have 1 watch");
    }

    // Verify blockers are correct
    Watch* w1 = &watch_list(wm, mkLit(1, false))->watches[0];
    Watch* w2 = &watch_list(wm, mkLit(2, false))->watches[0];

    if (w1->blocker != mkLit(2, false)) {
        FAIL("x1's blocker should be x2");
    }
    if (w2->blocker != mkLit(1, false)) {
        FAIL("x2's blocker should be x1");
    }

    watch_free(wm);
    arena_free(arena);
    PASS();
}

void test_watch_resize(void) {
    TEST("Watch manager in-place resize");

    Arena* arena = arena_init(1024);
    WatchManager* wm = watch_init(5);  // Start with 5 variables

    // Add watches for variables 1-5
    Lit lits[2] = {mkLit(1, false), mkLit(2, false)};
    CRef cref1 = arena_alloc(arena, lits, 2, false);
    watch_add(wm, mkLit(1, false), cref1, mkLit(2, false));
    watch_add(wm, mkLit(5, true), cref1, mkLit(1, false));

    // Verify initial watches exist
    if (watch_list(wm, mkLit(1, false))->size != 1) {
        FAIL("Variable 1 should have 1 watch before resize");
    }
    if (watch_list(wm, mkLit(5, true))->size != 1) {
        FAIL("Variable 5 (neg) should have 1 watch before resize");
    }

    // Resize to handle more variables (in-place growth)
    bool resized = watch_resize(wm, 20);
    if (!resized) {
        FAIL("watch_resize should succeed");
    }

    if (wm->num_vars != 20) {
        FAIL("num_vars should be 20 after resize");
    }

    // Verify original watches are preserved
    if (watch_list(wm, mkLit(1, false))->size != 1) {
        FAIL("Variable 1 watch should be preserved after resize");
    }
    if (watch_list(wm, mkLit(5, true))->size != 1) {
        FAIL("Variable 5 (neg) watch should be preserved after resize");
    }

    // Verify new variables have empty watch lists
    if (watch_list(wm, mkLit(10, false))->size != 0) {
        FAIL("New variable 10 should have empty watch list");
    }
    if (watch_list(wm, mkLit(20, true))->size != 0) {
        FAIL("New variable 20 (neg) should have empty watch list");
    }

    // Can add watches for new variables
    Lit lits2[2] = {mkLit(15, false), mkLit(20, false)};
    CRef cref2 = arena_alloc(arena, lits2, 2, false);
    watch_add(wm, mkLit(15, false), cref2, mkLit(20, false));

    if (watch_list(wm, mkLit(15, false))->size != 1) {
        FAIL("Should be able to add watch for new variable 15");
    }

    // Resize to same size should be no-op
    bool resized_same = watch_resize(wm, 20);
    if (!resized_same) {
        FAIL("Resize to same size should succeed");
    }

    // Resize to smaller should be no-op (keeps larger)
    bool resized_smaller = watch_resize(wm, 10);
    if (!resized_smaller) {
        FAIL("Resize to smaller should succeed (no-op)");
    }
    if (wm->num_vars != 20) {
        FAIL("Resize to smaller should not shrink");
    }

    watch_free(wm);
    arena_free(arena);
    PASS();
}

/*********************************************************************
 * Main Test Runner
 *********************************************************************/

int main(void) {
    printf("========================================\n");
    printf("BSAT Watch Manager Unit Tests\n");
    printf("========================================\n\n");

    // Basic tests
    test_watch_manager_creation();
    test_add_single_watch();
    test_add_multiple_watches_same_literal();
    test_add_watches_different_literals();

    // Operations
    test_remove_clause_watches();
    test_watch_clear();

    // Feature tests
    test_blocker_literal();
    test_positive_and_negative_literals();
    test_binary_clause_watches();

    // Edge cases
    test_watch_list_growth();

    // New optimization tests
    test_watch_resize();

    printf("\n========================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("========================================\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
