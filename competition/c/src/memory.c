#include "solver.h"
#include <stdlib.h>
#include <stdio.h>

/* ============================================================================
 * MEMORY MANAGEMENT UTILITIES
 * ============================================================================
 * Safe wrappers around malloc/realloc that exit on failure
 */

void* xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr && size > 0) {
        fprintf(stderr, "FATAL: malloc failed for %zu bytes\n", size);
        exit(1);
    }
    return ptr;
}

void* xrealloc(void *ptr, size_t size) {
    void *new_ptr = realloc(ptr, size);
    if (!new_ptr && size > 0) {
        fprintf(stderr, "FATAL: realloc failed for %zu bytes\n", size);
        exit(1);
    }
    return new_ptr;
}
