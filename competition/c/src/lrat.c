/*********************************************************************
 * BSAT Competition Solver - Native LRAT Proof Logger & Verifier
 *********************************************************************/

#include "../include/lrat.h"
#include "../include/solver.h"
#include "../include/arena.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* =====================================================================
 * CRef -> 64-bit Clause ID Open-Addressed Hash Table
 * ===================================================================== */

#define LRAT_EMPTY_KEY     0u
#define LRAT_TOMBSTONE_KEY UINT32_MAX

typedef struct {
    CRef key;
    uint64_t id;
} LratSlot;

typedef struct {
    uint32_t trail_pos;
    uint64_t id;
} LratHintPair;

struct LratState {
    FILE *file;
    bool binary;
    bool emitted_empty;
    bool error;

    uint64_t next_id;
    uint64_t last_written_id;

    /* CRef -> clause ID hash table */
    LratSlot *slots;
    uint32_t cap;   /* Always a power of two */
    uint32_t count; /* Live entries */
    uint32_t used;  /* Live + tombstones */

    /* Level-0 unit clause ID per variable (1-based) */
    uint64_t *unit_id;
    uint32_t unit_cap;

    /* Buffered deletions flushed before next addition */
    uint64_t *del_buf;
    uint32_t del_size;
    uint32_t del_cap;

    /* Reusable BFS/hint scratch buffers */
    uint8_t *in_learnt;
    uint8_t *visited;
    uint8_t *root_seen;
    uint32_t mark_cap;

    Var *bfs_q;
    uint32_t bfs_cap;
    Var *root_vars;
    uint32_t root_cap;
    LratHintPair *pairs;
    uint32_t pairs_cap;
    uint64_t *hints;
    uint32_t hints_cap;

    /* Pending binary-resolution minimization hints for the next lrat_emit_learned call */
    Var *bin_min_vars;
    uint64_t *bin_min_ids;
    uint32_t bin_min_count;
    uint32_t bin_min_cap;
};

static inline uint32_t hash_cref(CRef cr) {
    uint32_t x = cr;
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

static bool map_rehash(LratState *st, uint32_t new_cap) {
    LratSlot *ns = calloc(new_cap, sizeof *ns);
    if (!ns) { st->error = true; return false; }
    uint32_t mask = new_cap - 1u;
    for (uint32_t i = 0; i < st->cap; ++i) {
        CRef k = st->slots[i].key;
        if (k != LRAT_EMPTY_KEY && k != LRAT_TOMBSTONE_KEY) {
            uint32_t pos = hash_cref(k) & mask;
            while (ns[pos].key != LRAT_EMPTY_KEY) pos = (pos + 1u) & mask;
            ns[pos] = st->slots[i];
        }
    }
    free(st->slots);
    st->slots = ns;
    st->cap = new_cap;
    st->used = st->count;
    return true;
}

static bool map_set(LratState *st, CRef cr, uint64_t id) {
    if (cr == INVALID_CLAUSE || cr == LRAT_EMPTY_KEY || cr == LRAT_TOMBSTONE_KEY) return true;
    if ((st->used + 1u) * 10u >= st->cap * 6u) {
        uint32_t ncap = st->cap ? st->cap : 1024u;
        if ((st->count + 1u) * 10u >= st->cap * 5u) ncap *= 2u;
        if (!map_rehash(st, ncap)) return false;
    }
    uint32_t mask = st->cap - 1u;
    uint32_t pos = hash_cref(cr) & mask;
    uint32_t first_tomb = UINT32_MAX;
    for (;;) {
        CRef k = st->slots[pos].key;
        if (k == cr) {
            st->slots[pos].id = id;
            return true;
        }
        if (k == LRAT_EMPTY_KEY) {
            if (first_tomb != UINT32_MAX) {
                pos = first_tomb;
            } else {
                st->used++;
            }
            st->slots[pos].key = cr;
            st->slots[pos].id = id;
            st->count++;
            return true;
        }
        if (k == LRAT_TOMBSTONE_KEY && first_tomb == UINT32_MAX) {
            first_tomb = pos;
        }
        pos = (pos + 1u) & mask;
    }
}

static uint64_t map_get(const LratState *st, CRef cr) {
    if (!st || !st->cap || cr == INVALID_CLAUSE || cr == LRAT_EMPTY_KEY || cr == LRAT_TOMBSTONE_KEY)
        return 0;
    uint32_t mask = st->cap - 1u;
    uint32_t pos = hash_cref(cr) & mask;
    for (;;) {
        CRef k = st->slots[pos].key;
        if (k == cr) return st->slots[pos].id;
        if (k == LRAT_EMPTY_KEY) return 0;
        pos = (pos + 1u) & mask;
    }
}

static uint64_t map_erase(LratState *st, CRef cr) {
    if (!st || !st->cap || cr == INVALID_CLAUSE || cr == LRAT_EMPTY_KEY || cr == LRAT_TOMBSTONE_KEY)
        return 0;
    uint32_t mask = st->cap - 1u;
    uint32_t pos = hash_cref(cr) & mask;
    for (;;) {
        CRef k = st->slots[pos].key;
        if (k == cr) {
            uint64_t id = st->slots[pos].id;
            st->slots[pos].key = LRAT_TOMBSTONE_KEY;
            st->slots[pos].id = 0;
            st->count--;
            return id;
        }
        if (k == LRAT_EMPTY_KEY) return 0;
        pos = (pos + 1u) & mask;
    }
}

static bool ensure_var_arrays(LratState *st, uint32_t num_vars) {
    if (num_vars + 1u <= st->unit_cap && num_vars + 1u <= st->mark_cap) return true;
    uint32_t nc = st->unit_cap ? st->unit_cap * 2u : 256u;
    while (nc <= num_vars) nc *= 2u;

    uint64_t *nu = realloc(st->unit_id, (size_t)nc * sizeof *nu);
    if (!nu) { st->error = true; return false; }
    memset(nu + st->unit_cap, 0, (size_t)(nc - st->unit_cap) * sizeof *nu);
    st->unit_id = nu;
    st->unit_cap = nc;

    uint8_t *il = realloc(st->in_learnt, (size_t)nc);
    uint8_t *vi = realloc(st->visited, (size_t)nc);
    uint8_t *rs = realloc(st->root_seen, (size_t)nc);
    if (!il || !vi || !rs) {
        if (il) st->in_learnt = il;
        if (vi) st->visited = vi;
        if (rs) st->root_seen = rs;
        st->error = true;
        return false;
    }
    memset(il + st->mark_cap, 0, (size_t)(nc - st->mark_cap));
    memset(vi + st->mark_cap, 0, (size_t)(nc - st->mark_cap));
    memset(rs + st->mark_cap, 0, (size_t)(nc - st->mark_cap));
    st->in_learnt = il;
    st->visited = vi;
    st->root_seen = rs;
    st->mark_cap = nc;
    return true;
}

/* =====================================================================
 * Wire-Format Output (Text & Binary LRAT)
 * ===================================================================== */

static bool write_leb128(FILE *fp, uint64_t val) {
    unsigned char buf[12];
    size_t n = 0;
    do {
        unsigned char b = (unsigned char)(val & 0x7Fu);
        val >>= 7;
        if (val) b |= 0x80u;
        buf[n++] = b;
    } while (val);
    return fwrite(buf, 1, n, fp) == n;
}

static inline uint64_t encode_binary_lit(Lit l) {
    /* Standard DRAT/LRAT binary literal encoding: +v -> 2*v, -v -> 2*v + 1 */
    return ((uint64_t)var(l) << 1) | (sign(l) ? 1ULL : 0ULL);
}

static void flush_deletions(LratState *st) {
    if (!st || st->error || st->del_size == 0) return;
    uint64_t anchor = st->last_written_id ? st->last_written_id : st->next_id;
    if (anchor == 0) { st->del_size = 0; return; }
    if (st->binary) {
        if (fputc('d', st->file) == EOF) { st->error = true; return; }
        for (uint32_t i = 0; i < st->del_size; ++i) {
            if (!write_leb128(st->file, st->del_buf[i] << 1)) { st->error = true; return; }
        }
        if (fputc(0, st->file) == EOF) { st->error = true; return; }
    } else {
        if (fprintf(st->file, "%llu d", (unsigned long long)anchor) < 0) { st->error = true; return; }
        for (uint32_t i = 0; i < st->del_size; ++i) {
            if (fprintf(st->file, " %llu", (unsigned long long)st->del_buf[i]) < 0) {
                st->error = true; return;
            }
        }
        if (fputs(" 0\n", st->file) == EOF) { st->error = true; return; }
    }
    st->del_size = 0;
}

static uint64_t write_addition(LratState *st, const Lit *lits, uint32_t n_lits,
                               const uint64_t *hints, uint32_t n_hints) {
    if (!st || st->error) return 0;
    flush_deletions(st);
    if (st->error) return 0;
    uint64_t id = ++st->next_id;
    if (st->binary) {
        if (fputc('a', st->file) == EOF || !write_leb128(st->file, id << 1)) {
            st->error = true; return 0;
        }
        for (uint32_t i = 0; i < n_lits; ++i) {
            if (!write_leb128(st->file, encode_binary_lit(lits[i]))) { st->error = true; return 0; }
        }
        if (fputc(0, st->file) == EOF) { st->error = true; return 0; }
        for (uint32_t i = 0; i < n_hints; ++i) {
            if (!write_leb128(st->file, hints[i] << 1)) { st->error = true; return 0; }
        }
        if (fputc(0, st->file) == EOF) { st->error = true; return 0; }
    } else {
        if (fprintf(st->file, "%llu", (unsigned long long)id) < 0) { st->error = true; return 0; }
        for (uint32_t i = 0; i < n_lits; ++i) {
            if (fprintf(st->file, " %d", toDimacs(lits[i])) < 0) { st->error = true; return 0; }
        }
        if (fputs(" 0", st->file) == EOF) { st->error = true; return 0; }
        for (uint32_t i = 0; i < n_hints; ++i) {
            if (fprintf(st->file, " %llu", (unsigned long long)hints[i]) < 0) {
                st->error = true; return 0;
            }
        }
        if (fputs(" 0\n", st->file) == EOF) { st->error = true; return 0; }
    }
    st->last_written_id = id;
    if (n_lits == 0) {
        st->emitted_empty = true;
        if (fflush(st->file) != 0) st->error = true;
    }
    return id;
}

/* =====================================================================
 * Lifecycle & Registration API
 * ===================================================================== */

LratState *lrat_create(const char *path, bool binary) {
    if (!path) return NULL;
    LratState *st = calloc(1, sizeof *st);
    if (!st) return NULL;
    st->binary = binary;
    st->file = fopen(path, binary ? "wb" : "w");
    if (!st->file) { free(st); return NULL; }
    if (!map_rehash(st, 1024u)) {
        fclose(st->file);
        free(st);
        return NULL;
    }
    return st;
}

void lrat_free(LratState *st) {
    if (!st) return;
    if (st->file) {
        if (!st->error) flush_deletions(st);
        fclose(st->file);
    }
    free(st->slots);
    free(st->unit_id);
    free(st->del_buf);
    free(st->in_learnt);
    free(st->visited);
    free(st->root_seen);
    free(st->bfs_q);
    free(st->root_vars);
    free(st->pairs);
    free(st->hints);
    free(st->bin_min_vars);
    free(st->bin_min_ids);
    free(st);
}

bool lrat_flush(LratState *st) {
    if (!st) return true;
    flush_deletions(st);
    if (st->file && fflush(st->file) != 0) st->error = true;
    return !st->error;
}

bool lrat_has_error(const LratState *st) {
    return st && st->error;
}

bool lrat_emitted_empty(const LratState *st) {
    return st && st->emitted_empty;
}

uint64_t lrat_register_original(Solver *s, CRef cr) {
    if (!s || !s->lrat) return 0;
    uint64_t id = ++s->lrat->next_id;
    if (cr != INVALID_CLAUSE) {
        if (!map_set(s->lrat, cr, id)) { s->error = true; return 0; }
    }
    return id;
}

void lrat_set_cref_id(Solver *s, CRef cr, uint64_t id) {
    if (!s || !s->lrat || cr == INVALID_CLAUSE || id == 0) return;
    if (!map_set(s->lrat, cr, id)) s->error = true;
}

uint64_t lrat_get_cref_id(const Solver *s, CRef cr) {
    if (!s || !s->lrat) return 0;
    return map_get(s->lrat, cr);
}

void lrat_set_unit_id(Solver *s, Var v, uint64_t id) {
    if (!s || !s->lrat || v == 0 || id == 0) return;
    if (!ensure_var_arrays(s->lrat, v)) { s->error = true; return; }
    s->lrat->unit_id[v] = id;
}

uint64_t lrat_get_unit_id(const Solver *s, Var v) {
    if (!s || !s->lrat || v == 0 || v >= s->lrat->unit_cap) return 0;
    return s->lrat->unit_id[v];
}

/* =====================================================================
 * Hint Extraction & Proof Emission
 * ===================================================================== */

static bool ensure_hints_cap(LratState *st, uint32_t need) {
    if (need <= st->hints_cap) return true;
    uint32_t nc = st->hints_cap ? st->hints_cap * 2u : 64u;
    while (nc < need) nc *= 2u;
    uint64_t *p = realloc(st->hints, (size_t)nc * sizeof *p);
    if (!p) { st->error = true; return false; }
    st->hints = p;
    st->hints_cap = nc;
    return true;
}

static bool push_hint_pair(LratState *st, uint32_t *n, uint32_t trail_pos, uint64_t id) {
    if (*n == st->pairs_cap) {
        uint32_t nc = st->pairs_cap ? st->pairs_cap * 2u : 64u;
        LratHintPair *p = realloc(st->pairs, (size_t)nc * sizeof *p);
        if (!p) { st->error = true; return false; }
        st->pairs = p;
        st->pairs_cap = nc;
    }
    st->pairs[(*n)++] = (LratHintPair){trail_pos, id};
    return true;
}

static bool push_bfs(LratState *st, uint32_t *tail, Var v) {
    if (*tail == st->bfs_cap) {
        uint32_t nc = st->bfs_cap ? st->bfs_cap * 2u : 64u;
        Var *p = realloc(st->bfs_q, (size_t)nc * sizeof *p);
        if (!p) { st->error = true; return false; }
        st->bfs_q = p;
        st->bfs_cap = nc;
    }
    st->bfs_q[(*tail)++] = v;
    return true;
}

static bool push_root_var(LratState *st, uint32_t *n, Var v) {
    if (*n == st->root_cap) {
        uint32_t nc = st->root_cap ? st->root_cap * 2u : 64u;
        Var *p = realloc(st->root_vars, (size_t)nc * sizeof *p);
        if (!p) { st->error = true; return false; }
        st->root_vars = p;
        st->root_cap = nc;
    }
    st->root_vars[(*n)++] = v;
    return true;
}

static int cmp_hint_pair(const void *a, const void *b) {
    uint32_t pa = ((const LratHintPair *)a)->trail_pos;
    uint32_t pb = ((const LratHintPair *)b)->trail_pos;
    return (pa > pb) - (pa < pb);
}

void lrat_sync_level0_units(Solver *s, uint32_t from_trail) {
    if (!s || !s->lrat || s->error || s->lrat->error) return;
    LratState *st = s->lrat;
    if (!ensure_var_arrays(st, s->num_vars)) { s->error = true; return; }

    for (uint32_t t = from_trail; t < s->trail_size && !s->error; ++t) {
        Lit lit = s->trail[t].lit;
        Var v = var(lit);
        if (s->vars[v].level != 0) continue;
        if (st->unit_id[v] != 0) continue;

        CRef r = s->vars[v].reason;
        if (r == INVALID_CLAUSE || r == BINARY_CONFLICT) continue;
        uint64_t rid = map_get(st, r);
        if (rid == 0) { st->error = true; s->error = true; return; }

        uint32_t sz = CLAUSE_SIZE(s->arena, r);
        const Lit *lits = CLAUSE_LITS(s->arena, r);
        if (sz == 1) {
            st->unit_id[v] = rid;
            continue;
        }
        if (!ensure_hints_cap(st, sz + 1u)) { s->error = true; return; }
        uint32_t nh = 0;
        for (uint32_t i = 0; i < sz; ++i) {
            Var qv = var(lits[i]);
            if (qv == v) continue;
            uint64_t uid = (qv < st->unit_cap) ? st->unit_id[qv] : 0;
            if (uid == 0) { st->error = true; s->error = true; return; }
            /* Avoid duplicates if clause had any */
            bool dup = false;
            for (uint32_t k = 0; k < nh; ++k) {
                if (st->hints[k] == uid) { dup = true; break; }
            }
            if (!dup) st->hints[nh++] = uid;
        }
        st->hints[nh++] = rid;
        uint64_t new_uid = write_addition(st, &lit, 1, st->hints, nh);
        if (!new_uid) { s->error = true; return; }
        st->unit_id[v] = new_uid;
    }
}

void lrat_clear_bin_hints(Solver *s) {
    if (!s || !s->lrat) return;
    s->lrat->bin_min_count = 0;
}

bool lrat_record_bin_hint(Solver *s, Var removed_var, CRef bin_cr) {
    if (!s || !s->lrat || s->error || s->lrat->error) return false;
    LratState *st = s->lrat;
    uint64_t bid = map_get(st, bin_cr);
    if (bid == 0) return false;
    if (st->bin_min_count == st->bin_min_cap) {
        uint32_t nc = st->bin_min_cap ? st->bin_min_cap * 2u : 32u;
        Var *nv = realloc(st->bin_min_vars, (size_t)nc * sizeof *nv);
        if (!nv) { st->error = true; s->error = true; return false; }
        st->bin_min_vars = nv;
        uint64_t *ni = realloc(st->bin_min_ids, (size_t)nc * sizeof *ni);
        if (!ni) { st->error = true; s->error = true; return false; }
        st->bin_min_ids = ni;
        st->bin_min_cap = nc;
    }
    st->bin_min_vars[st->bin_min_count] = removed_var;
    st->bin_min_ids[st->bin_min_count] = bid;
    st->bin_min_count++;
    return true;
}

uint64_t lrat_emit_learned(Solver *s, CRef conflict, const Lit *learnt, uint32_t n) {
    if (!s || !s->lrat || s->error || s->lrat->error) return 0;
    LratState *st = s->lrat;
    if (!ensure_var_arrays(st, s->num_vars)) {
        st->bin_min_count = 0;
        s->error = true;
        return 0;
    }

    uint64_t cid = map_get(st, conflict);
    if (cid == 0) {
        st->bin_min_count = 0;
        st->error = true;
        s->error = true;
        return 0;
    }

    /* Mark all variables present in the final minimized learned clause AND any
     * variables removed by binary-resolution minimization (propagated first via bin_min_ids). */
    for (uint32_t i = 0; i < n; ++i) st->in_learnt[var(learnt[i])] = 1;
    uint32_t n_bin = st->bin_min_count;
    for (uint32_t i = 0; i < n_bin; ++i) st->in_learnt[st->bin_min_vars[i]] = 1;

    uint32_t bfs_head = 0, bfs_tail = 0;
    uint32_t n_roots = 0;
    uint32_t n_pairs = 0;
    bool ok = true;

    /* Seed BFS from all literals in conflict clause. */
    uint32_t csz = CLAUSE_SIZE(s->arena, conflict);
    const Lit *clits = CLAUSE_LITS(s->arena, conflict);
    for (uint32_t i = 0; i < csz && ok; ++i) {
        Var v = var(clits[i]);
        if (st->in_learnt[v]) continue;
        if (s->vars[v].level == 0) {
            if (!st->root_seen[v]) {
                st->root_seen[v] = 1;
                ok = push_root_var(st, &n_roots, v);
            }
        } else if (!st->visited[v]) {
            st->visited[v] = 1;
            ok = push_bfs(st, &bfs_tail, v);
        }
    }

    while (ok && bfs_head < bfs_tail) {
        Var u = st->bfs_q[bfs_head++];
        CRef r = s->vars[u].reason;
        uint64_t rid = map_get(st, r);
        if (r == INVALID_CLAUSE || r == BINARY_CONFLICT || rid == 0) {
            ok = false; break;
        }
        if (!push_hint_pair(st, &n_pairs, s->vars[u].trail_pos, rid)) { ok = false; break; }

        uint32_t rsz = CLAUSE_SIZE(s->arena, r);
        const Lit *rlits = CLAUSE_LITS(s->arena, r);
        for (uint32_t i = 0; i < rsz && ok; ++i) {
            Var qv = var(rlits[i]);
            if (qv == u || st->in_learnt[qv]) continue;
            if (s->vars[qv].level == 0) {
                if (!st->root_seen[qv]) {
                    st->root_seen[qv] = 1;
                    ok = push_root_var(st, &n_roots, qv);
                }
            } else if (!st->visited[qv]) {
                st->visited[qv] = 1;
                ok = push_bfs(st, &bfs_tail, qv);
            }
        }
    }

    uint64_t new_id = 0;
    if (ok) {
        /* Sort implied reason clauses in ascending trail_pos order (exact forward RUP order!). */
        if (n_pairs > 1) qsort(st->pairs, n_pairs, sizeof *st->pairs, cmp_hint_pair);

        if (ensure_hints_cap(st, n_bin + n_roots + n_pairs + 1u)) {
            uint32_t nh = 0;
            /* Binary-resolution minimization hints fire immediately under ~learnt[0]. */
            for (uint32_t i = 0; i < n_bin; ++i) {
                uint64_t bid = st->bin_min_ids[i];
                if (bid && bid != cid) st->hints[nh++] = bid;
            }
            for (uint32_t i = 0; i < n_roots; ++i) {
                uint64_t uid = st->unit_id[st->root_vars[i]];
                if (uid == 0) { ok = false; break; }
                st->hints[nh++] = uid;
            }
            if (ok) {
                for (uint32_t i = 0; i < n_pairs; ++i) {
                    uint64_t hid = st->pairs[i].id;
                    if (hid != cid) st->hints[nh++] = hid;
                }
                st->hints[nh++] = cid;
                new_id = write_addition(st, learnt, n, st->hints, nh);
            }
        } else {
            ok = false;
        }
    }

    /* Clean all scratch marks. */
    for (uint32_t i = 0; i < n; ++i) st->in_learnt[var(learnt[i])] = 0;
    for (uint32_t i = 0; i < n_bin; ++i) st->in_learnt[st->bin_min_vars[i]] = 0;
    st->bin_min_count = 0;
    for (uint32_t i = 0; i < bfs_tail; ++i) st->visited[st->bfs_q[i]] = 0;
    for (uint32_t i = 0; i < n_roots; ++i) st->root_seen[st->root_vars[i]] = 0;

    if (!ok || !new_id) {
        st->error = true;
        s->error = true;
        return 0;
    }
    return new_id;
}

uint64_t lrat_emit_pair_rup(Solver *s, const Lit *lits, uint32_t n, CRef cr1, CRef cr2) {
    if (!s || !s->lrat || s->error || s->lrat->error) return 0;
    LratState *st = s->lrat;
    if (!ensure_var_arrays(st, s->num_vars)) { s->error = true; return 0; }
    uint64_t id1 = map_get(st, cr1);
    uint64_t id2 = map_get(st, cr2);
    if (!id1 || !id2) { st->error = true; s->error = true; return 0; }

    /* Case 1: If any literal of `lits` is already TRUE at level 0, then ~lits
     * immediately contradicts its level-0 unit clause! */
    for (uint32_t i = 0; i < n; ++i) {
        Lit l = lits[i];
        Var v = var(l);
        if (s->values[v] == (uint8_t)(2u - sign(l))) {
            uint64_t uid = (v < st->unit_cap) ? st->unit_id[v] : 0;
            if (!uid) { st->error = true; s->error = true; return 0; }
            uint64_t new_id = write_addition(st, lits, n, &uid, 1);
            if (!new_id) { s->error = true; return 0; }
            return new_id;
        }
    }

    uint32_t sz1 = CLAUSE_SIZE(s->arena, cr1);
    uint32_t sz2 = CLAUSE_SIZE(s->arena, cr2);
    if (!ensure_hints_cap(st, sz1 + sz2 + 2u)) { s->error = true; return 0; }

    /* Mark exact signed literals of `lits` in st->in_learnt (1+sign(l)). */
    for (uint32_t i = 0; i < n; ++i) st->in_learnt[var(lits[i])] = (uint8_t)(1u + sign(lits[i]));

    /* Check if either single clause alone derives `lits` via level-0 units
     * (happens when the pivot or another literal became assigned mid-BVE). */
    bool c1_suffices = true, c2_suffices = true;
    const Lit *ls1 = CLAUSE_LITS(s->arena, cr1);
    const Lit *ls2 = CLAUSE_LITS(s->arena, cr2);
    for (uint32_t i = 0; i < sz1; ++i) {
        Lit l = ls1[i];
        Var v = var(l);
        if (st->in_learnt[v] == (uint8_t)(1u + sign(l))) continue;
        if (s->values[v] == (uint8_t)(1u + sign(l))) continue;
        c1_suffices = false; break;
    }
    if (!c1_suffices) {
        for (uint32_t i = 0; i < sz2; ++i) {
            Lit l = ls2[i];
            Var v = var(l);
            if (st->in_learnt[v] == (uint8_t)(1u + sign(l))) continue;
            if (s->values[v] == (uint8_t)(1u + sign(l))) continue;
            c2_suffices = false; break;
        }
    } else {
        c2_suffices = false;
    }

    uint32_t nh = 0;
    bool ok = true;
    int first_side = c2_suffices ? 1 : 0;
    int last_side = (c1_suffices || c2_suffices) ? first_side : 1;

    for (int side = first_side; side <= last_side && ok; ++side) {
        const Lit *cls = side ? ls2 : ls1;
        uint32_t sz = side ? sz2 : sz1;
        for (uint32_t i = 0; i < sz; ++i) {
            Lit l = cls[i];
            Var v = var(l);
            if (st->in_learnt[v] == (uint8_t)(1u + sign(l))) continue;
            if (s->values[v] == (uint8_t)(1u + sign(l))) {
                uint64_t uid = (v < st->unit_cap) ? st->unit_id[v] : 0;
                if (!uid) { ok = false; break; }
                bool dup = false;
                for (uint32_t k = 0; k < nh; ++k) {
                    if (st->hints[k] == uid) { dup = true; break; }
                }
                if (!dup && uid != id1 && uid != id2) st->hints[nh++] = uid;
            }
        }
    }
    for (uint32_t i = 0; i < n; ++i) st->in_learnt[var(lits[i])] = 0;

    if (!ok) { st->error = true; s->error = true; return 0; }
    if (c1_suffices) {
        st->hints[nh++] = id1;
    } else if (c2_suffices) {
        st->hints[nh++] = id2;
    } else {
        st->hints[nh++] = id1;
        if (id2 != id1) st->hints[nh++] = id2;
    }
    uint64_t new_id = write_addition(st, lits, n, st->hints, nh);
    if (!new_id) { s->error = true; return 0; }
    return new_id;
}

uint64_t lrat_emit_root_strengthen(Solver *s, CRef old_cr, const Lit *new_lits, uint32_t new_n) {
    if (!s || !s->lrat || s->error || s->lrat->error) return 0;
    LratState *st = s->lrat;
    if (!ensure_var_arrays(st, s->num_vars)) { s->error = true; return 0; }
    uint64_t oid = map_get(st, old_cr);
    if (!oid) { st->error = true; s->error = true; return 0; }

    uint32_t sz = CLAUSE_SIZE(s->arena, old_cr);
    const Lit *olits = CLAUSE_LITS(s->arena, old_cr);
    if (!ensure_hints_cap(st, sz + 1u)) { s->error = true; return 0; }

    uint32_t nh = 0;
    for (uint32_t i = 0; i < sz; ++i) {
        Lit l = olits[i];
        if (s->values[var(l)] == (uint8_t)(1u + sign(l))) {
            Var v = var(l);
            uint64_t uid = (v < st->unit_cap) ? st->unit_id[v] : 0;
            if (!uid) { st->error = true; s->error = true; return 0; }
            bool dup = false;
            for (uint32_t k = 0; k < nh; ++k) {
                if (st->hints[k] == uid) { dup = true; break; }
            }
            if (!dup && uid != oid) st->hints[nh++] = uid;
        }
    }
    st->hints[nh++] = oid;
    uint64_t new_id = write_addition(st, new_lits, new_n, st->hints, nh);
    if (!new_id) { s->error = true; return 0; }
    return new_id;
}

void lrat_emit_empty_from_conflict(Solver *s, CRef conflict) {
    if (!s || !s->lrat || s->error || s->lrat->error || s->lrat->emitted_empty) return;
    LratState *st = s->lrat;
    if (!ensure_var_arrays(st, s->num_vars)) { s->error = true; return; }

    uint64_t cid = map_get(st, conflict);
    if (cid == 0) { st->error = true; s->error = true; return; }

    uint32_t sz = CLAUSE_SIZE(s->arena, conflict);
    const Lit *lits = CLAUSE_LITS(s->arena, conflict);
    if (!ensure_hints_cap(st, sz + 1u)) { s->error = true; return; }

    uint32_t nh = 0;
    for (uint32_t i = 0; i < sz; ++i) {
        Var v = var(lits[i]);
        uint64_t uid = (v < st->unit_cap) ? st->unit_id[v] : 0;
        if (uid == 0) { st->error = true; s->error = true; return; }
        bool dup = false;
        for (uint32_t k = 0; k < nh; ++k) {
            if (st->hints[k] == uid) { dup = true; break; }
        }
        if (!dup && uid != cid) st->hints[nh++] = uid;
    }
    st->hints[nh++] = cid;
    if (!write_addition(st, NULL, 0, st->hints, nh)) { s->error = true; return; }
    lrat_flush(st);
}

void lrat_emit_empty_from_pair(Solver *s, uint64_t id_a, uint64_t id_b) {
    if (!s || !s->lrat || s->error || s->lrat->error || s->lrat->emitted_empty) return;
    uint64_t h[2];
    uint32_t nh = 0;
    if (id_a) h[nh++] = id_a;
    if (id_b && id_b != id_a) h[nh++] = id_b;
    if (!write_addition(s->lrat, NULL, 0, h, nh)) { s->error = true; return; }
    lrat_flush(s->lrat);
}

void lrat_buffer_delete_id(Solver *s, uint64_t id) {
    if (!s || !s->lrat || s->lrat->error || id == 0) return;
    LratState *st = s->lrat;
    if (st->del_size == st->del_cap) {
        uint32_t nc = st->del_cap ? st->del_cap * 2u : 128u;
        uint64_t *p = realloc(st->del_buf, (size_t)nc * sizeof *p);
        if (!p) { st->error = true; s->error = true; return; }
        st->del_buf = p;
        st->del_cap = nc;
    }
    st->del_buf[st->del_size++] = id;
}

void lrat_buffer_delete(Solver *s, CRef cr) {
    if (!s || !s->lrat || s->lrat->error || cr == INVALID_CLAUSE) return;
    uint64_t id = map_erase(s->lrat, cr);
    if (id == 0) return;
    lrat_buffer_delete_id(s, id);
}

void lrat_relocate_clauses(Solver *s, const Arena *old_arena) {
    if (!s || !s->lrat || !old_arena || s->lrat->error) return;
    LratState *st = s->lrat;
    uint32_t old_cap = st->cap;
    LratSlot *old_slots = st->slots;

    uint32_t new_cap = 1024u;
    while (new_cap * 6u <= (st->count + 1u) * 10u) new_cap *= 2u;
    st->slots = calloc(new_cap, sizeof *st->slots);
    if (!st->slots) {
        st->slots = old_slots;
        st->error = true;
        s->error = true;
        return;
    }
    st->cap = new_cap;
    st->count = 0;
    st->used = 0;

    for (uint32_t i = 0; i < old_cap; ++i) {
        CRef ocr = old_slots[i].key;
        if (ocr == LRAT_EMPTY_KEY || ocr == LRAT_TOMBSTONE_KEY) continue;
        CRef ncr = CLAUSE_HEADER(old_arena, ocr)->search;
        if (ncr != INVALID_CLAUSE) {
            map_set(st, ncr, old_slots[i].id);
        }
    }
    free(old_slots);
}

/* =====================================================================
 * Independent Strict Linear-Time LRAT Verifier (Text & Binary)
 * ===================================================================== */

typedef struct {
    int32_t *lits;
    uint32_t size;
    bool alive;
} VerClause;

typedef struct {
    VerClause *cls; /* Indexed directly by 1-based clause ID */
    uint64_t cap;
    uint64_t max_id;
    uint32_t max_var;
    int8_t *val;    /* 0=UNDEF, 1=TRUE, -1=FALSE, indexed by var */
    uint32_t val_cap;
    uint32_t *trail;
    uint32_t trail_sz;
    uint32_t trail_cap;
} VerDb;

static bool ver_ensure_id(VerDb *db, uint64_t id) {
    if (id < db->cap) return true;
    uint64_t nc = db->cap ? db->cap * 2u : 1024u;
    while (nc <= id) nc *= 2u;
    VerClause *p = realloc(db->cls, (size_t)nc * sizeof *p);
    if (!p) return false;
    memset(p + db->cap, 0, (size_t)(nc - db->cap) * sizeof *p);
    db->cls = p;
    db->cap = nc;
    return true;
}

static bool ver_ensure_var(VerDb *db, uint32_t v) {
    if (v > db->max_var) db->max_var = v;
    if (v + 1u <= db->val_cap) return true;
    uint32_t nc = db->val_cap ? db->val_cap * 2u : 256u;
    while (nc <= v) nc *= 2u;
    int8_t *nv = realloc(db->val, (size_t)nc);
    uint32_t *nt = realloc(db->trail, (size_t)nc * sizeof *nt);
    if (!nv || !nt) { if (nv) db->val = nv; if (nt) db->trail = nt; return false; }
    memset(nv + db->val_cap, 0, (size_t)(nc - db->val_cap));
    db->val = nv;
    db->trail = nt;
    db->val_cap = nc;
    db->trail_cap = nc;
    return true;
}

static bool ver_add_clause(VerDb *db, uint64_t id, const int32_t *lits, uint32_t sz) {
    if (id == 0 || !ver_ensure_id(db, id)) return false;
    if (db->cls[id].alive) return false;
    int32_t *copy = NULL;
    if (sz > 0) {
        copy = malloc((size_t)sz * sizeof *copy);
        if (!copy) return false;
        memcpy(copy, lits, (size_t)sz * sizeof *copy);
        for (uint32_t i = 0; i < sz; ++i) {
            uint32_t v = (uint32_t)(lits[i] > 0 ? lits[i] : -lits[i]);
            if (v == 0 || !ver_ensure_var(db, v)) { free(copy); return false; }
        }
    }
    db->cls[id] = (VerClause){copy, sz, true};
    if (id > db->max_id) db->max_id = id;
    return true;
}

static void ver_del_clause(VerDb *db, uint64_t id) {
    if (id < db->cap && db->cls[id].alive) {
        free(db->cls[id].lits);
        db->cls[id].lits = NULL;
        db->cls[id].size = 0;
        db->cls[id].alive = false;
    }
}

static inline int8_t ver_eval(const VerDb *db, int32_t lit) {
    uint32_t v = (uint32_t)(lit > 0 ? lit : -lit);
    int8_t s = db->val[v];
    return (lit > 0) ? s : (int8_t)(-s);
}

static inline void ver_assign(VerDb *db, int32_t lit) {
    uint32_t v = (uint32_t)(lit > 0 ? lit : -lit);
    db->val[v] = (lit > 0) ? (int8_t)1 : (int8_t)(-1);
    db->trail[db->trail_sz++] = v;
}

static void ver_clear_trail(VerDb *db) {
    for (uint32_t i = 0; i < db->trail_sz; ++i) db->val[db->trail[i]] = 0;
    db->trail_sz = 0;
}

static bool ver_check_rup(VerDb *db, const int32_t *lits, uint32_t n_lits,
                          const uint64_t *hints, uint32_t n_hints) {
    /* Ensure all clause vars fit in db->val before checking */
    for (uint32_t i = 0; i < n_lits; ++i) {
        uint32_t v = (uint32_t)(lits[i] > 0 ? lits[i] : -lits[i]);
        if (v == 0 || !ver_ensure_var(db, v)) return false;
    }
    bool tautology = false;
    for (uint32_t i = 0; i < n_lits; ++i) {
        int8_t ev = ver_eval(db, lits[i]);
        if (ev > 0) { tautology = true; break; } /* ~lits[i] already set FALSE => tautology */
        if (ev == 0) ver_assign(db, -lits[i]);
    }
    if (tautology) { ver_clear_trail(db); return true; }

    bool conflict = false;
    for (uint32_t h = 0; h < n_hints; ++h) {
        uint64_t hid = hints[h];
        if (hid == 0 || hid >= db->cap || !db->cls[hid].alive) {
            ver_clear_trail(db);
            return false;
        }
        const VerClause *c = &db->cls[hid];
        uint32_t undef_cnt = 0;
        int32_t unit_lit = 0;
        bool sat = false;
        for (uint32_t i = 0; i < c->size; ++i) {
            int8_t ev = ver_eval(db, c->lits[i]);
            if (ev > 0) { sat = true; break; }
            if (ev == 0) {
                if (undef_cnt == 0) { unit_lit = c->lits[i]; undef_cnt = 1; }
                else if (c->lits[i] != unit_lit) { undef_cnt = 2; break; }
            }
        }
        if (sat || undef_cnt >= 2) {
            ver_clear_trail(db);
            return false;
        }
        if (undef_cnt == 1) {
            ver_assign(db, unit_lit);
        } else {
            conflict = true;
            break;
        }
    }
    ver_clear_trail(db);
    return conflict;
}

static bool read_leb128(FILE *fp, uint64_t *out) {
    uint64_t res = 0;
    unsigned shift = 0;
    for (;;) {
        int c = fgetc(fp);
        if (c == EOF) return false;
        res |= (uint64_t)(c & 0x7Fu) << shift;
        if ((c & 0x80) == 0) break;
        shift += 7;
        if (shift >= 64) return false;
    }
    *out = res;
    return true;
}

static void ver_db_free(VerDb *db) {
    for (uint64_t i = 0; i < db->cap; ++i) free(db->cls[i].lits);
    free(db->cls);
    free(db->val);
    free(db->trail);
}

bool lrat_verify_file(const char *cnf_path, const char *lrat_path, bool binary) {
    if (!cnf_path || !lrat_path) return false;
    FILE *cf = fopen(cnf_path, "r");
    if (!cf) return false;

    VerDb db;
    memset(&db, 0, sizeof db);

    /* 1. Load original CNF clauses strictly numbered 1..M */
    int32_t *buf = NULL;
    uint32_t bsz = 0, bcap = 0;
    bool ok = true;
    uint64_t orig_id = 0;
    int ch;
    while (ok && (ch = fgetc(cf)) != EOF) {
        if (isspace((unsigned char)ch)) continue;
        if (ch == 'c' || ch == 'p') {
            while (ch != '\n' && ch != EOF) ch = fgetc(cf);
            continue;
        }
        ungetc(ch, cf);
        long long lit = 0;
        if (fscanf(cf, "%lld", &lit) != 1) { ok = false; break; }
        if (lit == 0) {
            orig_id++;
            if (!ver_add_clause(&db, orig_id, buf, bsz)) { ok = false; break; }
            bsz = 0;
        } else {
            if (bsz == bcap) {
                uint32_t nc = bcap ? bcap * 2u : 32u;
                int32_t *p = realloc(buf, (size_t)nc * sizeof *p);
                if (!p) { ok = false; break; }
                buf = p; bcap = nc;
            }
            buf[bsz++] = (int32_t)lit;
        }
    }
    fclose(cf);
    if (!ok || bsz != 0) { free(buf); ver_db_free(&db); return false; }

    /* 2. Stream & verify LRAT proof */
    FILE *pf = fopen(lrat_path, binary ? "rb" : "r");
    if (!pf) { free(buf); ver_db_free(&db); return false; }

    uint64_t *hbuf = NULL;
    uint32_t hcap = 0;
    bool derived_empty = false;

    if (binary) {
        while (ok) {
            int tag = fgetc(pf);
            if (tag == EOF) break;
            if (tag == 'a') {
                uint64_t id2 = 0;
                if (!read_leb128(pf, &id2) || (id2 & 1ULL) || id2 == 0) { ok = false; break; }
                uint64_t id = id2 >> 1;
                if (id <= db.max_id) { ok = false; break; }
                bsz = 0;
                for (;;) {
                    uint64_t enc = 0;
                    if (!read_leb128(pf, &enc)) { ok = false; break; }
                    if (enc == 0) break;
                    int32_t v = (int32_t)(enc >> 1);
                    int32_t lit = (enc & 1ULL) ? -v : v;
                    if (bsz == bcap) {
                        uint32_t nc = bcap ? bcap * 2u : 32u;
                        int32_t *p = realloc(buf, (size_t)nc * sizeof *p);
                        if (!p) { ok = false; break; }
                        buf = p; bcap = nc;
                    }
                    buf[bsz++] = lit;
                }
                if (!ok) break;
                uint32_t hsz = 0;
                for (;;) {
                    uint64_t h2 = 0;
                    if (!read_leb128(pf, &h2)) { ok = false; break; }
                    if (h2 == 0) break;
                    if (h2 & 1ULL) { ok = false; break; } /* RAT negative hint not expected */
                    if (hsz == hcap) {
                        uint32_t nc = hcap ? hcap * 2u : 32u;
                        uint64_t *p = realloc(hbuf, (size_t)nc * sizeof *p);
                        if (!p) { ok = false; break; }
                        hbuf = p; hcap = nc;
                    }
                    hbuf[hsz++] = h2 >> 1;
                }
                if (!ok) break;
                if (!ver_check_rup(&db, buf, bsz, hbuf, hsz)) { ok = false; break; }
                if (!ver_add_clause(&db, id, buf, bsz)) { ok = false; break; }
                if (bsz == 0) { derived_empty = true; break; }
            } else if (tag == 'd') {
                for (;;) {
                    uint64_t d2 = 0;
                    if (!read_leb128(pf, &d2)) { ok = false; break; }
                    if (d2 == 0) break;
                    if (d2 & 1ULL) { ok = false; break; }
                    ver_del_clause(&db, d2 >> 1);
                }
            } else {
                ok = false;
                break;
            }
        }
    } else {
        char tok[64];
        while (ok) {
            if (fscanf(pf, "%63s", tok) != 1) break;
            if (tok[0] == 'c') {
                int c;
                while ((c = fgetc(pf)) != '\n' && c != EOF) {}
                continue;
            }
            char *end = NULL; errno = 0;
            unsigned long long id = strtoull(tok, &end, 10);
            if (errno || end == tok || *end != '\0' || id == 0) { ok = false; break; }

            if (fscanf(pf, "%63s", tok) != 1) { ok = false; break; }
            if (strcmp(tok, "d") == 0) {
                for (;;) {
                    unsigned long long did = 0;
                    if (fscanf(pf, "%llu", &did) != 1) { ok = false; break; }
                    if (did == 0) break;
                    ver_del_clause(&db, (uint64_t)did);
                }
                continue;
            }

            if (id <= db.max_id) { ok = false; break; }
            bsz = 0;
            for (;;) {
                errno = 0;
                long long lit = strtoll(tok, &end, 10);
                if (errno || end == tok || *end != '\0') { ok = false; break; }
                if (lit == 0) break;
                if (bsz == bcap) {
                    uint32_t nc = bcap ? bcap * 2u : 32u;
                    int32_t *p = realloc(buf, (size_t)nc * sizeof *p);
                    if (!p) { ok = false; break; }
                    buf = p; bcap = nc;
                }
                buf[bsz++] = (int32_t)lit;
                if (fscanf(pf, "%63s", tok) != 1) { ok = false; break; }
            }
            if (!ok) break;
            uint32_t hsz = 0;
            for (;;) {
                long long h = 0;
                if (fscanf(pf, "%lld", &h) != 1) { ok = false; break; }
                if (h == 0) break;
                if (h < 0) { ok = false; break; }
                if (hsz == hcap) {
                    uint32_t nc = hcap ? hcap * 2u : 32u;
                    uint64_t *p = realloc(hbuf, (size_t)nc * sizeof *p);
                    if (!p) { ok = false; break; }
                    hbuf = p; hcap = nc;
                }
                hbuf[hsz++] = (uint64_t)h;
            }
            if (!ok) break;
            if (!ver_check_rup(&db, buf, bsz, hbuf, hsz)) { ok = false; break; }
            if (!ver_add_clause(&db, (uint64_t)id, buf, bsz)) { ok = false; break; }
            if (bsz == 0) { derived_empty = true; break; }
        }
    }

    fclose(pf);
    free(buf);
    free(hbuf);
    ver_db_free(&db);
    return ok && derived_empty;
}
