# Opaque shared-library embedding

Current capability and release status: [PRODUCTION_READINESS.md](PRODUCTION_READINESS.md).
Validation counts and timings below describe this feature's original milestone.

`include/bsat.h` defines ABI version 1 without exposing solver layouts. Build with
`make shared`; run `make embedding-test` for a client linked to that library using
only the public header. Shared exports carry explicit visibility; internal solver
symbols are hidden. Unknown ABI versions/flags are rejected. Linux GCC/Clang and
macOS Clang CI run the client in release and ASan/UBSan builds.

The API supports copied clause/assumption arrays, result-lifetime-checked model
and failed-core queries, pre-input CPU/conflict/decision limits, persistent error
reporting, and optional conservative learned-clause reuse. Clauses introduce
variables; assumptions reference existing ones. It deliberately exposes a small
versioned surface rather than freezing `Solver` or `SolverOpts` layouts.

Each instance owns diagnostics and cancellation state. The CLI alone reads
output-related environment variables and installs SIGUSR1. Callbacks execute on
the solving thread, survive solver replacement, and return UNKNOWN when they
request termination. The application can update its own atomic cancellation flag
from another thread. One handle still requires serialized calls. See
`API_CONTRACT.md` for ownership and cooperative-polling limits.

CPU deadlines/accounting use POSIX thread CPU time. A regression holds one
limited solve inside a sleeping callback while another thread consumes more CPU
than the limit; the limited solve still succeeds. This prevents process CPU from
charging unrelated threads against an instance's budget.

Local validation: all 52 C test executables and 2,016 allocation-failure cases
pass in release and ASan/UBSan. The shared client checks 12,000 queries across four
threads per build, handler preservation, atomic cancellation/retry, invalid input,
empty clauses and independent CPU budgets. Dedicated core tests cancel search,
rebuild/equivalence paths and cached UNSAT, then retry. The release CLI also passes
69 deadline cases, six process-failure cases, and 24 longer structured cases with
independently checked models/proofs. This is targeted concurrency testing, not a
claim of a formal data-race proof or hard real-time cancellation.

Eight fixed-work CLI runs (two inputs, two repetitions, before/after, 20,000
conflicts) preserve selected work counters and binary-proof prefix hashes exactly;
see `benchmark_results/embedding-work-20260908.json`. Short CPU measurements are
noisy (especially station-repacking); no performance improvement is claimed for
this embedding change. macOS shared-symbol inspection exposes only ten `bsat_*`
functions, with no internal solver symbols.

The additive ABI-v1 query controls/statistics and installed IPASIR adapter are
documented in `IPASIR.md`; its newer validation supersedes the original export
count above. Internal solver symbols remain hidden.
