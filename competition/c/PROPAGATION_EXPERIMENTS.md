# Propagation experiments — September 2026

Two propagation changes were implemented and measured against commit
`49aa90a9001df7455d58796b95c63c64eb0d0993`. Neither is retained in the solver.
The surviving change adds 256 deterministic scan-order regression cases.

## Blocker refresh

After a long clause becomes unit, the candidate refreshed the current watch's
blocking literal to the newly implied literal. This follows the general approach
in [MiniSat propagation](https://github.com/niklasso/minisat/blob/master/minisat/core/Solver.cc).
The hypothesis was that later visits would avoid more arena reads.

This did change search behavior. Skipping a satisfied clause also avoids literal
reordering that the original propagation would perform, which can affect subsequent
watch movement and conflict analysis. A blocker update is therefore not necessarily
a change at identical search work, even though it preserves logical correctness.

The baseline verified nine of 84 runs; the candidate verified six. The difference
was the Hamiltonian instance, solved by the baseline in all three repeats but
UNKNOWN at the candidate's conflict limit. No completed answer was reported invalid.
This is a regression on the development sample, so the candidate was discarded.
Its implementation and targeted backtracking test are preserved only as an
[experimental patch](benchmark_results/blocker-refresh-rejected-20260906.patch),
with [raw results](benchmark_results/blocker-refresh-rejected-20260906.json).

## Contiguous circular ranges

The second candidate replaced the circular scan's per-literal wraparound arithmetic
with two contiguous ranges: cursor to end, then index two to cursor. This preserves
the literal inspection sequence, including the existing fallback for invalid cursors.

Eleven recorded counters matched across every baseline/candidate repetition on
all 28 instances, including decisions, propagations, conflicts, learned literals,
minimization inspections, total work, reductions and garbage collections. Both
versions verified nine of 84 runs, with zero reported errors.

For the 25 instances with baseline median CPU time above 0.05 seconds, the geometric
mean candidate/baseline CPU ratio was **0.99965**: approximately 0.03% less time.
Individual changes went in both directions. This does not establish a useful speed
improvement; the implementation was reverted. The three very short instances are
excluded only from this timing aggregate, not from correctness or counter checks.

- [Raw comparison](benchmark_results/circular-ranges-rejected-20260906.json)
- [Derived summary and counter list](benchmark_results/propagation-experiments-summary-20260906.json)
- [Experimental source patch](benchmark_results/circular-ranges-rejected-20260906.patch)

## Method and retained tests

Both comparisons used the existing 16-instance minimization sample plus the
12-instance larger sample, three repetitions per version, serial runs and seeded
ordering. Each process had a 10000-conflict cap, a 20-second CPU limit and a
25-second wall limit. No builds or test suites ran concurrently with these timing
measurements. Raw JSON contains executable/input hashes and exact commands.
SAT models were independently checked; UNSAT proofs used DRAT-trim. UNKNOWN
results are not certified answers. PAR-2 under this conflict cap is a diagnostic,
not a competition score. The samples share development families and provide no
held-out competition-performance evidence.

The retained `circular_scan_order` regression checks both circular and linear modes,
eight cursor settings and all sixteen patterns of available tail literals on a
six-literal clause. A separate modulo-based oracle computes the expected first
replacement and exact inspection count. Cases with no replacement must propagate
the remaining watched literal with the correct reason. These tests pass for the
original implementation as well as the rejected range implementation.

After restoring the original solver, release and ASan/UBSan unit suites both
passed all nine test executables, including the 256 new scan-order cases.
The earlier milestone's 3842 release and 3842 sanitizer formula-validation results
apply to that unchanged solver source; they are not new validation runs of these
experimental candidates. The candidates passed unit tests and benchmark certificate
checks, which are narrower evidence than exhaustive solver validation.
