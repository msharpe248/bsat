#include "ipasir.h"
#include "solver.h"

typedef struct {
    Solver *core;
    Lit *clause, *assumptions;
    uint32_t clause_count, clause_capacity, assumption_count, assumption_capacity;
    int result;
} Ipasir;

const char *
ipasir_signature(void)
{
    return "bsat-ipasir-1.0";
}

void *
ipasir_init(void)
{
    Ipasir *s = calloc(1, sizeof *s);

    if (!s) return NULL;
    SolverOpts o = default_opts();

    o.reuse_learnts = true;
    o.probing = false;
    s->core = solver_new_with_opts(&o);
    if (!s->core) {
        free(s);
        return NULL;
    }
    return s;
}

void
ipasir_release(void *solver)
{
    Ipasir *s = solver;

    if (!s) return;
    solver_free(s->core);
    free(s->clause);
    free(s->assumptions);
    free(s);
}

static bool
valid(Ipasir *s)
{
    return s && !s->core->error && !s->core->watches->failed;
}

static bool
literal(Ipasir *s, int lit)
{
    int64_t x = lit;
    uint64_t v = x < 0 ? (uint64_t)-x : (uint64_t)x;

    if (!v || v > MAX_VARS) {
        s->core->error = true;
        return false;
    }
    while (s->core->num_vars < v)
        if (!solver_new_var(s->core)) return false;
    return true;
}

static void
append(Ipasir *s, Lit **a, uint32_t *n, uint32_t *cap, Lit lit)
{
    if (*n == ((1u << 28) - 1)) {
        s->core->error = true;
        return;
    }
    if (*n == *cap) {
        uint32_t next = *cap ? *cap * 2 : 8;
        Lit *p = realloc(*a, (size_t)next * sizeof *p);

        if (!p) {
            s->core->error = true;
            return;
        }
        *a = p;
        *cap = next;
    }
    (*a)[(*n)++] = lit;
}

void
ipasir_add(void *solver, int lit)
{
    Ipasir *s = solver;

    if (!valid(s)) return;
    s->result = 0;
    if (lit) {
        if (literal(s, lit))
            append(s, &s->clause, &s->clause_count, &s->clause_capacity, fromDimacs(lit));
    } else {
        solver_add_clause(s->core, s->clause, s->clause_count);
        s->clause_count = 0;
    }
}

void
ipasir_assume(void *solver, int lit)
{
    Ipasir *s = solver;

    if (!valid(s)) return;
    s->result = 0;
    if (literal(s, lit))
        append(s, &s->assumptions, &s->assumption_count, &s->assumption_capacity, fromDimacs(lit));
}

int
ipasir_solve(void *solver)
{
    Ipasir *s = solver;

    if (!s) return 0;
    s->result = 0;
    if (s->clause_count) s->core->error = true;
    if (valid(s)) {
        lbool r = solver_solve_with_assumptions(s->core, s->assumptions, s->assumption_count);

        s->result = r == TRUE ? 10 : r == FALSE ? 20 : 0;
    }
    s->assumption_count = 0;
    return s->result;
}

int
ipasir_val(void *solver, int lit)
{
    Ipasir *s = solver;

    if (!valid(s) || s->result != 10) return 0;
    int64_t x = lit;
    uint64_t v = x < 0 ? (uint64_t)-x : (uint64_t)x;

    if (!v || v > s->core->num_vars) return 0;
    lbool value = solver_model_value(s->core, (Var)v);

    return value == UNDEF ? 0 : ((value == TRUE) == (lit > 0) ? lit : -lit);
}

int
ipasir_failed(void *solver, int lit)
{
    Ipasir *s = solver;

    if (!valid(s) || s->result != 20) return 0;
    uint32_t n;
    const Lit *core = solver_conflict(s->core, &n);

    for (uint32_t i = 0; i < n; ++i)
        if (toDimacs(neg(core[i])) == lit) return 1;
    return 0;
}

void
ipasir_set_terminate(void *solver, void *state, int (*callback)(void *))
{
    Ipasir *s = solver;

    if (s) solver_set_terminate(s->core, state, callback);
}

void
ipasir_set_learn(void *solver, void *state, int max_length, void (*callback)(void *, int *))
{
    Ipasir *s = solver;

    if (!valid(s)) return;
    if (callback && max_length < 0) {
        s->core->error = true;
        return;
    }
    s->core->learn_callback = callback;
    s->core->learn_state = state;
    s->core->learn_max_length = callback ? (uint32_t)max_length : 0;
}
