/*********************************************************************
 * BSAT Competition Solver - Local Search Implementation
 *
 * WalkSAT-style local search for hybrid CDCL+LS solving.
 *********************************************************************/

#include "../include/local_search.h"
#include "../include/solver.h"
#include "../include/arena.h"
#include "../include/types.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*********************************************************************
 * Helper Functions
 *********************************************************************/

static inline bool lit_value(LocalSearchState* ls, Lit lit) {
    Var v = var(lit);
    bool val = ls->assignment[v];
    return sign(lit) ? !val : val;
}

static inline void flip_var(LocalSearchState* ls, Var v) {
    ls->assignment[v] = !ls->assignment[v];
}

/**
 * Initialize assignment from solver's saved phases.
 */
static bool root_fixed(const Solver *s, Var v) {
    return s->values[v] != UNDEF && s->vars[v].level == 0;
}

static bool init_assignment_from_phases(LocalSearchState* ls, Solver* s) {
    bool fixed = false;
    for (Var v = 1; v <= ls->num_vars; v++) {
        bool root = root_fixed(s, v);
        fixed |= root;
        ls->assignment[v] = root ? s->values[v] == TRUE : s->vars[v].polarity;
    }
    return fixed;
}

/**
 * Initialize satisfaction counts and exact break-minus-make scores together.
 */
static void init_clause_state(LocalSearchState* ls) {
    ls->num_unsat = 0;
    memset(ls->break_count, 0, (ls->num_vars + 1) * sizeof(int32_t));
    for (uint32_t c = 0; c < ls->num_clauses; ++c) {
        Lit *lits = ls->clause_lits[c];
        uint32_t size = ls->clause_sizes[c], count = 0;
        Var sole = INVALID_VAR;
        for (uint32_t i = 0; i < size; ++i) {
            if (lit_value(ls, lits[i])) {
                ++count;
                sole = var(lits[i]);
            }
        }
        ls->num_true_lits[c] = count;
        if (!count) {
            ++ls->num_unsat;
            for (uint32_t i = 0; i < size; ++i) --ls->break_count[var(lits[i])];
        } else if (count == 1) {
            ++ls->break_count[sole];
        }
    }

    /* Linear-time Fenwick construction. A rank query preserves the old scan's
       clause order and consumes exactly the same random draw. */
    ls->unsat_tree[0] = 0;
    for (size_t i = 1; i <= ls->num_clauses; ++i)
        ls->unsat_tree[i] = !ls->num_true_lits[i - 1];
    for (size_t i = 1; i <= ls->num_clauses; ++i) {
        size_t parent = i + (i & -i);
        if (parent <= ls->num_clauses) ls->unsat_tree[parent] += ls->unsat_tree[i];
    }
}

/* Change membership without reordering unsatisfied clauses. Indices use the
   allocation size type; allocation bounds also leave room for parent steps. */
static void update_unsat_tree(LocalSearchState *ls, uint32_t c, bool added) {
    for (size_t i = (size_t)c + 1; i <= ls->num_clauses; i += i & -i) {
        if (added) ++ls->unsat_tree[i];
        else --ls->unsat_tree[i];
    }
}

/**
 * Update clause state after flipping variable v.
 */
static void update_clause_after_flip(LocalSearchState *ls, uint32_t c, Var v,
                                     bool became_true) {
    uint32_t old = ls->num_true_lits[c];
    ASSERT(became_true || old);
    uint32_t now = became_true ? old + 1 : old - 1;
    ls->num_true_lits[c] = now;
    Lit *lits = ls->clause_lits[c];
    uint32_t size = ls->clause_sizes[c];
    if (!old) {
        --ls->num_unsat;
        update_unsat_tree(ls, c, false);
        /* Remove the make contribution for every literal; v is now the sole
           satisfying variable and also acquires a break contribution. */
        for (uint32_t i = 0; i < size; ++i) ++ls->break_count[var(lits[i])];
        ++ls->break_count[v];
    } else if (!now) {
        ++ls->num_unsat;
        update_unsat_tree(ls, c, true);
        for (uint32_t i = 0; i < size; ++i) --ls->break_count[var(lits[i])];
        --ls->break_count[v];
    } else if (old == 1 || now == 1) {
        /* Only the other true literal changes its break contribution at
           1<->2. Clauses with at least two truths before and after need no work. */
        for (uint32_t i = 0; i < size; ++i) {
            Var other = var(lits[i]);
            if (other != v && lit_value(ls, lits[i])) {
                ls->break_count[other] += now == 1 ? 1 : -1;
                return;
            }
        }
        ASSERT(false); /* Normalized clauses contain each variable once. */
    }
}

static void update_after_flip(LocalSearchState* ls, Var v) {
    bool new_val = ls->assignment[v];
    for (uint32_t i = 0; i < ls->pos_occ_count[v]; ++i)
        update_clause_after_flip(ls, ls->pos_occs[v][i], v, new_val);
    for (uint32_t i = 0; i < ls->neg_occ_count[v]; ++i)
        update_clause_after_flip(ls, ls->neg_occs[v][i], v, !new_val);
}

/**
 * Pick a random unsatisfied clause.
 */
static uint32_t pick_unsat_clause(LocalSearchState* ls) {
    ASSERT(ls->num_unsat);
    uint32_t target = bsat_random(&ls->random_state) % ls->num_unsat;
    size_t index = 0, step = 1;
    while (step <= ls->num_clauses / 2) step <<= 1;
    for (; step; step >>= 1) {
        size_t next = index + step;
        if (next <= ls->num_clauses && ls->unsat_tree[next] <= target) {
            target -= ls->unsat_tree[next];
            index = next;
        }
    }
    ASSERT(index < ls->num_clauses && !ls->num_true_lits[index]);
    return (uint32_t)index;
}

/**
 * Pick variable to flip from clause using WalkSAT heuristic.
 * With probability (1-noise), pick variable with minimum break count.
 * With probability noise, pick random variable from clause.
 */
static Var pick_var_to_flip(Solver *s, LocalSearchState* ls, uint32_t c, double noise, bool fixed) {
    if (fixed) {
        bool random = (bsat_random(&ls->random_state) / 4294967296.0) < noise;
        Var best = INVALID_VAR;
        uint32_t count = 0;
        for (uint32_t i = 0; i < ls->clause_sizes[c]; ++i) {
            Var v = var(ls->clause_lits[c][i]);
            if (root_fixed(s, v)) continue;
            ++count;
            if (best == INVALID_VAR || ls->break_count[v] < ls->break_count[best]) best = v;
        }
        if (!random || !count) return best;
        uint32_t target = bsat_random(&ls->random_state) % count;
        for (uint32_t i = 0; i < ls->clause_sizes[c]; ++i) {
            Var v = var(ls->clause_lits[c][i]);
            if (!root_fixed(s, v) && !target--) return v;
        }
        return INVALID_VAR;
    }
    // Preserve the original random draws and choices without fixed variables.
    // Random walk with probability noise
    if ((bsat_random(&ls->random_state) / 4294967296.0) < noise) {
        uint32_t idx = bsat_random(&ls->random_state) % ls->clause_sizes[c];
        return var(ls->clause_lits[c][idx]);
    }

    // Greedy: pick variable with minimum break count
    Var best_var = var(ls->clause_lits[c][0]);
    int32_t best_break = ls->break_count[best_var];

    for (uint32_t i = 1; i < ls->clause_sizes[c]; i++) {
        Var v = var(ls->clause_lits[c][i]);
        if (ls->break_count[v] < best_break) {
            best_var = v;
            best_break = ls->break_count[v];
        }
    }

    return best_var;
}

/*********************************************************************
 * Public API
 *********************************************************************/

LocalSearchState* local_search_init(Solver* s) {
    LocalSearchState* ls = (LocalSearchState*)calloc(1, sizeof(LocalSearchState));
    if (!ls) return NULL;

    ls->random_state = s->opts.seed ^ 0xa511e9b3u;
    ls->num_vars = s->num_vars;
    ls->num_clauses = s->num_clauses;

    // Allocate assignment
    ls->assignment = (bool*)calloc(ls->num_vars + 1, sizeof(bool));
    if (!ls->assignment) goto error;

    // Allocate clause tracking
    ls->num_true_lits = (uint32_t*)calloc(ls->num_clauses, sizeof(uint32_t));
    if (!ls->num_true_lits) goto error;

    if ((uint64_t)ls->num_clauses + 1 > SIZE_MAX / sizeof(uint32_t)) goto error;
    ls->unsat_tree = (uint32_t*)calloc((size_t)ls->num_clauses + 1, sizeof(uint32_t));
    if (!ls->unsat_tree) goto error;

    // Allocate break counts
    ls->break_count = (int32_t*)calloc(ls->num_vars + 1, sizeof(int32_t));
    if (!ls->break_count) goto error;

    // Allocate clause data arrays
    ls->clause_lits = (Lit**)calloc(ls->num_clauses, sizeof(Lit*));
    ls->clause_sizes = (uint32_t*)calloc(ls->num_clauses, sizeof(uint32_t));
    if (!ls->clause_lits || !ls->clause_sizes) goto error;

    // Copy clause data from solver
    for (uint32_t i = 0; i < s->num_clauses; i++) {
        CRef cref = s->clauses[i];
        uint32_t size = CLAUSE_SIZE(s->arena, cref);
        Lit* lits = CLAUSE_LITS(s->arena, cref);

        ls->clause_sizes[i] = size;
        ls->clause_lits[i] = (Lit*)malloc(size * sizeof(Lit));
        if (!ls->clause_lits[i]) goto error;

        for (uint32_t j = 0; j < size; j++) {
            ls->clause_lits[i][j] = lits[j];
        }
    }

    // Allocate occurrence lists
    ls->pos_occs = (uint32_t**)calloc(ls->num_vars + 1, sizeof(uint32_t*));
    ls->pos_occ_count = (uint32_t*)calloc(ls->num_vars + 1, sizeof(uint32_t));
    ls->neg_occs = (uint32_t**)calloc(ls->num_vars + 1, sizeof(uint32_t*));
    ls->neg_occ_count = (uint32_t*)calloc(ls->num_vars + 1, sizeof(uint32_t));
    if (!ls->pos_occs || !ls->pos_occ_count || !ls->neg_occs || !ls->neg_occ_count) goto error;

    // Count occurrences
    for (uint32_t c = 0; c < ls->num_clauses; c++) {
        for (uint32_t j = 0; j < ls->clause_sizes[c]; j++) {
            Lit lit = ls->clause_lits[c][j];
            Var v = var(lit);
            if (sign(lit)) {
                ls->neg_occ_count[v]++;
            } else {
                ls->pos_occ_count[v]++;
            }
        }
    }

    // Allocate occurrence list arrays
    for (Var v = 1; v <= ls->num_vars; v++) {
        if (ls->pos_occ_count[v] > 0) {
            ls->pos_occs[v] = (uint32_t*)malloc(ls->pos_occ_count[v] * sizeof(uint32_t));
            if (!ls->pos_occs[v]) goto error;
        }
        if (ls->neg_occ_count[v] > 0) {
            ls->neg_occs[v] = (uint32_t*)malloc(ls->neg_occ_count[v] * sizeof(uint32_t));
            if (!ls->neg_occs[v]) goto error;
        }
    }

    // Fill occurrence lists (reset counts as indices)
    memset(ls->pos_occ_count, 0, (ls->num_vars + 1) * sizeof(uint32_t));
    memset(ls->neg_occ_count, 0, (ls->num_vars + 1) * sizeof(uint32_t));

    for (uint32_t c = 0; c < ls->num_clauses; c++) {
        for (uint32_t j = 0; j < ls->clause_sizes[c]; j++) {
            Lit lit = ls->clause_lits[c][j];
            Var v = var(lit);
            if (sign(lit)) {
                ls->neg_occs[v][ls->neg_occ_count[v]++] = c;
            } else {
                ls->pos_occs[v][ls->pos_occ_count[v]++] = c;
            }
        }
    }

    return ls;

error:
    local_search_free(ls);
    return NULL;
}

void local_search_free(LocalSearchState* ls) {
    if (!ls) return;

    free(ls->assignment);
    free(ls->num_true_lits);
    free(ls->unsat_tree);
    free(ls->break_count);

    if (ls->clause_lits) {
        for (uint32_t i = 0; i < ls->num_clauses; i++) {
            free(ls->clause_lits[i]);
        }
        free(ls->clause_lits);
    }
    free(ls->clause_sizes);

    if (ls->pos_occs) {
        for (Var v = 1; v <= ls->num_vars; v++) {
            free(ls->pos_occs[v]);
        }
        free(ls->pos_occs);
    }
    free(ls->pos_occ_count);

    if (ls->neg_occs) {
        for (Var v = 1; v <= ls->num_vars; v++) {
            free(ls->neg_occs[v]);
        }
        free(ls->neg_occs);
    }
    free(ls->neg_occ_count);

    free(ls);
}

/* Save hints only: live assignments, reasons, levels and the trail are untouched.
   A cancelled copy may leave a prefix of valid phase hints, never a partial model. */
static bool save_walk_phases(Solver *s, const LocalSearchState *ls) {
    for (Var v = 1; v <= ls->num_vars; ++v) {
        if (!(v & 1023) && solver_budget_exhausted_now(s)) return false;
        if (!root_fixed(s, v)) s->vars[v].polarity = ls->assignment[v];
    }
    return true;
}

bool local_search_run(Solver* s, LocalSearchState* ls, uint32_t max_flips, double noise) {
    // Initialize assignment from saved phases
    bool fixed = init_assignment_from_phases(ls, s);

    // Initialize clause satisfaction state
    init_clause_state(ls);

    uint32_t best_unsat = ls->num_unsat;
    bool save_phases = s->opts.ls_save_phases && s->opts.phase_saving;

    // If already satisfied, we're done
    if (ls->num_unsat == 0) {
        return true;
    }

    // Main WalkSAT loop
    for (uint32_t flip = 0; flip < max_flips && ls->num_unsat > 0; flip++) {
        /* This loop already throttles polls; do not throttle the clock again. */
        if ((flip & 255) == 0 && solver_budget_exhausted_now(s)) return false;
        // Pick a random unsatisfied clause
        uint32_t c = pick_unsat_clause(ls);

        // Pick variable to flip
        Var v = pick_var_to_flip(s, ls, c, noise, fixed);
        // A walk cannot repair a clause falsified entirely by root assignments.
        if (v == INVALID_VAR) return false;

        // Flip the variable
        flip_var(ls, v);

        // Update clause state
        update_after_flip(ls, v);

        ls->flips++;
        if (save_phases && ls->num_unsat && ls->num_unsat < best_unsat) {
            best_unsat = ls->num_unsat;
            if (!save_walk_phases(s, ls)) return false;
        }
    }

    return ls->num_unsat == 0;
}

void local_search_copy_solution(Solver* s, LocalSearchState* ls) {
    ASSERT(ls->num_vars == s->num_vars);
    /* A successful walk supplies a complete model. Install one coherent root
       assignment, including metadata left over from earlier CDCL assignments. */
    for (Var v = 1; v <= ls->num_vars; v++) {
        s->values[v] = ls->assignment[v] ? TRUE : FALSE;
        s->vars[v].polarity = ls->assignment[v];
        s->vars[v].level = 0;
        s->vars[v].reason = INVALID_CLAUSE;
        s->binary_reasons[v] = LIT_UNDEF;
        s->vars[v].trail_pos = v - 1;
        s->trail[v - 1].lit = mkLit(v, !ls->assignment[v]);
    }
    s->trail_size = ls->num_vars;
    /* Every clause is satisfied; there are no pending implications to scan. */
    s->qhead = s->trail_size;
    s->decision_level = 0;
    s->rephase.best_prefix_valid = false;
}
