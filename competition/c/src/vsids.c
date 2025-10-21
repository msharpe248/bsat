#include "solver.h"
#include <stdlib.h>

/* ============================================================================
 * VSIDS (Variable State Independent Decaying Sum) HEAP
 * ============================================================================
 * Max-heap implementation for variable selection
 * Variables with highest activity scores are selected first
 */

#define PARENT(i)       (((i) - 1) >> 1)
#define LEFT_CHILD(i)   (((i) << 1) + 1)
#define RIGHT_CHILD(i)  (((i) << 1) + 2)

static void heap_swap(VSIDSHeap *heap, uint32_t i, uint32_t j) {
    uint32_t var_i = heap->heap[i];
    uint32_t var_j = heap->heap[j];

    heap->heap[i] = var_j;
    heap->heap[j] = var_i;

    heap->positions[var_i] = j;
    heap->positions[var_j] = i;
}

static void heap_bubble_up(VSIDSHeap *heap, uint32_t i) {
    if (i >= heap->size) return;  // Bounds check

    while (i > 0) {
        uint32_t parent = PARENT(i);
        uint32_t var = heap->heap[i];
        uint32_t parent_var = heap->heap[parent];

        if (heap->activity[var] <= heap->activity[parent_var]) {
            break;
        }

        heap_swap(heap, i, parent);
        i = parent;
    }
}

static void heap_bubble_down(VSIDSHeap *heap, uint32_t i) {
    if (i >= heap->size) return;  // Bounds check

    while (true) {
        uint32_t left = LEFT_CHILD(i);
        uint32_t right = RIGHT_CHILD(i);
        uint32_t largest = i;

        if (left < heap->size) {
            uint32_t left_var = heap->heap[left];
            uint32_t largest_var = heap->heap[largest];
            if (heap->activity[left_var] > heap->activity[largest_var]) {
                largest = left;
            }
        }

        if (right < heap->size) {
            uint32_t right_var = heap->heap[right];
            uint32_t largest_var = heap->heap[largest];
            if (heap->activity[right_var] > heap->activity[largest_var]) {
                largest = right;
            }
        }

        if (largest == i) {
            break;
        }

        heap_swap(heap, i, largest);
        i = largest;
    }
}

VSIDSHeap* vsids_create(uint32_t num_vars) {
    VSIDSHeap *heap = (VSIDSHeap*)xmalloc(sizeof(VSIDSHeap));

    // Allocate arrays (indexed from 1 to num_vars)
    heap->heap = (uint32_t*)xmalloc((num_vars + 1) * sizeof(uint32_t));
    heap->positions = (uint32_t*)xmalloc((num_vars + 1) * sizeof(uint32_t));
    heap->activity = (double*)xmalloc((num_vars + 1) * sizeof(double));

    heap->capacity = num_vars;
    heap->size = 0;

    // Initialize all variables with activity 0
    for (uint32_t i = 1; i <= num_vars; i++) {
        heap->activity[i] = 0.0;
    }

    // Insert all variables into heap
    for (uint32_t i = 1; i <= num_vars; i++) {
        heap->heap[heap->size] = i;
        heap->positions[i] = heap->size;
        heap->size++;
    }

    return heap;
}

void vsids_destroy(VSIDSHeap *heap) {
    free(heap->heap);
    free(heap->positions);
    free(heap->activity);
    free(heap);
}

uint32_t vsids_select(VSIDSHeap *heap, int8_t *assignments) {
    // Heap-based selection: scan from top of heap for first unassigned variable
    // Key insight: We NEVER remove variables from heap, just skip assigned ones
    // This handles backtracking automatically - unassigned variables are already in heap

    // Scan heap from highest activity down until we find an unassigned variable
    for (uint32_t i = 0; i < heap->size; i++) {
        uint32_t var = heap->heap[i];

        if (assignments[var] == 0) {
            // Found unassigned variable
            return var;
        }
    }

    return 0;  // All variables assigned
}

void vsids_bump_var(VSIDSHeap *heap, uint32_t var, double increment) {
    // Bounds check
    if (var == 0 || var > heap->capacity) {
        return;  // Invalid variable
    }

    // Update activity
    heap->activity[var] += increment;

    // Restore heap property by bubbling up
    // The variable is always in the heap (we never remove variables)
    uint32_t pos = heap->positions[var];

    // Verify position is valid
    if (pos < heap->size && heap->heap[pos] == var) {
        heap_bubble_up(heap, pos);
    }
}

void vsids_decay(VSIDSHeap *heap, double decay) {
    // Decay all activities (done by increasing increment instead)
    // This function is kept for API compatibility but actual decay
    // is handled by increasing var_inc in the solver
    (void)heap;
    (void)decay;
}
