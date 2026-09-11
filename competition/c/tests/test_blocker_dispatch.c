/* Mixed-watch truth-table oracle: representation must not change propagation. */
#include "../include/solver.h"
#include <assert.h>

static void
assigned(Solver *s, Lit lit)
{
    Var v = var(lit);

    s->values[v] = sign(lit) ? FALSE : TRUE;
    s->vars[v].level = 1;
    s->vars[v].trail_pos = s->trail_size;
    s->trail[s->trail_size++] = (Trail){lit};
}

int
main(void)
{
    const unsigned orders[][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    unsigned cases = 0;

    for (unsigned order = 0; order < 6; ++order)
        for (unsigned signs = 0; signs < 16; ++signs)
            for (unsigned tail_sign = 0; tail_sign < 2; ++tail_sign)
                for (unsigned phases = 0; phases < 2; ++phases)
                    for (unsigned states = 0; states < 27; ++states) {
                        Solver *s = solver_new();

                        assert(s);
                        for (unsigned v = 1; v <= 7; ++v)
                            assert(solver_new_var(s) == v);
                        s->opts.phase_saving = phases;
                        s->decision_level = 1;
                        s->trail_lims[1] = 0;
                        Lit p = mkLit(1, signs & 1u), q[3], tail[3];
                        CRef cr[3];
                        unsigned state[3], code = states;

                        for (unsigned i = 0; i < 3; ++i) {
                            state[i] = code % 3;
                            code /= 3; // 0 unassigned, 1 false, 2 true (literal truth)
                            q[i] = mkLit(i + 2, (signs >> (i + 1)) & 1u);
                            tail[i] = mkLit(i + 5, tail_sign);
                            Lit lits[] = {p, q[i], tail[i]};

                            cr[i] = arena_alloc(s->arena, lits, i == 2 ? 3 : 2, true);
                            assert(cr[i] != INVALID_CLAUSE);
                            if (state[i]) assigned(s, state[i] == 2 ? q[i] : neg(q[i]));
                            assigned(s, neg(tail[i]));
                        }
                        for (unsigned i = 0; i < 3; ++i) {
                            unsigned type = orders[order][i];
                            CRef ref = type == 0   ? INVALID_CLAUSE
                                       : type == 1 ? arena_binary_watch_ref(cr[type])
                                                   : cr[type];

                            watch_add(s->watches, p, ref, q[type]);
                            watch_add(s->watches, q[type], ref, p);
                        }
                        assert(!s->watches->failed);
                        s->qhead = s->trail_size;
                        assigned(s, neg(p));
                        unsigned before = s->trail_size, added = 0, processed = 0, scans = 0,
                                 skipped = 0;
                        int conflict = -1;
                        bool implied[3] = {false, false, false};

                        for (unsigned at = 0; at < 3; ++at) {
                            unsigned type = orders[order][at];

                            ++processed;
                            if (state[type] == 2) {
                                skipped += type == 2;
                                continue;
                            }
                            scans += type == 2;
                            if (state[type] == 1) {
                                conflict = (int)type;
                                break;
                            }
                            implied[type] = true;
                            ++added;
                        }
                        CRef actual = solver_propagate(s);
                        CRef expected = conflict < 0    ? INVALID_CLAUSE
                                        : conflict == 0 ? BINARY_CONFLICT
                                                        : cr[conflict];

                        assert(actual == expected && !s->error);
                        assert(s->trail_size == before + added);
                        assert(s->work == processed + scans);
                        assert(s->watches->skipped == skipped);
                        assert(s->stats.propagations == (conflict < 0 ? 1 + added : 1));
                        WatchList *wl = watch_list(s->watches, p);

                        assert(wl->size == 3);
                        unsigned next = before;

                        for (unsigned at = 0; at < 3; ++at) {
                            unsigned type = orders[order][at];
                            Watch w = wl->watches[at];
                            CRef ref = type == 0   ? INVALID_CLAUSE
                                       : type == 1 ? arena_binary_watch_ref(cr[type])
                                                   : cr[type];

                            assert(w.cref == ref && w.blocker == q[type]);
                            Var v = var(q[type]);

                            if (implied[type]) {
                                assert(s->values[v] == (sign(q[type]) ? FALSE : TRUE));
                                assert(s->trail[next].lit == q[type] &&
                                       s->vars[v].trail_pos == next);
                                ++next;
                                assert(s->vars[v].level == 1);
                                assert(s->vars[v].reason ==
                                       (type == 0 ? INVALID_CLAUSE : cr[type]));
                                if (type == 0)
                                    assert(s->binary_reasons[v] == p);
                                else
                                    assert(CLAUSE_LITS(s->arena, cr[type])[0] == q[type]);
                                if (phases) assert(s->vars[v].polarity == !sign(q[type]));
                            } else {
                                lbool value = state[type] == 0                       ? UNDEF
                                              : (state[type] == 2) == !sign(q[type]) ? TRUE
                                                                                     : FALSE;

                                assert(s->values[v] == value);
                            }
                        }
                        if (conflict == 0)
                            assert(s->binary_conflict_lits[0] == p &&
                                   s->binary_conflict_lits[1] == q[0]);
                        solver_free(s);
                        ++cases;
                    }
    assert(cases == 10368);
    puts("PASS: 10368 mixed implicit/tagged/long watch truth, order, reason and phase cases");
}
