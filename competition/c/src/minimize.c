/* Learned-clause minimization: established default and opt-in iterative mode.
 *
 * The source-clause/implication-closure approach is standard CDCL minimization
 * (see MiniSat and Kissat). The experimental mode uses a bounded work queue:
 * no C recursion, no per-conflict allocation, and successful dependency checks
 * are shared across literals of the same learned clause.
 */
#include "../include/solver.h"

/* Preserve the conservative coverage and depth rules of the default
 * minimizer, while bounding work and caching proofs within one candidate. */
// Compute abstract level bitmask (for quick pruning)
static inline uint64_t abstract_level(Level level) {
    return (uint64_t)1 << (level & 63);
}

// Check if literal is redundant using recursive deep analysis
// abstract_levels: bitmask of levels present in learned clause
// Uses seen array: 0=unseen, 1=in learned clause, 2=exploring,
// >=3=proven subtree height + 3. Cached heights preserve the depth-128 rule.
//
// Returns true if literal p can be proven redundant (its reason chain
// terminates at literals in the learned clause or level 0).
static bool lit_redundant(Solver* s, Lit p, uint64_t abstract_levels, unsigned depth,
                          uint32_t *remaining, uint32_t *touched, unsigned *height) {
    if (depth > 128) return false;
    Var v = var(p);

    // Check seen status
    uint8_t seen_val = s->seen[v];
    if (seen_val == 1) {
        *height = 0;
        return true;   // In learned clause - definitely covered
    }
    if (seen_val == 2) {
        return false;  // Cycle - being explored
    }
    if (seen_val >= 3) {
        *height = seen_val - 3;
        s->stats.minimize_cache_hits++;
        return depth + *height <= 128;
    }

    // Check for reason clause
    CRef reason = s->vars[v].reason;

    // Skip binary conflict markers
    if (reason == BINARY_CONFLICT) {
        return false;
    }

    // Quick abstract level check: if this level isn't in abstract_levels,
    // we need to keep this literal (it's at a level not covered)
    Level level = s->vars[v].level;
    if (level > 0 && !(abstract_levels & abstract_level(level))) {
        return false;
    }

    // Handle binary propagation or decision
    if (reason == INVALID_CLAUSE) {
        // Both decision variables and binary propagations are never redundant.
        // Binary propagations store their reason in binary_reasons[], but we
        // conservatively treat them as non-redundant to avoid complexity.
        return false;
    }

    // Mark as being explored (cycle detection)
    // Save original value to restore on failure
    uint8_t orig_seen = s->seen[v];
    s->seen[v] = 2;
    unsigned max_height = 0;

    // Check all literals in reason clause (normal clause from arena)
    uint32_t size = CLAUSE_SIZE(s->arena, reason);
    Lit* lits = CLAUSE_LITS(s->arena, reason);

    for (uint32_t i = 0; i < size; i++) {
        /* A depth bound alone does not bound repeated traversal of shared
           subgraphs. Charge every inspected reason literal and unwind all
           scratch marks when the clause budget or solver deadline expires. */
        if (!*remaining) { s->seen[v] = orig_seen; return false; }
        --*remaining;
        if ((++s->stats.minimize_inspections & 1023) == 0 && solver_budget_exhausted(s)) {
            s->seen[v] = orig_seen;
            return false;
        }
        Lit q = lits[i];
        Var qv = var(q);

        // Skip the literal itself (it's the implied literal)
        if (qv == v) continue;

        // SAFETY CHECK: If a reason literal is at a higher level than the implied
        // literal, the reason clause is stale (the variable was reassigned after
        // the original propagation). This can happen during chronological
        // backtracking when we backtrack partway, reassign some variables, and
        // then reach another conflict. In this case, return false to keep the
        // literal in the learned clause.
        if (s->vars[qv].level > level) {
            s->seen[v] = orig_seen;
            return false;
        }

        // Root antecedents need no source-clause literal.
        if (s->vars[qv].level == 0) {
            continue;
        }

        // Recursively check if this literal is covered
        unsigned child_height;
        if (!lit_redundant(s, q, abstract_levels, depth + 1, remaining, touched, &child_height)) {
            s->seen[v] = orig_seen;
            return false;
        }
        if (child_height + 1 > max_height) max_height = child_height + 1;
    }

    // Cache only complete proofs under the current source-literal set. All
    // cached marks are cleared before considering the next source literal.
    ASSERT(depth + max_height <= 128);
    s->seen[v] = (uint8_t)(max_height + 3);
    ASSERT(*touched < s->num_vars + 1);
    s->minimize_touched[(*touched)++] = v;
    *height = max_height;
    return true;
}

// Main minimization function - called after conflict analysis
// learnt[0] is the asserting literal (always kept)
// Returns number of literals removed
//
// Algorithm: For each literal L in the clause, check if all paths from L
// back to decision variables pass through other literals in the clause.
// If so, L is redundant and can be removed.
//
// Key safety: coverage comes from source literals or fully proved cached
// subtrees; exploring nodes never count as coverage. Cache lifetime ends before
// the next candidate, because removal changes the source-literal set.
static uint32_t legacy_minimize_clause(Solver* s, Lit* learnt, uint32_t* learnt_size) {
    // Skip if minimization is disabled
    if (!s->opts.minimize || !s->opts.minimize_budget || s->interrupted) {
        return 0;
    }

    if (*learnt_size <= 2) {
        return 0;  // Don't minimize unit or binary clauses
    }

    uint32_t original_size = *learnt_size;
    uint32_t remaining = s->opts.minimize_budget;

    // Step 1: Compute abstract level bitmask for quick filtering
    uint64_t abstract_levels = 0;
    for (uint32_t i = 0; i < *learnt_size; i++) {
        Level level = s->vars[var(learnt[i])].level;
        abstract_levels |= abstract_level(level);
    }

    // Step 2: Mark all literals in learned clause (seen = 1)
    for (uint32_t i = 0; i < *learnt_size; i++) {
        s->seen[var(learnt[i])] = 1;
    }

    // Step 3: Try to remove each literal (except asserting literal at [0])
    uint32_t new_size = 1;  // Keep learnt[0] (asserting literal)
    for (uint32_t i = 1; i < *learnt_size; i++) {
        Lit p = learnt[i];
        Var v = var(p);

        // Skip decision variables and binary propagations
        if (!remaining || s->interrupted || s->vars[v].reason == INVALID_CLAUSE) {
            learnt[new_size++] = p;
            continue;
        }

        // CRITICAL: Temporarily clear seen[v] before checking redundancy.
        // This prevents circular dependencies where literal p uses itself
        // as coverage (e.g., p's reason leads to q, q's reason leads to p).
        s->seen[v] = 0;

        uint32_t touched = 0;
        unsigned height;
        bool redundant = lit_redundant(s, p, abstract_levels, 0, &remaining, &touched, &height);
        /* Removing this source literal changes coverage for the next check.
           Never carry a cached proof across that change. */
        for (uint32_t k = 0; k < touched; ++k) s->seen[s->minimize_touched[k]] = 0;
        if (!redundant) {
            // Not redundant - restore and keep
            s->seen[v] = 1;
            learnt[new_size++] = p;
        }
        // else: redundant, leave seen[v] = 0 so future checks can't use it
    }

    *learnt_size = new_size;
    if (!remaining) s->stats.minimize_budget_hits++;

    // Step 4: Clear seen array
    // Clear remaining literals in the learned clause
    for (uint32_t i = 0; i < new_size; i++) {
        s->seen[var(learnt[i])] = 0;
    }

    return original_size - new_size;
}


enum { UNSEEN, SOURCE, PENDING, REMOVABLE, RETRY };

static uint64_t level_bit(Level level) { return UINT64_C(1) << (level & 63); }

static bool has_reason(const Solver *s, Var v) {
    return s->vars[v].reason != INVALID_CLAUSE || s->binary_reasons[v] != LIT_UNDEF;
}

/* Success means every antecedent ends at a source literal, an already proven
 * removable literal, or the root. Source literals may themselves be removed
 * later: strict trail ordering makes this dependency relation acyclic.
 * On failure, PENDING entries become RETRY, never a successful cache entry.
 * RETRY also remembers that a variable is already on the cleanup list.
 */
static bool redundant(Solver *s, Lit lit, uint64_t levels,
                      uint32_t *remaining, uint32_t *touched) {
    Var root = var(lit);
    uint32_t head = 0, tail = 1;
    s->analyze_stack[0] = lit;
    s->seen[root] = PENDING;
    bool success = true;
    while (head < tail && success) {
        Var v = var(s->analyze_stack[head++]);
        CRef reason = s->vars[v].reason;
        Lit binary = s->binary_reasons[v];
        bool implicit = reason == INVALID_CLAUSE;
        if (implicit && binary == LIT_UNDEF) { success = false; break; }
        if (!implicit && (reason == BINARY_CONFLICT || clause_deleted(s->arena, reason))) {
            success = false; break;
        }
        uint32_t size = implicit ? 1 : CLAUSE_SIZE(s->arena, reason);
        if (implicit) s->stats.minimize_binary_steps++;
        for (uint32_t i = 0; i < size; ++i) {
            if (!*remaining) { success = false; break; }
            --*remaining;
            if ((++s->stats.minimize_inspections & 1023) == 0 && solver_budget_exhausted(s)) {
                success = false; break;
            }
            Lit q = implicit ? binary : CLAUSE_LITS(s->arena, reason)[i];
            Var u = var(q);
            if (!implicit && u == v) continue;  // The propagated literal in an explicit reason.
            /* Reject stale or cyclic reasons rather than making a deletion
               depend on scratch-mark traversal order. */
            if (s->values[u] == UNDEF || s->vars[u].trail_pos >= s->vars[v].trail_pos ||
                lxor(s->values[u], sign(q)) != FALSE) { success = false; break; }
            if (!s->vars[u].level) continue;
            uint8_t mark = s->seen[u];
            if (mark == SOURCE) continue;
            if (mark == REMOVABLE) { s->stats.minimize_cache_hits++; continue; }
            if (!(levels & level_bit(s->vars[u].level)) || !has_reason(s, u)) {
                success = false; break;
            }
            if (mark == PENDING) continue; // Shared earlier node, not a cycle.
            if (mark == UNSEEN) s->minimize_touched[(*touched)++] = u;
            s->seen[u] = PENDING;
            ASSERT(tail < s->num_vars + 1);
            s->analyze_stack[tail++] = q;
        }
    }
    for (uint32_t i = 0; i < tail; ++i)
        s->seen[var(s->analyze_stack[i])] = success ? REMOVABLE : RETRY;
    if (!success) s->seen[root] = SOURCE;
    return success;
}

uint32_t solver_minimize_clause(Solver *s, Lit *learnt, uint32_t *size) {
    if (!s->opts.iterative_minimize) return legacy_minimize_clause(s, learnt, size);
    if (!s->opts.minimize || !s->opts.minimize_budget || *size <= 2) return 0;
    uint32_t original = *size, touched = 0, remaining = s->opts.minimize_budget;
    uint64_t levels = 0;
    for (uint32_t i = 0; i < original; ++i) {
        Var v = var(learnt[i]);
        ASSERT(s->seen[v] == UNSEEN);
        s->seen[v] = SOURCE;
        s->minimize_touched[touched++] = v;
        levels |= level_bit(s->vars[v].level);
    }
    uint32_t out = 1; // Preserve the asserting literal.
    for (uint32_t i = 1; i < original; ++i) {
        Lit lit = learnt[i];
        Var v = var(lit);
        if (!remaining || s->interrupted || !has_reason(s, v) ||
            !redundant(s, lit, levels, &remaining, &touched)) learnt[out++] = lit;
    }
    if (!remaining) s->stats.minimize_budget_hits++;
    for (uint32_t i = 0; i < touched; ++i) s->seen[s->minimize_touched[i]] = UNSEEN;
    *size = out;
    return original - out;
}

/* Resolve (a | b | rest) with (a | ~b), keeping the asserting literal a.
   Exact signed marks make this independent of the current assignment values.
   Eager watch deletion guarantees every inspected binary is still active. */
uint32_t solver_minimize_binary(Solver *s, Lit *learnt, uint32_t *size, uint32_t lbd) {
    if (!s->opts.binary_minimize || !s->opts.minimize || !s->opts.minimize_budget ||
        *size < 2 || *size > 30 || lbd > 6 || solver_budget_exhausted(s)) return 0;
    uint32_t n = *size;
    for (uint32_t i = 1; i < n; ++i) {
        Var v = var(learnt[i]);ASSERT(!s->seen[v]);
        s->seen[v] = 1 + sign(learnt[i]);
    }
    WatchList *wl = watch_list(s->watches, learnt[0]);
    uint32_t limit = MIN(wl->size, s->opts.minimize_budget);
    for (uint32_t i = 0; i < limit; ++i) {
        s->stats.binary_minimize_checks++;
        if ((++s->stats.minimize_inspections & 1023) == 0 && solver_budget_exhausted(s)) break;
        Watch w = wl->watches[i];
        if (!is_binary_watch(w) && !is_arena_binary_watch(w)) continue;
        Lit removed = neg(w.blocker);
        if (s->seen[var(removed)] == 1 + sign(removed)) s->seen[var(removed)] = 3;
    }
    uint32_t out = 1;
    for (uint32_t i = 1; i < n; ++i) {
        Lit lit = learnt[i];Var v = var(lit);
        if (s->seen[v] != 3) learnt[out++] = lit;
        s->seen[v] = 0;
    }
    *size = out;s->stats.binary_minimize_removed += n-out;
    return n-out;
}
