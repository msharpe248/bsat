# Glucose restart minimum experiment

## Decision

Retain the default minimum of 100 conflicts since the previous restart.
Reducing it to 10 or 1 lost the battleship solve in both repeats and slowed
both other solved inputs. No search-policy change is retained.

## Method

The baseline is the no-random-phase default at `61b39b1`, executable SHA256
`728215427aa6b244d9f6b3cfb284ebbce20313140aa2cf17e4eb527e44677b8f`.
All three profiles use the same preserved executable, differing only in
`--glucose-min-conflicts`. Four development inputs (battleship, belpyramid,
Hamiltonian and multiplier circuits) were selected before timing. Each profile
ran twice per input in seeded serial order, with ten solving CPU seconds and
fifteen wall seconds. No local builds or tests ran during timing.

SAT models and UNSAT proofs were checked independently outside solver timing.
The [complete record](benchmark_results/restart-minimum-targeted-20260907.json)
contains commands, executable/input/checker hashes, statuses, statistics and
process measurements. These reused inputs are a targeted rejection screen,
not a held-out or competition-scale evaluation.

## Results

| Minimum conflicts | Certified runs | Mean wall PAR-2 | Errors |
| --- | ---: | ---: | ---: |
| 100 (default) | 6/8 | 9.9880 | 0 |
| 10 | 4/8 | 17.0058 | 0 |
| 1 | 4/8 | 17.6142 | 0 |

PAR-2 charges unresolved runs twice the wall limit (30 seconds). Process CPU
includes startup, parsing and proof writing; the internal solving limit excludes
parsing. CPU ranges across the two repeats:

| Input | Minimum 100 | Minimum 10 | Minimum 1 |
| --- | ---: | ---: | ---: |
| Battleship | 2.437–2.441 s | UNKNOWN | UNKNOWN |
| Belpyramid | 5.915–5.954 s | 7.488–7.557 s | 9.952–9.967 s |
| Hamiltonian | 0.161–0.163 s | 0.189–0.193 s | 0.270–0.271 s |
| Multiplier circuits | UNKNOWN | UNKNOWN | UNKNOWN |

Battleship's default run uses 107,461 conflicts and 630 restarts. Minimum 10
reaches 474,000 conflicts and 10,453 restarts without a certified answer;
minimum 1 reaches roughly 454,000 conflicts and 25,000 restarts. Belpyramid
illustrates why conflict counts are insufficient: lowering the minimum from
100 to 1 reduces conflicts from 30,460 to 28,928 but raises restarts from 247
to 7,960 and substantially increases CPU time. These observations reject the
simple minimum reduction; they do not rule out other restart policies.

## Retained changes and validation

The regression suite now checks the EMA interval boundary at minimums 1, 10
and 100 across successive restarts, strict threshold equality, absence of LBD
samples, and the no-restarts override. Existing sliding-window and Luby tests
remain in place.

CLI help now correctly groups the minimum and K as shared Glucose parameters.
The [restart and clause-management guide](docs/RESTARTS_AND_CLAUSE_MANAGEMENT.md)
now describes the actual default, EMA update and comparison, window reset,
clause retention and arena collection. Obsolete claims about implemented
restart postponement, universal performance and linked watch lists are removed.

All 19 C test executables passed in both release and ASan/UBSan debug builds,
including the extended restart regression and 800 incremental API solves per
build. All completed benchmark answers passed independent certificate checks.
The search algorithm and its defaults are unchanged.
