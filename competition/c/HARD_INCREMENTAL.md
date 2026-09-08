# Hard incremental histories

`tests/test_hard_incremental.c` grows four independent histories to 456 variables
each. Every history adds eight separately activated pigeonhole modules (eight
pigeons into seven holes), interleaving conditional UNSAT and base SAT queries.
The four profiles combine chronological backtracking, VMTF and alternating
search. Reduction and restart intervals are deliberately small to exercise
clause deletion and garbage collection under real search.

Each activated module is independently known UNSAT by the pigeonhole principle.
The permanent formula is SAT by setting every selector false; returned SAT
models are checked by scanning the original input literals. The returned
conditional core must be the negation of the sole activating assumption.
This structural oracle does not depend on another solver's answer. Existing
small-history tests additionally enumerate all assignments, and longer CLI
tests independently check proof artifacts.

Each query first permits up to eight 127-conflict slices, then 32,768-conflict
slices. Tiny repeated budgets with aggressive reductions can stall; the test
does not assume convergence under that schedule. Twelve callback cancellations
are interleaved with retries and later clause/variable additions.

Before extending conditional reuse, both release and ASan/UBSan builds pass:
364 non-cancellation queries, including 300 UNKNOWN budget slices, 32 SAT and
32 conditional UNSAT answers. The histories exercise 1,895,189 conflicts,
58,964 reductions and 304 queries observing a nonzero GC counter. The latter
counts queries, not distinct GC events. These counts describe the baseline
policy; retaining more state may legitimately change search work.
