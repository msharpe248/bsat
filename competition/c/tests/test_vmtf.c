#include "../include/solver.h"
#include "../include/dimacs.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>

static void move(Var *order, unsigned n, Var v) {
    unsigned i = 0;
    while (i < n && order[i] != v) ++i;
    assert(i < n);
    for (; i; --i) order[i] = order[i-1];
    order[0] = v;
}

static void compare(Solver *s, const Var *order, unsigned n) {
    Var expected = INVALID_VAR;
    for (unsigned i = 0; i < n; ++i)
        if (s->values[order[i]] == UNDEF) { expected = order[i];break; }
    assert(solver_vmtf_pick(s) == expected);
    assert(s->vmtf.count == n && s->vmtf.head == order[0] && s->vmtf.tail == order[n-1]);
    for (unsigned i = 0; i < n; ++i) {
        VmtfNode node = s->vmtf.nodes[order[i]];
        assert(node.prev == (i ? order[i-1] : INVALID_VAR));
        assert(node.next == (i+1 < n ? order[i+1] : INVALID_VAR));
        if (i) assert(s->vmtf.nodes[order[i-1]].stamp > node.stamp);
    }
}

static void randomized_queue(void) {
    SolverOpts o = default_opts();o.vmtf = true;
    Solver *s = solver_new_with_opts(&o);assert(s && !s->vmtf.nodes);
    unsigned n = 64;Var order[65];uint32_t rng = 20260919;
    for (Var v = 1; v <= n; ++v) { assert(solver_new_var(s) == v);order[n-v] = v; }
    compare(s, order, n);
    for (unsigned step = 0; step < 1000; ++step) {
        rng ^= rng << 13;rng ^= rng >> 17;rng ^= rng << 5;
        Var v = 1 + rng % n;
        if (step == 500) {
            assert(solver_new_var(s) == ++n);
            for (unsigned i = n-1; i; --i) order[i] = order[i-1];
            order[0] = n;
        } else if (step % 3 == 0) {
            solver_vmtf_bump(s, v);move(order, n, v);
        } else if (step % 3 == 1) s->values[v] = TRUE;
        else { s->values[v] = UNDEF;solver_vmtf_unassign(s, v); }
        compare(s, order, n);
    }
    for (Var v = 1; v <= n; ++v) s->values[v] = TRUE;
    assert(solver_vmtf_pick(s) == INVALID_VAR);
    s->values[order[n-1]] = UNDEF;solver_vmtf_unassign(s, order[n-1]);
    compare(s, order, n);
    s->values[order[0]] = UNDEF;solver_vmtf_unassign(s, order[0]);
    compare(s, order, n);
    s->vmtf.stamp = UINT64_MAX;
    Var v = order[n-1];solver_vmtf_bump(s, v);move(order, n, v);
    assert(s->vmtf.stamp == n+1);compare(s, order, n);
    solver_free(s);
}

static void ordered_batch(void) {
    SolverOpts o = default_opts();o.vmtf = true;
    Solver *s = solver_new_with_opts(&o);assert(s);
    enum { N = 257 };
    Var order[N], batch[N], expected[N];uint32_t rng = 20260920;
    for (Var v = 1; v <= N; ++v) { assert(solver_new_var(s) == v);order[N-v] = v; }
    compare(s, order, N);
    for (unsigned round = 0; round < 100; ++round) {
        bool chosen[N+1] = {false};unsigned count = 0, out = 0;
        for (Var v = 1; v <= N; ++v) {
            rng ^= rng << 13;rng ^= rng >> 17;rng ^= rng << 5;
            if (rng & 1) { chosen[v] = true;batch[count++] = v; }
        }
        for (unsigned i = 0; i < N; ++i) if (chosen[order[i]]) expected[out++] = order[i];
        for (unsigned i = 0; i < N; ++i) if (!chosen[order[i]]) expected[out++] = order[i];
        if (round == 50) s->vmtf.stamp = UINT64_MAX;
        solver_vmtf_bump_batch(s, batch, count);
        for (unsigned i = 0; i < N; ++i) order[i] = expected[i];
        compare(s, order, N);
    }
    solver_vmtf_bump_batch(s, NULL, 0);compare(s, order, N);
    solver_free(s);
}

static void integration(void) {
    SolverOpts o = default_opts();o.vmtf = true;o.random_phase = false;o.phase_saving = false;
    Solver *s = solver_new_with_opts(&o);assert(s);
    assert(dimacs_parse_string(s, "p cnf 2 2\n-1 2 0\n-1 -2 0\n") == DIMACS_OK);
    solver_vmtf_bump(s, 1);
    assert(solver_decide(s) && var(s->trail[0].lit) == 1);
    CRef conflict = solver_propagate(s);assert(conflict == BINARY_CONFLICT);
    uint64_t stamp = s->vmtf.stamp;
    Lit learned[2];uint32_t size;Level level;
    solver_analyze(s, conflict, learned, &size, &level);
    assert(s->vmtf.stamp > stamp && size == 1 && learned[0] == mkLit(1, true));
    solver_backtrack(s, 0);
    assert(solver_vmtf_pick(s) != INVALID_VAR);
    assert(solver_solve(s) == TRUE && solver_check_model(s));
    assert(solver_new_var(s) == 3 && s->opts.vmtf && !s->vmtf.nodes);
    assert(solver_solve(s) == TRUE && solver_check_model(s));
    Lit assumption = mkLit(1, false);
    assert(solver_solve_with_assumptions(s, &assumption, 1) == FALSE);
    assumption = neg(assumption);
    assert(solver_solve_with_assumptions(s, &assumption, 1) == TRUE && solver_check_model(s));
    solver_free(s);
    s = solver_new();assert(s);
    assert(solver_new_var(s) == 1 && solver_solve(s) == TRUE && !s->vmtf.nodes);
    solver_free(s);
}

static Solver *pigeonhole(bool weighted) {
    SolverOpts o = default_opts();o.vmtf = true;o.lrb = weighted;
    o.max_conflicts = 200;o.probing = false;
    if (weighted) { o.var_inc = 1e99;o.var_decay = 0.01; }
    Solver *s = solver_new_with_opts(&o);assert(s);
    enum { H = 6, P = 7 };
    for (Var v = 1; v <= H*P; ++v) assert(solver_new_var(s) == v);
    for (unsigned p = 0; p < P; ++p) {
        Lit clause[H];
        for (unsigned h = 0; h < H; ++h) clause[h] = mkLit(p*H+h+1, false);
        assert(solver_add_clause(s, clause, H));
    }
    for (unsigned h = 0; h < H; ++h)
        for (unsigned p = 0; p < P; ++p)
            for (unsigned q = p+1; q < P; ++q) {
                Lit clause[2] = {mkLit(p*H+h+1, true), mkLit(q*H+h+1, true)};
                assert(solver_add_clause(s, clause, 2));
            }
    return s;
}

static void score_independence(void) {
    Solver *a = pigeonhole(false), *b = pigeonhole(true);
    assert(solver_solve(a) == solver_solve(b));
    assert(!a->error && !b->error && a->stats.conflicts > 0);
    assert(a->stats.conflicts == b->stats.conflicts);
    assert(a->stats.decisions == b->stats.decisions);
    assert(a->stats.propagations == b->stats.propagations);
    assert(a->stats.learned_literals == b->stats.learned_literals);
    assert(a->trail_size == b->trail_size);
    for (unsigned i = 0; i < a->trail_size; ++i) assert(a->trail[i].lit == b->trail[i].lit);
    assert(a->vmtf.search == b->vmtf.search);
    for (Var v = a->vmtf.head, w = b->vmtf.head; v || w;) {
        assert(v == w);v = a->vmtf.nodes[v].next;w = b->vmtf.nodes[w].next;
    }
    solver_free(a);solver_free(b);
}

static void allocation_growth(void) {
    SolverOpts o = default_opts();o.vmtf = true;
    Solver *s = solver_new_with_opts(&o);assert(s);
    assert(solver_new_var(s) == 1 && solver_vmtf_pick(s) == 1);
    uint32_t capacity = s->vmtf.capacity;
    for (Var v = 2; v <= capacity+1; ++v) assert(solver_new_var(s) == v);
    assert(solver_vmtf_pick(s) == capacity+1 && s->vmtf.capacity > capacity);
    assert(s->vmtf.tail == 1 && s->vmtf.nodes[1].prev == 2);
    solver_vmtf_bump(s, 1);
    assert(solver_vmtf_pick(s) == 1 && s->vmtf.tail == 2);
    solver_free(s);
}

static void deadline_polls(void) {
    SolverOpts o = default_opts();o.vmtf = true;o.max_time = 0.001;
    Solver *s = solver_new_with_opts(&o);assert(s);
    for (Var v = 1; v <= 1000; ++v) assert(solver_new_var(s) == v);
    s->stats.start_time = solver_cpu_time() - 1;
    assert(solver_vmtf_pick(s) == INVALID_VAR && s->interrupted && !s->error);
    o.max_time = s->opts.max_time = 0;s->interrupted = false;
    assert(solver_vmtf_pick(s) == 1000);
    for (Var v = 1; v <= 1000; ++v) s->values[v] = TRUE;
    s->opts.max_time = 0.001;s->clock_initialized = false;
    s->stats.start_time = solver_cpu_time() - 1;
    assert(solver_vmtf_pick(s) == INVALID_VAR && s->interrupted && !s->error);
    solver_free(s);
    s = solver_new_with_opts(&o);assert(s);
    assert(solver_solve(s) == TRUE && !s->vmtf.nodes);
    solver_free(s);
}

int main(void) {
    randomized_queue();ordered_batch();integration();score_independence();allocation_growth();deadline_polls();
    puts("PASS: VMTF order, assignment cursor, growth, stamp rollover, conflict use and API rebuild");
    return 0;
}
