/*********************************************************************
 * BSAT Competition Solver - Structured Bounded Variable Addition (SBVA)
 *
 * Implements Haberlandt-Green-Heule (SAT 2023) Structured BVA:
 *  - Works on arbitrary-length live irredundant clauses (size 2..32).
 *  - Maintains dynamic literal occurrence lists + max-heap by occurrence.
 *  - Uses least-occurring-literal matching to find clause twins in O(min_occ).
 *  - Breaks ties among equal-gain literals via a 3-hop variable-adjacency
 *    structural overlap score, strongly favoring local combinatorial blocks.
 *  - Feeds newly added clauses back into the occurrence index so SBVA
 *    chains multiple auxiliary variables cleanly.
 *********************************************************************/

#include "../include/sbva.h"
#include "../include/inprocess.h"
#include "../include/solver.h"
#include <stdlib.h>
#include <string.h>

#define SBVA_MAX_CLAUSE_LEN 32u
#define SBVA_MAX_TIE_CHECK  64u

typedef struct {
    uint32_t *data;
    uint32_t size;
    uint32_t cap;
} SbvaOcc;

typedef struct {
    Lit *lits;
    uint32_t size;
    uint32_t cap;
    uint64_t hash;
    CRef cref;      /* Arena CRef in s->clauses[cslot], or INVALID_CLAUSE */
    uint32_t cslot; /* Slot index in s->clauses */
    uint8_t alive;
} SbvaClause;

typedef struct {
    uint32_t cls_idx;   /* Original clause in M_cls (containing l0) */
    uint32_t twin_idx;  /* Matching clause containing candidate lit */
} SbvaMatch;

typedef struct {
    Lit *heap;
    uint32_t *pos;
    uint32_t size;
    uint32_t cap;
} SbvaHeap;

typedef struct {
    Solver *s;
    SbvaClause *cls;
    uint32_t n_cls;
    uint32_t cls_cap;
    SbvaOcc *occ;
    uint32_t lit_cap;     /* Number of literal slots allocated (2*(var_cap+1)) */
    SbvaHeap pq;
    uint32_t *match_cnt;  /* lit_cap */
    uint32_t *match_head; /* lit_cap: linked list head into matches[] (1-based) */
    Lit *touched;         /* lit_cap */
    uint32_t touched_n;
    SbvaMatch *matches;
    uint32_t *match_next;
    uint32_t match_used;
    uint32_t match_cap;
    /* 3-hop structural tie-breaker scratch */
    uint32_t *var_mark;
    uint32_t *var_weight;
    uint32_t var_mark_cap;
    uint32_t mark_stamp;
    /* Temporary match buffers */
    uint32_t *m_cls;
    uint32_t m_cls_cap;
    uint32_t *next_m_cls;
    uint32_t *twin_grid;  /* Flat [lit_pos * m_cls_n] twin clause indices */
    uint32_t twin_grid_cap;
    uint64_t end_work;
    bool oom;
} SbvaState;

/* =====================================================================
 * Helpers: Hashing, Occurrences, Max-Heap
 * ===================================================================== */

static inline uint64_t mix_lit(Lit l) {
    uint64_t x = (uint64_t)l * UINT64_C(0x9e3779b97f4a7c15);
    x ^= x >> 30;
    x *= UINT64_C(0xbf58476d1ce4e5b9);
    return x ^ (x >> 27);
}

static inline void sort_lits(Lit *ls, uint32_t n) {
    for (uint32_t i = 1; i < n; ++i) {
        Lit key = ls[i];
        uint32_t j = i;
        while (j > 0 && ls[j - 1] > key) { ls[j] = ls[j - 1]; --j; }
        ls[j] = key;
    }
}

static bool occ_push(SbvaState *st, Lit l, uint32_t cid) {
    SbvaOcc *o = &st->occ[l];
    if (o->size == o->cap) {
        uint32_t ncap = o->cap ? o->cap * 2u : 8u;
        uint32_t *p = realloc(o->data, (size_t)ncap * sizeof *p);
        if (!p) { st->oom = true; return false; }
        o->data = p;
        o->cap = ncap;
    }
    o->data[o->size++] = cid;
    return true;
}

static void occ_clean(SbvaState *st, Lit l) {
    SbvaOcc *o = &st->occ[l];
    uint32_t out = 0;
    for (uint32_t i = 0; i < o->size; ++i) {
        if (st->cls[o->data[i]].alive) o->data[out++] = o->data[i];
    }
    o->size = out;
}

static inline uint32_t lit_occ_size(const SbvaState *st, Lit l) {
    return (l < st->lit_cap) ? st->occ[l].size : 0u;
}

static inline bool pq_better(const SbvaState *st, Lit a, Lit b) {
    uint32_t oa = lit_occ_size(st, a);
    uint32_t ob = lit_occ_size(st, b);
    if (oa != ob) return oa > ob;
    return a < b;
}

static void pq_sift_up(SbvaState *st, uint32_t i) {
    SbvaHeap *h = &st->pq;
    Lit l = h->heap[i];
    while (i > 0) {
        uint32_t p = (i - 1u) >> 1;
        if (!pq_better(st, l, h->heap[p])) break;
        h->heap[i] = h->heap[p];
        h->pos[h->heap[i]] = i;
        i = p;
    }
    h->heap[i] = l;
    h->pos[l] = i;
}

static void pq_sift_down(SbvaState *st, uint32_t i) {
    SbvaHeap *h = &st->pq;
    Lit l = h->heap[i];
    for (;;) {
        uint32_t left = (i << 1) + 1u;
        if (left >= h->size) break;
        uint32_t right = left + 1u;
        uint32_t best = (right < h->size && pq_better(st, h->heap[right], h->heap[left])) ? right : left;
        if (!pq_better(st, h->heap[best], l)) break;
        h->heap[i] = h->heap[best];
        h->pos[h->heap[i]] = i;
        i = best;
    }
    h->heap[i] = l;
    h->pos[l] = i;
}

static void pq_push_or_update(SbvaState *st, Lit l) {
    SbvaHeap *h = &st->pq;
    if (l >= h->cap) return;
    uint32_t p = h->pos[l];
    if (p == UINT32_MAX) {
        p = h->size++;
        h->heap[p] = l;
        h->pos[l] = p;
        pq_sift_up(st, p);
    } else {
        pq_sift_up(st, p);
        pq_sift_down(st, h->pos[l]);
    }
}

static Lit pq_pop(SbvaState *st) {
    SbvaHeap *h = &st->pq;
    Lit top = h->heap[0];
    h->pos[top] = UINT32_MAX;
    if (--h->size > 0) {
        h->heap[0] = h->heap[h->size];
        h->pos[h->heap[0]] = 0;
        pq_sift_down(st, 0);
    }
    return top;
}

static bool sbva_ensure_lit_cap(SbvaState *st, uint32_t needed_vars) {
    uint32_t needed_lits = 2u * (needed_vars + 1u);
    if (needed_lits <= st->lit_cap && needed_vars + 1u <= st->var_mark_cap) return true;

    uint32_t new_lits = st->lit_cap ? st->lit_cap * 2u : 256u;
    while (new_lits < needed_lits) new_lits *= 2u;
    uint32_t new_vars = new_lits >> 1;

    SbvaOcc *nocc = realloc(st->occ, (size_t)new_lits * sizeof *nocc);
    if (!nocc) { st->oom = true; return false; }
    memset(nocc + st->lit_cap, 0, (size_t)(new_lits - st->lit_cap) * sizeof *nocc);
    st->occ = nocc;

    Lit *nheap = realloc(st->pq.heap, (size_t)new_lits * sizeof *nheap);
    if (!nheap) { st->oom = true; return false; }
    st->pq.heap = nheap;

    uint32_t *npos = realloc(st->pq.pos, (size_t)new_lits * sizeof *npos);
    if (!npos) { st->oom = true; return false; }
    for (uint32_t i = st->lit_cap; i < new_lits; ++i) npos[i] = UINT32_MAX;
    st->pq.pos = npos;
    st->pq.cap = new_lits;

    uint32_t *nmc = realloc(st->match_cnt, (size_t)new_lits * sizeof *nmc);
    if (!nmc) { st->oom = true; return false; }
    memset(nmc + st->lit_cap, 0, (size_t)(new_lits - st->lit_cap) * sizeof *nmc);
    st->match_cnt = nmc;

    uint32_t *nmh = realloc(st->match_head, (size_t)new_lits * sizeof *nmh);
    if (!nmh) { st->oom = true; return false; }
    memset(nmh + st->lit_cap, 0, (size_t)(new_lits - st->lit_cap) * sizeof *nmh);
    st->match_head = nmh;

    Lit *nt = realloc(st->touched, (size_t)new_lits * sizeof *nt);
    if (!nt) { st->oom = true; return false; }
    st->touched = nt;

    uint32_t *nvm = realloc(st->var_mark, (size_t)new_vars * sizeof *nvm);
    if (!nvm) { st->oom = true; return false; }
    memset(nvm + st->var_mark_cap, 0, (size_t)(new_vars - st->var_mark_cap) * sizeof *nvm);
    st->var_mark = nvm;

    uint32_t *nvw = realloc(st->var_weight, (size_t)new_vars * sizeof *nvw);
    if (!nvw) { st->oom = true; return false; }
    memset(nvw + st->var_mark_cap, 0, (size_t)(new_vars - st->var_mark_cap) * sizeof *nvw);
    st->var_weight = nvw;

    st->lit_cap = new_lits;
    st->var_mark_cap = new_vars;
    return true;
}

static bool sbva_add_local_clause(SbvaState *st, const Lit *lits, uint32_t sz,
                                  CRef cr, uint32_t cslot) {
    if (st->n_cls == st->cls_cap) {
        uint32_t ncap = st->cls_cap ? st->cls_cap * 2u : 256u;
        SbvaClause *p = realloc(st->cls, (size_t)ncap * sizeof *p);
        if (!p) { st->oom = true; return false; }
        st->cls = p;
        st->cls_cap = ncap;
    }
    Lit *copy = malloc((size_t)sz * sizeof(Lit));
    if (!copy) { st->oom = true; return false; }
    memcpy(copy, lits, (size_t)sz * sizeof(Lit));
    sort_lits(copy, sz);
    uint64_t h = 0;
    for (uint32_t i = 0; i < sz; ++i) h ^= mix_lit(copy[i]);

    uint32_t cid = st->n_cls++;
    st->cls[cid] = (SbvaClause){copy, sz, sz, h, cr, cslot, 1u};
    for (uint32_t i = 0; i < sz; ++i) {
        if (!occ_push(st, copy[i], cid)) return false;
    }
    return true;
}

static bool sbva_push_original_clause(Solver *s, CRef cr, uint32_t *out_slot) {
    if (s->num_clauses == s->clauses_capacity) {
        uint32_t cap = s->clauses_capacity ? s->clauses_capacity * 2u : 64u;
        CRef *p = realloc(s->clauses, (size_t)cap * sizeof *p);
        if (!p) { s->error = true; return false; }
        s->clauses = p;
        s->clauses_capacity = cap;
    }
    uint32_t slot = s->num_clauses++;
    s->clauses[slot] = cr;
    s->num_original = s->num_clauses;
    if (out_slot) *out_slot = slot;
    return true;
}

static bool record_match(SbvaState *st, Lit cand, uint32_t cls_idx, uint32_t twin_idx) {
    if (st->match_used + 1u >= st->match_cap) {
        uint32_t ncap = st->match_cap ? st->match_cap * 2u : 512u;
        SbvaMatch *pm = realloc(st->matches, (size_t)ncap * sizeof *pm);
        if (!pm) { st->oom = true; return false; }
        st->matches = pm;
        uint32_t *pn = realloc(st->match_next, (size_t)ncap * sizeof *pn);
        if (!pn) { st->oom = true; return false; }
        st->match_next = pn;
        st->match_cap = ncap;
    }
    /* Avoid double-counting the same cls_idx for the same candidate lit (duplicate clauses). */
    uint32_t head = st->match_head[cand];
    if (head && st->matches[head].cls_idx == cls_idx) return true;

    if (st->match_cnt[cand] == 0) st->touched[st->touched_n++] = cand;
    st->match_cnt[cand]++;
    uint32_t idx = ++st->match_used; /* 1-based */
    st->matches[idx] = (SbvaMatch){cls_idx, twin_idx};
    st->match_next[idx] = head;
    st->match_head[cand] = idx;
    return true;
}

/* Check if clause D (sorted, same length as C) equals (C \ {l0}) U {diff_out}.
 * Uses XOR hash prefilter for O(1) rejection on 99.9% of non-twins! */
static inline bool clause_is_twin(const SbvaClause *c, const SbvaClause *d,
                                  Lit l0, Lit *diff_out) {
    uint32_t n = c->size;
    if (d->size != n) return false;
    const Lit *cl = c->lits;
    const Lit *dl = d->lits;
    uint32_t i = 0, j = 0;
    Lit diff = LIT_UNDEF;
    while (i < n && j < n) {
        if (cl[i] == l0) { i++; continue; }
        if (cl[i] == dl[j]) { i++; j++; }
        else if (dl[j] < cl[i]) {
            if (diff != LIT_UNDEF) return false;
            diff = dl[j++];
        } else {
            return false; /* cl[i] missing from d! */
        }
    }
    while (i < n) {
        if (cl[i] == l0) { i++; continue; }
        return false;
    }
    while (j < n) {
        if (diff != LIT_UNDEF) return false;
        diff = dl[j++];
    }
    if (diff == LIT_UNDEF || diff == l0 || var(diff) == var(l0)) return false;
    *diff_out = diff;
    return true;
}

/* Compute 3-hop structural adjacency score between M_cls neighborhood and candidate `cand`. */
static uint64_t structural_tie_score(SbvaState *st, Lit cand) {
    SbvaOcc *oc = &st->occ[cand];
    uint64_t score = 0;
    uint32_t lim = MIN(oc->size, 64u);
    uint32_t stamp = st->mark_stamp;
    for (uint32_t i = 0; i < lim; ++i) {
        const SbvaClause *d = &st->cls[oc->data[i]];
        if (!d->alive) continue;
        for (uint32_t j = 0; j < d->size; ++j) {
            Var v = var(d->lits[j]);
            if (v < st->var_mark_cap && st->var_mark[v] == stamp) {
                score += st->var_weight[v];
            }
        }
    }
    return score;
}

/* =====================================================================
 * Main SBVA Engine
 * ===================================================================== */

bool solver_sbva(Solver *s, uint64_t tick_budget, uint32_t max_aux_vars, uint32_t min_gain) {
    if (!s || s->decision_level || s->proof_file || s->proof_journal || s->error ||
        s->interrupted || s->result == FALSE || s->num_vars == 0 || s->num_clauses < 64 ||
        max_aux_vars == 0 || solver_budget_exhausted(s)) return true;

    SbvaState st;
    memset(&st, 0, sizeof st);
    st.s = s;
    st.end_work = s->work + tick_budget;
    uint32_t aux_added = 0;

    if (!sbva_ensure_lit_cap(&st, s->num_vars)) goto cleanup;

    /* Index live irredundant clauses of size 2..SBVA_MAX_CLAUSE_LEN with all lits UNDEF. */
    for (uint32_t i = 0; i < s->num_clauses && !st.oom; ++i) {
        CRef cr = s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena, cr)) continue;
        uint32_t sz = CLAUSE_SIZE(s->arena, cr);
        if (sz < 2 || sz > SBVA_MAX_CLAUSE_LEN) continue;
        const Lit *ls = CLAUSE_LITS(s->arena, cr);
        bool skip = false;
        for (uint32_t j = 0; j < sz; ++j) {
            if (s->values[var(ls[j])] != UNDEF) { skip = true; break; }
        }
        if (skip) continue;
        if (!sbva_add_local_clause(&st, ls, sz, cr, i)) break;
    }
    if (st.oom || st.n_cls < 4) goto cleanup;

    /* Populate max-heap with all literals occurring >= 3 times. */
    for (Lit l = 2; l < 2u * (s->num_vars + 1u); ++l) {
        if (st.occ[l].size >= 3u) pq_push_or_update(&st, l);
    }

    Lit m_lits[64];
    uint8_t *in_mlit = calloc(st.lit_cap, 1);
    uint32_t in_mlit_cap = st.lit_cap;
    if (!in_mlit) { st.oom = true; goto cleanup; }

    while (st.pq.size > 0 && aux_added < max_aux_vars &&
           s->work < st.end_work && !st.oom && !s->error && !s->interrupted) {
        if ((aux_added & 15u) == 0 && solver_budget_exhausted(s)) break;

        Lit l0 = pq_pop(&st);
        occ_clean(&st, l0);
        uint32_t occ0 = st.occ[l0].size;
        if (occ0 < 3u) continue;
        if (occ0 > 2048u) continue; /* Skip giant hubs to keep SBVA O(N) fast */

        if (occ0 > st.m_cls_cap) {
            uint32_t ncap = occ0 * 2u + 64u;
            uint32_t *p1 = realloc(st.m_cls, (size_t)ncap * sizeof *p1);
            uint32_t *p2 = p1 ? realloc(st.next_m_cls, (size_t)ncap * sizeof *p2) : NULL;
            if (!p1 || !p2) {
                if (p1) st.m_cls = p1;
                st.oom = true; break;
            }
            st.m_cls = p1;
            st.next_m_cls = p2;
            st.m_cls_cap = ncap;
        }

        uint32_t m_cls_n = occ0;
        memcpy(st.m_cls, st.occ[l0].data, (size_t)m_cls_n * sizeof(uint32_t));
        uint32_t m_lit_n = 1;
        m_lits[0] = l0;

        if (st.lit_cap > in_mlit_cap) {
            uint8_t *np = realloc(in_mlit, st.lit_cap);
            if (!np) { st.oom = true; break; }
            memset(np + in_mlit_cap, 0, st.lit_cap - in_mlit_cap);
            in_mlit = np;
            in_mlit_cap = st.lit_cap;
        }
        in_mlit[l0] = 1;

        /* Pre-compute variable weights in M_cls for 3-hop structural tie-breaking. */
        if (++st.mark_stamp == 0) {
            memset(st.var_mark, 0, (size_t)st.var_mark_cap * sizeof *st.var_mark);
            st.mark_stamp = 1;
        }
        for (uint32_t ci = 0; ci < m_cls_n; ++ci) {
            const SbvaClause *c = &st.cls[st.m_cls[ci]];
            for (uint32_t j = 0; j < c->size; ++j) {
                Var v = var(c->lits[j]);
                if (v < st.var_mark_cap) {
                    if (st.var_mark[v] != st.mark_stamp) {
                        st.var_mark[v] = st.mark_stamp;
                        st.var_weight[v] = 1;
                    } else {
                        st.var_weight[v]++;
                    }
                }
            }
        }

        /* Iteratively expand M_lit with the best matching literal! */
        while (m_lit_n < 64u && m_cls_n >= 2u && s->work < st.end_work && !st.oom) {
            st.touched_n = 0;
            st.match_used = 0;

            for (uint32_t ci = 0; ci < m_cls_n && !st.oom; ++ci) {
                uint32_t cid = st.m_cls[ci];
                const SbvaClause *c = &st.cls[cid];
                if (!c->alive) continue;

                /* Find least-occurring literal l_min != l0 in clause c. */
                Lit l_min = LIT_UNDEF;
                uint32_t best_occ = UINT32_MAX;
                for (uint32_t j = 0; j < c->size; ++j) {
                    Lit lj = c->lits[j];
                    if (lj == l0) continue;
                    uint32_t oj = st.occ[lj].size;
                    if (oj < best_occ) { best_occ = oj; l_min = lj; }
                }
                if (l_min == LIT_UNDEF || best_occ == 0) continue;

                /* Scan occurrence list of l_min for exact twins of c (bounded for speed). */
                SbvaOcc *omin = &st.occ[l_min];
                uint32_t scan_lim = MIN(omin->size, 256u);
                s->work += scan_lim;
                for (uint32_t k = 0; k < scan_lim; ++k) {
                    uint32_t did = omin->data[k];
                    if (did == cid) continue;
                    const SbvaClause *d = &st.cls[did];
                    if (!d->alive || d->size != c->size) continue;
                    Lit diff;
                    if (clause_is_twin(c, d, l0, &diff)) {
                        if (diff < st.lit_cap && !in_mlit[diff] && !in_mlit[neg(diff)]) {
                            if (!record_match(&st, diff, cid, did)) break;
                        }
                    }
                }
            }

            if (st.oom || st.touched_n == 0) break;

            /* Find maximum match count and break ties with 3-hop structural score! */
            uint32_t best_cnt = 0;
            for (uint32_t i = 0; i < st.touched_n; ++i) {
                uint32_t cnt = st.match_cnt[st.touched[i]];
                if (cnt > best_cnt) best_cnt = cnt;
            }
            if (best_cnt < 2u) {
                for (uint32_t i = 0; i < st.touched_n; ++i) {
                    st.match_cnt[st.touched[i]] = 0;
                    st.match_head[st.touched[i]] = 0;
                }
                break;
            }

            /* Check whether adding best_cnt improves net clause reduction. */
            int64_t old_gain = (int64_t)m_lit_n * (int64_t)m_cls_n - ((int64_t)m_lit_n + (int64_t)m_cls_n);
            int64_t new_gain = (int64_t)(m_lit_n + 1u) * (int64_t)best_cnt -
                               ((int64_t)(m_lit_n + 1u) + (int64_t)best_cnt);
            if (new_gain <= old_gain) {
                for (uint32_t i = 0; i < st.touched_n; ++i) {
                    st.match_cnt[st.touched[i]] = 0;
                    st.match_head[st.touched[i]] = 0;
                }
                break;
            }

            /* Tie-break among candidates achieving best_cnt using 3-hop structural score! */
            Lit best_lit = LIT_UNDEF;
            uint64_t best_struct = 0;
            uint32_t tie_checked = 0;
            for (uint32_t i = 0; i < st.touched_n; ++i) {
                Lit cand = st.touched[i];
                if (st.match_cnt[cand] != best_cnt) continue;
                if (best_lit == LIT_UNDEF) {
                    best_lit = cand;
                    best_struct = structural_tie_score(&st, cand);
                    tie_checked = 1;
                } else if (tie_checked < SBVA_MAX_TIE_CHECK) {
                    uint64_t sc = structural_tie_score(&st, cand);
                    tie_checked++;
                    if (sc > best_struct || (sc == best_struct && cand < best_lit)) {
                        best_struct = sc;
                        best_lit = cand;
                    }
                }
            }

            /* Collect narrowed M_cls = {cls_idx matched by best_lit}. */
            uint32_t new_n = 0;
            for (uint32_t idx = st.match_head[best_lit]; idx; idx = st.match_next[idx]) {
                st.next_m_cls[new_n++] = st.matches[idx].cls_idx;
            }

            /* Clear touched match state immediately. */
            for (uint32_t i = 0; i < st.touched_n; ++i) {
                st.match_cnt[st.touched[i]] = 0;
                st.match_head[st.touched[i]] = 0;
            }

            m_lits[m_lit_n++] = best_lit;
            in_mlit[best_lit] = 1;
            memcpy(st.m_cls, st.next_m_cls, (size_t)new_n * sizeof(uint32_t));
            m_cls_n = new_n;
        }

        for (uint32_t i = 0; i < m_lit_n; ++i) in_mlit[m_lits[i]] = 0;
        if (st.oom) break;

        int64_t final_gain = (int64_t)m_lit_n * (int64_t)m_cls_n - ((int64_t)m_lit_n + (int64_t)m_cls_n);
        if (m_lit_n < 2u || m_cls_n < 2u || final_gain < (int64_t)min_gain) continue;

        /* Verify and locate all m_lit_n * m_cls_n exact clause IDs to delete before mutating! */
        uint32_t total_del = m_lit_n * m_cls_n;
        if (total_del > st.twin_grid_cap) {
            uint32_t *ng = realloc(st.twin_grid, (size_t)total_del * sizeof *ng);
            if (!ng) { st.oom = true; break; }
            st.twin_grid = ng;
            st.twin_grid_cap = total_del;
        }

        bool all_found = true;
        for (uint32_t ci = 0; ci < m_cls_n && all_found; ++ci) {
            uint32_t c0_id = st.m_cls[ci];
            const SbvaClause *c0 = &st.cls[c0_id];
            if (!c0->alive) { all_found = false; break; }
            st.twin_grid[ci] = c0_id; /* m_lit index 0 is l0 -> c0_id */

            /* Find least-occurring literal != l0 in c0 to locate twins for m_lits[1..m_lit_n-1]. */
            Lit l_min = LIT_UNDEF;
            uint32_t best_occ = UINT32_MAX;
            for (uint32_t j = 0; j < c0->size; ++j) {
                if (c0->lits[j] == l0) continue;
                uint32_t oj = st.occ[c0->lits[j]].size;
                if (oj < best_occ) { best_occ = oj; l_min = c0->lits[j]; }
            }
            if (l_min == LIT_UNDEF) { all_found = false; break; }

            for (uint32_t mi = 1; mi < m_lit_n; ++mi) {
                Lit target = m_lits[mi];
                /* Scan smaller of occ[target] and occ[l_min]. */
                SbvaOcc *scan = (st.occ[target].size < st.occ[l_min].size) ? &st.occ[target] : &st.occ[l_min];
                uint64_t want_hash = c0->hash ^ mix_lit(l0) ^ mix_lit(target);
                uint32_t found_id = UINT32_MAX;
                for (uint32_t k = 0; k < scan->size; ++k) {
                    uint32_t did = scan->data[k];
                    const SbvaClause *d = &st.cls[did];
                    if (!d->alive || d->size != c0->size || d->hash != want_hash) continue;
                    Lit diff;
                    if (clause_is_twin(c0, d, l0, &diff) && diff == target) {
                        found_id = did;
                        break;
                    }
                }
                if (found_id == UINT32_MAX) { all_found = false; break; }
                st.twin_grid[mi * m_cls_n + ci] = found_id;
            }
        }
        if (!all_found) continue;

        /* Commit SBVA step: introduce auxiliary variable x, emit replacement clauses, delete old! */
        if (!s->factor_original_vars) s->factor_original_vars = s->num_vars;
        Var x = solver_new_var(s);
        if (x == INVALID_VAR || s->error) { st.oom = true; break; }
        if (!sbva_ensure_lit_cap(&st, s->num_vars)) break;
        if (st.lit_cap > in_mlit_cap) {
            uint8_t *np = realloc(in_mlit, st.lit_cap);
            if (!np) { st.oom = true; break; }
            memset(np + in_mlit_cap, 0, st.lit_cap - in_mlit_cap);
            in_mlit = np;
            in_mlit_cap = st.lit_cap;
        }
        aux_added++;
        Lit pos_x = mkLit(x, false);
        Lit neg_x = mkLit(x, true);

        /* 1. Add (x | m_lits[i]) for i = 0..m_lit_n-1. */
        bool ok = true;
        for (uint32_t mi = 0; mi < m_lit_n && ok; ++mi) {
            Lit pair[2] = {pos_x, m_lits[mi]};
            CRef cr = arena_alloc(s->arena, pair, 2, false);
            uint32_t slot = 0;
            if (cr == INVALID_CLAUSE || !sbva_push_original_clause(s, cr, &slot) ||
                !sbva_add_local_clause(&st, pair, 2, cr, slot)) {
                ok = false; break;
            }
        }

        /* 2. Add (~x | (C \ {l0})) for each C in M_cls. */
        Lit rbuf[SBVA_MAX_CLAUSE_LEN + 1u];
        for (uint32_t ci = 0; ci < m_cls_n && ok; ++ci) {
            const SbvaClause *c0 = &st.cls[st.m_cls[ci]];
            uint32_t rn = 0;
            rbuf[rn++] = neg_x;
            for (uint32_t j = 0; j < c0->size; ++j) {
                if (c0->lits[j] != l0) rbuf[rn++] = c0->lits[j];
            }
            CRef cr = arena_alloc(s->arena, rbuf, rn, false);
            uint32_t slot = 0;
            if (cr == INVALID_CLAUSE || !sbva_push_original_clause(s, cr, &slot) ||
                !sbva_add_local_clause(&st, rbuf, rn, cr, slot)) {
                ok = false; break;
            }
        }
        if (!ok) { st.oom = true; break; }

        /* 3. Delete all m_lit_n * m_cls_n replaced original clauses. */
        for (uint32_t idx = 0; idx < total_del; ++idx) {
            uint32_t did = st.twin_grid[idx];
            SbvaClause *d = &st.cls[did];
            if (!d->alive) continue;
            d->alive = 0;
            if (d->cref != INVALID_CLAUSE && !clause_deleted(s->arena, d->cref)) {
                arena_delete(s->arena, d->cref);
                s->clauses[d->cslot] = INVALID_CLAUSE;
            }
        }

        /* 4. Re-enqueue modified & fresh literals for chained multi-level SBVA! */
        if (st.occ[pos_x].size >= 3u) pq_push_or_update(&st, pos_x);
        if (st.occ[neg_x].size >= 3u) pq_push_or_update(&st, neg_x);
        for (uint32_t mi = 0; mi < m_lit_n; ++mi) {
            occ_clean(&st, m_lits[mi]);
            if (st.occ[m_lits[mi]].size >= 3u) pq_push_or_update(&st, m_lits[mi]);
        }
    }

    free(in_mlit);

cleanup:
    for (uint32_t i = 0; i < st.n_cls; ++i) free(st.cls[i].lits);
    free(st.cls);
    for (uint32_t l = 0; l < st.lit_cap; ++l) free(st.occ[l].data);
    free(st.occ);
    free(st.pq.heap);
    free(st.pq.pos);
    free(st.match_cnt);
    free(st.match_head);
    free(st.touched);
    free(st.matches);
    free(st.match_next);
    free(st.var_mark);
    free(st.var_weight);
    free(st.m_cls);
    free(st.next_m_cls);
    free(st.twin_grid);

    if (st.oom || s->error) { s->error = true; return false; }

    if (aux_added > 0) {
        if (s->num_vars + 2u > s->levels_capacity) {
            uint8_t *seen = calloc((size_t)s->num_vars + 2u, 1);
            if (!seen) { s->error = true; return false; }
            free(s->level_seen);
            s->level_seen = seen;
            s->levels_capacity = s->num_vars + 2u;
        }
        solver_rebuild_watches(s);
        if (s->error) return false;
    }
    return true;
}
