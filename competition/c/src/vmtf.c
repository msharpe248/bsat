#include "../include/solver.h"
#include "../include/elim.h"
#include <stdlib.h>

static void front(Solver *s, Var v) {
    if (s->vmtf.head == v) return;
    VmtfNode *nodes = s->vmtf.nodes;
    /* Preserve relative age if the monotonic timestamp wraps. */
    if (s->vmtf.stamp == UINT64_MAX) {
        uint64_t stamp = 0;
        for (Var at = s->vmtf.tail; at; at = nodes[at].prev)
            nodes[at].stamp = ++stamp;
        s->vmtf.stamp = stamp;
    }
    if (nodes[v].stamp) {
        Var prev = nodes[v].prev, next = nodes[v].next;
        if (prev) nodes[prev].next = next;
        if (next) nodes[next].prev = prev;
        else s->vmtf.tail = prev;
    }
    nodes[v].prev = INVALID_VAR;
    nodes[v].next = s->vmtf.head;
    if (s->vmtf.head) nodes[s->vmtf.head].prev = v;
    else s->vmtf.tail = v;
    s->vmtf.head = s->vmtf.search = v;
    nodes[v].stamp = ++s->vmtf.stamp;
}

static bool synchronize(Solver *s) {
    /* Lazy allocation keeps the default heuristic free of queue storage. */
    if (s->error) return false;
    if (s->num_vars > s->vmtf.capacity) {
        size_t count = (size_t)s->var_capacity + 1;
        VmtfNode *nodes = realloc(s->vmtf.nodes, count * sizeof *nodes);
        if (!nodes) { s->error = true;return false; }
        s->vmtf.nodes = nodes;s->vmtf.capacity = s->var_capacity;
    }
    while (s->vmtf.count < s->num_vars) {
        if (!(s->vmtf.count & 127) && solver_budget_exhausted(s)) return false;
        Var v = ++s->vmtf.count;
        s->vmtf.nodes[v] = (VmtfNode){0};
        front(s, v);
    }
    return true;
}

void solver_vmtf_bump(Solver *s, Var v) {
    if (!v || v > s->num_vars) { s->error = true;return; }
    if (synchronize(s)) front(s, v);
}

void solver_vmtf_unassign(Solver *s, Var v) {
    if (!v || v > s->vmtf.count) return;
    Var search = s->vmtf.search;
    /* Restore any newly available variable skipped by an earlier scan. */
    if (!search || s->vmtf.nodes[v].stamp > s->vmtf.nodes[search].stamp)
        s->vmtf.search = v;
}

Var solver_vmtf_pick(Solver *s) {
    if (!synchronize(s)) return INVALID_VAR;
    Var v = s->vmtf.search;
    uint32_t scanned = 0;
    while (v && (s->values[v] != UNDEF || (s->elim && s->elim->eliminated[v]))) {
        if (!(scanned++ & 127) && solver_budget_exhausted(s)) return INVALID_VAR;
        v = s->vmtf.nodes[v].next;
    }
    s->vmtf.search = v;
    return v;
}
