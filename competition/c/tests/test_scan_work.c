#include "../include/solver.h"
#include <assert.h>
#include <stdio.h>

static Solver *fixture(unsigned n, unsigned available, bool conflict, unsigned begin) {
    Solver *s = solver_new();assert(s);
    Lit *lits = malloc(n * sizeof *lits);assert(lits);
    for (unsigned i = 0; i < n; ++i) { assert(solver_new_var(s));lits[i] = mkLit(i + 1, false); }
    assert(solver_add_clause(s, lits, n));free(lits);
    for (Var v = 2; v <= n; ++v) {
        if ((v == 2 && !conflict) || v == available + 1) continue;
        s->values[v] = FALSE;s->vars[v].trail_pos = s->trail_size;
        s->trail[s->trail_size++] = (Trail){mkLit(v, true)};
    }
    s->qhead = s->trail_size;
    s->values[1] = FALSE;s->vars[1].trail_pos = s->trail_size;
    s->trail[s->trail_size++] = (Trail){mkLit(1, true)};
    CLAUSE_HEADER(s->arena, s->clauses[0])->search = begin;
    return s;
}

int main(void) {
    const unsigned n = 4098, starts[] = {0, 1, 1022, 1023, 1024, 2047};
    const unsigned cursors[] = {2, 33, n - 1}, available[] = {2, 35, n - 1};
    unsigned cases = 0;
    for (unsigned a = 0; a < sizeof starts / sizeof *starts; ++a)
    for (unsigned b = 0; b < sizeof cursors / sizeof *cursors; ++b) {
        uint64_t start = starts[a];unsigned begin = cursors[b];
        for (unsigned outcome = 0; outcome < 5; ++outcome) {
            unsigned found = outcome < 3 ? available[outcome] : 0;
            bool conflict = outcome == 4;
            Solver *s = fixture(n, found, conflict, begin);CRef cr = s->clauses[0];s->work = start;
            unsigned inspected = n - 2;
            if (found) inspected = 1 + (found >= begin ? found - begin : n - begin + found - 2);
            assert(solver_propagate(s) == (conflict ? cr : INVALID_CLAUSE));
            assert(s->work == start + 1 + inspected);
            if (found) {
                assert(CLAUSE_LITS(s->arena, cr)[1] == mkLit(found + 1, false));
                assert(CLAUSE_HEADER(s->arena, cr)->search == found);
            } else if (!conflict) assert(s->values[2] == TRUE && s->vars[2].reason == cr);
            solver_free(s);++cases;
        }
        Solver *s = fixture(n, 0, false, begin);CRef cr = s->clauses[0];
        s->work = start;s->work_limit = start + 17;
        uint64_t stopped = ((s->work_limit + 1023) / 1024) * 1024;
        uint32_t before = s->qhead;
        assert(solver_propagate(s) == INVALID_CLAUSE);
        assert(s->work == stopped && s->qhead == before && s->values[2] == UNDEF);
        assert(watch_list(s->watches, mkLit(1, false))->size == 1);
        s->work_limit = 0;
        assert(solver_propagate(s) == INVALID_CLAUSE);
        assert(s->work == stopped + 1 + n - 2 && s->qhead == s->trail_size);
        assert(s->values[2] == TRUE && s->vars[2].reason == cr);
        assert(!s->error && !s->watches->failed);solver_free(s);++cases;
    }
    assert(cases == 108);
    puts("PASS: 108 scan work cases, circular replacement, unit/conflict and interrupted replay");
}
