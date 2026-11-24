# Industrial search diagnosis

The frozen 8-CPU-second intrusive-accounting experiment compares the audited
industrial control with VMTF, no probing, and no chronological backtracking on
the previously unsolved random-CSP and planning inputs. All eight runs remain
UNKNOWN. Control spends 4.57/8 CPU seconds in propagation on random CSP and
5.43/8 on planning. Preprocessing is only 0.002/0.093 seconds. These inclusive
timings overlap and instrumentation affects throughput; they are not scores.

A separate uninstrumented 12-CPU-second experiment tests growing reductions,
disabling rephasing, and local search against control on those two inputs plus
one additional input from each family. All sixteen runs are retained. Disabling
rephasing solves the original random-CSP SAT instance in 9.607 wall seconds;
its model passes independent validation. The other fifteen trials remain UNKNOWN,
including both planning inputs. Growing reduction intervals increase maximum
RSS in this sample without increasing solved count. No default changes follow.

The phase result is consistent with a concrete policy difference worth isolating:
BSAT periodically restores its all-time best target; it does not reset the best
trail record at rephase. Kissat uses multiple phase sources and resets records
as part of its schedule ([primary implementation](https://github.com/arminbiere/kissat/blob/master/src/rephase.c),
reviewed 2026-09-08). This is a hypothesis about the performance mechanism, not
proof of repeated identical search or a reason to transplant a whole schedule.

Reproduce with `tests/search_diagnosis.py --help`. Reports and frozen input/options
policies are `benchmark_results/search-resource-{diagnosis,maintenance}-20260909*`.
The known-winning option is `--no-rephase` on the explicit industrial control
profile. Further confirmation is retained separately in
`search-resource-confirm-20260909.json`. These development experiments do not
establish a general speedup and do not close the planning gap. They narrow the
next algorithm work to phase policy and propagation/search behavior rather than
more preprocessing. No solver answers are accepted without independent checks.

Both independent repetition runs of the selected option finish SAT with checked
models (7.933 and 9.918 wall seconds), for three checked successes including the
selection run. The original audited control remained UNKNOWN at 60 CPU seconds.
The run suffix is an experiment identifier; execution occurred September 8.
