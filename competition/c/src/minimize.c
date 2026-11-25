/* Learned-clause shrinking and minimization.
 *
 * Implements:
 * 1. Block-level all-UIP shrinking (Fleury & Biere / Kissat shrink.c):
 *    collapses multi-literal same-level blocks down to a single local UIP
 *    using a bounded trail-position max-heap, sharing the 3-state minimizer
 *    for lower-level antecedents.
 * 2. Fast 3-state binary-aware DFS minimization (MiniSat / Kissat):
 *    traverses both explicit arena reasons and implicit binary_reasons[],
 *    caching both REMOVABLE and POISON (failed) subgraphs across all literals
 *    of the learned clause without clearing between candidates.
 * 3. Bounded binary-resolution minimization on short low-LBD clauses.
 */
#include "../include/solver.h"
#include "../include/gauss.h"

enum {
    MIN_UNSEEN    = 0,
    MIN_SOURCE    = 1,
    MIN_REMOVABLE = 2,
    MIN_POISON    = 3,
    MIN_EXPLORING = 4,
    MIN_BLOCK     = 5,
    MIN_RESOLVED  = 6
};

#define MIN_MAX_DEPTH 2048u
#define SHRINK_MAX_BLOCK_TOUCHED 256u

static inline uint64_t level_bit(Level level) {
    return UINT64_C(1) << (level & 63);
}

static inline bool has_reason(const Solver *s, Var v) {
    return s->vars[v].reason != INVALID_CLAUSE || s->binary_reasons[v] != LIT_UNDEF;
}

typedef struct {
    Var v;
    uint32_t next_idx;
} MinFrame;

/* Iterative 3-state DFS redundancy check with persistent REMOVABLE and POISON marks.
 * Uses a compact explicit stack (zero C call-stack recursion, safe on small pthread stacks).
 * Returns true iff root is proven covered by MIN_SOURCE literals + root units.
 * Only marks MIN_POISON on genuine logical failure (not budget/depth/interrupt).
 */
static bool dfs_redundant(Solver *s, Var root, uint64_t abstract_levels,
                          unsigned start_depth, uint32_t *remaining,
                          uint32_t *touched, bool *aborted) {
    uint8_t st = s->seen[root];
    if (st == MIN_SOURCE) return true;
    if (st == MIN_REMOVABLE) {
        s->stats.minimize_cache_hits++;
        return true;
    }
    if (st == MIN_POISON) {
        s->stats.minimize_cache_hits++;
        return false;
    }
    if (st == MIN_EXPLORING) return false;
    if (start_depth >= MIN_MAX_DEPTH) {
        *aborted = true;
        return false;
    }

    Level root_level = s->vars[root].level;
    if (root_level == 0) return true;

    CRef root_reason = s->vars[root].reason;
    bool root_implicit = (s->binary_reasons[root] != LIT_UNDEF);
    if ((!root_implicit && (root_reason == INVALID_CLAUSE || root_reason == BINARY_CONFLICT ||
                            clause_deleted(s->arena, root_reason))) ||
        !(abstract_levels & level_bit(root_level))) {
        s->seen[root] = MIN_POISON;
        s->minimize_touched[(*touched)++] = root;
        return false;
    }

    MinFrame stack[MIN_MAX_DEPTH];
    s->seen[root] = MIN_EXPLORING;
    stack[0] = (MinFrame){root, 0u};
    uint32_t top = 1u;
    bool result_ok = true;
    bool any_aborted = false;

    while (top > 0u) {
        MinFrame *f = &stack[top - 1u];
        Var v = f->v;

        if (!result_ok) {
            --top;
            if (any_aborted) {
                s->seen[v] = MIN_UNSEEN;
            } else {
                s->seen[v] = MIN_POISON;
                s->minimize_touched[(*touched)++] = v;
            }
            continue;
        }

        Level level = s->vars[v].level;
        uint32_t v_pos = s->vars[v].trail_pos;
        CRef reason = s->vars[v].reason;
        Lit binary = s->binary_reasons[v];
        bool implicit = (binary != LIT_UNDEF);

        if (implicit) {
            if (f->next_idx > 0u) {
                --top;
                s->seen[v] = MIN_REMOVABLE;
                s->minimize_touched[(*touched)++] = v;
                continue;
            }
            f->next_idx = 1u;
            if (!*remaining) {
                any_aborted = true;
                result_ok = false;
                continue;
            }
            --*remaining;
            s->stats.minimize_binary_steps++;
            if ((++s->stats.minimize_inspections & 1023) == 0 && solver_budget_exhausted(s)) {
                any_aborted = true;
                result_ok = false;
                continue;
            }
            Var u = var(binary);
            if (s->values[u] == UNDEF || s->vars[u].trail_pos >= v_pos ||
                s->vars[u].level > level ||
                lxor(s->values[u], sign(binary)) != FALSE) {
                result_ok = false;
                continue;
            }
            if (s->vars[u].level == 0) continue;

            uint8_t ust = s->seen[u];
            if (ust == MIN_SOURCE) continue;
            if (ust == MIN_REMOVABLE) { s->stats.minimize_cache_hits++; continue; }
            if (ust == MIN_POISON) { s->stats.minimize_cache_hits++; result_ok = false; continue; }
            if (ust == MIN_EXPLORING) { result_ok = false; continue; }
            if (start_depth + top >= MIN_MAX_DEPTH) {
                any_aborted = true;
                result_ok = false;
                continue;
            }
            Level ul = s->vars[u].level;
            CRef ur = s->vars[u].reason;
            bool uimp = (s->binary_reasons[u] != LIT_UNDEF);
            if ((!uimp && (ur == INVALID_CLAUSE || ur == BINARY_CONFLICT ||
                           clause_deleted(s->arena, ur))) ||
                !(abstract_levels & level_bit(ul))) {
                s->seen[u] = MIN_POISON;
                s->minimize_touched[(*touched)++] = u;
                result_ok = false;
                continue;
            }
            s->seen[u] = MIN_EXPLORING;
            stack[top++] = (MinFrame){u, 0u};
        } else {
            if (s->proof_file && s->gauss) gauss_ensure_clause_proved(s, reason);
            uint32_t size = CLAUSE_SIZE(s->arena, reason);
            const Lit *lits = CLAUSE_LITS(s->arena, reason);
            bool pushed = false;
            while (f->next_idx < size) {
                if (!*remaining) {
                    any_aborted = true;
                    result_ok = false;
                    break;
                }
                --*remaining;
                if ((++s->stats.minimize_inspections & 1023) == 0 && solver_budget_exhausted(s)) {
                    any_aborted = true;
                    result_ok = false;
                    break;
                }
                Lit q = lits[f->next_idx++];
                Var u = var(q);
                if (u == v) continue;
                if (s->values[u] == UNDEF || s->vars[u].trail_pos >= v_pos ||
                    s->vars[u].level > level ||
                    lxor(s->values[u], sign(q)) != FALSE) {
                    result_ok = false;
                    break;
                }
                if (s->vars[u].level == 0) continue;

                uint8_t ust = s->seen[u];
                if (ust == MIN_SOURCE) continue;
                if (ust == MIN_REMOVABLE) { s->stats.minimize_cache_hits++; continue; }
                if (ust == MIN_POISON) { s->stats.minimize_cache_hits++; result_ok = false; break; }
                if (ust == MIN_EXPLORING) { result_ok = false; break; }
                if (start_depth + top >= MIN_MAX_DEPTH) {
                    any_aborted = true;
                    result_ok = false;
                    break;
                }
                Level ul = s->vars[u].level;
                CRef ur = s->vars[u].reason;
                bool uimp = (s->binary_reasons[u] != LIT_UNDEF);
                if ((!uimp && (ur == INVALID_CLAUSE || ur == BINARY_CONFLICT ||
                               clause_deleted(s->arena, ur))) ||
                    !(abstract_levels & level_bit(ul))) {
                    s->seen[u] = MIN_POISON;
                    s->minimize_touched[(*touched)++] = u;
                    result_ok = false;
                    break;
                }
                s->seen[u] = MIN_EXPLORING;
                stack[top++] = (MinFrame){u, 0u};
                pushed = true;
                break;
            }
            if (!pushed && result_ok) {
                --top;
                s->seen[v] = MIN_REMOVABLE;
                s->minimize_touched[(*touched)++] = v;
            }
        }
    }

    if (any_aborted) *aborted = true;
    return result_ok;
}

/* Check whether a MIN_SOURCE literal v is redundant wrt the remaining SOURCE set. */
static bool source_redundant(Solver *s, Var v, uint64_t abstract_levels,
                             uint32_t *remaining, uint32_t *touched) {
    CRef reason = s->vars[v].reason;
    Lit binary = s->binary_reasons[v];
    bool implicit = (binary != LIT_UNDEF);
    if (!implicit && (reason == INVALID_CLAUSE || reason == BINARY_CONFLICT || clause_deleted(s->arena, reason))) return false;

    Level level = s->vars[v].level;
    if (level == 0) return true;

    uint32_t v_pos = s->vars[v].trail_pos;
    bool aborted = false;

    /* Temporarily mark as EXPLORING so v cannot cover itself. */
    s->seen[v] = MIN_EXPLORING;
    bool ok = true;

    if (implicit) {
        if (!*remaining) {
            aborted = true;
            ok = false;
        } else {
            --*remaining;
            s->stats.minimize_binary_steps++;
            if ((++s->stats.minimize_inspections & 1023) == 0 && solver_budget_exhausted(s)) {
                aborted = true;
                ok = false;
            } else {
                Var u = var(binary);
                if (s->values[u] == UNDEF || s->vars[u].trail_pos >= v_pos ||
                    s->vars[u].level > level ||
                    lxor(s->values[u], sign(binary)) != FALSE) {
                    ok = false;
                } else if (s->vars[u].level > 0) {
                    ok = dfs_redundant(s, u, abstract_levels, 1,
                                       remaining, touched, &aborted);
                }
            }
        }
    } else {
        if (s->proof_file && s->gauss) gauss_ensure_clause_proved(s, reason);
        uint32_t size = CLAUSE_SIZE(s->arena, reason);
        const Lit *lits = CLAUSE_LITS(s->arena, reason);
        for (uint32_t i = 0; i < size; ++i) {
            if (!*remaining) {
                aborted = true;
                ok = false;
                break;
            }
            --*remaining;
            if ((++s->stats.minimize_inspections & 1023) == 0 && solver_budget_exhausted(s)) {
                aborted = true;
                ok = false;
                break;
            }
            Lit q = lits[i];
            Var u = var(q);
            if (u == v) continue;
            if (s->values[u] == UNDEF || s->vars[u].trail_pos >= v_pos ||
                s->vars[u].level > level ||
                lxor(s->values[u], sign(q)) != FALSE) {
                ok = false;
                break;
            }
            if (s->vars[u].level == 0) continue;
            if (!dfs_redundant(s, u, abstract_levels, 1,
                               remaining, touched, &aborted)) {
                ok = false;
                break;
            }
        }
    }

    /* Leave removed SOURCE literals marked MIN_REMOVABLE so subsequent checks
     * can transitively reuse them (acyclic trail order guarantees soundness).
     * Non-redundant SOURCE literals remain MIN_SOURCE. */
    s->seen[v] = ok ? MIN_REMOVABLE : MIN_SOURCE;
    (void)aborted;
    return ok;
}

/* Max-heap helpers on trail_pos for block shrinking. */
static inline void bheap_push(const Solver *s, Var *heap, uint32_t *hsize, Var v) {
    uint32_t i = (*hsize)++;
    uint32_t pos = s->vars[v].trail_pos;
    while (i > 0) {
        uint32_t p = (i - 1) >> 1;
        Var pv = heap[p];
        if (s->vars[pv].trail_pos >= pos) break;
        heap[i] = pv;
        i = p;
    }
    heap[i] = v;
}

static inline Var bheap_pop(const Solver *s, Var *heap, uint32_t *hsize) {
    Var top = heap[0];
    Var last = heap[--(*hsize)];
    uint32_t n = *hsize;
    if (n == 0) return top;
    uint32_t pos = s->vars[last].trail_pos;
    uint32_t i = 0;
    for (;;) {
        uint32_t c = (i << 1) + 1;
        if (c >= n) break;
        if (c + 1 < n && s->vars[heap[c + 1]].trail_pos > s->vars[heap[c]].trail_pos) c++;
        if (pos >= s->vars[heap[c]].trail_pos) break;
        heap[i] = heap[c];
        i = c;
    }
    heap[i] = last;
    return top;
}

/* Sort learnt[1..n-1] by descending (level, trail_pos) using Shell sort. */
static void sort_by_level_desc(const Solver *s, Lit *lits, uint32_t count) {
    for (uint32_t gap = count >> 1; gap > 0; gap >>= 1) {
        for (uint32_t i = gap; i < count; ++i) {
            Lit tmp = lits[i];
            Level tl = s->vars[var(tmp)].level;
            uint32_t tp = s->vars[var(tmp)].trail_pos;
            uint32_t j = i;
            while (j >= gap) {
                Lit prev = lits[j - gap];
                Level pl = s->vars[var(prev)].level;
                if (pl > tl || (pl == tl && s->vars[var(prev)].trail_pos >= tp)) break;
                lits[j] = prev;
                j -= gap;
            }
            lits[j] = tmp;
        }
    }
}

/* Try to collapse a same-level block [start, end) at level L to a single UIP.
 * On success, writes the single replacement literal to *out_uip and returns true. */
static bool shrink_block(Solver *s, const Lit *block, uint32_t k, Level L,
                         uint64_t abstract_levels, uint32_t *remaining,
                         uint32_t *touched, Lit *out_uip) {
    if (k < 2 || k > SHRINK_MAX_BLOCK_TOUCHED) return false;

    Var heap[SHRINK_MAX_BLOCK_TOUCHED];
    Var btouched[SHRINK_MAX_BLOCK_TOUCHED];
    uint8_t orig_seen[SHRINK_MAX_BLOCK_TOUCHED];
    uint32_t hsize = 0, bt_count = 0;

    for (uint32_t i = 0; i < k; ++i) {
        Var v = var(block[i]);
        orig_seen[bt_count] = s->seen[v];
        s->seen[v] = MIN_BLOCK;
        btouched[bt_count++] = v;
        bheap_push(s, heap, &hsize, v);
    }

    bool ok = true;
    while (hsize > 1 && ok) {
        Var v = bheap_pop(s, heap, &hsize);
        s->seen[v] = MIN_RESOLVED;
        CRef reason = s->vars[v].reason;
        Lit binary = s->binary_reasons[v];
        bool implicit = (binary != LIT_UNDEF);
        if (!implicit && (reason == INVALID_CLAUSE || reason == BINARY_CONFLICT || clause_deleted(s->arena, reason))) {
            ok = false; break;
        }
        uint32_t v_pos = s->vars[v].trail_pos;
        if (!implicit && s->proof_file && s->gauss) gauss_ensure_clause_proved(s, reason);
        uint32_t rsize = implicit ? 1 : CLAUSE_SIZE(s->arena, reason);
        const Lit *rlits = implicit ? &binary : CLAUSE_LITS(s->arena, reason);

        for (uint32_t i = 0; i < rsize; ++i) {
            if (!*remaining) { ok = false; break; }
            --*remaining;
            if ((++s->stats.minimize_inspections & 1023) == 0 && solver_budget_exhausted(s)) {
                ok = false; break;
            }
            Lit q = rlits[i];
            Var u = var(q);
            if (!implicit && u == v) continue;
            if (s->values[u] == UNDEF || s->vars[u].trail_pos >= v_pos ||
                lxor(s->values[u], sign(q)) != FALSE) {
                ok = false; break;
            }
            Level ul = s->vars[u].level;
            if (ul == 0) continue;
            if (ul > L) { ok = false; break; }
            if (ul == L) {
                uint8_t st = s->seen[u];
                if (st == MIN_BLOCK || st == MIN_RESOLVED) continue;
                if (bt_count >= SHRINK_MAX_BLOCK_TOUCHED) { ok = false; break; }
                orig_seen[bt_count] = st;
                s->seen[u] = MIN_BLOCK;
                btouched[bt_count++] = u;
                bheap_push(s, heap, &hsize, u);
            } else {
                uint8_t st = s->seen[u];
                if (st == MIN_SOURCE || st == MIN_REMOVABLE) continue;
                if (st == MIN_POISON) { ok = false; break; }
                bool aborted = false;
                if (!dfs_redundant(s, u, abstract_levels, 1, remaining, touched, &aborted)) {
                    ok = false; break;
                }
            }
        }
    }

    if (ok && hsize == 1) {
        Var uip = heap[0];
        if (s->values[uip] == UNDEF || s->vars[uip].level != L) {
            ok = false;
        } else {
            /* Clear all block marks, then mark the single UIP as SOURCE.
             * Only push uip onto minimize_touched if it was previously UNSEEN. */
            uint8_t uip_orig = MIN_UNSEEN;
            for (uint32_t i = 0; i < bt_count; ++i) {
                if (btouched[i] == uip) uip_orig = orig_seen[i];
                s->seen[btouched[i]] = (orig_seen[i] == MIN_SOURCE) ? MIN_UNSEEN : orig_seen[i];
            }
            s->seen[uip] = MIN_SOURCE;
            if (uip_orig == MIN_UNSEEN) s->minimize_touched[(*touched)++] = uip;
            *out_uip = mkLit(uip, s->values[uip] == TRUE);
            return true;
        }
    }

    /* Rollback: restore exact prior seen[] states for every touched L-variable. */
    for (uint32_t i = 0; i < bt_count; ++i) s->seen[btouched[i]] = orig_seen[i];
    return false;
}

uint32_t solver_shrink_clause(Solver *s, Lit *learnt, uint32_t *size) {
    if (!s->opts.minimize || !s->opts.shrink || !s->opts.minimize_budget ||
        s->interrupted || *size <= 2) {
        return 0;
    }

    uint32_t original = *size;
    uint32_t remaining = s->opts.minimize_budget;
    uint32_t touched = 0;
    uint64_t abstract_levels = 0;

    /* Sort non-asserting literals by descending (level, trail_pos) so each
     * decision level forms a contiguous block and higher levels shrink first. */
    sort_by_level_desc(s, learnt + 1, original - 1);

    for (uint32_t i = 0; i < original; ++i) {
        Var v = var(learnt[i]);
        ASSERT(s->seen[v] == MIN_UNSEEN);
        s->seen[v] = MIN_SOURCE;
        s->minimize_touched[touched++] = v;
        abstract_levels |= level_bit(s->vars[v].level);
    }

    /* Pass 1: Block-level all-UIP shrinking (high level -> low level). */
    uint32_t out = 1;
    for (uint32_t i = 1; i < original;) {
        Level L = s->vars[var(learnt[i])].level;
        uint32_t end = i + 1;
        while (end < original && s->vars[var(learnt[end])].level == L) ++end;
        uint32_t k = end - i;
        Lit uip = LIT_UNDEF;
        if (k >= 2 && remaining && !s->interrupted &&
            shrink_block(s, learnt + i, k, L, abstract_levels, &remaining, &touched, &uip)) {
            learnt[out++] = uip;
        } else {
            for (uint32_t j = i; j < end; ++j) learnt[out++] = learnt[j];
        }
        i = end;
    }

    /* Pass 2: 3-state recursive minimization reusing REMOVABLE/POISON marks. */
    uint32_t shrunk = out;
    out = 1;
    for (uint32_t i = 1; i < shrunk; ++i) {
        Lit p = learnt[i];
        Var v = var(p);
        if (!remaining || s->interrupted || !has_reason(s, v) ||
            !source_redundant(s, v, abstract_levels, &remaining, &touched)) {
            learnt[out++] = p;
        }
    }

    if (!remaining) s->stats.minimize_budget_hits++;
    for (uint32_t i = 0; i < touched; ++i) s->seen[s->minimize_touched[i]] = MIN_UNSEEN;
    *size = out;
    return original - out;
}

uint32_t solver_minimize_clause(Solver *s, Lit *learnt, uint32_t *size) {
    if (!s->opts.minimize || !s->opts.minimize_budget || s->interrupted || *size <= 2) {
        return 0;
    }

    uint32_t original = *size;
    uint32_t remaining = s->opts.minimize_budget;
    uint32_t touched = 0;
    uint64_t abstract_levels = 0;

    for (uint32_t i = 0; i < original; ++i) {
        Var v = var(learnt[i]);
        ASSERT(s->seen[v] == MIN_UNSEEN);
        s->seen[v] = MIN_SOURCE;
        s->minimize_touched[touched++] = v;
        abstract_levels |= level_bit(s->vars[v].level);
    }

    uint32_t out = 1;
    for (uint32_t i = 1; i < original; ++i) {
        Lit p = learnt[i];
        Var v = var(p);
        if (!remaining || s->interrupted || !has_reason(s, v) ||
            !source_redundant(s, v, abstract_levels, &remaining, &touched)) {
            learnt[out++] = p;
        }
    }

    if (!remaining) s->stats.minimize_budget_hits++;
    for (uint32_t i = 0; i < touched; ++i) s->seen[s->minimize_touched[i]] = MIN_UNSEEN;
    *size = out;
    return original - out;
}

/* Resolve (a | b | rest) with (a | ~b), keeping the asserting literal a.
   Exact signed marks make this independent of the current assignment values.
   Eager/lazy watch invariants: skip deleted clauses if present. */
uint32_t solver_minimize_binary(Solver *s, Lit *learnt, uint32_t *size, uint32_t lbd) {
    if (s->lrat) lrat_clear_bin_hints(s);
    if (!s->opts.binary_minimize || !s->opts.minimize || !s->opts.minimize_budget ||
        *size < 2 || *size > 30 || lbd > 6 || solver_budget_exhausted(s)) return 0;
    uint32_t n = *size;
    for (uint32_t i = 1; i < n; ++i) {
        Var v = var(learnt[i]); ASSERT(!s->seen[v]);
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
        if (s->seen[var(removed)] == 1 + sign(removed)) {
            if (s->lrat) {
                CRef bcr = watch_clause(w);
                if (bcr == INVALID_CLAUSE || clause_deleted(s->arena, bcr)) continue;
                if (!lrat_record_bin_hint(s, var(removed), bcr)) continue;
            }
            s->seen[var(removed)] = 3;
        }
    }
    uint32_t out = 1;
    for (uint32_t i = 1; i < n; ++i) {
        Lit lit = learnt[i]; Var v = var(lit);
        if (s->seen[v] != 3) learnt[out++] = lit;
        s->seen[v] = 0;
    }
    *size = out; s->stats.binary_minimize_removed += n - out;
    return n - out;
}
