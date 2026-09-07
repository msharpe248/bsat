# Consistent local-search model transfer

## Problem and fix

`local_search_copy_solution` installed values, phases and root levels, but left
old `trail_pos` entries, implicit binary reasons and the propagation cursor in
place. Thus the returned model could be correct while the assignment metadata
did not describe the newly constructed trail. The ordinary solve path validates
the original formula before returning SAT, and subsequent public solve/add APIs
rebuild state; this investigation did not establish an incorrect reported SAT
answer caused by the stale metadata.

The transfer now installs each variable and its trail entry in one pass, clears
both arena and implicit-binary reasons, sets the assignment position, and ends
with `qhead == trail_size`. A successful walk supplies a complete assignment
satisfying the active clauses, so there are no pending implications to process.
The saved target prefix remains invalidated, and the existing final original-
formula model check is retained. Local search remains opt-in.

## Regression evidence

`tests/test_model_transfer.c` fails against `0bef58d` at the propagation-cursor
invariant before the fix. It then passes with the fixed implementation. It
exercises stale metadata across repeated transfers, verifies every value,
position, level and reason, and checks that propagation does no redundant work
after the complete-model transfer. Integration cases require an actual successful
local-search call under both heap and VMTF ordering, then exercise assumption
solves, repeated solves and clause addition through the public solver API.

All 21 C test executables pass in release and ASan/UBSan builds. Focused validation
uses two fixed formulas and 50 random formulas, six local-search configurations,
and seed 20261004: 312 solves per build. It checks truth-table answers, original
SAT models, text/binary RUP and external DRAT proofs. Both builds also pass the
existing 30 deadline cases, with maximum observed CPU overrun 0.000 seconds at
printed precision. Those deadline cases are general regression coverage, not a
measurement of worst-case transfer latency.

The CI formula/certificate step also runs this focused suite in each build mode.
The suite was later expanded for [state-preserving walks](LOCAL_SEARCH_STATE.md);
the counts above describe the transfer milestone. Reproduce the current suite
from the C directory:

```sh
python3 tests/validate_local_search.py --solver bin/bsat --checker /path/to/drat-trim
python3 tests/validate_local_search.py --solver bin/bsat_debug --checker /path/to/drat-trim
```

## Effect on the production goal

This restores consistent metadata after a successful local search and adds direct
coverage of a previously untested state-transfer boundary. The transfer uses one
pass instead of two, while performing additional necessary metadata writes. No
solver-speed improvement is claimed or inferred from that loop change. It does
not close the measured competition-performance gap.
