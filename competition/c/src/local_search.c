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
static void init_assignment_from_phases(LocalSearchState* ls, Solver* s) {
    for (Var v = 1; v <= ls->num_vars; v++) {
        ls->assignment[v] = s->vars[v].polarity;
    }
}

/**
 * Count true literals in a clause.
 */
static uint32_t count_true_lits(LocalSearchState* ls, uint32_t c) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < ls->clause_sizes[c]; i++) {
        if (lit_value(ls, ls->clause_lits[c][i])) {
            count++;
        }
    }
    return count;
}

/**
 * Initialize satisfaction counts and break scores.
 */
static void init_clause_state(LocalSearchState* ls) {
    ls->num_unsat = 0;

    // Initialize true literal counts
    for (uint32_t c = 0; c < ls->num_clauses; c++) {
        ls->num_true_lits[c] = count_true_lits(ls, c);
        if (ls->num_true_lits[c] == 0) {
            ls->num_unsat++;
        }
    }

    // Initialize break counts
    // break_count[v] = (clauses that become unsat if we flip v) - (clauses that become sat)
    memset(ls->break_count, 0, (ls->num_vars + 1) * sizeof(int32_t));

    for (Var v = 1; v <= ls->num_vars; v++) {
        bool current_val = ls->assignment[v];

        // Clauses where v appears positively
        // If v is true, flipping makes these literals false
        // If v is false, flipping makes these literals true
        for (uint32_t i = 0; i < ls->pos_occ_count[v]; i++) {
            uint32_t c = ls->pos_occs[v][i];
            if (current_val) {
                // v is true, lit is true. Flipping makes lit false.
                if (ls->num_true_lits[c] == 1) {
                    ls->break_count[v]++;  // Would break clause
                }
            } else {
                // v is false, lit is false. Flipping makes lit true.
                if (ls->num_true_lits[c] == 0) {
                    ls->break_count[v]--;  // Would satisfy clause
                }
            }
        }

        // Clauses where v appears negatively
        for (uint32_t i = 0; i < ls->neg_occ_count[v]; i++) {
            uint32_t c = ls->neg_occs[v][i];
            if (!current_val) {
                // v is false, neg(v) is true. Flipping makes neg(v) false.
                if (ls->num_true_lits[c] == 1) {
                    ls->break_count[v]++;  // Would break clause
                }
            } else {
                // v is true, neg(v) is false. Flipping makes neg(v) true.
                if (ls->num_true_lits[c] == 0) {
                    ls->break_count[v]--;  // Would satisfy clause
                }
            }
        }
    }
}

/**
 * Update clause state after flipping variable v.
 */
static void update_after_flip(LocalSearchState* ls, Var v) {
    bool new_val = ls->assignment[v];

    // Process positive occurrences
    for (uint32_t i = 0; i < ls->pos_occ_count[v]; i++) {
        uint32_t c = ls->pos_occs[v][i];
        uint32_t old_true = ls->num_true_lits[c];

        if (new_val) {
            // Literal became true
            ls->num_true_lits[c]++;
            if (old_true == 0) {
                ls->num_unsat--;
            }
        } else {
            // Literal became false
            ls->num_true_lits[c]--;
            if (old_true == 1) {
                ls->num_unsat++;
            }
        }
    }

    // Process negative occurrences
    for (uint32_t i = 0; i < ls->neg_occ_count[v]; i++) {
        uint32_t c = ls->neg_occs[v][i];
        uint32_t old_true = ls->num_true_lits[c];

        if (!new_val) {
            // neg(v) literal became true
            ls->num_true_lits[c]++;
            if (old_true == 0) {
                ls->num_unsat--;
            }
        } else {
            // neg(v) literal became false
            ls->num_true_lits[c]--;
            if (old_true == 1) {
                ls->num_unsat++;
            }
        }
    }

    // Update break counts for v and its neighbors
    // This is simplified - a full implementation would update all affected variables
    // For now, we just recalculate break_count for v
    ls->break_count[v] = 0;
    for (uint32_t i = 0; i < ls->pos_occ_count[v]; i++) {
        uint32_t c = ls->pos_occs[v][i];
        if (new_val && ls->num_true_lits[c] == 1) {
            ls->break_count[v]++;
        } else if (!new_val && ls->num_true_lits[c] == 0) {
            ls->break_count[v]--;
        }
    }
    for (uint32_t i = 0; i < ls->neg_occ_count[v]; i++) {
        uint32_t c = ls->neg_occs[v][i];
        if (!new_val && ls->num_true_lits[c] == 1) {
            ls->break_count[v]++;
        } else if (new_val && ls->num_true_lits[c] == 0) {
            ls->break_count[v]--;
        }
    }
}

/**
 * Pick a random unsatisfied clause.
 */
static uint32_t pick_unsat_clause(LocalSearchState* ls) {
    // Count unsatisfied clauses and pick random one
    uint32_t target = rand() % ls->num_unsat;
    uint32_t count = 0;
    for (uint32_t c = 0; c < ls->num_clauses; c++) {
        if (ls->num_true_lits[c] == 0) {
            if (count == target) {
                return c;
            }
            count++;
        }
    }
    // Shouldn't reach here
    return 0;
}

/**
 * Pick variable to flip from clause using WalkSAT heuristic.
 * With probability (1-noise), pick variable with minimum break count.
 * With probability noise, pick random variable from clause.
 */
static Var pick_var_to_flip(LocalSearchState* ls, uint32_t c, double noise) {
    // Random walk with probability noise
    if ((rand() / (double)RAND_MAX) < noise) {
        uint32_t idx = rand() % ls->clause_sizes[c];
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

    ls->num_vars = s->num_vars;
    ls->num_clauses = s->num_clauses;

    // Allocate assignment
    ls->assignment = (bool*)calloc(ls->num_vars + 1, sizeof(bool));
    if (!ls->assignment) goto error;

    // Allocate clause tracking
    ls->num_true_lits = (uint32_t*)calloc(ls->num_clauses, sizeof(uint32_t));
    if (!ls->num_true_lits) goto error;

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

bool local_search_run(Solver* s, LocalSearchState* ls, uint32_t max_flips, double noise) {
    // Initialize assignment from saved phases
    init_assignment_from_phases(ls, s);

    // Initialize clause satisfaction state
    init_clause_state(ls);

    // If already satisfied, we're done
    if (ls->num_unsat == 0) {
        return true;
    }

    // Main WalkSAT loop
    for (uint32_t flip = 0; flip < max_flips && ls->num_unsat > 0; flip++) {
        // Pick a random unsatisfied clause
        uint32_t c = pick_unsat_clause(ls);

        // Pick variable to flip
        Var v = pick_var_to_flip(ls, c, noise);

        // Flip the variable
        flip_var(ls, v);

        // Update clause state
        update_after_flip(ls, v);

        ls->flips++;
    }

    return ls->num_unsat == 0;
}

void local_search_copy_solution(Solver* s, LocalSearchState* ls) {
    // Copy solution to solver's variable values
    for (Var v = 1; v <= ls->num_vars; v++) {
        s->vars[v].value = ls->assignment[v] ? TRUE : FALSE;
        s->vars[v].polarity = ls->assignment[v];
        s->vars[v].level = 0;
        s->vars[v].reason = INVALID_CLAUSE;
    }

    // Update trail to reflect full assignment
    s->trail_size = 0;
    for (Var v = 1; v <= ls->num_vars; v++) {
        Lit lit = mkLit(v, !ls->assignment[v]);
        s->trail[s->trail_size].lit = lit;
        s->trail[s->trail_size].level = 0;
        s->trail_size++;
    }
    s->decision_level = 0;
}
