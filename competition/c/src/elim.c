/*********************************************************************
 * BSAT Competition Solver - Bounded Variable Elimination (BVE)
 *
 * Implements SatELite-style variable elimination preprocessing.
 * Reference: Eén & Biere, "Effective Preprocessing in SAT through
 *            Variable and Clause Elimination" (SAT 2005)
 *********************************************************************/

#include "../include/elim.h"
#include "../include/solver.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

/*********************************************************************
 * Initial Capacity Constants
 *********************************************************************/

#define INITIAL_OCC_CAPACITY 8
#define INITIAL_STACK_CAPACITY 256

/*********************************************************************
 * Forward Declarations
 *********************************************************************/

// DRAT proof logging (defined in solver.c when proof is enabled)
extern void proof_add_clause(Solver* s, const Lit* lits, uint32_t size);
extern void proof_delete_clause(Solver* s, const Lit* lits, uint32_t size);

/*********************************************************************
 * Occurrence List Management
 *********************************************************************/

static void occ_ensure_capacity(OccList* occ, uint32_t min_cap) {
    if (occ->capacity >= min_cap) return;

    uint32_t new_cap = occ->capacity ? occ->capacity * 2 : INITIAL_OCC_CAPACITY;
    while (new_cap < min_cap) new_cap *= 2;

    CRef* new_clauses = (CRef*)realloc(occ->clauses, new_cap * sizeof(CRef));
    if (!new_clauses) {
        // Out of memory - just return, caller will handle
        return;
    }
    occ->clauses = new_clauses;
    occ->capacity = new_cap;
}

void elim_add_occ(Solver* s, Lit lit, CRef cref) {
    if (!s->elim || lit >= s->elim->occs_capacity) return;

    OccList* occ = &s->elim->occs[lit];
    occ_ensure_capacity(occ, occ->size + 1);
    if (occ->size < occ->capacity) {
        occ->clauses[occ->size++] = cref;
    }
}

void elim_remove_occ(Solver* s, Lit lit, CRef cref) {
    if (!s->elim || lit >= s->elim->occs_capacity) return;

    OccList* occ = &s->elim->occs[lit];
    for (uint32_t i = 0; i < occ->size; i++) {
        if (occ->clauses[i] == cref) {
            // Swap with last and shrink
            occ->clauses[i] = occ->clauses[occ->size - 1];
            occ->size--;
            return;
        }
    }
}

void elim_clear_occs(Solver* s) {
    if (!s->elim) return;

    for (uint32_t i = 0; i < s->elim->occs_capacity; i++) {
        s->elim->occs[i].size = 0;
    }
}

/*********************************************************************
 * Initialization and Cleanup
 *********************************************************************/

void elim_init(Solver* s) {
    if (s->elim) return;  // Already initialized

    s->elim = (ElimState*)calloc(1, sizeof(ElimState));
    if (!s->elim) return;

    // Allocate occurrence lists (2 per variable: positive and negative literal)
    uint32_t num_lits = 2 * (s->num_vars + 1);
    s->elim->occs = (OccList*)calloc(num_lits, sizeof(OccList));
    s->elim->occs_capacity = num_lits;

    // Allocate elimination stack
    s->elim->stack = (ElimEntry*)malloc(INITIAL_STACK_CAPACITY * sizeof(ElimEntry));
    s->elim->stack_capacity = INITIAL_STACK_CAPACITY;
    s->elim->stack_size = 0;

    // Allocate eliminated flags
    s->elim->eliminated = (bool*)calloc(s->num_vars + 1, sizeof(bool));
    s->elim->elim_capacity = s->num_vars + 1;

    // Initialize statistics
    s->elim->vars_eliminated = 0;
    s->elim->clauses_removed = 0;
    s->elim->resolvents_added = 0;

    // Initialize resolvent tracking for debugging
    s->elim->resolvent_crefs = NULL;
    s->elim->resolvent_crefs_size = 0;
    s->elim->resolvent_crefs_capacity = 0;
}

void elim_free(Solver* s) {
    if (!s->elim) return;

    // Free occurrence lists
    if (s->elim->occs) {
        for (uint32_t i = 0; i < s->elim->occs_capacity; i++) {
            free(s->elim->occs[i].clauses);
        }
        free(s->elim->occs);
    }

    // Free elimination stack entries (clauses are copies)
    if (s->elim->stack) {
        for (uint32_t i = 0; i < s->elim->stack_size; i++) {
            free(s->elim->stack[i].clause);
        }
        free(s->elim->stack);
    }

    // Free eliminated flags
    free(s->elim->eliminated);

    // Free resolvent tracking
    free(s->elim->resolvent_crefs);

    free(s->elim);
    s->elim = NULL;
}

/*********************************************************************
 * Build Occurrence Lists
 *********************************************************************/

void elim_build_occs(Solver* s) {
    if (!s->elim) return;

    elim_clear_occs(s);

    // Iterate through all clauses in the arena
    for (uint32_t i = 0; i < s->num_clauses; i++) {
        CRef cref = s->clauses[i];
        if (cref == INVALID_CLAUSE) continue;
        if (clause_deleted(s->arena, cref)) continue;

        uint32_t size = CLAUSE_SIZE(s->arena, cref);
        Lit* lits = CLAUSE_LITS(s->arena, cref);

        for (uint32_t j = 0; j < size; j++) {
            elim_add_occ(s, lits[j], cref);
        }
    }

    // Also process binary clauses from watch lists
    // Binary clauses are stored implicitly in watches
    // We need to iterate through watches to find them
    for (Var v = 1; v <= s->num_vars; v++) {
        for (int sign_val = 0; sign_val <= 1; sign_val++) {
            Lit lit = mkLit(v, sign_val);
            WatchList* wl = watch_list(s->watches, lit);

            for (uint32_t i = 0; i < wl->size; i++) {
                Watch w = wl->watches[i];
                // Binary clauses have INVALID_CLAUSE as cref
                if (w.cref == INVALID_CLAUSE) {
                    // This is a binary clause: (neg(lit), w.blocker)
                    // We record it in occurrence list for the other literal
                    // But we need to be careful not to double-count
                    // Only add when we see the smaller literal first
                    if (lit < neg(lit)) {
                        // We can't use arena clauses for binary - skip for now
                        // Binary clause handling is more complex
                    }
                }
            }
        }
    }
}

/*********************************************************************
 * Tautology Check
 *********************************************************************/

bool elim_is_tautology(const Lit* c1, uint32_t s1,
                       const Lit* c2, uint32_t s2,
                       Var pivot) {
    // Check if resolving c1 and c2 on pivot produces a tautology
    // A resolvent is a tautology if it contains both x and ¬x for some variable

    // Quick check: mark all literals from c1 (except pivot)
    // Then check if c2 contains the negation of any

    // For efficiency, use a simple O(n*m) approach for small clauses
    // For larger clauses, could use a bitmap

    for (uint32_t i = 0; i < s1; i++) {
        Var vi = var(c1[i]);
        if (vi == pivot) continue;

        for (uint32_t j = 0; j < s2; j++) {
            Var vj = var(c2[j]);
            if (vj == pivot) continue;

            // Check if c1[i] and c2[j] are opposite literals of same variable
            if (vi == vj && sign(c1[i]) != sign(c2[j])) {
                return true;  // Tautology!
            }
        }
    }

    return false;
}

/*********************************************************************
 * Elimination Cost Calculation
 *********************************************************************/

// Sentinel value meaning "don't eliminate this variable"
#define ELIM_SKIP INT_MAX

int elim_cost(Solver* s, Var v) {
    if (!s->elim || v >= s->elim->elim_capacity) return ELIM_SKIP;
    if (s->elim->eliminated[v]) return ELIM_SKIP;
    if (s->vars[v].value != UNDEF) return ELIM_SKIP;  // Already assigned

    Lit pos = mkLit(v, false);  // Positive literal
    Lit negl = mkLit(v, true);   // Negative literal

    OccList* pos_occs = &s->elim->occs[pos];
    OccList* neg_occs = &s->elim->occs[negl];

    uint32_t pos_count = pos_occs->size;
    uint32_t neg_count = neg_occs->size;

    // Check occurrence limits
    if (pos_count > s->opts.elim_max_occ || neg_count > s->opts.elim_max_occ) {
        return ELIM_SKIP;  // Too many occurrences
    }

    // If either side has no occurrences, elimination is trivially beneficial
    if (pos_count == 0 || neg_count == 0) {
        return 0;  // Can eliminate with 0 resolvents
    }

    // Count non-tautological resolvents
    int resolvent_count = 0;
    int original_count = (int)pos_count + (int)neg_count;

    for (uint32_t i = 0; i < pos_count; i++) {
        CRef cref_i = pos_occs->clauses[i];
        if (clause_deleted(s->arena, cref_i)) continue;

        uint32_t size_i = CLAUSE_SIZE(s->arena, cref_i);
        Lit* lits_i = CLAUSE_LITS(s->arena, cref_i);

        for (uint32_t j = 0; j < neg_count; j++) {
            CRef cref_j = neg_occs->clauses[j];
            if (clause_deleted(s->arena, cref_j)) continue;

            uint32_t size_j = CLAUSE_SIZE(s->arena, cref_j);
            Lit* lits_j = CLAUSE_LITS(s->arena, cref_j);

            if (!elim_is_tautology(lits_i, size_i, lits_j, size_j, v)) {
                resolvent_count++;

                // Early termination: if resolvents already exceed original count + growth limit
                if (resolvent_count > original_count + (int)s->opts.elim_grow) {
                    return ELIM_SKIP;  // Not beneficial
                }
            }
        }
    }

    // Check if elimination is beneficial
    if (resolvent_count <= original_count + (int)s->opts.elim_grow) {
        return resolvent_count - original_count;  // Return net change (negative = good)
    }

    return ELIM_SKIP;  // Not beneficial
}

/*********************************************************************
 * Compute Resolvent
 *********************************************************************/

// Compute resolvent of two clauses on pivot variable
// Returns allocated array of literals (caller must free)
// Returns NULL if resolvent is tautology
static Lit* compute_resolvent(const Lit* c1, uint32_t s1,
                              const Lit* c2, uint32_t s2,
                              Var pivot, uint32_t* result_size) {
    // Maximum size of resolvent
    uint32_t max_size = s1 + s2 - 2;  // Remove one literal from each
    Lit* result = (Lit*)malloc(max_size * sizeof(Lit));
    if (!result) return NULL;

    uint32_t rsize = 0;

    // Add all literals from c1 except pivot
    for (uint32_t i = 0; i < s1; i++) {
        if (var(c1[i]) != pivot) {
            result[rsize++] = c1[i];
        }
    }

    // Add literals from c2 except pivot and duplicates
    for (uint32_t j = 0; j < s2; j++) {
        Lit lit = c2[j];
        if (var(lit) == pivot) continue;

        // Check for duplicate or opposite
        bool found = false;
        for (uint32_t k = 0; k < rsize; k++) {
            if (var(result[k]) == var(lit)) {
                if (sign(result[k]) != sign(lit)) {
                    // Tautology! (opposite literals)
                    free(result);
                    return NULL;
                }
                found = true;  // Duplicate
                break;
            }
        }

        if (!found) {
            result[rsize++] = lit;
        }
    }

    *result_size = rsize;
    return result;
}

/*********************************************************************
 * Eliminate Variable
 *********************************************************************/

bool elim_eliminate_var(Solver* s, Var v) {
    if (!s->elim) return false;
    if (s->elim->eliminated[v]) return false;
    if (s->vars[v].value != UNDEF) return false;

    Lit pos = mkLit(v, false);
    Lit negl = mkLit(v, true);

    OccList* pos_occs = &s->elim->occs[pos];
    OccList* neg_occs = &s->elim->occs[negl];

    // Save one positive clause for reconstruction
    // (We need to save a clause containing +v before deletion)
    ElimEntry entry;
    entry.var = v;
    entry.clause = NULL;
    entry.clause_size = 0;

    if (pos_occs->size > 0) {
        CRef cref = pos_occs->clauses[0];
        if (!clause_deleted(s->arena, cref)) {
            uint32_t size = CLAUSE_SIZE(s->arena, cref);
            Lit* lits = CLAUSE_LITS(s->arena, cref);
            entry.clause = (Lit*)malloc(size * sizeof(Lit));
            if (entry.clause) {
                memcpy(entry.clause, lits, size * sizeof(Lit));
                entry.clause_size = size;
            }
        }
    }

    // If no positive clause, try negative
    if (!entry.clause && neg_occs->size > 0) {
        CRef cref = neg_occs->clauses[0];
        if (!clause_deleted(s->arena, cref)) {
            uint32_t size = CLAUSE_SIZE(s->arena, cref);
            Lit* lits = CLAUSE_LITS(s->arena, cref);
            entry.clause = (Lit*)malloc(size * sizeof(Lit));
            if (entry.clause) {
                memcpy(entry.clause, lits, size * sizeof(Lit));
                entry.clause_size = size;
            }
        }
    }

    // Generate all resolvents
    for (uint32_t i = 0; i < pos_occs->size; i++) {
        CRef cref_i = pos_occs->clauses[i];
        if (clause_deleted(s->arena, cref_i)) continue;

        uint32_t size_i = CLAUSE_SIZE(s->arena, cref_i);
        Lit* lits_i = CLAUSE_LITS(s->arena, cref_i);

        for (uint32_t j = 0; j < neg_occs->size; j++) {
            CRef cref_j = neg_occs->clauses[j];
            if (clause_deleted(s->arena, cref_j)) continue;

            uint32_t size_j = CLAUSE_SIZE(s->arena, cref_j);
            Lit* lits_j = CLAUSE_LITS(s->arena, cref_j);

            uint32_t rsize;
            Lit* resolvent = compute_resolvent(lits_i, size_i, lits_j, size_j, v, &rsize);

            if (resolvent) {
                if (rsize == 0) {
                    // Empty clause - UNSAT
                    free(resolvent);
                    free(entry.clause);
                    s->result = FALSE;
                    return false;
                }

                if (rsize == 1) {
                    // Unit clause - propagate immediately at level 0
                    Lit unit = resolvent[0];
                    Var uv = var(unit);
                    lbool val = s->vars[uv].value;
                    if (val == UNDEF) {
                        // Propagate immediately at level 0
                        s->vars[uv].value = sign(unit) ? FALSE : TRUE;
                        s->vars[uv].level = 0;
                        s->vars[uv].reason = INVALID_CLAUSE;
                        s->vars[uv].trail_pos = s->trail_size;
                        s->trail[s->trail_size].lit = unit;
                        s->trail[s->trail_size].level = 0;
                        s->trail_size++;
                        if (s->proof_file) {
                            proof_add_clause(s, resolvent, 1);
                        }
                        s->elim->resolvents_added++;
                    } else if ((val == TRUE && sign(unit)) || (val == FALSE && !sign(unit))) {
                        // Conflict - unit clause is falsified
                        free(resolvent);
                        free(entry.clause);
                        s->result = FALSE;
                        return false;
                    }
                    // Already satisfied - skip
                } else {
                    // Check resolvent status at level 0 before adding
                    // Some literals might already be assigned from earlier unit propagations
                    uint32_t unassigned_count = 0;
                    uint32_t first_unassigned = UINT32_MAX;
                    uint32_t second_unassigned = UINT32_MAX;
                    bool satisfied = false;

                    for (uint32_t k = 0; k < rsize; k++) {
                        Var rv = var(resolvent[k]);
                        lbool val = s->vars[rv].value;
                        if (val == UNDEF) {
                            if (first_unassigned == UINT32_MAX) {
                                first_unassigned = k;
                            } else if (second_unassigned == UINT32_MAX) {
                                second_unassigned = k;
                            }
                            unassigned_count++;
                        } else if ((val == TRUE && !sign(resolvent[k])) ||
                                   (val == FALSE && sign(resolvent[k]))) {
                            // Literal is true - clause is satisfied
                            satisfied = true;
                            break;
                        }
                        // else: literal is false, continue checking
                    }

                    if (satisfied) {
                        // Clause already satisfied, skip it
                        free(resolvent);
                        continue;
                    }

                    if (unassigned_count == 0) {
                        // All literals are false - conflict at level 0 = UNSAT
                        free(resolvent);
                        free(entry.clause);
                        s->result = FALSE;
                        return false;
                    }

                    if (unassigned_count == 1) {
                        // Unit clause - propagate immediately
                        Lit unit = resolvent[first_unassigned];
                        Var uv = var(unit);
                        s->vars[uv].value = sign(unit) ? FALSE : TRUE;
                        s->vars[uv].level = 0;
                        s->vars[uv].reason = INVALID_CLAUSE;
                        s->vars[uv].trail_pos = s->trail_size;
                        s->trail[s->trail_size].lit = unit;
                        s->trail[s->trail_size].level = 0;
                        s->trail_size++;
                        if (s->proof_file) {
                            proof_add_clause(s, resolvent, rsize);
                        }
                        s->elim->resolvents_added++;
                        free(resolvent);
                        continue;
                    }

                    // Move unassigned literals to front for proper watching
                    if (first_unassigned != 0) {
                        Lit tmp = resolvent[0];
                        resolvent[0] = resolvent[first_unassigned];
                        resolvent[first_unassigned] = tmp;
                        if (second_unassigned == 0) {
                            second_unassigned = first_unassigned;
                        }
                    }
                    if (second_unassigned != 1) {
                        Lit tmp = resolvent[1];
                        resolvent[1] = resolvent[second_unassigned];
                        resolvent[second_unassigned] = tmp;
                    }

                    // Add resolvent clause
                    CRef new_cref = arena_alloc(s->arena, resolvent, rsize, false);
                    if (new_cref != INVALID_CLAUSE) {
                        // Add to occurrence lists for all literals
                        for (uint32_t k = 0; k < rsize; k++) {
                            elim_add_occ(s, resolvent[k], new_cref);
                        }

                        // Add watches for first two (unassigned) literals
                        watch_add(s->watches, resolvent[0], new_cref, resolvent[1]);
                        watch_add(s->watches, resolvent[1], new_cref, resolvent[0]);

                        // Log to DRAT if enabled
                        if (s->proof_file) {
                            proof_add_clause(s, resolvent, rsize);
                        }

                        // Track resolvent CRef for debugging
                        if (s->elim->resolvent_crefs_size >= s->elim->resolvent_crefs_capacity) {
                            uint32_t new_cap = s->elim->resolvent_crefs_capacity ? s->elim->resolvent_crefs_capacity * 2 : 64;
                            CRef* new_arr = (CRef*)realloc(s->elim->resolvent_crefs, new_cap * sizeof(CRef));
                            if (new_arr) {
                                s->elim->resolvent_crefs = new_arr;
                                s->elim->resolvent_crefs_capacity = new_cap;
                            }
                        }
                        if (s->elim->resolvent_crefs_size < s->elim->resolvent_crefs_capacity) {
                            s->elim->resolvent_crefs[s->elim->resolvent_crefs_size++] = new_cref;
                        }

                        s->elim->resolvents_added++;
                    }
                }

                free(resolvent);
            }
        }
    }

    // Delete original clauses containing v
    for (uint32_t i = 0; i < pos_occs->size; i++) {
        CRef cref = pos_occs->clauses[i];
        if (clause_deleted(s->arena, cref)) continue;

        uint32_t size = CLAUSE_SIZE(s->arena, cref);
        Lit* lits = CLAUSE_LITS(s->arena, cref);

        // Log deletion to DRAT
        if (s->proof_file) {
            proof_delete_clause(s, lits, size);
        }

        // Remove from occurrence lists
        for (uint32_t j = 0; j < size; j++) {
            if (var(lits[j]) != v) {
                elim_remove_occ(s, lits[j], cref);
            }
        }

        // Mark as deleted
        arena_delete(s->arena, cref);
        s->elim->clauses_removed++;
    }

    for (uint32_t i = 0; i < neg_occs->size; i++) {
        CRef cref = neg_occs->clauses[i];
        if (clause_deleted(s->arena, cref)) continue;

        uint32_t size = CLAUSE_SIZE(s->arena, cref);
        Lit* lits = CLAUSE_LITS(s->arena, cref);

        // Log deletion to DRAT
        if (s->proof_file) {
            proof_delete_clause(s, lits, size);
        }

        // Remove from occurrence lists
        for (uint32_t j = 0; j < size; j++) {
            if (var(lits[j]) != v) {
                elim_remove_occ(s, lits[j], cref);
            }
        }

        // Mark as deleted
        arena_delete(s->arena, cref);
        s->elim->clauses_removed++;
    }

    // Clear occurrence lists for this variable
    pos_occs->size = 0;
    neg_occs->size = 0;

    // Mark variable as eliminated
    s->elim->eliminated[v] = true;
    s->elim->vars_eliminated++;

    // Push to elimination stack
    if (s->elim->stack_size >= s->elim->stack_capacity) {
        uint32_t new_cap = s->elim->stack_capacity * 2;
        ElimEntry* new_stack = (ElimEntry*)realloc(s->elim->stack, new_cap * sizeof(ElimEntry));
        if (new_stack) {
            s->elim->stack = new_stack;
            s->elim->stack_capacity = new_cap;
        }
    }

    if (s->elim->stack_size < s->elim->stack_capacity) {
        s->elim->stack[s->elim->stack_size++] = entry;
    } else {
        free(entry.clause);  // Couldn't save, free the copy
    }

    // Note: eliminated variables are tracked in s->elim->eliminated[v]
    // The solver's decide() function should check this before making decisions
    // We don't set value=TRUE because that would confuse the solver

    return true;
}

/*********************************************************************
 * Main BVE Preprocessing Loop
 *********************************************************************/

uint32_t elim_preprocess(Solver* s) {
    if (!s->opts.elim) return 0;

    // Initialize elimination state
    elim_init(s);
    if (!s->elim) return 0;

    // Build occurrence lists
    elim_build_occs(s);

    uint32_t eliminated = 0;

    // Single pass elimination (safer than iterative)
    // Can do multiple passes but requires careful occurrence list management
    for (Var v = 1; v <= s->num_vars; v++) {
        if (s->elim->eliminated[v]) continue;
        if (s->vars[v].value != UNDEF) continue;

        int cost = elim_cost(s, v);
        if (cost != ELIM_SKIP && cost <= 0) {  // Beneficial (cost <= 0 means net reduction)
            if (elim_eliminate_var(s, v)) {
                eliminated++;

                // Check for UNSAT
                if (s->result == FALSE) {
                    return eliminated;
                }
            }
        }
    }

    if (eliminated > 0 && !s->opts.quiet) {
        printf("c [BVE] Eliminated %u variables, removed %llu clauses, added %llu resolvents\n",
               eliminated,
               (unsigned long long)s->elim->clauses_removed,
               (unsigned long long)s->elim->resolvents_added);
    }

    return eliminated;
}

/*********************************************************************
 * Solution Reconstruction
 *********************************************************************/

void elim_extend_model(Solver* s) {
    if (!s->elim || s->elim->stack_size == 0) return;

    // Process elimination stack in reverse order
    for (int i = (int)s->elim->stack_size - 1; i >= 0; i--) {
        ElimEntry* entry = &s->elim->stack[i];
        Var v = entry->var;

        if (!entry->clause || entry->clause_size == 0) {
            // No saved clause - assign arbitrarily (true)
            s->vars[v].value = TRUE;
            continue;
        }

        // Check if saved clause is satisfied by current model
        bool satisfied = false;
        Lit v_lit = INVALID_LIT;

        for (uint32_t j = 0; j < entry->clause_size; j++) {
            Lit lit = entry->clause[j];
            if (var(lit) == v) {
                v_lit = lit;
            } else {
                // Check if this literal is satisfied
                lbool val = s->vars[var(lit)].value;
                if ((val == TRUE && !sign(lit)) || (val == FALSE && sign(lit))) {
                    satisfied = true;
                    break;
                }
            }
        }

        if (!satisfied && v_lit != INVALID_LIT) {
            // Clause is not satisfied by other literals
            // v_lit must be true to satisfy clause
            s->vars[v].value = sign(v_lit) ? FALSE : TRUE;
        } else {
            // Clause already satisfied, assign v arbitrarily
            s->vars[v].value = TRUE;
        }
    }
}

/*********************************************************************
 * Utility Functions
 *********************************************************************/

bool elim_is_eliminated(const Solver* s, Var v) {
    if (!s->elim || v >= s->elim->elim_capacity) return false;
    return s->elim->eliminated[v];
}

OccList* elim_get_occs(Solver* s, Lit lit) {
    if (!s->elim || lit >= s->elim->occs_capacity) return NULL;
    return &s->elim->occs[lit];
}

void elim_print_stats(const Solver* s) {
    if (!s->elim) return;

    printf("c ========== BVE Statistics ==========\n");
    printf("c Variables eliminated: %llu\n", (unsigned long long)s->elim->vars_eliminated);
    printf("c Clauses removed     : %llu\n", (unsigned long long)s->elim->clauses_removed);
    printf("c Resolvents added    : %llu\n", (unsigned long long)s->elim->resolvents_added);
    printf("c =====================================\n");
}
