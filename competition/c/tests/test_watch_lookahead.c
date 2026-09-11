#include "../include/solver.h"
#include <assert.h>
#include <stdio.h>

/* Mix all watch encodings around short-list/read-ahead boundaries. */
int
main(void)
{
    unsigned checks = 0;

    for (unsigned count = 0; count <= 16; ++count)
        for (unsigned rotation = 0; rotation < 3; ++rotation)
            for (unsigned conflict = 0; conflict <= count; ++conflict) {
                Solver *s = solver_new();

                assert(s);
                for (unsigned v = 0; v < 2 * count + 1; ++v)
                    assert(solver_new_var(s));
                CRef refs[16];
                unsigned kinds[16];

                for (unsigned i = 0; i < count; ++i) {
                    kinds[i] = (i + rotation) % 3;
                    Lit lits[] = {mkLit(1, false), mkLit(2 + i, false),
                                  mkLit(2 + count + i, false)};

                    refs[i] = arena_alloc(s->arena, lits, kinds[i] == 2 ? 3 : 2, kinds[i] == 1);
                    assert(refs[i] != INVALID_CLAUSE);
                    CRef w = kinds[i] == 0   ? INVALID_CLAUSE
                             : kinds[i] == 1 ? arena_binary_watch_ref(refs[i])
                                             : refs[i];

                    watch_add(s->watches, lits[0], w, lits[1]);
                    watch_add(s->watches, lits[1], w, lits[0]);
                    if (kinds[i] == 2) {
                        s->values[2 + count + i] = FALSE;
                        s->trail[s->trail_size++] = (Trail){neg(lits[2])};
                    }
                }
                if (conflict < count) {
                    s->values[2 + conflict] = FALSE;
                    s->trail[s->trail_size++] = (Trail){mkLit(2 + conflict, true)};
                }
                s->qhead = s->trail_size;
                s->values[1] = FALSE;
                s->trail[s->trail_size++] = (Trail){mkLit(1, true)};
                CRef result = solver_propagate(s);

                if (conflict == count)
                    assert(result == INVALID_CLAUSE);
                else
                    assert(result == (kinds[conflict] == 0 ? BINARY_CONFLICT : refs[conflict]));
                for (unsigned i = 0; i < conflict; ++i) {
                    assert(s->values[2 + i] == TRUE);
                    assert(s->vars[2 + i].reason == (kinds[i] == 0 ? INVALID_CLAUSE : refs[i]));
                }
                assert(watch_list(s->watches, mkLit(1, false))->size == count);
                solver_free(s);
                ++checks;
            }
    printf("PASS: %u mixed-watch boundary, unit and conflict cases\n", checks);
}
