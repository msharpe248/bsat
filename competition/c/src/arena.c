/*********************************************************************
 * BSAT Competition Solver - Arena Memory Allocator Implementation
 *********************************************************************/

#include "../include/arena.h"
#include <stdio.h>
#include <string.h>

// Initial capacity in uint32_t units (16MB)
#define INITIAL_CAPACITY (4 * 1024 * 1024)

// Growth factor when expanding arena
#define GROWTH_FACTOR 1.5

/*********************************************************************
 * Arena Management
 *********************************************************************/

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
    size_t new_capacity = arena->capacity;

    // Calculate required capacity
    while (new_capacity < arena->size + needed) {
        new_capacity = (size_t)(new_capacity * GROWTH_FACTOR);
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

    return true;
}

CRef arena_alloc(Arena* arena, const Lit* lits, uint32_t size, bool learned) {
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
    header->size = size;
    header->flags = learned ? CLAUSE_LEARNED : CLAUSE_ORIGINAL;
    header->lbd = 0;
    header->activity = 0.0f;

    // Copy literals
    Lit* dest = CLAUSE_LITS(arena, cref);
    memcpy(dest, lits, size * sizeof(Lit));

    // Update arena size
    arena->size += total_words;

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

// Helper to relocate a clause during GC
static CRef relocate_clause(uint32_t* dest, uint32_t* src, CRef old_cref) {
    ClauseHeader* header = (ClauseHeader*)src;
    size_t header_words = (sizeof(ClauseHeader) + sizeof(uint32_t) - 1) / sizeof(uint32_t);
    size_t total_words = header_words + header->size;

    // Copy entire clause
    memcpy(dest, src, total_words * sizeof(uint32_t));

    // Return new location
    return (CRef)(dest - ((Arena*)NULL)->memory);  // Offset from start
}

void arena_gc(Arena* arena, CRef** watches, uint32_t num_watches,
              CRef* clauses, uint32_t* num_clauses) {

    // Only GC if wasted space is significant (>25% of used space)
    if (arena->wasted * 4 < arena->size) {
        return;
    }

    // Create relocation map
    CRef* reloc = (CRef*)calloc(arena->size, sizeof(CRef));
    if (!reloc) return;  // Can't GC without map

    // Compact arena
    uint32_t dest = 1;  // Start after reserved index
    uint32_t src = 1;

    while (src < arena->size) {
        ClauseHeader* header = (ClauseHeader*)&arena->memory[src];
        size_t header_words = (sizeof(ClauseHeader) + sizeof(uint32_t) - 1) / sizeof(uint32_t);
        size_t total_words = header_words + header->size;

        if (!(header->flags & CLAUSE_DELETED)) {
            // Relocate active clause
            if (dest != src) {
                memmove(&arena->memory[dest], &arena->memory[src],
                        total_words * sizeof(uint32_t));
            }
            reloc[src] = dest;
            dest += total_words;
        } else {
            reloc[src] = INVALID_CLAUSE;
        }

        src += total_words;
    }

    // Update all references

    // Update watches
    for (uint32_t i = 0; i < num_watches; i++) {
        if (watches[i]) {
            uint32_t watch_size = 0;  // Need to track this properly
            for (uint32_t j = 0; j < watch_size; j++) {
                if (watches[i][j] != INVALID_CLAUSE) {
                    watches[i][j] = reloc[watches[i][j]];
                }
            }
        }
    }

    // Update clause array
    uint32_t j = 0;
    for (uint32_t i = 0; i < *num_clauses; i++) {
        CRef new_ref = reloc[clauses[i]];
        if (new_ref != INVALID_CLAUSE) {
            clauses[j++] = new_ref;
        }
    }
    *num_clauses = j;

    // Update arena size
    arena->size = dest;
    arena->wasted = 0;

    free(reloc);
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