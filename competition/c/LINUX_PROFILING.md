# Search work and execution cost

`tests/compare_work.py` freezes a randomized repeated policy and requires equal
search counters and proof bytes across baseline/candidate runs. Its first 36-run
comparison is recorded in `BINARY_PROOFS.md`. CPU per propagation includes input
loading, preprocessing and proof output, so it must not be labeled BCP-only cost.
A search-policy change can alter those counters; use verified solved counts,
PAR-2, conflict/decision counts and memory alongside timing for such changes.

`tests/profile_linux.py` adds actual Linux hardware events. It pins each serial
run to an allowed CPU using taskset, records CPU/kernel/governor/perf identity,
freezes input/binary hashes and policy, and stores raw perf CSV, stdout/stderr,
proofs and results. It measures user-mode cycles, instructions, branches, branch
misses, cache references and cache misses, plus exact solver work. It records
counter running percentages so multiplexed/scaled measurements remain visible.
Unavailable counters, missing/duplicate events, denied PMU access, invalid
statuses, timeouts and failed answer checks fail the collection. UNKNOWN is never
a verified answer. The same solver's repetitions must preserve search/proof traces.
Different solver policies need not perform equal work.

```sh
python3 competition/c/tests/profile_linux.py \
  --solver baseline=/path/to/baseline --solver candidate=/path/to/candidate \
  --cpu 2 --conflicts 100000 --repeats 3 --proof-format binary \
  --options='--chrono --congruence --equiv --equiv-budget 100000000 --alternating --vmtf' \
  --checker /path/to/drat-trim --output /tmp/bsat-perf-run input1.cnf input2.cnf
```

Use an idle dedicated target machine, record its compiler/build flags, and keep
builds/checkers/other benchmarks out of timed runs. Run enough work to amortize
startup. Compare events per propagation only with search/work context; PMU cache
event meanings depend on architecture. Low event running percentages warrant a
repeat with a smaller event set or a compatible dedicated PMU setup before making
fine-grained claims. See the upstream [perf stat manual](https://man7.org/linux/man-pages/man1/perf-stat.1.html)
for counter scaling and CSV semantics. The script deliberately does not change
system security settings or silently substitute simulated counters.

Local validation covers successful CSV parsing and rejection of unavailable,
missing, duplicate, nonfinite and zero-running counters. It is **not** a Linux
hardware measurement. This workspace is macOS ARM64, and an intended Linux server
has not yet been supplied. Native sample evidence remains in
`READINESS_PROFILING.md`; propagation-only PMU attribution and target-server
measurements remain outstanding. Hosted compiler CI is a separate correctness
scope, not a replacement for target-server performance evidence.
