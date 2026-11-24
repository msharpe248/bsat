# Expanded certified probing validation — 2026-09-08

Keep `BSAT_CERTIFIED_PROBING` opt-in. On four freshly selected larger circuits,
both modes solve 56/64 queries and time out on the same eight. Probing increases
total CPU PAR-2 by 3.4%, while slightly increasing peak journal size. This does
not overturn its strong gen23-specific gain, but does rule out describing that
gain as broadly representative.

## Frozen comparison

The pinned new sources are leader election, cal100, h_b05 and a DSP fast-FIR
circuit. Each source is larger than the original four circuit sources. See
`tests/fixtures/expanded_industrial_aig.json` for the upstream revision, selection
seed/rules, download URLs and content hashes. Histories grow through depths
0,2,8,24 with both output polarities, flags 3 and 7, and two repetitions reversing
mode order. Timed work runs serially without concurrent local builds/tests/fuzz.

Each BSAT and retained CaDiCaL query gets five CPU seconds; fresh Kissat and
certificate checking are validation stages outside query timing. Every SAT
answer is checked against the exact original CNF and independent circuit
simulation. Every conclusive certified UNSAT export passes DRAT-to-LRAT and
CakeML. The largest history has **730,151 variables and 1,960,296 permanent
clauses**, plus one temporary query unit.

The first attempt stopped on a [checker heap/stack limit](CHECKER_RESOURCE_SCALING.md).
It is retained as incomplete evidence. The full campaign was rerun from the
start with explicit 2,048 MB heap / 512 MB stack settings for both modes, with
unchanged query budgets, sources, depths and mode order. All 128 query checks
complete in the rerun, without answer disagreements.

| Metric, across two repetitions | Certified retained (3) | With probing (7) |
|---|---:|---:|
| Queries | 64 | 64 |
| Checked SAT / UNSAT | 36 / 20 | 36 / 20 |
| UNKNOWN | 8 | 8 |
| Total CPU PAR-2 | 92.528 s | 95.637 s |
| Peak journal | 19.77 MB | 20.18 MB |
| Peak solver-owned capacity | 271.17 MB | 261.86 MB |
| Total query export CPU | 4.261 s | 4.259 s |

PAR-2 charges each unfinished query ten seconds. There are no late conclusive
answers and no lost/gained solved cases. Probing is slower in both repetitions:
46.402→47.795 and 46.126→47.842 seconds. Although the 3.4% aggregate increase is
inside the frozen 5% confirmation limit, it is not an improvement. Excluding
the identical timeout penalties, solved-query CPU rises from 12.528 to 15.637
seconds, about 24.8%. Owned capacity is not process RSS and excludes facade,
allocator and validation memory.

| Circuit | Solved queries per mode | CPU PAR-2, control → probing |
|---|---:|---:|
| Leader election | 14/16 | 24.944 → 25.746 s |
| cal100 | 12/16 | 41.613 → 41.361 s |
| h_b05 | 16/16 | 0.248 → 0.293 s |
| DSP fast FIR | 14/16 | 25.723 → 28.238 s |

This is bounded circuit verification, not unbounded safety or customer workload
acceptance. Once inspected, these sources are development evidence for future
tuning. No production default changes follow from this campaign.

## Checkpoints and reproduction

The separate sanitizer campaign runs 32 queries on the four new circuits at
depths 0 and 2 with both flags, checkpointing after each pair. All pass, including
journal reset to zero, invalidation of prior values, subsequent model/simulation
checks and certificate verification. Its CPU values are validation diagnostics,
not performance scores; that smoke run overlapped an independent checker-resource
reproduction.

The serial release checkpoint campaign adds 64 queries on the original four
circuits through depths 0,2,8,32, reaching 306,901 variables and 760,768 permanent
clauses. Both modes return 16 checked SAT, 14 checked UNSAT and two UNKNOWNs;
the unresolved queries are positive cal3/gen23 at depth 32. All 24 intermediate
checkpoints reset the journal and invalidate prior values, followed by independently
checked queries. Maximum observed checkpoint CPU is 0.0503 seconds. Journals
before these intermediate checkpoints reach 1.10 MB; the final depth-32 queries
grow peak journals to 20.19/21.10 MB and are not followed by another checkpoint
in this campaign. This is not a maximum-size checkpoint latency guarantee.

Checkpoint CPU totals are 0.122/0.124 seconds and export CPU totals are
0.518/0.519 seconds for controls/probing. Query CPU PAR-2 is 21.190/21.174 seconds;
that tiny single-history difference is not evidence of a performance advantage.
The campaign and summary are `deep-probing-checkpoints-20260908.json` and
`deep-probing-checkpoints-summary-20260908.json`; reproduce with the original
circuit manifest, depths `0,2,8,32`, and `--checkpoint-every 2`.

Main evidence: `expanded-certified-probing-20260908.json` and its summary under
`benchmark_results/`. `analyze_expanded_probing.py` checks exact query pairing,
rejects unchecked or duplicate evidence, preserves UNKNOWN/late outcomes and
reports lost solves, CPU penalties, capacity, journals and exports. Its regression
tests cover late/UNKNOWN loss and malformed evidence.

Reproduce with `prepare_industrial_aig.py --manifest
tests/fixtures/expanded_industrial_aig.json`, then `industrial_histories.py` using
the recorded flags/depths, `--repeats 2 --cpu 5 --reference-cpu 5
--reference-wall 30 --fresh-wall 30 --checker-wall 120 --checker-heap-mb 2048
--checker-stack-mb 512`. Supply the pinned reference/checker binaries, an output
path and an explicit `--retain-unverified` directory. Checker resource settings
and harness/input/tool hashes are recorded in the report.
