# Larger fresh-input baseline

This screen expands the C solver's development evidence beyond the previously
reused short, small-input comparisons. The solver is unchanged from `c15b957`.
The corpus selector now supports an inclusive minimum byte size, with tests
for both size boundaries, history exclusions, unlimited maximum and invalid
ranges. Existing default selection order remains smallest eligible file first.

## Selection and limits

Before running either solver, select eight distinct families with CNF files
between 5,000,000 and 25,000,000 bytes. Exclude filenames and content hashes
appearing in 231 existing JSON reports/manifests, and duplicate selected content.
The committed manifest records those history hashes and the exact input hashes.
This establishes absence from that recorded history, not absence from all prior
human or unrecorded testing. Previously seen families are allowed. This is a
fresh-input **development** screen, not a held-out family evaluation.

| Family | Declared variables | Declared clauses | File bytes |
| --- | ---: | ---: | ---: |
| grs-fp-comm | 90,662 | 262,100 | 5,015,324 |
| scheduling | 7,200 | 372,720 | 5,039,346 |
| oddball-weighing | 10,876 | 239,878 | 5,132,271 |
| ktf | 60,247 | 281,152 | 5,555,501 |
| unknown-cases | 21,888 | 253,852 | 5,613,993 |
| hardware-verification | 17,710 | 304,026 | 5,694,299 |
| hamiltonian-cycle | 29,934 | 247,657 | 5,723,473 |
| multiplier-verification | 50,777 | 304,370 | 5,733,771 |

The selection is deterministic and size-biased; file size is not a measure of
search difficulty. These inputs are larger than the latest 28-input regression
screen, but do not represent the largest instances in the dataset.

Both default solvers receive a 30-second external wall limit, including startup,
parsing and proof output, without a conflict cap or internal solve-time limit.
Run one repetition per input/solver in seeded serial order (seed 20261019).
No builds or validation suites run concurrently with timing. Original SAT models
and UNSAT proofs are independently checked outside solver timing; proof checks
have a separate 120-second limit. UNKNOWN and unverified results receive 60-second
PAR-2 penalties. A single repetition cannot establish precise speed ratios.

## Results

The [raw comparison](benchmark_results/large-fresh-comparison-20260907.json)
completed all 16 runs with zero errors. Every reported SAT model and UNSAT proof
verified. Results at the 30-second wall limit:

| Family | BSAT | Kissat |
| --- | --- | --- |
| grs-fp-comm | UNKNOWN | UNKNOWN |
| scheduling | UNKNOWN | UNKNOWN |
| oddball-weighing | UNKNOWN | UNKNOWN |
| ktf | UNKNOWN | SAT, 20.866 s |
| unknown-cases | UNKNOWN | UNKNOWN |
| hardware-verification | UNKNOWN | UNSAT, 18.083 s |
| hamiltonian-cycle | UNKNOWN | UNKNOWN |
| multiplier-verification | UNSAT, 0.074 s | UNSAT, 3.203 s |

BSAT verifies **1/8**, versus Kissat **3/8**. Mean wall PAR-2 is 52.5093 versus
42.7692 seconds; maximum observed process RSS is 252,788,736 versus 185,368,576
bytes. The result contradicts competition parity on this screen. It supplies no
soundness verdict for UNKNOWN cases; only completed answers have certificates.
No incorrect answer was observed, but the small number of completed answers
provides limited additional soundness coverage.

The surprising multiplier result received a separate
[five-repeat confirmation](benchmark_results/large-fresh-multiplier-confirm-20260907.json)
with the same solver commands and limits, seed 20261020. Both solvers verify
5/5 UNSAT proofs with zero errors. BSAT median wall time is 0.076679 seconds
(range 0.076121–0.133004), versus Kissat 3.194080 seconds
(range 2.629621–3.355782). Median process CPU is 0.054810 versus 3.157756
seconds. The result repeats on this one selected instance; it is not a general
speedup, a solver change, or evidence that BSAT wins the wider corpus. Executable
and checker hashes match the recorded hashes after both completed jobs.

## What this changes

Retain the minimum-size selector and this baseline to stop relying exclusively
on the smallest eligible inputs. Five selector tests and three benchmark-manifest
tests pass; the C solver executable is unchanged, so this milestone does not
claim a solver performance improvement or require another C regression run.

The next search investigation should examine the two new Kissat-only successes:
`ktf/86173292a2c6a506acf27a81db853bb5.cnf` and
`hardware-verification/d672bdbca3b1fb26d585e78add79537e.cnf`.
Collect BSAT counters under an internal limit that permits normal statistics
output, then compare relevant search options before implementing another default
change. External kills in this fair wall-limit baseline may discard buffered
statistics, so absence of a counter is not evidence that no search occurred.
Keep the fast multiplier case as a regression guard. Longer limits, repeat
measurements on the gaps, more families and a genuinely held-out final evaluation
are still needed; this eight-input screen does not close the competition goal.

## Reproduction

```sh
python3 -m unittest discover -s competition/c/tests -p 'test_select_corpus.py'
python3 -m unittest discover -s competition/c/tests -p 'test_benchmark_manifest.py'
python3 competition/c/tests/select_corpus.py \
  --dataset dataset/sat_competition2025 \
  --reports competition/c/benchmark_results \
  --output competition/c/benchmark_results/large-fresh-corpus-20260907.json \
  --families 8 --min-bytes 5000000 --max-bytes 25000000
python3 competition/c/tests/benchmark.py --checker /tmp/bsat-drat-trim \
  --solver 'bsat=competition/c/bin/bsat --proof {proof} {input}' \
  --solver 'kissat=/private/tmp/kissat-8af8e56f174b778aef3aa45af9f739b2a5f492c2/build/kissat {input} {proof}' \
  --timeout 30 --check-timeout 120 --repeats 1 --seed 20261019 \
  --split development \
  --manifest competition/c/benchmark_results/large-fresh-corpus-20260907.json \
  --output competition/c/benchmark_results/large-fresh-comparison-20260907.json
```

For the multiplier confirmation, use the same benchmark command with
`--repeats 5 --seed 20261020`, omit `--manifest`, pass
`dataset/sat_competition2025/multiplier-verification/03e852aa864cfe6eb49c264b462b8157.cnf`
as the input and choose a separate output path.

Reuse the committed manifest to repeat the same inputs. Rerunning selection
against expanded history can intentionally choose different inputs or fail if
there are too few eligible families. Restore the recorded history snapshot to
reproduce selection itself. Binary paths are environment-specific; raw results
pin the executable and checker hashes.
