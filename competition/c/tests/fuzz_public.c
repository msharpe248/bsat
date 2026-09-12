/* Opaque facade/IPASIR histories, certified sessions, limits and OOM, checked
   against a six-variable exhaustive oracle. */
#include "bsat.h"
#include "ipasir.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
void fuzz_alloc_reset(size_t);
bool fuzz_alloc_failed(void);

typedef struct {
    int lits[3];
    unsigned n;
} Clause;

static bool
satisfies(unsigned bits, const int *a, unsigned n)
{
    for (unsigned i = 0; i < n; ++i) {
        int v = a[i] < 0 ? -a[i] : a[i];

        if (((bits >> (v - 1)) & 1u) == (unsigned)(a[i] > 0)) return true;
    }
    return false;
}

static bool
oracle(Clause *cs, unsigned count, int *a, unsigned n)
{
    for (unsigned bits = 0; bits < 64; ++bits) {
        bool ok = true;

        for (unsigned i = 0; i < count; ++i)
            ok &= satisfies(bits, cs[i].lits, cs[i].n);
        for (unsigned i = 0; i < n; ++i)
            ok &= satisfies(bits, a + i, 1);
        if (ok) return true;
    }
    return false;
}

static int
literal(unsigned x)
{
    int v = 1 + (int)((x >> 1) % 6);

    return x & 1 ? -v : v;
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 3 || size > 512) return 0;
    fuzz_alloc_reset(data[0] & 128 ? (size_t)data[1] + 1 : 0);
    bool ip = data[0] & 4;
    bsat *s = NULL;
    void *adapter = NULL;
    unsigned flags = data[0] & 3;

    if ((flags & BSAT_CERTIFICATES) && (data[1] & 1)) flags |= BSAT_CERTIFIED_PROBING;
    if ((flags & 3) == 3 && (data[1] & 2)) flags |= BSAT_VSIDS;
    if (ip)
        adapter = ipasir_init();
    else
        s = bsat_create(1, flags);
    if (!s && !adapter) return 0;
    if (s && (data[0] & 32) && (data[0] & 2)) bsat_set_journal_limit(s, 32);
    Clause cs[32];
    unsigned count = 0;

    /* Introduce the allowed namespace without constraining any model. */
    if (ip) {
        ipasir_add(adapter, 6);
        ipasir_add(adapter, -6);
        ipasir_add(adapter, 0);
    } else {
        int t[] = {6, -6};

        bsat_add_clause(s, t, 2);
    }
    for (size_t at = 2; at < size && count < 32;) {
        if (s && (data[0] & 16) && !(at & 7)) bsat_checkpoint(s);
        unsigned op = data[at++] % 4;

        if (op == 0) {
            if (at == size) break;
            Clause c = {.n = data[at++] % 4};

            if (size - at < c.n) break;
            for (unsigned i = 0; i < c.n; ++i)
                c.lits[i] = literal(data[at++]);
            if (ip) {
                for (unsigned i = 0; i < c.n; ++i)
                    ipasir_add(adapter, c.lits[i]);
                ipasir_add(adapter, 0);
            } else
                bsat_add_clause(s, c.lits, c.n);
            cs[count++] = c;
        } else {
            int a[3];
            unsigned n = op == 1 ? 0 : op == 2 ? 1 : 3;

            if (size - at < n) break;
            for (unsigned i = 0; i < n; ++i)
                a[i] = literal(data[at++]);
            bool expected = oracle(cs, count, a, n);
            int r;

            if (ip) {
                for (unsigned i = 0; i < n; ++i)
                    ipasir_assume(adapter, a[i]);
                r = ipasir_solve(adapter);
            } else {
                bsat_set_query_limits(s, 0, (data[0] & 8) ? 1 : 0, 0);
                if (data[0] & 64) bsat_set_service_limits(s, (data[at - 1] & 1) ? 1e-12 : 0, 0);
                r = bsat_solve(s, a, n);
            }
            if (fuzz_alloc_failed()) {
                assert(!r);
                break;
            }
            if (s && bsat_error(s)) {
                assert(!r);
                break;
            }
            if (!r) {
                assert(!ip && (data[0] & (8 | 64)));
                continue;
            }
            assert(r == (expected ? 10 : 20));
            if (r == 10) {
                unsigned bits = 0;

                for (int v = 1; v <= 6; ++v)
                    if ((ip ? ipasir_val(adapter, v) : bsat_value(s, v)) > 0) bits |= 1u << (v - 1);
                for (unsigned i = 0; i < count; ++i)
                    assert(satisfies(bits, cs[i].lits, cs[i].n));
                for (unsigned i = 0; i < n; ++i)
                    assert(satisfies(bits, a + i, 1));
            }
        }
        if (!ip && bsat_error(s)) break;
    }
    if (ip)
        ipasir_release(adapter);
    else
        bsat_destroy(s);
    return 0;
}
