# Learned-clause eligibility and strengthening cost — 2026-09-08

The preceding planning recommendation overlooked the rejected
[hot-clause experiment](HOT_CLAUSE_STRENGTHENING.md). That prototype already
selected frequently scanned learned clauses, limited the candidate count/work,
and failed its fresh holdout. This investigation corrects that omission and
measures whether the existing strengthening routine covers the expensive work.
It adds diagnostics, not a production heuristic or larger production header.

## Eligibility at the point of scan

Eight runs compare release and intrusive diagnostic builds, ordinary solving
and `--inprocess`, on the two development planning inputs. Each stops normally
at 100,000 conflicts; all remain UNKNOWN. The diagnostic control finds:

| Input | Learned replacement scans | Size 3–64 | Size >64 |
|---|---:|---:|---:|
| 16c999 | 90,090,534 | 25,711,048 (28.5%) | 64,379,486 (71.5%) |
| 21b173 | 25,889,873 | 15,263,119 (59.0%) | 10,626,754 (41.0%) |

The existing vivifier ignores clauses longer than 64 literals. Selecting different
eligible clauses therefore cannot directly cover most learned scan work on the
larger development input. Unlock eligibility is measured at each scan, not at a
future root-level maintenance pass. These aggregates include later-deleted
clauses and do not have the survivor bias of a final top-ten snapshot.

## Work and subsequent use

The instrumented `--inprocess` runs record 797 / 607 candidate attempts,
351 / 418 replacements, and 837 / 1,067 removed literals. Their literal-removal
trials consume 7,346,394 / 832,974 work units. Replacement clauses subsequently
receive 575,004 / 48,592 replacement scans during the measured run. These are
observed uses, **not saved scans**: shortened clauses may avoid work, imply units
or change search. The measurements do not identify that counterfactual benefit.
Scan counts include propagation during later simplification trials as well as
ordinary search; subsequent scans alone do not establish productive search use.
Trial work excludes mandatory root closure, allocation and cleanup.

In the uninstrumented release screen, ordinary versus inprocessing CPU is
3.962 versus 4.205 seconds on the larger input and 1.185 versus 1.198 on the
smaller. Propagations rise from 120,495,242 to 129,819,339 and from 14,668,550 to
15,245,521 respectively. These are single diagnostic-screen repetitions, not a
statistically established regression or a promotion gate. No solve is gained.

The diagnostic build adds a lineage word to its already-expanded clause header
and performs extra counters/timers. Its strengthening trajectories differ from
release, so its CPU and aggregate work must not be treated as production scores.
Control release/diagnostic propagation counts and proof-prefix hashes agree on
both inputs. No UNKNOWN proof prefix is accepted as an UNSAT certificate.

## Decision and validation

Keep the production selection policy unchanged. A cheaper heat representation
alone would repeat an already rejected policy while leaving the largest observed
clause-size gap untouched. Evidence does not yet justify raising the size limit
or another selector implementation. A future candidate needs bounded treatment
of costly longer clauses and a measured end-to-end benefit, including protection
against the solved-case losses seen in earlier vivification experiments.

New counters and lineage metadata compile only with `BSAT_SEARCH_DIAGNOSTICS`.
All 65 C tests pass in release, ASan/UBSan debug and diagnostic builds. The
4,704-case independent truth-table/RUP vivification suite exercises the new
replacement accounting. Aggregate partition/bounds checks pass on all diagnostic
rows. CI already runs the detailed diagnostic build and its tests.

Raw commands, binary/input hashes and results are recorded in
`benchmark_results/learned-clause-eligibility-20260908.json`. Reproduce using its
four solver templates, `--timeout 60 --repeats 1 --seed 2026090865`, and the
recorded two input paths. The bounds are investigation limits, not service SLOs.
