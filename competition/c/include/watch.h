/*********************************************************************
 * BSAT Competition Solver - Watch Lists for Two-Watched Literals
 *
 * Efficient implementation of the two-watched literal scheme for
 * unit propagation. Uses blocking literals for cache optimization.
 *********************************************************************/

#ifndef BSAT_WATCH_H
#define BSAT_WATCH_H

#include "types.h"
#include "arena.h"

/*********************************************************************
 * Watch Structure
 *
 * Compact representation of a watched clause with blocking literal.
 * The blocking literal optimization avoids unnecessary clause visits.
 *********************************************************************/

typedef struct Watch {
    CRef cref;        // Clause reference
    Lit  blocker;     // Blocking literal (satisfied => skip clause)
} Watch;

/*********************************************************************
 * Watch List Structure
 *
 * Dynamic array of watches for each literal.
 * Indexed by literal value (toInt(lit)).
 *********************************************************************/

typedef struct WatchList {
    Watch*   watches;     // Dynamic array of watches
    uint32_t size;        // Current number of watches
    uint32_t capacity;    // Allocated capacity
} WatchList;

/*********************************************************************
 * Watch Manager
 *
 * Manages all watch lists for the solver.
 *********************************************************************/

typedef struct WatchManager {
    WatchList* lists;     // Array of watch lists (2 * num_vars)
    uint32_t   num_vars;  // Number of variables
    uint64_t   updates;   // Statistics: watch updates
    uint64_t   visits;    // Statistics: clause visits
    uint64_t   skipped;   // Statistics: skipped by blocker
} WatchManager;

/*********************************************************************
 * Watch Manager Operations
 *********************************************************************/

// Initialize watch manager for given number of variables
WatchManager* watch_init(uint32_t num_vars);

// Resize watch manager to handle more variables (in-place, preserves existing watches)
bool watch_resize(WatchManager* wm, uint32_t new_num_vars);

// Free watch manager and all watch lists
void watch_free(WatchManager* wm);

// Clear all watches (for restart/cleanup)
void watch_clear(WatchManager* wm);

// Add a watch for literal lit watching clause cref
// The blocker should be another literal in the clause
void watch_add(WatchManager* wm, Lit lit, CRef cref, Lit blocker);

// Remove all watches for a clause (when deleting clause)
void watch_remove_clause(WatchManager* wm, Arena* arena, CRef cref);

// Get watch list for a literal
static inline WatchList* watch_list(WatchManager* wm, Lit lit) {
    return &wm->lists[toInt(lit)];
}

/*********************************************************************
 * Watch List Operations
 *********************************************************************/

// Ensure watch list has capacity for at least one more watch
static inline bool watchlist_ensure_capacity(WatchList* wl) {
    if (wl->size >= wl->capacity) {
        uint32_t new_cap = wl->capacity ? wl->capacity * 2 : 16;
        Watch* new_watches = (Watch*)realloc(wl->watches, new_cap * sizeof(Watch));
        if (!new_watches) return false;
        wl->watches = new_watches;
        wl->capacity = new_cap;
    }
    return true;
}

// Add a watch to a watch list
static inline void watchlist_push(WatchList* wl, Watch w) {
    if (watchlist_ensure_capacity(wl)) {
        wl->watches[wl->size++] = w;
    }
}

// Remove watch at index i (swap with last and shrink)
static inline void watchlist_remove(WatchList* wl, uint32_t i) {
    ASSERT(i < wl->size);
    wl->watches[i] = wl->watches[--wl->size];
}

// Clear a watch list
static inline void watchlist_clear(WatchList* wl) {
    wl->size = 0;
}

/*********************************************************************
 * Binary Clause Optimization
 *
 * Binary clauses don't need full watch structure.
 * Store them implicitly in watch lists for faster propagation.
 *********************************************************************/

// Check if watch represents a binary clause
static inline bool is_binary_watch(Watch w) {
    return w.cref == INVALID_CLAUSE;
}

// Create binary watch (cref = INVALID_CLAUSE, blocker = other literal)
static inline Watch make_binary_watch(Lit other) {
    return (Watch){INVALID_CLAUSE, other};
}

// Get the implied literal from binary watch
static inline Lit binary_other(Watch w) {
    ASSERT(is_binary_watch(w));
    return w.blocker;
}

/*********************************************************************
 * Watch Statistics
 *********************************************************************/

typedef struct {
    uint64_t total_watches;   // Total number of watches
    uint64_t binary_watches;  // Number of binary clause watches
    uint64_t updates;         // Total watch updates
    uint64_t visits;          // Total clause visits
    uint64_t skipped;         // Clauses skipped by blocker
    double   skip_rate;       // Percentage of skipped visits
} WatchStats;

WatchStats watch_stats(const WatchManager* wm);

#endif // BSAT_WATCH_H