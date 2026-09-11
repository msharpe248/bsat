#include "../include/solver.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void
add(Solver *s, const Lit *l, unsigned n)
{
    solver_add_clause(s, l, n);
    assert(!s->error);
}

static void
definition(Solver *s, unsigned kind, Lit out, Lit a, Lit b, Lit c, unsigned omit)
{
    Lit clauses[4][3];
    unsigned sizes[4] = {3, 3, 3, 3}, count = 4;

    if (!kind) {
        Lit d[3][3] = {{neg(out), a, 0}, {neg(out), b, 0}, {out, neg(a), neg(b)}};

        memcpy(clauses, d, sizeof d);
        sizes[0] = sizes[1] = 2;
        count = 3;
    } else if (kind == 1) {
        Lit d[4][3] = {
            {a, b, neg(out)}, {neg(a), neg(b), neg(out)}, {a, neg(b), out}, {neg(a), b, out}};

        memcpy(clauses, d, sizeof d);
    } else {
        Lit d[4][3] = {
            {neg(c), neg(a), out}, {neg(c), a, neg(out)}, {c, neg(b), out}, {c, b, neg(out)}};

        memcpy(clauses, d, sizeof d);
    }
    for (unsigned i = 0; i < count; ++i)
        if (i != omit) add(s, clauses[i], sizes[i]);
}

static bool
lit_value(Lit l, unsigned bits)
{
    return ((bits >> (var(l) - 1)) & 1) != sign(l);
}

static bool
input_value(Solver *s, unsigned bits)
{
    bool sat = false;

    for (size_t i = 0; i < s->input_size; ++i) {
        if (!s->input[i]) {
            if (!sat) return false;
            sat = false;
        } else
            sat |= lit_value(s->input[i], bits);
    }
    return true;
}

static bool
verify(Solver *s, uint32_t original, size_t input_size)
{
    assert(!s->error && !s->decision_level && s->input_size == input_size);
    bool sat = false;

    for (unsigned bits = 0; bits < (1u << s->num_vars); ++bits)
        if (input_value(s, bits)) {
            sat = true;
            for (uint32_t i = original; i < s->num_clauses; ++i) {
                CRef cr = s->clauses[i];
                bool valid = false;

                for (uint32_t j = 0; j < CLAUSE_SIZE(s->arena, cr); ++j)
                    valid |= lit_value(CLAUSE_LITS(s->arena, cr)[j], bits);
                assert(valid); // Every emitted lemma is entailed by the original CNF.
            }
        }
    return sat;
}

static void
signed_gates(void)
{
    unsigned tested = 0, merges = 0;

    for (unsigned kind = 0; kind < 3; ++kind)
        for (unsigned signs = 0; signs < 32; ++signs)
            for (unsigned variant = 0; variant < 4; ++variant) {
                Solver *s = solver_new();

                assert(s);
                for (unsigned i = 0; i < 5; ++i)
                    assert(solver_new_var(s));
                /* Rotate variable numbering, invert literals, and exchange operands. */
                unsigned shift = signs % 5;
                Lit v[5];

                for (unsigned i = 0; i < 5; ++i)
                    v[i] = mkLit(1 + (i + shift) % 5, (signs >> i) & 1);
                definition(s, kind, v[3], v[0], v[1], v[2], 99);
                definition(s, kind, v[4], variant & 1 ? v[1] : v[0], variant & 1 ? v[0] : v[1],
                           (variant & 1) && kind == 2 ? neg(v[2]) : v[2], variant == 3 ? 0 : 99);
                if (variant == 2) {
                    Lit x[] = {v[3], v[4]}, y[] = {neg(v[3]), neg(v[4])};

                    add(s, x, 2);
                    add(s, y, 2);
                }
                uint32_t original = s->num_clauses;
                size_t input_size = s->input_size;

                solver_congruence(s);
                bool sat = verify(s, original, input_size);

                merges += s->stats.congruence_merges > 0;
                if (variant < 2) assert(s->stats.congruence_merges > 0);
                s->work_limit = 0;
                assert(solver_solve(s) == (sat ? TRUE : FALSE));
                if (sat) assert(solver_check_model(s));
                solver_free(s);
                ++tested;
            }
    printf("PASS: %u signed AND/XOR/ITE cases; %u with proved aliases; every added clause "
           "truth-table checked\n",
           tested, merges);
}

static Solver *
cascade(bool contradictory)
{
    Solver *s = solver_new();

    assert(s);
    for (unsigned i = 0; i < 9; ++i)
        assert(solver_new_var(s));
    definition(s, 0, mkLit(4, 0), mkLit(1, 0), mkLit(2, 0), 0, 99);
    definition(s, 0, mkLit(5, 0), mkLit(2, 0), mkLit(1, 0), 0, 99);
    definition(s, 1, mkLit(6, 0), mkLit(4, 0), mkLit(3, 0), 0, 99);
    definition(s, 1, mkLit(7, 0), mkLit(5, 0), mkLit(3, 0), 0, 99);
    definition(s, 2, mkLit(8, 0), mkLit(6, 0), mkLit(1, 0), mkLit(2, 0), 99);
    definition(s, 2, mkLit(9, 0), mkLit(7, 0), mkLit(1, 0), mkLit(2, 0), 99);
    if (contradictory) {
        Lit a[] = {mkLit(8, 0), mkLit(9, 0)}, b[] = {mkLit(8, 1), mkLit(9, 1)};

        add(s, a, 2);
        add(s, b, 2);
    }
    return s;
}

static void
cutoffs(void)
{
    Solver *s = cascade(false);
    uint32_t original = s->num_clauses;
    size_t input_size = s->input_size;

    solver_congruence(s);
    uint64_t work = s->work;

    assert(verify(s, original, input_size) && s->stats.congruence_merges >= 3);
    solver_free(s);
    for (unsigned i = 0; i < 65; ++i) {
        s = cascade(false);
        original = s->num_clauses;
        input_size = s->input_size;
        s->work = 1;
        s->work_limit = 1 + work * i / 64;
        solver_congruence(s);
        assert(verify(s, original, input_size));
        assert(s->decision_level == 0);
        s->work_limit = 0;
        assert(solver_solve(s) == TRUE && !s->error && solver_check_model(s));
        solver_free(s);
    }
    s = cascade(true);
    solver_congruence(s);
    assert(solver_solve(s) == FALSE && !s->error);
    solver_free(s);
    printf("PASS: 65 congruence budget cutoffs and subsequent complete solves (full work %llu)\n",
           (unsigned long long)work);
}

static void
rup_guard(void)
{
    Solver *s = solver_new();

    assert(s);
    assert(solver_new_var(s));
    assert(solver_new_var(s));
    Lit clause[] = {mkLit(1, 0), mkLit(2, 0)};

    add(s, clause, 2);
    s->vars[1].polarity = true;
    s->vars[2].polarity = false;
    uint32_t clauses = s->num_clauses;
    size_t input = s->input_size;

    assert(!solver_add_rup_clause(s, clause, 1)); // x is not entailed by (x or y).
    assert(s->opts.phase_saving && s->vars[1].polarity && !s->vars[2].polarity);
    assert(s->num_clauses == clauses && s->input_size == input && !s->trail_size &&
           !s->decision_level);
    s->work = 1;
    s->work_limit = 1;
    assert(!solver_add_rup_clause(s, clause, 2) && s->num_clauses == clauses);
    s->work_limit = 0;
    assert(solver_add_rup_clause(s, clause, 2));
    assert(s->opts.phase_saving && s->vars[1].polarity && !s->vars[2].polarity);
    assert(s->num_clauses == clauses + 1 && s->input_size == input);
    solver_free(s);
    s = solver_new();
    assert(s);
    for (unsigned i = 0; i < 4; ++i)
        assert(solver_new_var(s));
    Lit xy[] = {mkLit(1, 1), mkLit(2, 0)}, yz[] = {mkLit(2, 1), mkLit(3, 0)}, x = mkLit(1, 0);

    add(s, xy, 2);
    add(s, yz, 2);
    add(s, &x, 1);
    assert(s->values[2] == UNDEF && s->values[3] == UNDEF);
    Lit implied[] = {mkLit(2, 0), mkLit(4, 0)};

    assert(solver_add_rup_clause(s, implied, 2));
    assert(s->values[2] == TRUE && s->values[3] == TRUE && !s->vars[2].level && !s->vars[3].level);
    assert(!s->decision_level);
    solver_free(s);
    s = solver_new();
    assert(s);
    for (unsigned i = 0; i < 4; ++i)
        assert(solver_new_var(s));
    Lit xa[] = {mkLit(1, 0), mkLit(4, 0)}, xna[] = {mkLit(1, 0), mkLit(4, 1)};

    add(s, xy, 2);
    add(s, yz, 2);
    add(s, xa, 2);
    add(s, xna, 2);
    input = s->input_size;
    assert(solver_add_rup_clause(s, &x, 1));
    assert(s->values[1] == TRUE && s->values[2] == TRUE && s->values[3] == TRUE);
    assert(!s->vars[1].level && !s->vars[2].level && !s->vars[3].level);
    assert(!s->decision_level && s->input_size == input);
    solver_free(s);
    puts("PASS: RUP rejection and exhausted-budget paths leave input and core unchanged");
}

static void
aliases_and_constants(void)
{
    unsigned tested = 0;

    for (unsigned kind = 0; kind < 3; ++kind)
        for (unsigned bits = 0; bits < 32; ++bits)
            for (unsigned relation = 0; relation < 12; ++relation) {
                Solver *s = solver_new();

                assert(s);
                for (unsigned i = 0; i < 5; ++i)
                    assert(solver_new_var(s));
                Lit a = mkLit(1, bits & 1), b = mkLit(2, (bits >> 1) & 1),
                    c = mkLit(3, (bits >> 2) & 1);
                Lit out = mkLit(4, (bits >> 3) & 1), other = mkLit(5, (bits >> 4) & 1);

                definition(s, kind, out, a, b, c, 99);
                definition(s, kind, other, a, b, c, 99);
                if (relation < 8) {
                    Lit left = relation < 2 ? out : relation < 4 ? a : relation < 6 ? c : out;
                    Lit right = relation < 2 ? a : relation < 4 ? b : relation < 6 ? a : c;

                    right ^= relation & 1;
                    Lit x[] = {neg(left), right}, y[] = {left, neg(right)};

                    add(s, x, 2);
                    add(s, y, 2);
                } else {
                    Lit unit = (relation < 10 ? out : a) ^ (relation & 1);

                    add(s, &unit, 1);
                }
                uint32_t original = s->num_clauses;
                size_t input = s->input_size;

                solver_congruence(s);
                bool sat = verify(s, original, input);

                if (kind == 1 && relation < 2)
                    assert(lxor(s->values[var(b)], sign(b)) == (relation ? TRUE : FALSE));
                if (!kind && relation == 1) assert(lxor(s->values[var(out)], sign(out)) == FALSE);
                assert(solver_solve(s) == (sat ? TRUE : FALSE));
                if (sat) assert(solver_check_model(s));
                solver_free(s);
                ++tested;
            }
    printf("PASS: %u signed binary-alias, fixed-output, fixed-input and cyclic-gate cases\n",
           tested);
}

static void
conditional_alternatives(void)
{
    for (unsigned bits = 0; bits < 64; ++bits) {
        Solver *s = solver_new();

        assert(s);
        for (unsigned i = 0; i < 6; ++i)
            assert(solver_new_var(s));
        Lit v[6];

        for (unsigned i = 0; i < 6; ++i)
            v[i] = mkLit(1 + (i + bits % 6) % 6, (bits >> i) & 1);
        definition(s, 2, v[4], v[1], v[3], v[0], 99);
        definition(s, 2, v[4], v[2], v[3], v[0], 99);
        definition(s, 2, v[5], v[2], v[3], v[0], 99);
        uint32_t original = s->num_clauses;
        size_t input = s->input_size;

        solver_congruence(s);
        assert(verify(s, original, input) && s->stats.congruence_merges > 0);
        assert(solver_solve(s) == TRUE && solver_check_model(s));
        solver_free(s);
    }
    puts("PASS: 64 signed conditional-alternative ITE cases with proved output equivalence");
}

static void
strengthening(void)
{
    for (unsigned bits = 0; bits < 8; ++bits)
        for (unsigned pivot = 0; pivot < 3; ++pivot) {
            Solver *s = solver_new();

            assert(s);
            for (unsigned i = 0; i < 3; ++i)
                assert(solver_new_var(s));
            Lit v[3];

            for (unsigned i = 0; i < 3; ++i)
                v[i] = mkLit(i + 1, (bits >> i) & 1);
            Lit binary[] = {neg(v[pivot]), v[(pivot + 1) % 3]};

            add(s, v, 3);
            add(s, binary, 2);
            uint32_t original = s->num_clauses;
            size_t input = s->input_size;

            solver_congruence(s);
            assert(verify(s, original, input) && s->stats.congruence_strengthened > 0);
            assert(solver_solve(s) == TRUE && solver_check_model(s));
            solver_free(s);
        }
    for (unsigned bits = 0; bits < 4; ++bits) {
        Solver *s = solver_new();

        assert(s);
        assert(solver_new_var(s));
        assert(solver_new_var(s));
        Lit a = mkLit(1, bits & 1), b = mkLit(2, (bits >> 1) & 1), x[] = {a, b}, y[] = {a, neg(b)};

        add(s, x, 2);
        add(s, y, 2);
        uint32_t original = s->num_clauses;
        size_t input = s->input_size;

        solver_congruence(s);
        assert(verify(s, original, input));
        assert(lxor(s->values[var(a)], sign(a)) == TRUE && !s->vars[var(a)].level);
        assert(solver_solve(s) == TRUE && solver_check_model(s));
        solver_free(s);
    }
    puts("PASS: 24 signed ternary/binary strengthening cases and 4 binary-unit cases");
}

int
main(void)
{
    signed_gates();
    cutoffs();
    rup_guard();
    aliases_and_constants();
    conditional_alternatives();
    strengthening();
    return 0;
}
