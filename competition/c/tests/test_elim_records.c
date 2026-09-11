#include "../include/solver.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool
tail(unsigned mask, unsigned bits)
{
    for (unsigned i = 0; i < 2; ++i, mask /= 3)
        if (mask % 3 && (!!(bits & (1u << i)) == (mask % 3 == 1))) return true;
    return false;
}

static void
add(Solver *s, bool negative, unsigned mask)
{
    Lit c[3] = {mkLit(1, negative)};
    unsigned n = 1;

    for (Var v = 2; v <= 3; ++v, mask /= 3)
        if (mask % 3) c[n++] = mkLit(v, mask % 3 == 2);
    assert(solver_add_clause(s, c, n));
}

static void
word_choice(void)
{
    Solver *s = solver_new();

    assert(s);
    for (unsigned i = 0; i < 8; ++i)
        assert(solver_new_var(s));
    Lit a[] = {mkLit(1, false), mkLit(2, false)}, b[] = {mkLit(1, false), mkLit(3, false)};
    Lit c[] = {mkLit(1, true),  mkLit(4, false), mkLit(5, false),
               mkLit(6, false), mkLit(7, false), mkLit(8, false)};

    assert(solver_add_clause(s, a, 2) && solver_add_clause(s, b, 2) && solver_add_clause(s, c, 6));
    elim_build_occs(s);
    assert(elim_eliminate_var(s, 1));
    // Two short positive parents use fewer words than one long negative parent.
    assert(s->elim->stack[0].clause_size == 8 && s->elim->stack[0].clause[0] == mkLit(1, true));
    solver_free(s);
}

int
main(void)
{
    unsigned checks = 0;

    for (unsigned a = 1; a < 9; ++a)
        for (unsigned b = 1; b < 9; ++b)
            for (unsigned c = 1; c < 9; ++c)
                for (unsigned d = 1; d < 9; ++d) {
                    Solver *s = solver_new();

                    assert(s);
                    for (unsigned i = 0; i < 3; ++i)
                        assert(solver_new_var(s));
                    add(s, false, a);
                    add(s, false, b);
                    add(s, true, c);
                    add(s, true, d);
                    unsigned wp = 0, wn = 0;

                    for (unsigned i = 0; i < 4; ++i) {
                        unsigned n = CLAUSE_SIZE(s->arena, s->clauses[i]);

                        if (i < 2)
                            wp += n + 1;
                        else
                            wn += n + 1;
                        Lit *l = CLAUSE_LITS(s->arena, s->clauses[i]);
                        Lit t = l[0];

                        l[0] = l[1];
                        l[1] = t;
                    }
                    elim_build_occs(s);
                    assert(elim_eliminate_var(s, 1));
                    assert(s->elim->stack_size == 1);
                    ElimEntry *e = &s->elim->stack[0];

                    assert(e->clause_size == 2 + MIN(wp, wn));
                    assert(e->clause[0] == mkLit(1, wp <= wn) && !e->clause[1]);
                    uint8_t roots[4];

                    memcpy(roots, s->values, sizeof roots);
                    for (unsigned bits = 0; bits < 4; ++bits) {
                        bool expected =
                            (tail(a, bits) && tail(b, bits)) || (tail(c, bits) && tail(d, bits));
                        bool actual = s->result != FALSE;

                        for (Var v = 2; v <= 3; ++v) {
                            lbool val = bits & (1u << (v - 2)) ? TRUE : FALSE;

                            if (roots[v] != UNDEF && roots[v] != val) actual = false;
                            s->values[v] = val;
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
                        ++checks;
                    }
                    for (Var v = 1; v <= 3; ++v)
                        assert(!s->seen[v]);
                    solver_free(s);
                }
    word_choice();
    Solver *pure = solver_new();

    assert(pure);
    for (unsigned i = 0; i < 3; ++i)
        assert(solver_new_var(pure));
    Lit negative[] = {mkLit(1, true), mkLit(2, true), mkLit(3, true)};

    assert(solver_add_clause(pure, negative, 3));
    elim_build_occs(pure);
    assert(elim_eliminate_var(pure, 1));
    assert(pure->elim->stack[0].clause_size == 2 &&
           pure->elim->stack[0].clause[0] == mkLit(1, true));
    for (Var v = 1; v <= 3; ++v)
        pure->values[v] = TRUE;
    elim_extend_model(pure);
    assert(pure->values[1] == FALSE && solver_check_model(pure));
    solver_free(pure);
    puts("PASS: 16384 multi-parent projection/model checks and minimum-word record choice");
    assert(checks == 16384);
}
