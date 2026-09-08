# Query controls and IPASIR

ABI version 1 adds `bsat_set_query_limits` and `bsat_get_stats` without changing
old signatures or the frozen ABI consumer. The original `bsat_set_limits` keeps
its before-input contract. Query limits can change between solves and preserve
the last model/core. Zero means unlimited. Invalid limits poison the handle.
Statistics are a size-checked version-1 snapshot of the latest solve: result,
conflicts, decisions, propagations, cumulative reused preparations, estimated
owned capacity and thread CPU seconds. Reading them later cannot increase the
recorded CPU time. A cancelled rebuild reports no search work from the old query.

`include/ipasir.h` is installed alongside `bsat.h` and exports the IPASIR 1.0
functions from the same shared library. Add clauses with a zero terminator;
assumptions may introduce new variables and are cleared after every solve,
including UNKNOWN. Add/assume invalidate model/core reads immediately. An empty
clause is supported. An unfinished clause at solve, invalid literal or allocation
failure poisons the adapter and returns UNKNOWN. The implementation uses BSAT's
resource bound `MAX_VARS` (currently 536,870,911), so unsupported larger indices
fail closed instead of overflowing. `INT_MIN` is invalid.

Termination callbacks run on the solving thread and survive rebuilds. Learned
clauses are delivered as borrowed, zero-terminated DIMACS arrays when within the
requested length. They are consequences of the permanent formula, including
when learned under assumptions. Copy them inside the callback if needed. A NULL
callback disables delivery. Calls on one instance must be serialized; neither
callback may reenter that instance. The adapter enables conservative learning
reuse and disables probing; it does not expose experimental preprocessing modes.

Release and ASan/UBSan tests cover 24,576 exact-oracle IPASIR queries per build,
186 callback clauses checked against every model of their permanent formulas,
assumption growth/reset, cancellation/retry, result lifetimes and invalid input.
Allocation injection covers 2,971 failures across 18 paths/formulas, including
adapter buffers and learned-clause export. The installed-header C++ consumer
exercises the added public functions and IPASIR through the shared library; the
old C consumer still builds against its frozen header.

The protocol follows the upstream IPASIR interface:
https://github.com/biotomas/ipasir/blob/master/ipasir.h
