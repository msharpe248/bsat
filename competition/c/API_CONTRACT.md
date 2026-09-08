# Supported C embedding contract

BSAT currently supports a synchronous, single-threaded C11/POSIX API. This
contract defines the supported boundary; it does not claim ABI stability,
concurrent embedding. Learned-clause retention is an opt-in capability described below.

## Ownership and construction

Create with `solver_new()` or `solver_new_with_opts(&opts)` and release exactly
once with `solver_free(s)`. `solver_free(NULL)` is valid. A NULL constructor
result means invalid options, allocation failure or failure opening the proof.
Use `default_opts()` as the starting options. Numeric options are copied.
`proof_path` is borrowed: its string must remain valid and unchanged until the
solver is freed, because later solves may reopen the path. The solver owns its
proof stream. Construction truncates the configured proof file; the caller must
ensure that it does not alias an input or any other file that must be preserved.
The CLI performs an input/proof inode check; the C constructor has no input path.

Allocate variables before adding clauses. Variables are numbered from 1;
`solver_new_var` returns 0 on failure. Build literals with `mkLit(variable, sign)`;
`sign=true` means negation. The clause array is copied before return and stays
caller-owned. `(NULL, 0)` adds the empty clause. A NULL array with nonzero size,
zero literal, or out-of-range variable is an API error.

## Results and errors

| Operation/result | Meaning |
| --- | --- |
| `solver_add_clause` returns true | Clause accepted without a detected contradiction |
| `solver_add_clause` returns false, no error | A contradiction was detected; call solve for UNSAT/proof finalization |
| `solver_add_clause` returns false, error | Input operation failed; discard the solver |
| Solve returns `TRUE` | SAT; model checked internally against original input and assumptions |
| Solve returns `FALSE` | UNSAT under the current call's assumptions |
| Solve returns `UNDEF` | No answer: budget exhaustion, unsupported conditional proof, or error |
| `s->error` | Persistent failure; later solves and input mutations cannot recover this instance |

Check every input-operation return value. Watch allocation failures are also
recorded in `s->watches->failed` and promoted to `s->error` by solve. A failed
instance must be freed and rebuilt by the caller. Do not clear error flags or
edit solver internals to retry. Time/decision/conflict limits are unfinished
search, not UNSAT. No numeric accuracy percentage applies to UNKNOWN results.

Read `solver_model_value(s,v)` only after the latest solve returned TRUE, before
the next solve or mutation. Invalid indices or a NULL solver return UNDEF.
Other model reads outside that lifetime are not answer evidence. The failed
assumption clause returned by `solver_conflict` is borrowed, nonminimal, and
contains negated assumptions; copy it before any solve/mutation or free. Base
UNSAT has an empty failed-assumption clause. Internal pointers, reasons and
clause references may move or become invalid during solving and garbage collection.

## Repeated solves and assumptions

Assumptions are caller-owned and consumed synchronously for one call; they do
not become permanent input. Repeated and contradictory assumptions are allowed.
By default, a subsequent solve or added input rebuilds from the original formula,
restoring clauses removed by preprocessing. Set `opts.reuse_learnts=true` before
construction to retain learned clauses, root consequences and activities after
compatible SAT or conflict/decision-limited calls. Temporary assumptions are
removed and root assignments replayed. Proofs, destructive preprocessing, local
search, inprocessing, interrupted propagation and conditional UNSAT use the
rebuild path. See `INCREMENTAL_REUSE.md` for the exact boundary and evidence.
Repeated-solve preparation is charged to the new call's CPU limit. Do not mutate
options or internal storage after construction.

Assumptions plus a configured proof stream return UNDEF because conditional
proofs are unsupported. For an independently checkable certificate, create a
separate solver/input with those assumptions explicitly added as unit clauses.
Repeated proof-producing solves reset the proof file; copy a completed proof
before starting the next call. Successful flushing is checked before a conclusive
solve return; durable storage across power loss is the application's responsibility.

## Threads, signals and cancellation

Serialize all BSAT calls, including calls on different instances. The library
uses process-wide diagnostic flags and installs a SIGUSR1 progress handler when
solving; embedding applications must reserve that signal. It does not restore
the previous handler. There is no supported asynchronous cancellation callback
or concurrent access to a Solver. Do not change its fields from another thread
or a signal handler. Use configured synchronous budgets or process isolation.

The CLI retains normal POSIX SIGINT/SIGTERM termination. A killed process may
leave a partial proof and no status line; consumers must never treat that as a
certificate. Use exit codes 10/20 plus independent model/proof validation for
accepted results. Exit 0 means UNKNOWN, exit 1 an error, and signal termination
means interrupted execution. External supervision can enforce memory/wall limits.

## Executable coverage and remaining scope

`test_api_contract.c` exercises option/clause ownership, repeated assumption and
base solves, failed-core lifetime, added variables/clauses, invalid arguments,
empty clauses and poisoned-instance behavior. `test_regressions.c` additionally
checks API sequences against a small independent truth-table oracle.
`make fault-test` injects allocation failures in isolated core objects;
`check_process_failures.py` checks actual process termination and exhausted proof
files. These tests run in release and sanitizer CI builds.

Concurrent embedding, callbacks, stable ABI and conditional proofs require
separate designs and tests before they can be advertised. Applications
requiring those capabilities cannot yet use this API as their production contract.
