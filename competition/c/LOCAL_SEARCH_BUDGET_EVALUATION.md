# Broader local-search flip-budget evaluation

## Question and method

The four-target study in [LOCAL_SEARCH_BUDGETS.md](LOCAL_SEARCH_BUDGETS.md) found
that 10,000 flips retained the early battleship local-search win seen at 100,000
flips, while reducing wasted work on belpyramid. That selected sample did not
establish a better default. This evaluation compares those budgets on the fixed
44-input development corpus after the fresh-deadline checkpoint fix.

Both profiles use the same `bf6dfc7` binary, VMTF, trail reuse, local search every
5,000 conflicts, and noise 0.5. Limits are ten solving CPU seconds and fifteen
wall seconds, with one run per profile/input. Jobs run serially, separately from
builds and validation. The harness verifies SAT models against original clauses
and UNSAT proofs with an independent checker, outside timing. UNKNOWN is an
unfinished attempt, not an incorrect answer.

The [corpus manifest](benchmark_results/no-random-corpus-20260907.json) fixes all
44 inputs and hashes. This is a development sample, not a held-out competition
ranking. These profiles also use VMTF; their comparison alone cannot justify a
new default for the heap-based solver. The
[raw comparison](benchmark_results/walk-budget-broad-20260907.json) records each
command, executable and input hashes, resource usage, and certificate outcome.

## Results and decision

| Budget | Verified solves | Mean wall PAR-2 | Errors |
| ---: | ---: | ---: | ---: |
| 10,000 flips | 8/44 | 24.6735 s | 0 |
| 100,000 flips | 8/44 | 24.7402 s | 0 |

The solved sets are identical: no gains and no losses. Both profiles invoke local
search on 41 of the 44 inputs. Battleship `ed6d842f` is the only input with a
local-search win in either profile. The PAR-2 difference is about 0.27%, too small
and based on too few repetitions to establish a reliable general speedup.

The nontrivial inputs solved by both profiles have these process CPU times:

| Input | 10,000 flips | 100,000 flips |
| --- | ---: | ---: |
| Belpyramid `5831f356` | 3.211929 s | 4.976875 s |
| Hamiltonian `a497d784` | 0.594520 s | 0.715343 s |
| Hamiltonian `6e139783` | 0.212664 s | 0.267210 s |
| Multiplier `90bec6dc` | 0.635662 s | 0.882340 s |
| Battleship `ed6d842f` | 0.123887 s | 0.125104 s |

This supports the earlier observation that the lower budget reduces unproductive
walk time on some solved inputs while retaining the battleship win. It does not
recover ensemble-computation `67aa1a25`, and most of the corpus still times out.
Keep local search off by default and retain its existing opt-in default budget
of 100,000. The 10,000-flip VMTF profile is a measured experimental choice, not a
competition-performance claim or a demonstrated new default for heap search:

```sh
competition/c/bin/bsat --vmtf --reuse-trail --local-search --ls-max-flips 10000 input.cnf
```

## Existing work

Kissat's [local-search implementation](https://github.com/arminbiere/kissat/blob/master/src/walk.c)
sets an effort limit on accumulated `walk_steps`, and counts work inside its
literal-scoring and clause-update loops. Thus its budget is not just a count of
flips. This suggests a separate direction for BSAT: account for variable flip
costs when allocating local-search effort. It does not establish a suitable BSAT
threshold, and no such policy is introduced by this experiment.

## Instrumentation

The C solver now reports `Local search flips` beside local-search calls and wins.
This reads the existing cumulative state counter, reports zero if no state was
created, and adds no work to the flip loop. The counter belongs to the current
solve; the existing API rebuild discards the previous local-search state. This
statistics-only addition is made after the timed comparison and does not change
its budget or heuristic. The recorded timings refer to the hashed `bf6dfc7`
binary, rather than claiming to measure this added output line.

All 25 C test executables pass in release and ASan/UBSan builds after the
statistics addition. Four smoke runs independently verify models in both builds,
checking zero flips without a walk and positive flips within the aggregate call
budget on battleship. No new solver or local-search algorithm is introduced by
the instrumentation.
