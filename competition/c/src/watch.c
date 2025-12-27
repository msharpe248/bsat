/*********************************************************************
 * BSAT Competition Solver - Watch Manager Implementation
 *********************************************************************/

#include "../include/watch.h"
#include <stdlib.h>
#include <string.h>

/*********************************************************************
 * Watch Manager
 *********************************************************************/

WatchManager* watch_init(uint32_t num_vars) {
    WatchManager* wm = (WatchManager*)calloc(1, sizeof(WatchManager));
    if (!wm) return NULL;

    wm->num_vars = num_vars;

    // Allocate watch lists for all literals (2 per variable)
    // We allocate for literals 0..2*num_vars+1 to handle 1-based variables
    uint32_t num_lits = 2 * (num_vars + 1);
    wm->lists = (WatchList*)calloc(num_lits, sizeof(WatchList));
    if (!wm->lists) {
        free(wm);
        return NULL;
    }

    return wm;
}

bool watch_resize(WatchManager* wm, uint32_t new_num_vars) {
    if (!wm) return false;
    if (new_num_vars <= wm->num_vars) return true;  // Already big enough

    uint32_t old_num_lits = 2 * (wm->num_vars + 1);
    uint32_t new_num_lits = 2 * (new_num_vars + 1);

    // Grow the lists array
    WatchList* new_lists = (WatchList*)realloc(wm->lists, new_num_lits * sizeof(WatchList));
    if (!new_lists) return false;
    wm->lists = new_lists;

    // Initialize new watch lists to empty
    for (uint32_t i = old_num_lits; i < new_num_lits; i++) {
        wm->lists[i].watches = NULL;
        wm->lists[i].size = 0;
        wm->lists[i].capacity = 0;
    }

    wm->num_vars = new_num_vars;
    return true;
}

void watch_free(WatchManager* wm) {
    if (!wm) return;

    if (wm->lists) {
        uint32_t num_lits = 2 * (wm->num_vars + 1);
        for (uint32_t i = 0; i < num_lits; i++) {
            free(wm->lists[i].watches);
        }
        free(wm->lists);
    }

    free(wm);
}

void watch_clear(WatchManager* wm) {
    if (!wm) return;

    uint32_t num_lits = 2 * (wm->num_vars + 1);
    for (uint32_t i = 0; i < num_lits; i++) {
        wm->lists[i].size = 0;
    }

    wm->updates = 0;
    wm->visits = 0;
    wm->skipped = 0;
}

void watch_add(WatchManager* wm, Lit lit, CRef cref, Lit blocker) {
    WatchList* wl = watch_list(wm, lit);
    Watch w = {cref, blocker};
    watchlist_push(wl, w);
    wm->updates++;
}

void watch_remove_clause(WatchManager* wm, Arena* arena, CRef cref) {
    if (!wm || !arena || cref == INVALID_CLAUSE) return;

    // Get clause literals
    uint32_t size = CLAUSE_SIZE(arena, cref);
    Lit* lits = CLAUSE_LITS(arena, cref);

    // For binary clauses, both literals watch each other
    if (size == 2) {
        // Remove from first literal's watch list
        WatchList* wl0 = watch_list(wm, lits[0]);
        for (uint32_t i = 0; i < wl0->size; i++) {
            if (wl0->watches[i].cref == cref ||
                (is_binary_watch(wl0->watches[i]) && wl0->watches[i].blocker == lits[1])) {
                watchlist_remove(wl0, i);
                break;
            }
        }

        // Remove from second literal's watch list
        WatchList* wl1 = watch_list(wm, lits[1]);
        for (uint32_t i = 0; i < wl1->size; i++) {
            if (wl1->watches[i].cref == cref ||
                (is_binary_watch(wl1->watches[i]) && wl1->watches[i].blocker == lits[0])) {
                watchlist_remove(wl1, i);
                break;
            }
        }
    } else if (size > 2) {
        // For larger clauses, only first two literals are watched
        WatchList* wl0 = watch_list(wm, lits[0]);
        for (uint32_t i = 0; i < wl0->size; i++) {
            if (wl0->watches[i].cref == cref) {
                watchlist_remove(wl0, i);
                break;
            }
        }

        WatchList* wl1 = watch_list(wm, lits[1]);
        for (uint32_t i = 0; i < wl1->size; i++) {
            if (wl1->watches[i].cref == cref) {
                watchlist_remove(wl1, i);
                break;
            }
        }
    }
}

/*********************************************************************
 * Statistics
 *********************************************************************/

WatchStats watch_stats(const WatchManager* wm) {
    WatchStats stats = {0};

    if (!wm) return stats;

    uint32_t num_lits = 2 * (wm->num_vars + 1);

    for (uint32_t i = 0; i < num_lits; i++) {
        const WatchList* wl = &wm->lists[i];
        stats.total_watches += wl->size;

        for (uint32_t j = 0; j < wl->size; j++) {
            if (is_binary_watch(wl->watches[j])) {
                stats.binary_watches++;
            }
        }
    }

    stats.updates = wm->updates;
    stats.visits = wm->visits;
    stats.skipped = wm->skipped;

    if (stats.visits > 0) {
        stats.skip_rate = 100.0 * stats.skipped / stats.visits;
    }

    return stats;
}