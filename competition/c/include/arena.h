/*********************************************************************
 * BSAT Competition Solver - Arena Memory Allocator
 *
 * Efficient memory management for clauses using a single contiguous
 * memory region. This improves cache locality and reduces fragmentation.
 *********************************************************************/

#ifndef BSAT_ARENA_H
#define BSAT_ARENA_H

#include "types.h"
#include <stdlib.h>
#include <string.h>

/*********************************************************************
 * Arena Structure
 *********************************************************************/

typedef struct Arena {
    uint32_t* memory;      // Contiguous memory block
    size_t    size;        // Current size in uint32_t units
    size_t    capacity;    // Total capacity in uint32_t units
    size_t    wasted;      // Wasted space from deletions
} Arena;

/*********************************************************************
 * Clause Header Structure
 *
 * Stored inline before clause literals in arena.
 * Compact representation to minimize memory overhead.
 *********************************************************************/

typedef struct ClauseHeader {
    uint32_t size     : 28;  // Number of literals (max 268M)
    uint32_t flags    : 4;   // Clause flags (learned, deleted, etc.)
    uint32_t lbd;            // Literal Block Distance score
    float    activity;       // Clause activity for deletion
} ClauseHeader;

// Get clause header from CRef
#define CLAUSE_HEADER(arena, cref) ((ClauseHeader*)&(arena)->memory[cref])

// Get literals array from CRef
#define CLAUSE_LITS(arena, cref) ((Lit*)&(arena)->memory[cref + sizeof(ClauseHeader)/sizeof(uint32_t)])

// Get clause size from CRef
#define CLAUSE_SIZE(arena, cref) (CLAUSE_HEADER(arena, cref)->size)

// Calculate total memory needed for a clause
static inline size_t clause_bytes(uint32_t size) {
    return sizeof(ClauseHeader) + size * sizeof(Lit);
}

/*********************************************************************
 * Arena Operations
 *********************************************************************/

// Initialize arena with initial capacity
Arena* arena_init(size_t initial_capacity);

// Free arena and all its memory
void arena_free(Arena* arena);

// Allocate space for a new clause
// Returns INVALID_CLAUSE on failure
CRef arena_alloc(Arena* arena, const Lit* lits, uint32_t size, bool learned);

// Mark clause as deleted (doesn't free memory immediately)
void arena_delete(Arena* arena, CRef cref);

// Garbage collect deleted clauses and compact memory
// Updates all CRefs in the provided arrays
void arena_gc(Arena* arena, CRef** watches, uint32_t num_watches,
              CRef* clauses, uint32_t* num_clauses);

// Get current memory usage statistics
typedef struct {
    size_t total_bytes;     // Total allocated memory
    size_t used_bytes;      // Currently used memory
    size_t wasted_bytes;    // Wasted from deletions
    uint32_t num_clauses;   // Number of active clauses
} ArenaStats;

ArenaStats arena_stats(const Arena* arena);

/*********************************************************************
 * Inline Helper Functions
 *********************************************************************/

// Check if clause is deleted
static inline bool clause_deleted(const Arena* arena, CRef cref) {
    return (CLAUSE_HEADER(arena, cref)->flags & CLAUSE_DELETED) != 0;
}

// Check if clause is learned
static inline bool clause_learned(const Arena* arena, CRef cref) {
    return (CLAUSE_HEADER(arena, cref)->flags & CLAUSE_LEARNED) != 0;
}

// Get clause LBD
static inline uint32_t clause_lbd(const Arena* arena, CRef cref) {
    return CLAUSE_HEADER(arena, cref)->lbd;
}

// Set clause LBD
static inline void set_clause_lbd(Arena* arena, CRef cref, uint32_t lbd) {
    CLAUSE_HEADER(arena, cref)->lbd = lbd;
}

// Get clause activity
static inline float clause_activity(const Arena* arena, CRef cref) {
    return CLAUSE_HEADER(arena, cref)->activity;
}

// Bump clause activity
static inline void bump_clause_activity(Arena* arena, CRef cref, float inc) {
    CLAUSE_HEADER(arena, cref)->activity += inc;
}

#endif // BSAT_ARENA_H