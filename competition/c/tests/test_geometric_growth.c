/*********************************************************************
 * Test Geometric Growth Optimization
 *
 * Verifies that variable arrays grow geometrically instead of linearly.
 *********************************************************************/

#include "../include/solver.h"
#include <stdio.h>
#include <stdlib.h>

// Required for linking (normally defined in main.c)
bool g_verbose = false;

int main(void) {
    printf("Testing geometric growth optimization\n");
    printf("=====================================\n\n");

    Solver* s = solver_new();
    if (!s) {
        fprintf(stderr, "Failed to create solver\n");
        return 1;
    }

    printf("Initial state:\n");
    printf("  num_vars: %u\n", s->num_vars);
    printf("  var_capacity: %u\n\n", s->var_capacity);

    // Add variables and track capacity changes
    uint32_t prev_capacity = s->var_capacity;
    uint32_t realloc_count = 0;
    uint32_t num_test_vars = 200;

    printf("Adding %u variables:\n", num_test_vars);
    printf("  Var# | Capacity | Growth Event\n");
    printf("  -----|----------|-------------\n");

    for (uint32_t i = 1; i <= num_test_vars; i++) {
        Var v = solver_new_var(s);
        if (v == INVALID_VAR) {
            fprintf(stderr, "Failed to add variable %u\n", i);
            solver_free(s);
            return 1;
        }

        // Check if capacity changed (reallocation occurred)
        if (s->var_capacity != prev_capacity) {
            realloc_count++;
            printf("  %4u | %8u | ✅ Grew from %u\n", i, s->var_capacity, prev_capacity);
            prev_capacity = s->var_capacity;
        }
    }

    printf("\nFinal state:\n");
    printf("  num_vars: %u\n", s->num_vars);
    printf("  var_capacity: %u\n", s->var_capacity);
    printf("  Reallocations: %u\n", realloc_count);
    printf("  Wasted capacity: %u (%.1f%%)\n",
           s->var_capacity - s->num_vars,
           100.0 * (s->var_capacity - s->num_vars) / s->var_capacity);

    // Calculate theoretical minimum reallocs for geometric growth
    uint32_t theoretical_min = 0;
    uint32_t capacity = 0;
    while (capacity < num_test_vars) {
        if (capacity == 0) {
            capacity = 128;  // VAR_INITIAL_CAPACITY
        } else {
            capacity *= 2;   // VAR_GROWTH_FACTOR
        }
        theoretical_min++;
    }

    printf("\n");
    printf("Performance analysis:\n");
    printf("  Theoretical minimum reallocs (2x growth from 128): %u\n", theoretical_min);
    printf("  Actual reallocs: %u\n", realloc_count);
    printf("  Linear growth would require: %u reallocs\n", num_test_vars);
    printf("  Improvement: %.1fx fewer reallocs\n",
           (float)num_test_vars / realloc_count);

    // Verify correctness
    if (s->num_vars != num_test_vars) {
        fprintf(stderr, "\n❌ FAIL: Expected %u variables, got %u\n",
                num_test_vars, s->num_vars);
        solver_free(s);
        return 1;
    }

    if (realloc_count > theoretical_min + 1) {
        fprintf(stderr, "\n❌ FAIL: Too many reallocations (%u > %u)\n",
                realloc_count, theoretical_min + 1);
        solver_free(s);
        return 1;
    }

    printf("\n✅ SUCCESS: Geometric growth working correctly!\n");

    solver_free(s);
    return 0;
}
