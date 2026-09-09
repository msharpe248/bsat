#ifndef BSAT_REDUCTION_SORT_H
#define BSAT_REDUCTION_SORT_H
#include "types.h"
typedef struct {
    CRef cref;
    uint32_t lbd;
    float activity;
} ClauseScore;
/* Fixed partition/tie policy, no allocation, bounded stack, O(n log n) worst case. */
void bsat_sort_clause_scores(ClauseScore *scores, size_t n);
/* Internal entry also used to exercise depth-exhaustion in regression tests. */
void bsat_sort_clause_scores_depth(ClauseScore *scores, size_t n, unsigned depth);
#endif
