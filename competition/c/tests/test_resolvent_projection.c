/* Independent projected truth tables for conservatively bounded BVE. */
#include "../include/solver.h"
#include <assert.h>

static unsigned
word(unsigned *s)
{
    *s = *s * 1664525u + 1013904223u;
    return *s;
}

static unsigned
mask(unsigned code)
{
    unsigned bits = 0;

    for (unsigned i = 0; i < 6; ++i, code /= 3)
        if (code % 3) bits |= 1u << (2 * i + (code % 3 == 2));
    return bits;
}

static Solver *
formula(const unsigned *p, unsigned np, const unsigned *n, unsigned nn, unsigned vars)
{
    Solver *s = solver_new();

    assert(s);
    for (unsigned v = 1; v <= vars; ++v)
        assert(solver_new_var(s) == v);
    for (unsigned side = 0; side < 2; ++side)
        for (unsigned i = 0; i < (side ? nn : np); ++i) {
            Lit lits[10] = {mkLit(1, side)};
            unsigned z = 1, bits = (side ? n : p)[i];

            for (unsigned k = 0; k < 2 * (vars - 1); ++k)
                if (bits & (1u << k)) lits[z++] = mkLit(2 + k / 2, k & 1u);
            assert(solver_add_clause(s, lits, z));
        }
    elim_build_occs(s);
    assert(s->elim->occs_complete);
    // Preserve the watched pair while permuting otherwise identical parent sets.
    for (unsigned i = 0; i < s->num_clauses; ++i)
        if (i & 1u) {
            Lit *l = CLAUSE_LITS(s->arena, s->clauses[i]);
            unsigned z = CLAUSE_SIZE(s->arena, s->clauses[i]);
            Lit tmp = l[0];

            l[0] = l[1];
            l[1] = tmp;
            for (unsigned a = 2, b = z - 1; a < b; ++a, --b) {
                tmp = l[a];
                l[a] = l[b];
                l[b] = tmp;
            }
        }
    return s;
}

static bool
satisfied(unsigned clause, unsigned assignment, unsigned vars)
{
    for (unsigned v = 0; v < vars - 1; ++v) {
        if ((clause & (1u << (2 * v))) && (assignment & (1u << v))) return true;
        if ((clause & (2u << (2 * v))) && !(assignment & (1u << v))) return true;
    }
    return false;
}

static void
unchanged(Solver *s, unsigned clauses)
{
    assert(!s->error && !s->elim->stack_size && !s->elim->vars_eliminated);
    assert(s->num_clauses == clauses);
    for (unsigned i = 0; i < clauses; ++i)
        assert(!clause_deleted(s->arena, s->clauses[i]));
    for (Var v = 1; v <= s->num_vars; ++v)
        assert(!s->seen[v]);
}

static void
models(Solver *s, const unsigned *res, unsigned count)
{
    uint8_t roots[10];

    memcpy(roots, s->values, s->num_vars + 1);
    for (unsigned bits = 0; bits < (1u << (s->num_vars - 1)); ++bits) {
        bool expected = true, actual = s->result != FALSE;

        for (unsigned i = 0; i < count; ++i)
            expected &= satisfied(res[i], bits, s->num_vars);
        for (Var v = 2; v <= s->num_vars; ++v) {
            lbool value = (bits & (1u << (v - 2))) ? TRUE : FALSE;

            if (roots[v] != UNDEF && roots[v] != value) actual = false;
            s->values[v] = value;
        }
        for (unsigned i = 0; i < s->num_clauses; ++i) {
            CRef cr = s->clauses[i];

            if (clause_deleted(s->arena, cr)) continue;
            bool sat = false;

            for (unsigned j = 0; j < CLAUSE_SIZE(s->arena, cr); ++j) {
                Lit l = CLAUSE_LITS(s->arena, cr)[j];

                assert(var(l) != 1);
                sat |= lxor(s->values[var(l)], sign(l)) == TRUE;
            }
            actual &= sat;
        }
        assert(actual == expected);
        if (actual) {
            s->values[1] = UNDEF;
            elim_extend_model(s);
            assert(solver_check_model(s));
        }
    }
}

static Solver *
collision(void)
{
    // {2,9} and {6,7} both have sum of squared encoded literals equal to 340.
    unsigned tails[] = {(1u << 0) | (1u << 14), (1u << 8) | (1u << 10)};

    return formula(tails, 2, tails, 2, 9);
}

int
main(void)
{
    unsigned accepted = 0, rejected = 0;

    for (unsigned seed = 0; seed < 2048; ++seed) {
        unsigned rng = seed + 7137, p[4], n[4], np = 1 + seed % 4, nn = 1 + (seed / 4) % 4, res[16],
                 count = 0, raw = 0, variables = 7;

        for (unsigned i = 0; i < 4; ++i) {
            p[i] = mask(1 + (word(&rng) >> 8) % 728);
            n[i] = mask(1 + (word(&rng) >> 8) % 728);
        }
        if (seed % 16 == 0) {
            np = nn = 4;
            for (unsigned i = 0; i < 4; ++i)
                p[i] = n[i] = 5;
        }
        if (seed % 16 == 1) {
            variables = 9;
            np = nn = 4;
            for (unsigned i = 0; i < 4; ++i) {
                p[i] = 1u << (2 * i);
                n[i] = 1u << (2 * (i + 4));
            }
        }
        if (seed % 16 == 3) {
            np = nn = 3;
            for (unsigned i = 0; i < 3; ++i)
                p[i] = n[i] = 1u << (2 * i);
        }
        // Later unit resolvents imply earlier supersets. A conservative
        // growth estimate may decline this pivot; accepted projections must match.
        if (seed % 16 == 2) {
            np = nn = 4;
            for (unsigned i = 0; i < 4; ++i)
                p[i] = 1u << (2 * i);
            n[0] = 1u << 8;
            n[1] = 1u << 10;
            n[2] = 1u << 4;
            n[3] = 1u << 6;
        }
        for (unsigned i = 0; i < np; ++i)
            for (unsigned j = 0; j < nn; ++j) {
                unsigned r = p[i] | n[j];

                if (r & (r >> 1) & 0x5555u) continue;
                ++raw;
                bool duplicate = false;

                for (unsigned k = 0; k < count; ++k)
                    duplicate |= res[k] == r;
                if (!duplicate) res[count++] = r;
            }
        Solver *s = formula(p, np, n, nn, variables);
        int cost = elim_cost(s, 1);

        assert(cost == -1 || (cost >= 0 && (unsigned)cost <= raw && (unsigned)cost <= np + nn));
        for (Var v = 1; v <= s->num_vars; ++v)
            assert(!s->seen[v]);
        bool ok = elim_eliminate_var(s, 1);

        assert(ok == (cost >= 0) && !s->error);
        if (ok) {
            ++accepted;
            assert(s->elim->resolvents_added <= (unsigned)cost);
            models(s, res, count);
        } else {
            ++rejected;
            unchanged(s, np + nn);
        }
        solver_free(s);
    }
    assert(accepted && rejected);
    Solver *s = collision();
    int cost = elim_cost(s, 1);

    assert(cost >= 0 && cost <= 4);
    uint64_t start = s->work;

    assert(elim_eliminate_var(s, 1));
    uint64_t needed = s->work - start;

    assert(s->elim->resolvents_added <= (unsigned)cost);
    unsigned expected[] = {(1u << 0) | (1u << 14), (1u << 0) | (1u << 14) | (1u << 8) | (1u << 10),
                           (1u << 8) | (1u << 10)};

    models(s, expected, 3);
    solver_free(s);
    unsigned cutoffs = 0;

    for (uint64_t budget = 0; budget <= needed + 1; ++budget) {
        s = collision();
        s->work_limit = s->work + budget;
        bool ok = elim_eliminate_var(s, 1);

        assert(!s->error);
        for (Var v = 1; v <= s->num_vars; ++v)
            assert(!s->seen[v]);
        if (ok)
            models(s, expected, 3);
        else
            unchanged(s, 4);
        s->work_limit = 0;
        if (!ok) {
            assert(elim_eliminate_var(s, 1));
            models(s, expected, 3);
        }
        solver_free(s);
        ++cutoffs;
    }
    printf("PASS: 2048 bounded cost/projection cases (%u accepted, %u rejected), collision "
           "witnesses and %u staging cutoffs\n",
           accepted, rejected, cutoffs);
}
