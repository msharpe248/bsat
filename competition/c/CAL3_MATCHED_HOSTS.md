# Same-input cal3 comparison across hosts — 2026-09-08

Follow-up: [portable reduction ordering](PORTABLE_REDUCTION_ORDER.md) now isolates
the first divergence and improves Linux cal3 to checked UNSAT within the budget.
The measurements below describe the pre-fix baseline.

The hosted Linux comparison confirms that the Mac Mini is not the sole obstacle
behind BSAT's gap. On Linux, CaDiCaL and Kissat prove the exact same query in
about one CPU second while BSAT remains UNKNOWN at sixty. All conclusive answers
are independently verified. This measures host/OS/compiler combinations, not an
isolated causal comparison of CPU architectures.

The pinned query is the depth-16 positive cal3 one-shot snapshot, compressed in
`tests/fixtures/acceptance/cal3-query.cnf.gz`. Both hosts run the solvers serially
with sixty process CPU seconds, ninety wall seconds, binary proofs, two repeats
and external answer checking. Solver time includes parsing/proof production;
checking has a separate 600-second timeout and explicit 2,048/512 MB CakeML
heap/stack settings. Local timing does not overlap local tests/builds.

| Host/run | BSAT | CaDiCaL | Kissat |
|---|---|---|---|
| Fresh macOS ARM64 pair | 2 checked UNSAT, median 21.844 CPU s | 2 checked UNSAT, median 0.620 CPU s | 2 checked UNSAT, median 1.045 CPU s |
| First Linux pair | 2 UNKNOWN at 60 CPU s | 2 checked UNSAT, median 0.981 CPU s | 2 checked UNSAT, median 1.431 CPU s |
| Repeat Linux pair | 2 UNKNOWN at 60 CPU s | 2 checked UNSAT, median 1.016 CPU s | 2 checked UNSAT, median 1.426 CPU s |

Linux uses Ubuntu 24.04 / Linux 6.17 Azure x86-64. BSAT is built with portable
`-O3 -flto`, CaDiCaL at `c60730422e758ef1cebe7aeddf2dda31c996bf04`, and Kissat
at `8af8e56f174b778aef3aa45af9f739b2a5f492c2`. Exact executable and input hashes
are in the reports. The local baseline uses the preserved macOS ARM64 executable
and its existing native optimization flags. They are source-equivalent BSAT
baselines, not byte-identical binaries across machines.

## Search trajectories matter too

At 10,000 conflicts Linux BSAT records 26,704 decisions, 6,832,895 propagations
and 708,798 learned literals. The unchanged local binary records 25,519 decisions,
7,143,016 propagations and 766,096 learned literals. Their proof-prefix hashes
differ, while each host's recorded repetitions are deterministic. Fresh local
fixed-conflict repetitions confirm this. In contrast, CaDiCaL and Kissat each
produce identical proof-prefix hashes across the two hosts at the same stop. Thus the
observed solving-time difference cannot be interpreted as a pure throughput
ratio on an identical search path. These prefixes remain UNKNOWN and are not
accepted UNSAT proofs.

One follow-up worth isolating is tie handling in reduction: BSAT's LBD/activity
comparator returns equality for distinct clauses, leaving their relative ordering
to the platform's `qsort`. Floating-point and compiler differences are other
possibilities. The current evidence does not identify which event first caused
the cross-host divergence. Do not label this a proven qsort bug or a soundness
failure; clause selection can legitimately alter search order.

## Reproduction and evidence

Run `tests/benchmark_cal3_host.py --bsat ... --cadical ... --kissat ... --output ...`.
Add `--conflicts 10000` for the diagnostic or `--controls` for the frozen CaDiCaL
plain/no-OTFS/no-reason-bump controls. The manual workflow is
`.github/workflows/c-cal3-host.yml`.

- [First Linux results](benchmark_results/cal3-linux-host-20260908/results.json),
  [workflow run](https://github.com/msharpe248/bsat/actions/runs/34306477131).
- [Repeated Linux results](benchmark_results/cal3-linux-repeat-20260908/results.json),
  [fixed-conflict results](benchmark_results/cal3-linux-repeat-20260908/fixed/results.json),
  [workflow run](https://github.com/msharpe248/bsat/actions/runs/34306815634).
- [Fresh local fixed-conflict results](benchmark_results/cal3-mac-fixed-20260908/results.json).
- Adjacent files record host metadata and policies. The [local prototype screen](CAL3_REASON_SIDE_EXPERIMENT.md)
  includes the unchanged BSAT and reference binaries as controls.
