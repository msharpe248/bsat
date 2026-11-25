/*********************************************************************
 * BSAT Competition Solver - XOR, Cardinality, and Pseudo-Boolean Engine
 *********************************************************************/

#include "../include/pb_enc.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* =====================================================================
 * Internal Helpers
 * ===================================================================== */

static bool add_cls(Solver *s, const Lit *lits, uint32_t n) {
    if (s->error) return false;
    solver_add_clause(s, lits, n);
    return !s->error;
}

static bool add_unit(Solver *s, Lit a) {
    return add_cls(s, &a, 1);
}

static bool add_bin(Solver *s, Lit a, Lit b) {
    Lit c[2] = {a, b};
    return add_cls(s, c, 2);
}

static bool add_tern(Solver *s, Lit a, Lit b, Lit c) {
    Lit cl[3] = {a, b, c};
    return add_cls(s, cl, 3);
}

static int cmp_var_u32(const void *a, const void *b) {
    uint32_t va = *(const uint32_t *)a;
    uint32_t vb = *(const uint32_t *)b;
    return (va > vb) - (va < vb);
}

/* =====================================================================
 * XOR Clause Encoder
 * ===================================================================== */

static bool emit_small_xor(Solver *s, const Var *vars, uint32_t k, bool rhs) {
    if (k == 0) {
        if (rhs) return add_cls(s, NULL, 0);
        return true;
    }
    if (k == 1) {
        return add_unit(s, mkLit(vars[0], !rhs));
    }
    uint32_t total = 1u << k;
    Lit cl[4];
    for (uint32_t mask = 0; mask < total; ++mask) {
        uint32_t pc = 0;
        for (uint32_t i = 0; i < k; ++i) {
            if ((mask >> i) & 1u) pc++;
        }
        if (((pc & 1u) != 0) == rhs) continue;
        /* Block this forbidden assignment. */
        for (uint32_t i = 0; i < k; ++i) {
            bool is_true = ((mask >> i) & 1u) != 0;
            cl[i] = mkLit(vars[i], is_true);
        }
        if (!add_cls(s, cl, k)) return false;
    }
    return true;
}

bool pb_add_xor_clause(Solver *s, const Lit *lits, uint32_t n, bool rhs) {
    if (!s || s->error) return false;
    if (n == 0) return emit_small_xor(s, NULL, 0, rhs);

    Var *vars = malloc((size_t)n * sizeof *vars);
    if (!vars) { s->error = true; return false; }

    for (uint32_t i = 0; i < n; ++i) {
        vars[i] = var(lits[i]);
        if (sign(lits[i])) rhs = !rhs;
    }
    qsort(vars, n, sizeof *vars, cmp_var_u32);

    /* Cancel duplicate pairs modulo 2: x XOR x = 0. */
    uint32_t k = 0;
    for (uint32_t i = 0; i < n; ) {
        if (i + 1 < n && vars[i] == vars[i + 1]) {
            i += 2;
        } else {
            vars[k++] = vars[i++];
        }
    }

    bool ok = true;
    uint32_t pos = 0;
    while (ok && k - pos > 4) {
        Var aux = solver_new_var(s);
        if (aux == INVALID_VAR) { ok = false; break; }
        /* aux <-> vars[pos] XOR vars[pos+1] XOR vars[pos+2]  (i.e. XOR of 4 == 0) */
        Var chunk[4] = {vars[pos], vars[pos + 1], vars[pos + 2], aux};
        ok = emit_small_xor(s, chunk, 4, false);
        /* Replace consumed 3 vars with aux at vars[pos + 2]. */
        vars[pos + 2] = aux;
        pos += 2;
    }
    if (ok) {
        ok = emit_small_xor(s, vars + pos, k - pos, rhs);
    }

    free(vars);
    return ok;
}

/* =====================================================================
 * Sinz Sequential Counter (AtMost-k)
 * ===================================================================== */

bool pb_add_atmost_sinz(Solver *s, const Lit *lits, uint32_t n, uint32_t k) {
    if (!s || s->error) return false;
    if (k >= n) return true;
    if (k == 0) {
        for (uint32_t i = 0; i < n; ++i) {
            if (!add_unit(s, neg(lits[i]))) return false;
        }
        return true;
    }
    if (k == 1 && n <= 4) {
        for (uint32_t i = 0; i < n; ++i) {
            for (uint32_t j = i + 1; j < n; ++j) {
                if (!add_bin(s, neg(lits[i]), neg(lits[j]))) return false;
            }
        }
        return true;
    }

    /* Row i (0 <= i < n-1) stores w_i = min(i+1, k) register variables. */
    Var *prev = malloc((size_t)k * sizeof *prev);
    Var *curr = malloc((size_t)k * sizeof *curr);
    if (!prev || !curr) {
        free(prev); free(curr);
        s->error = true;
        return false;
    }

    bool ok = true;
    /* Row 0: w_0 = 1 */
    prev[0] = solver_new_var(s);
    if (prev[0] == INVALID_VAR) ok = false;
    if (ok) ok = add_bin(s, neg(lits[0]), mkLit(prev[0], false));
    uint32_t prev_w = 1;

    for (uint32_t i = 1; i < n - 1 && ok; ++i) {
        uint32_t w = (i + 1 < k) ? (i + 1) : k;
        for (uint32_t j = 0; j < w; ++j) {
            curr[j] = solver_new_var(s);
            if (curr[j] == INVALID_VAR) { ok = false; break; }
        }
        if (!ok) break;
        ok = add_bin(s, neg(lits[i]), mkLit(curr[0], false));
        for (uint32_t j = 0; j < prev_w && ok; ++j) {
            ok = add_bin(s, mkLit(prev[j], true), mkLit(curr[j], false));
        }
        for (uint32_t j = 1; j < w && ok; ++j) {
            ok = add_tern(s, neg(lits[i]), mkLit(prev[j - 1], true), mkLit(curr[j], false));
        }
        if (ok && prev_w == k) {
            ok = add_bin(s, neg(lits[i]), mkLit(prev[k - 1], true));
        }
        Var *tmp = prev; prev = curr; curr = tmp;
        prev_w = w;
    }

    if (ok) {
        ok = add_bin(s, neg(lits[n - 1]), mkLit(prev[k - 1], true));
    }

    free(prev);
    free(curr);
    return ok;
}

/* =====================================================================
 * Incremental Totalizer (Martins et al. 2014)
 * ===================================================================== */

struct PbTotNode {
    PbTotNode *left;
    PbTotNode *right;
    Lit *out;
    uint32_t size;       /* Number of input leaves in subtree */
    uint32_t encoded_ub; /* Number of output literals allocated & encoded */
    uint32_t cap;
};

static PbTotNode *tot_build_tree(const Lit *lits, uint32_t n) {
    PbTotNode *node = calloc(1, sizeof *node);
    if (!node) return NULL;
    node->size = n;
    if (n == 1) {
        node->out = malloc(sizeof(Lit));
        if (!node->out) { free(node); return NULL; }
        node->out[0] = lits[0];
        node->encoded_ub = 1;
        node->cap = 1;
        return node;
    }
    uint32_t mid = n / 2;
    node->left = tot_build_tree(lits, mid);
    node->right = tot_build_tree(lits + mid, n - mid);
    if (!node->left || !node->right) {
        /* Caller frees via tot_free_node */
        return node;
    }
    return node;
}

static void tot_free_node(PbTotNode *node) {
    if (!node) return;
    tot_free_node(node->left);
    tot_free_node(node->right);
    free(node->out);
    free(node);
}

static bool tot_encode_node(PbTotNode *node, Solver *s, uint32_t target_ub) {
    if (!node || !node->left || !node->right) return true; /* Leaf */
    uint32_t want = (target_ub < node->size) ? target_ub : node->size;
    if (want <= node->encoded_ub) return true;

    if (!tot_encode_node(node->left, s, want)) return false;
    if (!tot_encode_node(node->right, s, want)) return false;

    if (want > node->cap) {
        Lit *p = realloc(node->out, (size_t)want * sizeof *p);
        if (!p) { s->error = true; return false; }
        node->out = p;
        node->cap = want;
    }

    uint32_t old_ub = node->encoded_ub;
    for (uint32_t k = old_ub; k < want; ++k) {
        Var v = solver_new_var(s);
        if (v == INVALID_VAR) return false;
        node->out[k] = mkLit(v, false);
    }

    uint32_t max_a = (node->left->encoded_ub < want) ? node->left->encoded_ub : want;
    uint32_t max_b = (node->right->encoded_ub < want) ? node->right->encoded_ub : want;

    for (uint32_t a = 0; a <= max_a; ++a) {
        uint32_t b_lo = (a <= old_ub) ? (old_ub + 1 - a) : 0;
        if (b_lo > max_b) continue;
        uint32_t b_hi = (want >= a) ? (want - a) : 0;
        if (b_hi > max_b) b_hi = max_b;
        for (uint32_t b = b_lo; b <= b_hi; ++b) {
            uint32_t sum = a + b; /* old_ub < sum <= want */
            if (a == 0) {
                if (!add_bin(s, neg(node->right->out[b - 1]), node->out[sum - 1])) return false;
            } else if (b == 0) {
                if (!add_bin(s, neg(node->left->out[a - 1]), node->out[sum - 1])) return false;
            } else {
                if (!add_tern(s, neg(node->left->out[a - 1]),
                              neg(node->right->out[b - 1]), node->out[sum - 1])) return false;
            }
        }
    }

    node->encoded_ub = want;
    return true;
}

PbTotalizer *pb_totalizer_create(Solver *s, const Lit *lits, uint32_t n, uint32_t init_ub) {
    if (!s || n == 0) return NULL;
    PbTotalizer *t = calloc(1, sizeof *t);
    if (!t) { s->error = true; return NULL; }
    t->num_inputs = n;
    t->root = tot_build_tree(lits, n);
    if (!t->root || (n > 1 && (!t->root->left || !t->root->right))) {
        tot_free_node(t->root);
        free(t);
        s->error = true;
        return NULL;
    }
    uint32_t target = init_ub + 1;
    if (target > n) target = n;
    if (!tot_encode_node(t->root, s, target)) {
        tot_free_node(t->root);
        free(t);
        return NULL;
    }
    t->encoded_ub = t->root->encoded_ub;
    return t;
}

bool pb_totalizer_increase_bound(PbTotalizer *t, Solver *s, uint32_t new_ub) {
    if (!t || !t->root || !s) return false;
    uint32_t target = new_ub + 1;
    if (target > t->num_inputs) target = t->num_inputs;
    if (!tot_encode_node(t->root, s, target)) return false;
    t->encoded_ub = t->root->encoded_ub;
    return true;
}

Lit pb_totalizer_out(const PbTotalizer *t, uint32_t idx) {
    if (!t || !t->root || idx >= t->root->encoded_ub) return LIT_UNDEF;
    return t->root->out[idx];
}

void pb_totalizer_free(PbTotalizer *t) {
    if (!t) return;
    tot_free_node(t->root);
    free(t);
}

/* =====================================================================
 * Weighted Pseudo-Boolean Encoder:
 *   1. Generalized Totalizer (GTE) for bounded distinct partial sums
 *   2. Exact Binary Adder Network + Lexicographic Comparator fallback
 * ===================================================================== */

typedef struct {
    Lit lit;
    uint64_t weight;
} PbTerm;

static int cmp_pb_term_var(const void *a, const void *b) {
    Var va = var(((const PbTerm *)a)->lit);
    Var vb = var(((const PbTerm *)b)->lit);
    return (va > vb) - (va < vb);
}

static int cmp_pb_term_weight(const void *a, const void *b) {
    uint64_t wa = ((const PbTerm *)a)->weight;
    uint64_t wb = ((const PbTerm *)b)->weight;
    return (wa > wb) - (wa < wb);
}

/* --- Generalized Totalizer (GTE) --- */

#define GTE_MAX_SUMS 2048u

typedef struct {
    uint64_t val;
    Lit lit;
} GtePair;

typedef struct {
    GtePair *pairs;
    uint32_t size;
} GteNode;

static int cmp_u64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a;
    uint64_t y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

static uint32_t gte_find_val(const GtePair *pairs, uint32_t n, uint64_t v) {
    uint32_t lo = 0, hi = n;
    while (lo < hi) {
        uint32_t mid = lo + ((hi - lo) >> 1);
        if (pairs[mid].val < v) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

static uint64_t sat_add_cap(uint64_t a, uint64_t b, uint64_t cap) {
    if (a >= cap || b >= cap) return cap;
    if (a > cap - b) return cap;
    return a + b;
}

/* Dry-run check whether GTE stays within GTE_MAX_SUMS per node before allocating
 * any solver variables, so fallback to binary adder leaves zero dead vars. */
static bool gte_check_size(const PbTerm *terms, uint32_t n, uint64_t cap,
                           uint64_t **out_vals, uint32_t *out_n) {
    if (n == 1) {
        uint64_t *v = malloc(sizeof(uint64_t));
        if (!v) return false;
        v[0] = (terms[0].weight < cap) ? terms[0].weight : cap;
        *out_vals = v;
        *out_n = 1;
        return true;
    }
    uint32_t mid = n / 2;
    uint64_t *lv = NULL, *rv = NULL;
    uint32_t ln = 0, rn = 0;
    if (!gte_check_size(terms, mid, cap, &lv, &ln) ||
        !gte_check_size(terms + mid, n - mid, cap, &rv, &rn)) {
        free(lv); free(rv);
        return false;
    }
    size_t max_raw = (size_t)ln + rn + (size_t)ln * rn;
    if (max_raw > 65536u) {
        /* Even before dedup, verify compactly using a small sorted buffer */
    }
    uint64_t *buf = malloc((max_raw ? max_raw : 1u) * sizeof *buf);
    if (!buf) { free(lv); free(rv); return false; }
    size_t cnt = 0;
    for (uint32_t i = 0; i < ln; ++i) buf[cnt++] = lv[i];
    for (uint32_t j = 0; j < rn; ++j) buf[cnt++] = rv[j];
    for (uint32_t i = 0; i < ln; ++i) {
        for (uint32_t j = 0; j < rn; ++j) {
            buf[cnt++] = sat_add_cap(lv[i], rv[j], cap);
        }
    }
    free(lv); free(rv);
    qsort(buf, cnt, sizeof *buf, cmp_u64);
    uint32_t uniq = 0;
    for (size_t i = 0; i < cnt; ++i) {
        if (uniq == 0 || buf[i] != buf[uniq - 1]) buf[uniq++] = buf[i];
    }
    if (uniq > GTE_MAX_SUMS) {
        free(buf);
        return false;
    }
    *out_vals = buf;
    *out_n = uniq;
    return true;
}

static bool gte_encode_rec(Solver *s, const PbTerm *terms, uint32_t n,
                           uint64_t cap, bool is_root, GteNode *out) {
    out->pairs = NULL;
    out->size = 0;
    if (n == 1) {
        out->pairs = malloc(sizeof(GtePair));
        if (!out->pairs) { s->error = true; return false; }
        out->pairs[0].val = (terms[0].weight < cap) ? terms[0].weight : cap;
        out->pairs[0].lit = terms[0].lit;
        out->size = 1;
        return true;
    }
    uint32_t mid = n / 2;
    GteNode L = {NULL, 0}, R = {NULL, 0};
    if (!gte_encode_rec(s, terms, mid, cap, false, &L) ||
        !gte_encode_rec(s, terms + mid, n - mid, cap, false, &R)) {
        free(L.pairs); free(R.pairs);
        return false;
    }

    if (is_root) {
        /* Root only needs to block pairs whose sum >= cap (i.e. > rhs). */
        bool ok = true;
        for (uint32_t i = 0; i < L.size && ok; ++i) {
            if (L.pairs[i].val >= cap) ok = add_unit(s, neg(L.pairs[i].lit));
        }
        for (uint32_t j = 0; j < R.size && ok; ++j) {
            if (R.pairs[j].val >= cap) ok = add_unit(s, neg(R.pairs[j].lit));
        }
        for (uint32_t i = 0; i < L.size && ok; ++i) {
            for (uint32_t j = 0; j < R.size && ok; ++j) {
                if (sat_add_cap(L.pairs[i].val, R.pairs[j].val, cap) >= cap) {
                    ok = add_bin(s, neg(L.pairs[i].lit), neg(R.pairs[j].lit));
                }
            }
        }
        free(L.pairs); free(R.pairs);
        return ok;
    }

    size_t max_raw = (size_t)L.size + R.size + (size_t)L.size * R.size;
    uint64_t *vals = malloc((max_raw ? max_raw : 1u) * sizeof *vals);
    if (!vals) { free(L.pairs); free(R.pairs); s->error = true; return false; }
    size_t cnt = 0;
    for (uint32_t i = 0; i < L.size; ++i) vals[cnt++] = L.pairs[i].val;
    for (uint32_t j = 0; j < R.size; ++j) vals[cnt++] = R.pairs[j].val;
    for (uint32_t i = 0; i < L.size; ++i) {
        for (uint32_t j = 0; j < R.size; ++j) {
            vals[cnt++] = sat_add_cap(L.pairs[i].val, R.pairs[j].val, cap);
        }
    }
    qsort(vals, cnt, sizeof *vals, cmp_u64);
    uint32_t uniq = 0;
    for (size_t i = 0; i < cnt; ++i) {
        if (uniq == 0 || vals[i] != vals[uniq - 1]) vals[uniq++] = vals[i];
    }

    out->pairs = malloc((size_t)uniq * sizeof(GtePair));
    if (!out->pairs) { free(vals); free(L.pairs); free(R.pairs); s->error = true; return false; }
    out->size = uniq;

    bool ok = true;
    for (uint32_t i = 0; i < uniq; ++i) {
        Var v = solver_new_var(s);
        if (v == INVALID_VAR) { ok = false; break; }
        out->pairs[i].val = vals[i];
        out->pairs[i].lit = mkLit(v, false);
    }
    free(vals);

    for (uint32_t i = 0; i < L.size && ok; ++i) {
        uint32_t idx = gte_find_val(out->pairs, uniq, L.pairs[i].val);
        ok = add_bin(s, neg(L.pairs[i].lit), out->pairs[idx].lit);
    }
    for (uint32_t j = 0; j < R.size && ok; ++j) {
        uint32_t idx = gte_find_val(out->pairs, uniq, R.pairs[j].val);
        ok = add_bin(s, neg(R.pairs[j].lit), out->pairs[idx].lit);
    }
    for (uint32_t i = 0; i < L.size && ok; ++i) {
        for (uint32_t j = 0; j < R.size && ok; ++j) {
            uint64_t sm = sat_add_cap(L.pairs[i].val, R.pairs[j].val, cap);
            uint32_t idx = gte_find_val(out->pairs, uniq, sm);
            ok = add_tern(s, neg(L.pairs[i].lit), neg(R.pairs[j].lit), out->pairs[idx].lit);
        }
    }

    free(L.pairs);
    free(R.pairs);
    if (!ok) { free(out->pairs); out->pairs = NULL; out->size = 0; }
    return ok;
}

/* --- Exact Binary Weighted Adder Network + Lexicographic <= Comparator --- */

typedef struct {
    Lit *data;
    uint32_t size;
    uint32_t cap;
} BitBucket;

static bool bucket_push(BitBucket *b, Lit l) {
    if (b->size == b->cap) {
        uint32_t nc = b->cap ? b->cap * 2u : 8u;
        Lit *p = realloc(b->data, (size_t)nc * sizeof *p);
        if (!p) return false;
        b->data = p;
        b->cap = nc;
    }
    b->data[b->size++] = l;
    return true;
}

static bool encode_half_adder(Solver *s, Lit a, Lit b, Lit *out_sum, Lit *out_carry) {
    Var sv = solver_new_var(s);
    Var cv = solver_new_var(s);
    if (sv == INVALID_VAR || cv == INVALID_VAR) return false;
    Lit sum = mkLit(sv, false);
    Lit cry = mkLit(cv, false);
    /* sum <-> a XOR b */
    if (!add_tern(s, neg(a), neg(b), neg(sum))) return false;
    if (!add_tern(s, a, b, neg(sum))) return false;
    if (!add_tern(s, neg(a), b, sum)) return false;
    if (!add_tern(s, a, neg(b), sum)) return false;
    /* cry <-> a AND b */
    if (!add_tern(s, neg(a), neg(b), cry)) return false;
    if (!add_bin(s, a, neg(cry))) return false;
    if (!add_bin(s, b, neg(cry))) return false;
    *out_sum = sum;
    *out_carry = cry;
    return true;
}

static bool encode_full_adder(Solver *s, Lit a, Lit b, Lit c, Lit *out_sum, Lit *out_carry) {
    Var sv = solver_new_var(s);
    Var cv = solver_new_var(s);
    if (sv == INVALID_VAR || cv == INVALID_VAR) return false;
    Lit sum = mkLit(sv, false);
    Lit cry = mkLit(cv, false);
    /* sum <-> a XOR b XOR c */
    Lit cl4[4];
    cl4[0] = a;      cl4[1] = b;      cl4[2] = c;      cl4[3] = neg(sum); if (!add_cls(s, cl4, 4)) return false;
    cl4[0] = a;      cl4[1] = neg(b); cl4[2] = neg(c); cl4[3] = neg(sum); if (!add_cls(s, cl4, 4)) return false;
    cl4[0] = neg(a); cl4[1] = b;      cl4[2] = neg(c); cl4[3] = neg(sum); if (!add_cls(s, cl4, 4)) return false;
    cl4[0] = neg(a); cl4[1] = neg(b); cl4[2] = c;      cl4[3] = neg(sum); if (!add_cls(s, cl4, 4)) return false;
    cl4[0] = neg(a); cl4[1] = neg(b); cl4[2] = neg(c); cl4[3] = sum;      if (!add_cls(s, cl4, 4)) return false;
    cl4[0] = neg(a); cl4[1] = b;      cl4[2] = c;      cl4[3] = sum;      if (!add_cls(s, cl4, 4)) return false;
    cl4[0] = a;      cl4[1] = neg(b); cl4[2] = c;      cl4[3] = sum;      if (!add_cls(s, cl4, 4)) return false;
    cl4[0] = a;      cl4[1] = b;      cl4[2] = neg(c); cl4[3] = sum;      if (!add_cls(s, cl4, 4)) return false;
    /* cry <-> maj(a, b, c) */
    if (!add_tern(s, neg(a), neg(b), cry)) return false;
    if (!add_tern(s, neg(a), neg(c), cry)) return false;
    if (!add_tern(s, neg(b), neg(c), cry)) return false;
    if (!add_tern(s, a, b, neg(cry))) return false;
    if (!add_tern(s, a, c, neg(cry))) return false;
    if (!add_tern(s, b, c, neg(cry))) return false;
    *out_sum = sum;
    *out_carry = cry;
    return true;
}

static bool encode_pb_adder(Solver *s, const PbTerm *terms, uint32_t n, uint64_t rhs) {
    /* Enough bits for up to 64-bit weights plus carry headroom. */
    enum { MAX_BITS = 66 };
    BitBucket buckets[MAX_BITS];
    memset(buckets, 0, sizeof buckets);
    bool ok = true;

    for (uint32_t i = 0; i < n && ok; ++i) {
        uint64_t w = terms[i].weight;
        for (uint32_t b = 0; b < 64 && w; ++b, w >>= 1) {
            if (w & 1ULL) {
                if (!bucket_push(&buckets[b], terms[i].lit)) { s->error = true; ok = false; break; }
            }
        }
    }

    Lit sum_bits[MAX_BITS];
    for (uint32_t b = 0; b < MAX_BITS; ++b) sum_bits[b] = LIT_UNDEF;

    for (uint32_t b = 0; b < MAX_BITS && ok; ++b) {
        uint32_t head = 0;
        while (buckets[b].size - head >= 3 && ok) {
            Lit a = buckets[b].data[head++];
            Lit x = buckets[b].data[head++];
            Lit c = buckets[b].data[head++];
            Lit sb, cb;
            ok = encode_full_adder(s, a, x, c, &sb, &cb);
            if (ok && (!bucket_push(&buckets[b], sb) ||
                       (b + 1 < MAX_BITS && !bucket_push(&buckets[b + 1], cb)))) {
                s->error = true; ok = false;
            }
        }
        while (buckets[b].size - head == 2 && ok) {
            Lit a = buckets[b].data[head++];
            Lit x = buckets[b].data[head++];
            Lit sb, cb;
            ok = encode_half_adder(s, a, x, &sb, &cb);
            if (ok && (!bucket_push(&buckets[b], sb) ||
                       (b + 1 < MAX_BITS && !bucket_push(&buckets[b + 1], cb)))) {
                s->error = true; ok = false;
            }
        }
        if (ok && buckets[b].size - head == 1) {
            sum_bits[b] = buckets[b].data[head];
        }
    }

    for (uint32_t b = 0; b < MAX_BITS; ++b) free(buckets[b].data);
    if (!ok) return false;

    /* Enforce sum_bits <= rhs (zero extra variables):
     * For every bit b where sum_bits[b] exists and rhs bit b is 0, forbid
     * (sum_bits[b] == 1 AND all higher bits j > b with rhs_j == 1 have sum_bits[j] == 1).
     * Note: if any higher bit j > b with rhs_j == 1 has sum_bits[j] == LIT_UNDEF (const 0),
     * then sum_bits is strictly smaller than rhs at bit j, so lower bits cannot overflow! */
    Lit cl[MAX_BITS];
    for (int b = MAX_BITS - 1; b >= 0 && ok; --b) {
        if (sum_bits[b] == LIT_UNDEF) continue;
        bool rhs_b = (b < 64) ? (((rhs >> (unsigned)b) & 1ULL) != 0) : false;
        if (rhs_b) continue;
        uint32_t cl_sz = 0;
        cl[cl_sz++] = neg(sum_bits[b]);
        bool impossible_to_reach = false;
        for (int j = MAX_BITS - 1; j > b; --j) {
            bool rhs_j = (j < 64) ? (((rhs >> (unsigned)j) & 1ULL) != 0) : false;
            if (!rhs_j) continue;
            if (sum_bits[j] == LIT_UNDEF) {
                /* Constant 0 bit where rhs has 1 => strictly less than rhs already! */
                impossible_to_reach = true;
                break;
            }
            cl[cl_sz++] = neg(sum_bits[j]);
        }
        if (!impossible_to_reach) {
            ok = add_cls(s, cl, cl_sz);
        }
    }
    return ok;
}

bool pb_add_pb_le(Solver *s, const Lit *lits, const uint64_t *weights,
                  uint32_t n, uint64_t rhs) {
    if (!s || s->error) return false;
    if (n == 0) return true;

    PbTerm *terms = malloc((size_t)n * sizeof *terms);
    if (!terms) { s->error = true; return false; }

    uint32_t m = 0;
    for (uint32_t i = 0; i < n; ++i) {
        if (weights[i] == 0) continue;
        terms[m].lit = lits[i];
        terms[m].weight = weights[i];
        m++;
    }
    if (m == 0) { free(terms); return true; }

    /* Merge duplicate variables (including opposite polarities). */
    qsort(terms, m, sizeof *terms, cmp_pb_term_var);
    uint32_t out = 0;
    bool unsat = false;
    for (uint32_t i = 0; i < m && !unsat; ) {
        Var v = var(terms[i].lit);
        uint64_t pos_w = 0, neg_w = 0;
        while (i < m && var(terms[i].lit) == v) {
            uint64_t w = terms[i].weight;
            if (sign(terms[i].lit)) {
                neg_w = (neg_w > UINT64_MAX - w) ? UINT64_MAX : neg_w + w;
            } else {
                pos_w = (pos_w > UINT64_MAX - w) ? UINT64_MAX : pos_w + w;
            }
            i++;
        }
        uint64_t common = (pos_w < neg_w) ? pos_w : neg_w;
        if (common > rhs) { unsat = true; break; }
        rhs -= common;
        pos_w -= common;
        neg_w -= common;
        if (pos_w > 0) {
            terms[out].lit = mkLit(v, false);
            terms[out].weight = pos_w;
            out++;
        } else if (neg_w > 0) {
            terms[out].lit = mkLit(v, true);
            terms[out].weight = neg_w;
            out++;
        }
    }
    if (unsat) {
        free(terms);
        return add_cls(s, NULL, 0);
    }

    /* Filter terms with weight > rhs (immediately forced false) & compute total sum. */
    uint32_t kept = 0;
    uint64_t total_w = 0;
    bool overflow_sum = false;
    bool ok = true;
    for (uint32_t i = 0; i < out && ok; ++i) {
        if (terms[i].weight > rhs) {
            ok = add_unit(s, neg(terms[i].lit));
        } else {
            if (total_w > UINT64_MAX - terms[i].weight) overflow_sum = true;
            else total_w += terms[i].weight;
            terms[kept++] = terms[i];
        }
    }
    if (!ok || kept == 0 || (!overflow_sum && total_w <= rhs)) {
        free(terms);
        return ok;
    }

    /* Check if all remaining weights are equal (unweighted cardinality fast path). */
    bool uniform = true;
    uint64_t w0 = terms[0].weight;
    for (uint32_t i = 1; i < kept; ++i) {
        if (terms[i].weight != w0) { uniform = false; break; }
    }
    if (uniform) {
        uint64_t k64 = rhs / w0;
        if (k64 >= kept) { free(terms); return true; }
        Lit *ulits = malloc((size_t)kept * sizeof *ulits);
        if (!ulits) { free(terms); s->error = true; return false; }
        for (uint32_t i = 0; i < kept; ++i) ulits[i] = terms[i].lit;
        free(terms);
        if (k64 <= 32 || (uint64_t)kept * k64 <= 4096) {
            ok = pb_add_atmost_sinz(s, ulits, kept, (uint32_t)k64);
        } else {
            PbTotalizer *tot = pb_totalizer_create(s, ulits, kept, (uint32_t)k64);
            if (!tot) ok = false;
            else {
                ok = add_unit(s, neg(pb_totalizer_out(tot, (uint32_t)k64)));
                pb_totalizer_free(tot);
            }
        }
        free(ulits);
        return ok;
    }

    /* Sort by ascending weight so GTE pairs similar weights nicely. */
    qsort(terms, kept, sizeof *terms, cmp_pb_term_weight);

    uint64_t cap = (rhs < UINT64_MAX) ? (rhs + 1ULL) : UINT64_MAX;
    uint64_t *check_vals = NULL;
    uint32_t check_n = 0;
    if (rhs < UINT64_MAX && gte_check_size(terms, kept, cap, &check_vals, &check_n)) {
        free(check_vals);
        GteNode root = {NULL, 0};
        ok = gte_encode_rec(s, terms, kept, cap, true, &root);
        free(root.pairs);
    } else {
        ok = encode_pb_adder(s, terms, kept, rhs);
    }

    free(terms);
    return ok;
}

/* =====================================================================
 * Native .opb Parser & Decision / Optimization Driver
 * ===================================================================== */

void pb_instance_init(PbInstance *inst) {
    memset(inst, 0, sizeof *inst);
}

void pb_instance_free(PbInstance *inst) {
    if (!inst) return;
    free(inst->obj_lits);
    free(inst->obj_coeffs);
    for (uint32_t i = 0; i < inst->num_constraints; ++i) {
        free(inst->constraints[i].lits);
        free(inst->constraints[i].coeffs);
    }
    free(inst->constraints);
    memset(inst, 0, sizeof *inst);
}

static bool opb_push_term(Lit **lits, int64_t **coeffs, uint32_t *size, uint32_t *cap,
                          Lit l, int64_t c) {
    if (*size == *cap) {
        uint32_t nc = *cap ? (*cap * 2u) : 16u;
        Lit *nl = realloc(*lits, (size_t)nc * sizeof *nl);
        int64_t *nco = realloc(*coeffs, (size_t)nc * sizeof *nco);
        if (!nl || !nco) {
            if (nl) *lits = nl;
            if (nco) *coeffs = nco;
            return false;
        }
        *lits = nl;
        *coeffs = nco;
        *cap = nc;
    }
    (*lits)[*size] = l;
    (*coeffs)[*size] = c;
    (*size)++;
    return true;
}

static int opb_next_token(FILE *fp, char *buf, size_t cap) {
    int c;
    for (;;) {
        c = fgetc(fp);
        if (c == EOF) return ferror(fp) ? -1 : 0;
        if (isspace((unsigned char)c)) continue;
        if (c == '*') {
            while (c != '\n' && c != EOF) c = fgetc(fp);
            continue;
        }
        break;
    }
    if (c == ';') {
        buf[0] = ';'; buf[1] = '\0';
        return 1;
    }
    size_t n = 0;
    while (c != EOF && !isspace((unsigned char)c) && c != ';') {
        if (n + 1 >= cap) return -1;
        buf[n++] = (char)c;
        c = fgetc(fp);
    }
    if (c == ';') ungetc(c, fp);
    buf[n] = '\0';
    return 1;
}

static bool opb_parse_i64(const char *s, int64_t *out) {
    char *end = NULL;
    errno = 0;
    long long v = strtoll(s, &end, 10);
    if (errno || end == s || *end != '\0') return false;
    *out = (int64_t)v;
    return true;
}

static bool opb_parse_lit(const char *s, Lit *out, uint32_t *max_var) {
    bool neg_lit = false;
    if (*s == '~') { neg_lit = true; s++; }
    if (*s != 'x' && *s != 'X') return false;
    s++;
    if (!isdigit((unsigned char)*s)) return false;
    char *end = NULL;
    errno = 0;
    unsigned long long v = strtoull(s, &end, 10);
    if (errno || *end != '\0' || v == 0 || v > MAX_VARS) return false;
    if ((uint32_t)v > *max_var) *max_var = (uint32_t)v;
    *out = mkLit((Var)v, neg_lit);
    return true;
}

bool pb_parse_opb_stream(PbInstance *inst, FILE *fp) {
    if (!inst || !fp) return false;
    pb_instance_init(inst);
    char tok[128];

    for (;;) {
        int st = opb_next_token(fp, tok, sizeof tok);
        if (st < 0) { pb_instance_free(inst); return false; }
        if (st == 0) break;

        if (strcmp(tok, "min:") == 0) {
            if (inst->has_objective) { pb_instance_free(inst); return false; }
            inst->has_objective = true;
            for (;;) {
                st = opb_next_token(fp, tok, sizeof tok);
                if (st <= 0) { pb_instance_free(inst); return false; }
                if (strcmp(tok, ";") == 0) break;
                int64_t coeff = 0;
                if (!opb_parse_i64(tok, &coeff)) { pb_instance_free(inst); return false; }
                st = opb_next_token(fp, tok, sizeof tok);
                if (st <= 0) { pb_instance_free(inst); return false; }
                Lit lit;
                if (!opb_parse_lit(tok, &lit, &inst->num_vars)) {
                    pb_instance_free(inst); return false;
                }
                if (!opb_push_term(&inst->obj_lits, &inst->obj_coeffs,
                                   &inst->obj_size, &inst->obj_cap, lit, coeff)) {
                    pb_instance_free(inst); return false;
                }
            }
            continue;
        }

        /* Constraint: coeff lit ... ( >= | <= | = ) rhs ; */
        PbConstraint c;
        memset(&c, 0, sizeof c);
        bool got_op = false;

        for (;;) {
            if (strcmp(tok, ">=") == 0 || strcmp(tok, "<=") == 0 || strcmp(tok, "=") == 0) {
                c.op = (tok[0] == '>') ? PB_OP_GE : (tok[0] == '<' ? PB_OP_LE : PB_OP_EQ);
                got_op = true;
                break;
            }
            int64_t coeff = 0;
            if (!opb_parse_i64(tok, &coeff)) {
                free(c.lits); free(c.coeffs); pb_instance_free(inst); return false;
            }
            st = opb_next_token(fp, tok, sizeof tok);
            if (st <= 0) {
                free(c.lits); free(c.coeffs); pb_instance_free(inst); return false;
            }
            Lit lit;
            if (!opb_parse_lit(tok, &lit, &inst->num_vars)) {
                free(c.lits); free(c.coeffs); pb_instance_free(inst); return false;
            }
            if (!opb_push_term(&c.lits, &c.coeffs, &c.size, &c.cap, lit, coeff)) {
                free(c.lits); free(c.coeffs); pb_instance_free(inst); return false;
            }
            st = opb_next_token(fp, tok, sizeof tok);
            if (st <= 0) {
                free(c.lits); free(c.coeffs); pb_instance_free(inst); return false;
            }
        }
        if (!got_op) { free(c.lits); free(c.coeffs); pb_instance_free(inst); return false; }

        st = opb_next_token(fp, tok, sizeof tok);
        if (st <= 0 || !opb_parse_i64(tok, &c.rhs)) {
            free(c.lits); free(c.coeffs); pb_instance_free(inst); return false;
        }
        st = opb_next_token(fp, tok, sizeof tok);
        if (st <= 0 || strcmp(tok, ";") != 0) {
            free(c.lits); free(c.coeffs); pb_instance_free(inst); return false;
        }

        if (inst->num_constraints == inst->cap_constraints) {
            uint32_t nc = inst->cap_constraints ? inst->cap_constraints * 2u : 16u;
            PbConstraint *p = realloc(inst->constraints, (size_t)nc * sizeof *p);
            if (!p) { free(c.lits); free(c.coeffs); pb_instance_free(inst); return false; }
            inst->constraints = p;
            inst->cap_constraints = nc;
        }
        inst->constraints[inst->num_constraints++] = c;
    }

    return !ferror(fp);
}

bool pb_parse_opb_file(PbInstance *inst, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return false;
    bool ok = pb_parse_opb_stream(inst, fp);
    fclose(fp);
    return ok;
}

bool pb_parse_opb_string(PbInstance *inst, const char *text) {
    if (!text) return false;
    FILE *fp = fmemopen((void *)text, strlen(text), "r");
    if (!fp) return false;
    bool ok = pb_parse_opb_stream(inst, fp);
    fclose(fp);
    return ok;
}

/* Normalize signed linear constraint sum(c_i * l_i) <= rhs (or >= rhs when flip=true)
 * into non-negative weights sum(w_i * l'_i) <= rhs' and encode into solver s. */
static bool encode_signed_pb_le(Solver *s, const Lit *lits, const int64_t *coeffs,
                                uint32_t n, int64_t rhs, bool flip_ge) {
    Lit *nlits = malloc((size_t)(n ? n : 1u) * sizeof *nlits);
    uint64_t *nw = malloc((size_t)(n ? n : 1u) * sizeof *nw);
    if (!nlits || !nw) { free(nlits); free(nw); s->error = true; return false; }

    /* Use 128-bit accumulator via int64_t/uint64_t careful arithmetic to avoid UB. */
    int64_t adj_rhs = flip_ge ? -rhs : rhs;
    uint32_t m = 0;
    for (uint32_t i = 0; i < n; ++i) {
        int64_t c = flip_ge ? -coeffs[i] : coeffs[i];
        if (c == 0) continue;
        if (c > 0) {
            nlits[m] = lits[i];
            nw[m] = (uint64_t)c;
            m++;
        } else {
            /* (-w) * l = -w + w * (~l)  =>  add +w to adj_rhs */
            uint64_t w = (uint64_t)(-(c + 1)) + 1ULL;
            nlits[m] = neg(lits[i]);
            nw[m] = w;
            m++;
            if (adj_rhs > INT64_MAX - (int64_t)w) adj_rhs = INT64_MAX;
            else adj_rhs += (int64_t)w;
        }
    }

    bool ok;
    if (adj_rhs < 0) {
        ok = add_cls(s, NULL, 0); /* UNSAT */
    } else {
        ok = pb_add_pb_le(s, nlits, nw, m, (uint64_t)adj_rhs);
    }
    free(nlits);
    free(nw);
    return ok;
}

bool pb_encode_instance(Solver *s, const PbInstance *inst) {
    if (!s || !inst) return false;
    while (s->num_vars < inst->num_vars) {
        if (solver_new_var(s) == INVALID_VAR) return false;
    }
    for (uint32_t i = 0; i < inst->num_constraints; ++i) {
        const PbConstraint *c = &inst->constraints[i];
        if (c->op == PB_OP_LE || c->op == PB_OP_EQ) {
            if (!encode_signed_pb_le(s, c->lits, c->coeffs, c->size, c->rhs, false)) return false;
        }
        if (c->op == PB_OP_GE || c->op == PB_OP_EQ) {
            if (!encode_signed_pb_le(s, c->lits, c->coeffs, c->size, c->rhs, true)) return false;
        }
    }
    return !s->error;
}

static bool lit_val_in_model(const lbool *model, Lit l) {
    lbool v = model[var(l)];
    bool pos = (v == TRUE);
    return sign(l) ? !pos : pos;
}

bool pb_verify_model(const PbInstance *inst, const lbool *model, int64_t *out_obj) {
    if (!inst || !model) return false;
    for (uint32_t i = 0; i < inst->num_constraints; ++i) {
        const PbConstraint *c = &inst->constraints[i];
        int64_t lhs = 0;
        for (uint32_t j = 0; j < c->size; ++j) {
            if (lit_val_in_model(model, c->lits[j])) lhs += c->coeffs[j];
        }
        if (c->op == PB_OP_GE && !(lhs >= c->rhs)) return false;
        if (c->op == PB_OP_LE && !(lhs <= c->rhs)) return false;
        if (c->op == PB_OP_EQ && !(lhs == c->rhs)) return false;
    }
    if (out_obj && inst->has_objective) {
        int64_t obj = 0;
        for (uint32_t j = 0; j < inst->obj_size; ++j) {
            if (lit_val_in_model(model, inst->obj_lits[j])) obj += inst->obj_coeffs[j];
        }
        *out_obj = obj;
    }
    return true;
}

void pb_solve_result_free(PbSolveResult *res) {
    if (!res) return;
    free(res->best_model);
    res->best_model = NULL;
}

PbSolveResult pb_solve_opb(const PbInstance *inst, const SolverOpts *user_opts) {
    PbSolveResult res;
    memset(&res, 0, sizeof res);
    res.status = UNDEF;
    if (!inst) return res;

    SolverOpts opts = user_opts ? *user_opts : default_opts();
    if (inst->has_objective) {
        opts.reuse_learnts = true;
        opts.inprocess = false;
        opts.sbva = false;
        opts.sym = false;
    }

    Solver *s = solver_new_with_opts(&opts);
    if (!s) return res;

    if (!pb_encode_instance(s, inst)) {
        solver_free(s);
        return res;
    }

    if (!inst->has_objective) {
        res.solves = 1;
        lbool r = solver_solve(s);
        res.status = r;
        if (r == TRUE) {
            res.optimum = true;
            res.best_model = malloc((size_t)(inst->num_vars + 1u) * sizeof(lbool));
            if (res.best_model) {
                res.best_model[0] = UNDEF;
                for (Var v = 1; v <= inst->num_vars; ++v) {
                    lbool mv = solver_model_value(s, v);
                    res.best_model[v] = (mv == TRUE) ? TRUE : FALSE;
                }
                if (!pb_verify_model(inst, res.best_model, NULL)) {
                    res.status = UNDEF;
                    res.optimum = false;
                }
            } else {
                res.status = UNDEF;
            }
        }
        solver_free(s);
        return res;
    }

    /* Linear SAT-UNSAT optimization with incremental PB upper-bound tightening. */
    lbool *model = malloc((size_t)(inst->num_vars + 1u) * sizeof(lbool));
    if (!model) { solver_free(s); return res; }
    model[0] = UNDEF;

    for (;;) {
        res.solves++;
        lbool r = solver_solve(s);
        if (r == TRUE) {
            for (Var v = 1; v <= inst->num_vars; ++v) {
                lbool mv = solver_model_value(s, v);
                model[v] = (mv == TRUE) ? TRUE : FALSE;
            }
            int64_t obj = 0;
            if (!pb_verify_model(inst, model, &obj)) {
                res.status = UNDEF;
                break;
            }
            res.status = TRUE;
            res.best_obj = obj;
            if (!res.best_model) {
                res.best_model = malloc((size_t)(inst->num_vars + 1u) * sizeof(lbool));
                if (!res.best_model) { res.status = UNDEF; break; }
            }
            memcpy(res.best_model, model, (size_t)(inst->num_vars + 1u) * sizeof(lbool));
            if (!opts.quiet) {
                printf("o %lld\n", (long long)obj);
                fflush(stdout);
            }
            if (obj == INT64_MIN) {
                res.optimum = true;
                break;
            }
            if (!encode_signed_pb_le(s, inst->obj_lits, inst->obj_coeffs,
                                     inst->obj_size, obj - 1, false)) {
                res.status = UNDEF;
                break;
            }
        } else if (r == FALSE) {
            if (res.status == TRUE) {
                res.optimum = true;
            } else {
                res.status = FALSE;
            }
            break;
        } else {
            /* Budget/timeout hit: keep best incumbent if any (status stays TRUE, optimum=false). */
            break;
        }
    }

    free(model);
    solver_free(s);
    return res;
}
