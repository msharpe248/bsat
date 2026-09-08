# Service resource controls

`bsat_set_service_limits(s, wall_seconds, owned_bytes)` sets cooperative limits
for subsequent queries and checkpoints. Zero disables either limit. The wall
clock is monotonic and begins at public-call entry, including assumption
translation. `bsat_service_limit_hit` distinguishes wall and capacity exhaustion
from ordinary cancellation and existing CPU/conflict/decision budgets. Setting
limits preserves the latest answer; starting a new query invalidates it.

The capacity ceiling checks the core's owned-capacity estimate at input/query
boundaries, variable-capacity growth, and periodic solve callbacks. It excludes
temporary allocations, overlap while rebuilding, facade buffers, libc overhead
and RSS. Polling may overshoot. Wall deadlines are also cooperative; a long
uninterruptible operation may return late. These are explicitly not hard memory
or scheduling guarantees. Disabled limits do not scan capacity or read wall time.

Query exhaustion returns UNKNOWN, hides model/proof exports, and permits retry
after raising the limit. Interrupted retained state takes the existing rebuild
path. Input exhaustion sets persistent error because an input operation may
already have changed state; discard that handle. CPU limits, callbacks and the
independent journal ceiling compose with these limits. A failed checkpoint never
exposes an answer; a late deadline may be noticed after its rebuild committed.

For hard isolation, `tests/benchmark.py` now accepts `--address-space-limit BYTES`
(Linux RLIMIT_AS) and `--file-size-limit BYTES` (RLIMIT_FSIZE). They are applied
only to the isolated solver child and inherited descendants, never to the caller
or certificate checker. The existing external wall timeout kills the solver
process group; results record the actual exit code and whether that timeout fired.
File limits are per file, not a total disk quota. Address-space limits constrain
virtual mappings, not RSS. Use deployment-specific cgroups/filesystem quotas if
aggregate physical memory or total storage isolation is required.
The process-limit semantics follow the [Linux getrlimit manual](https://man7.org/linux/man-pages/man2/getrlimit.2.html).

Resource termination and crashes cannot count as solved. A conclusive exit must
also have the matching single status marker before independent model/proof
validation. Checker time remains separate from solver time. Linux CI exercises
actual allocation denial; macOS deliberately rejects requested address-space
limits rather than pretending to enforce them.

Local validation: release and ASan/UBSan suites, embedding and installed-package
clients pass. Tests include capacity exhaustion after learning begins, raising
limits and retrying, composed user callbacks, and checkpoint/input boundaries.
All 53 retained certificates per build pass after a wall-cancel/retry prefix.
Public API fuzzing completes 43,895 runs in 31 seconds without a finding. Python
worker, artifact and manifest checks pass; the Linux-only allocation test is
explicitly skipped locally and included in Linux CI.
