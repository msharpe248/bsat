/*********************************************************************
 * BSAT Competition Solver - Arena Memory Allocator Implementation
 *********************************************************************/

#include "../include/arena.h"
#include <stdio.h>
#include <string.h>

// Start small; DIMACS reservation and geometric growth size the arena.
#define INITIAL_CAPACITY 1024

// Growth factor when expanding arena
#define GROWTH_FACTOR 1.5

/*********************************************************************
 * Arena Management
 *********************************************************************/

size_t estimate_arena_size(uint32_t num_clauses, uint32_t num_vars) {
    // Estimate space needed:
    // - ClauseHeader: 3 words (12 bytes for size/flags/lbd + 4 bytes for activity)
    // - Average literals per clause: assume 3 for 3-SAT
    // So each clause needs ~6 words on average

    size_t clause_space = (size_t)num_clauses * 6;

    // Learned clauses: assume we'll learn 50% as many as original
    size_t learned_space = clause_space / 2;

    // Variable overhead: minimal, ~1 word per variable
    size_t var_space = num_vars;

    // Total with 25% safety margin
    size_t total = (clause_space + learned_space + var_space) * 5 / 4;

    // Enforce minimum of 1024 words (4KB)
    if (total < 1024) {
        total = 1024;
    }

    // Cap at 10M words (40MB) to avoid huge initial allocations
    if (total > 10000000) {
        total = 10000000;
    }

    return total;
}

Arena* arena_init(size_t initial_capacity) {
    Arena* arena = (Arena*)malloc(sizeof(Arena));
    if (!arena) return NULL;

    // Use default if no capacity specified
    if (initial_capacity == 0) {
        initial_capacity = INITIAL_CAPACITY;
    }

    arena->memory = (uint32_t*)malloc(initial_capacity * sizeof(uint32_t));
    if (!arena->memory) {
        free(arena);
        return NULL;
    }

    arena->size = 1;  // Reserve index 0 as invalid
    arena->capacity = initial_capacity;
    arena->wasted = 0;
    arena->num_growths = 0;
    arena->peak_size = 1;

    // Initialize first word to prevent CRef 0 from being valid
    arena->memory[0] = 0;

    return arena;
}

void arena_free(Arena* arena) {
    if (arena) {
        free(arena->memory);
        free(arena);
    }
}

/*********************************************************************
 * Allocation
 *********************************************************************/

static bool arena_grow(Arena* arena, size_t needed) {
    size_t old_capacity = arena->capacity;
    size_t new_capacity = arena->capacity;

    // Calculate required capacity
    while (new_capacity < arena->size + needed) {
        new_capacity += new_capacity / 2 + 1;
        if (new_capacity > MAX_CLAUSES) {
            return false;  // Hit maximum size
        }
    }

    // Reallocate memory
    uint32_t* new_memory = (uint32_t*)realloc(arena->memory,
                                               new_capacity * sizeof(uint32_t));
    if (!new_memory) {
        return false;  // Out of memory
    }

    arena->memory = new_memory;
    arena->capacity = new_capacity;
    arena->num_growths++;

    // Log growth if verbose (use global flag)
    if (g_verbose) {
        fprintf(stderr, "c [Arena] Grew from %zu to %zu words (%.1f KB -> %.1f KB) [growth #%u]\n",
                old_capacity, new_capacity,
                old_capacity * sizeof(uint32_t) / 1024.0,
                new_capacity * sizeof(uint32_t) / 1024.0,
                arena->num_growths);
    }

    return true;
}

bool arena_reserve(Arena* arena, size_t min_capacity) {
    // Already have enough capacity
    if (arena->capacity >= min_capacity) {
        return true;
    }

    // Calculate new capacity
    size_t new_capacity = arena->capacity;
    while (new_capacity < min_capacity) {
        new_capacity += new_capacity / 2 + 1;
        if (new_capacity > MAX_CLAUSES) {
            new_capacity = MAX_CLAUSES;
            if (new_capacity < min_capacity) {
                return false;  // Cannot satisfy request
            }
            break;
        }
    }

    // Reallocate memory
    uint32_t* new_memory = (uint32_t*)realloc(arena->memory,
                                               new_capacity * sizeof(uint32_t));
    if (!new_memory) {
        return false;  // Out of memory
    }

    size_t old_capacity = arena->capacity;
    arena->memory = new_memory;
    arena->capacity = new_capacity;

    // Log reservation if verbose
    if (g_verbose) {
        fprintf(stderr, "c [Arena] Reserved %zu words (%.1f MB) based on problem size\n",
                new_capacity, new_capacity * sizeof(uint32_t) / (1024.0 * 1024.0));
        fprintf(stderr, "c [Arena] Growth from %zu to %zu words (%.1f KB -> %.1f KB)\n",
                old_capacity, new_capacity,
                old_capacity * sizeof(uint32_t) / 1024.0,
                new_capacity * sizeof(uint32_t) / 1024.0);
    }

    return true;
}

CRef arena_alloc(Arena* arena, const Lit* lits, uint32_t size, bool learned) {
    if (size >= (1u << 28) || arena->size + size + sizeof(ClauseHeader)/4 >= BINARY_CONFLICT) return INVALID_CLAUSE;
    // Calculate space needed
    size_t header_words = (sizeof(ClauseHeader) + sizeof(uint32_t) - 1) / sizeof(uint32_t);
    size_t total_words = header_words + size;

    // Ensure we have enough space
    if (arena->size + total_words > arena->capacity) {
        if (!arena_grow(arena, total_words)) {
            return INVALID_CLAUSE;  // Allocation failed
        }
    }

    // Allocate at current position
    CRef cref = arena->size;

    // Initialize clause header
    ClauseHeader* header = CLAUSE_HEADER(arena, cref);
    header->search = 2;
    header->size = size;
    header->flags = learned ? CLAUSE_LEARNED : CLAUSE_ORIGINAL;
    header->lbd = 0;
    header->activity = 0.0f;

    // Copy literals
    Lit* dest = CLAUSE_LITS(arena, cref);
    if (size) memcpy(dest, lits, size * sizeof(Lit));

    // Update arena size
    arena->size += total_words;

    // Track peak size
    if (arena->size > arena->peak_size) {
        arena->peak_size = arena->size;
    }

    return cref;
}

/*********************************************************************
 * Deletion and Garbage Collection
 *********************************************************************/

void arena_delete(Arena* arena, CRef cref) {
    if (cref == INVALID_CLAUSE) return;

    ClauseHeader* header = CLAUSE_HEADER(arena, cref);
    if (header->flags & CLAUSE_DELETED) return;  // Already deleted

    header->flags |= CLAUSE_DELETED;

    // Track wasted space
    size_t header_words = (sizeof(ClauseHeader) + sizeof(uint32_t) - 1) / sizeof(uint32_t);
    arena->wasted += header_words + header->size;
}

/*********************************************************************
 * Statistics
 *********************************************************************/

ArenaStats arena_stats(const Arena* arena) {
    ArenaStats stats = {0};

    stats.total_bytes = arena->capacity * sizeof(uint32_t);
    stats.used_bytes = arena->size * sizeof(uint32_t);
    stats.wasted_bytes = arena->wasted * sizeof(uint32_t);

    // Count active clauses
    uint32_t pos = 1;
    while (pos < arena->size) {
        ClauseHeader* header = (ClauseHeader*)&arena->memory[pos];
        size_t header_words = (sizeof(ClauseHeader) + sizeof(uint32_t) - 1) / sizeof(uint32_t);

        if (!(header->flags & CLAUSE_DELETED)) {
            stats.num_clauses++;
        }

        pos += header_words + header->size;
    }

    return stats;
}