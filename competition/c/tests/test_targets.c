#include "../include/solver.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void decision(Solver *s, Lit lit) {
    Var v = var(lit);assert(s->values[v] == UNDEF);
    s->trail_lims[++s->decision_level] = s->trail_size;
    s->values[v] = sign(lit) ? FALSE : TRUE;
    s->vars[v].level = s->decision_level;
    s->vars[v].reason = INVALID_CLAUSE;
    s->vars[v].trail_pos = s->trail_size;
    s->trail[s->trail_size++] = (Trail){lit, s->decision_level};
}

static void compare_reference(Solver *s, lbool *reference, uint32_t *best) {
    if (s->trail_size > *best) {
        memset(reference, 0, (s->num_vars+1) * sizeof *reference);
        for (uint32_t i = 0; i < s->trail_size; ++i) {
            Lit lit = s->trail[i].lit;
            reference[var(lit)] = sign(lit) ? FALSE : TRUE;
        }
        *best = s->trail_size;
    }
    solver_maybe_save_best_phases(s);
    assert(s->rephase.best_trail_size == *best);
    assert(!memcmp(reference, s->rephase.best_phase, (s->num_vars+1) * sizeof *reference));
}

static void branching(void) {
    Solver *s = solver_new();assert(s);
    for (Var v = 1; v <= 12; ++v) assert(solver_new_var(s) == v);
    lbool reference[13] = {0};uint32_t best = 0;
    decision(s, mkLit(1, false));compare_reference(s, reference, &best);
    decision(s, mkLit(2, true));compare_reference(s, reference, &best);
    decision(s, mkLit(3, false));compare_reference(s, reference, &best);
    assert(s->stats.target_copied == 3 && s->stats.target_cleared == 13);
    // Backtrack into the saved prefix; no new record means the target stays.
    solver_backtrack(s, 1);assert(!s->rephase.best_prefix_valid);
    compare_reference(s, reference, &best);
    decision(s, mkLit(4, true));compare_reference(s, reference, &best);
    decision(s, mkLit(5, true));compare_reference(s, reference, &best);
    decision(s, mkLit(6, false));compare_reference(s, reference, &best);
    assert(s->rephase.best_phase[2] == UNDEF && s->rephase.best_phase[3] == UNDEF);
    assert(s->stats.target_copied == 7 && s->stats.target_cleared == 26);
    // Temporary assignments beyond the saved prefix need not invalidate it.
    decision(s, mkLit(7, false));solver_backtrack(s, 4);
    assert(s->rephase.best_prefix_valid);
    decision(s, mkLit(8, true));compare_reference(s, reference, &best);
    assert(s->stats.target_copied == 8 && s->stats.target_cleared == 26);
    // Restart with changed signs, then exceed the previous record.
    solver_backtrack(s, 0);
    for (Var v = 1; v <= 6; ++v) {
        decision(s, mkLit(v, v % 2));compare_reference(s, reference, &best);
    }
    assert(s->rephase.best_phase[8] == UNDEF);
    solver_free(s);
}

static void growth(void) {
    Solver *s = solver_new();assert(s);
    assert(solver_new_var(s) == 1);
    decision(s, mkLit(1, false));solver_maybe_save_best_phases(s);
    Var last = s->var_capacity + 1;
    for (Var v = 2; v <= last; ++v) {
        assert(solver_new_var(s) == v);
        assert(s->rephase.best_phase[v] == UNDEF);
    }
    assert(s->rephase.best_prefix_valid);
    decision(s, mkLit(last, true));solver_maybe_save_best_phases(s);
    assert(s->rephase.best_phase[1] == TRUE && s->rephase.best_phase[last] == FALSE);
    assert(s->stats.target_copied == 2 && s->stats.target_cleared == 2);
    solver_free(s);
}

static void linear_extension(void) {
    const Var n = 20000;
    Solver *s = solver_new();assert(s);
    for (Var v = 1; v <= n; ++v) assert(solver_new_var(s) == v);
    for (Var v = 1; v <= n; ++v) {
        decision(s, mkLit(v, v % 2));solver_maybe_save_best_phases(s);
    }
    assert(s->stats.target_copied == n && s->stats.target_cleared == n+1);
    for (Var v = 1; v <= n; ++v)
        assert(s->rephase.best_phase[v] == (v % 2 ? FALSE : TRUE));
    solver_backtrack(s, 0);
    assert(solver_solve(s) == TRUE && solver_check_model(s));
    assert(solver_new_var(s) == n+1); // API rebuild discards the old target cache.
    assert(!s->rephase.best_prefix_valid && s->rephase.best_trail_size == 0);
    assert(s->rephase.best_phase[n+1] == UNDEF);
    solver_free(s);
}

int main(void) {
    branching();growth();linear_extension();
    puts("PASS: exact target phases across extensions, changed branches, restarts and API rebuilds");
    return 0;
}
