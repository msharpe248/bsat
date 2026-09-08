# Planning propagation diagnosis

New `MODE=diagnostic` builds `bin/bsat_diagnostic`; add `--accounting` to collect
binary/long-watch visits, blocker/first-literal hits, replacement scans and moves,
unit/conflict counts, scan-length bands, learned-reason LBD/use counts, reduction
candidates and clauses deleted with zero analysis activity. Detailed counter code
is compiled out of normal release/debug builds. Inclusive CPU phases remain
intrusive and overlapping. Counters accumulate with the existing accounting
lifecycle; these CLI measurements each use a fresh process.

A real statistics bug was fixed: `WatchManager.visits` counted propagated literals
but its documented meaning and skip-rate denominator are examined watches. It now
increments for each examined watch, including blocker hits. Unexamined suffixes
at conflict/cancellation are not counted. The regression exercises 20 blocker hits
from a single propagated literal and requires 20 visits and exactly 100%, rather
than the previous misleading 2,000%. Separate tests cover counter gating, units,
scans and conservation on completed propagation. Release, diagnostic and ASan/UBSan
unit suites pass, as do release/debug shared-library embedding tests. CI includes
the diagnostic suite. This fixes diagnostic accuracy, not SAT answer semantics.

Eight serial diagnostic trials: two known planning inputs, four profiles, one
repetition, eight solver CPU seconds with a 13-second external guard. All are
UNKNOWN; no answer or performance-ranking claim follows. No concurrent local
builds/tests/fuzzing. Control measurements:

| Input prefix | Propagation CPU / search CPU | Blocker hits / long visits | Scans in clauses of 9+ literals | Replacement scans / conflict |
|---|---:|---:|---:|---:|
| 21b173 | 4.962 / 8.000 s | 60.5% | 81.8% | 1043 |
| 16c999 | 5.527 / 8.000 s | 47.9% | 69.7% | 2245 |

Disabling circular search increases scans/conflict from 2,245 to 6,202 on 16c999,
and from 1,043 to 1,974 on 21b173. Dynamic-LBD and used-clause protection do not
produce a solve in this screen. Mean LBD of learned reasons used in analysis is
9.4 and 7.1 for control. About 9.9% and 11.6% of deleted learned clauses have zero
analysis activity; this is not a clause-lifetime distribution and does not mean
those clauses never propagated. No per-clause timestamp storage was added.

The evidence points at repeated scanning of long clauses. A wrapping-cursor
prototype preserves literal order while replacing per-iteration index arithmetic.
Six paired, serial, uninstrumented 100,000-conflict trials have identical results,
core counters and binary proof prefixes. Median planning time changes from
4.654 to 4.709 seconds (1.2% slower); CSP changes from 0.572 to 0.561 seconds
(1.9% faster). These small mixed differences do not justify a planning optimization.
The prototype is archived, not enabled. Production keeps the original search loop.
The separate 1,000-conflict pre-batch/current trace check also passes on both inputs.
UNKNOWN-prefix equality is trace evidence, not an UNSAT certificate.

Native macOS sampling was attempted and refused by the OS (`sample` exit 255).
The command, solver/input hashes, diagnostic and solver output are recorded in
`benchmark_results/planning-sample-20260908/`. There is no successful native
stack profile or PMU measurement. Detailed software counters support the findings;
we do not substitute them for hardware counters or claim a flame graph exists.

Decision: retain circular scanning and existing learned-clause defaults; ship the
correct skip-rate denominator and an isolated diagnostic build. Planning remains
an algorithmic gap. The proposed simple scan-loop optimization was evaluated and
rejected for lack of a consistent gain. Further work needs a stronger long-clause
or search-policy design, rather than promoting these inconclusive variants.

Reproduce diagnostics with `tests/benchmark_search_policy.py --suite propagation
--accounting --solver bin/bsat_diagnostic`; exact options/hashes are in
`benchmark_results/propagation-diagnosis-20260908.policy.json`. Use
`tests/compare_search_traces.py --conflicts 100000 --repeats 3` to replay the cursor
comparison after applying `benchmark_results/cursor-experimental-20260908.patch`.
The cursor patch applies to this milestone's source. Retain an unpatched binary
before rebuilding. Raw results are in `propagation-*` and `cursor-fixed-work-*`.
