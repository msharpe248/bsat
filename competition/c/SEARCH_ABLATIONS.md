# Frozen search-policy ablation

This development experiment disables one feature at a time from the existing
`--chrono --congruence --equiv --equiv-budget 100000000 --alternating --vmtf`
profile. It compares the full profile with five omissions: chronological
backtracking, congruence, equivalence rewriting, alternating search, and VMTF.
Other options, the binary and proof format stay fixed.

The policy was saved before starting any timed process in
`benchmark_results/search-ablation-policy-20260908.json`. There are 36 serial runs:
three previously examined inputs (reg-n, Stedman-triples, argumentation), six
profiles and two repetitions in seeded order. Each uses a 40-second solving-thread
CPU limit and 45-second external wall limit. The CPU limit allows unfinished
runs to report their work counters before the external supervisor intervenes.
All profiles emit binary proofs. SAT models require independent validation;
UNSAT requires DRAT-to-LRAT conversion and the pinned cake_lpr checker.

No local build, test, fuzzer or other solver workload runs concurrently with the
timed comparison. Solver wall/CPU time, RSS, work counters, proof-prefix hashes,
validation time and PAR-2 are retained. UNKNOWN prefixes describe unfinished
search and are never accepted certificates. Different search work is expected
when disabling a policy; fewer propagations alone is not a speed or quality win.

This is a bounded development ablation, not a factorial interaction study or
held-out evaluation. Two repetitions are insufficient for fine timing claims.
The primary comparison is independently verified completion within the same
budget. This experiment alone does not authorize promoting a global default;
a proposed winner needs confirmation on additional workloads.

## Results

The complete 36-run grid passes the policy, input/binary-hash, unique-repeat and
acceptance audit. Ten SAT answers are independently verified; the other 26 runs
are UNKNOWN. No UNSAT answer or execution/validation error occurs in this screen.
Reg-n and Stedman-triples remain unresolved under every tested profile.

| Profile | Verified / 6 | Argumentation wall, two runs | Mean PAR-2 |
| --- | ---: | --- | ---: |
| Full | 2 | 31.606 / 31.726 s | 70.555 s |
| Without chronology | 2 | 32.108 / 32.745 s | 70.809 s |
| Without congruence | 2 | 31.413 / 31.448 s | 70.477 s |
| Without equivalence | 2 | 31.731 / 33.245 s | 70.829 s |
| Without alternating search | 0 | UNKNOWN / UNKNOWN | 90.000 s |
| Without VMTF | 2 | 4.871 / 4.221 s | 61.515 s |

Omitting VMTF gives about 7x lower mean wall time on this argumentation input.
Both runs use 92,015 conflicts, versus 1,044,768 with the full profile. This is a
search-history improvement on one workload, not a general propagation speedup.
It uses slightly more peak memory on that input (185.8–186.7 MB versus 180.0 MB).
On reg-n, the same omission increases conflict throughput substantially but
still does not produce an answer. Disabling alternating search loses the two
argumentation completions, so simply disabling every heuristic is not supported.

For this workload, the measured faster existing option combination is:

```sh
bin/bsat --chrono --congruence --equiv --equiv-budget 100000000 --alternating input.cnf
```

VMTF is already opt-in; no global default changes are made. Reg-n/Stedman need
further algorithmic work, and a broader interaction/confirmation experiment is
needed before recommending the faster combination generally. The complete
records are `search-ablation-20260908.json` and
`search-ablation-audit-20260908.json` under `benchmark_results/`.

The binary is pinned before the subsequently identified cancellation-boundary
fix. These runs never use portfolio mode. Later validation of that fix is
recorded separately; the timing numbers are not relabeled as a newer binary.
