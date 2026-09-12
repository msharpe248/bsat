/* Independent truth/model checks for complete and interrupted BVE passes. */
#include "../include/solver.h"
#include "../include/dimacs.h"
#include <assert.h>

static uint32_t
random_word(uint32_t *state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

static Solver *
formula(unsigned seed)
{
    Solver *s = solver_new();

    assert(s);
    s->opts.probing = false;
    unsigned n = 3 + seed % 5;
    uint32_t rng = seed + 91991;

    for (unsigned v = 1; v <= n; ++v)
        assert(solver_new_var(s) == v);
    s->opts.elim_max_occ = 1 + seed % 4;
    for (unsigned i = 0; i < 8 + seed % 16; ++i) {
        Lit lits[4];
        unsigned used = 0, z = 2 + random_word(&rng) % MIN(3u, n - 1);

        while (used < z) {
            Var v = 1 + (random_word(&rng) >> 8) % n;
            bool duplicate = false;

            for (unsigned j = 0; j < used; ++j)
                if (var(lits[j]) == v) duplicate = true;
            if (!duplicate) lits[used++] = mkLit(v, (random_word(&rng) >> 12) & 1u);
        }
        assert(solver_add_clause(s, lits, z));
    }
    s->proof_file = tmpfile();
    assert(s->proof_file);
    s->opts.binary_proof = seed & 1u;
    return s;
}

static bool
input_sat(const Solver *s, unsigned mask)
{
    bool satisfied = false;

    for (size_t i = 0; i < s->input_size; ++i) {
        Lit l = s->input[i];

        if (l)
            satisfied |= ((mask >> (var(l) - 1)) & 1u) != sign(l);
        else {
            if (!satisfied) return false;
            satisfied = false;
        }
    }
    return true;
}

static bool
truth(const Solver *s)
{
    for (unsigned mask = 0; mask < (1u << s->num_vars); ++mask)
        if (input_sat(s, mask)) return true;
    return false;
}

static void
residual_models(Solver *s, bool satisfiable)
{
    if (s->result == FALSE) {
        assert(!satisfiable);
        return;
    }
    uint8_t values[8];

    memcpy(values, s->values, s->num_vars + 1);
    bool found = false;

    for (unsigned mask = 0; mask < (1u << s->num_vars); ++mask) {
        bool valid = true;

        for (Var v = 1; v <= s->num_vars; ++v) {
            lbool value = (mask & (1u << (v - 1))) ? TRUE : FALSE;

            if (values[v] != UNDEF && values[v] != value) valid = false;
            s->values[v] = value;
        }
        for (unsigned i = 0; i < s->num_clauses && valid; ++i) {
            CRef cr = s->clauses[i];

            if (clause_deleted(s->arena, cr)) continue;
            bool sat = false;

            for (unsigned j = 0; j < CLAUSE_SIZE(s->arena, cr); ++j) {
                Lit l = CLAUSE_LITS(s->arena, cr)[j];

                sat |= lxor(s->values[var(l)], sign(l)) == TRUE;
            }
            valid = sat;
        }
        if (valid) {
            found = true;
            elim_extend_model(s);
            assert(solver_check_model(s));
        }
    }
    assert(found == satisfiable);
    memcpy(s->values, values, s->num_vars + 1);
}

int
main(void)
{
    for (unsigned seed = 0; seed < 1024; ++seed) {
        Solver *s = formula(seed);
        bool sat = truth(s);
        Lit frozen = mkLit(1, false);

        s->opts.retained_elim = seed >= 512;
        unsigned count =
            s->opts.retained_elim ? elim_preprocess_frozen(s, &frozen, 1) : elim_preprocess(s);

        assert(!s->error);
        assert(count == s->elim->vars_eliminated);
        if (s->opts.retained_elim) assert(!elim_is_eliminated(s, 1));
        residual_models(s, sat);
        lbool result = solver_solve(s);

        assert(result == (sat ? TRUE : FALSE));
        if (sat) assert(solver_check_model(s));
        solver_free(s);
    }
    unsigned cutoffs = 0;

    for (unsigned seed = 0; seed < 6; ++seed)
        for (unsigned limit = 0; limit <= 512; ++limit) {
            Solver *s = formula(seed + 30);
            bool sat = truth(s);

            s->work = 1;
            s->work_limit = 1 + limit;
            s->opts.retained_elim = seed >= 3;
            Lit frozen = mkLit(1, false);

            if (s->opts.retained_elim)
                elim_preprocess_frozen(s, &frozen, 1);
            else
                elim_preprocess(s);
            assert(!s->error);
            for (Var v = 1; v <= s->num_vars; ++v)
                assert(!s->seen[v]);
            s->work_limit = 0;
            lbool result = solver_solve(s);

            assert(result == (sat ? TRUE : FALSE));
            if (sat) assert(solver_check_model(s));
            solver_free(s);
            ++cutoffs;
        }
    const char *gates[] = {"p cnf 7 7\n-1 2 0\n-1 3 0\n1 -2 -3 0\n1 4 0\n1 5 0\n-1 6 0\n-1 7 0\n",
                           "p cnf 7 8\n1 2 3 0\n1 -2 -3 0\n-1 -2 3 0\n-1 2 -3 0\n"
                           "1 4 0\n1 5 0\n-1 6 0\n-1 7 0\n"};

    for (unsigned i = 0; i < 2; ++i) {
        Solver *s = solver_new();
        Lit frozen[6];

        assert(s && dimacs_parse_string(s, gates[i]) == DIMACS_OK);
        s->opts.retained_elim = true;
        s->opts.probing = false;
        for (unsigned j = 0; j < 6; ++j)
            frozen[j] = mkLit(j + 2, false);
        assert(elim_preprocess_frozen(s, frozen, 6) == 1);
        assert(elim_is_eliminated(s, 1));
        for (unsigned j = 2; j <= 7; ++j)
            assert(!elim_is_eliminated(s, j));
        residual_models(s, truth(s));
        solver_free(s);
    }
    printf("PASS: 1024 BVE truth/residual-model cases, AND/XOR gates and %u cutoff/resume cases\n",
           cutoffs);
}
