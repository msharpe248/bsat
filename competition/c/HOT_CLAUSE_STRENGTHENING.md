# Bounded hot-clause strengthening experiment

Attribution motivated selecting learned clauses by saturating scan heat. The
[archived patch](benchmark_results/hot-clause-experimental-20260908.patch), based
on b1de132/357c110 core sources, adds `--hot-vivify`. Each ordinary inprocessing
interval (10,000 conflicts) examines at most 512 rotating learned records,
selects the four highest heat scores >=4,096 with sizes 3–64, and reuses the
existing independently checked RUP literal-removal routine. Attempted records
reset their heat. Selection and optional propagation share a 25,000-work maximum,
also respecting a smaller caller preprocessing allowance. Mandatory root closure
and allocation/cleanup are outside this optional inspection bound; normal
cancellation/deadline checks remain active. This is not a hard real-time bound.

The prototype adds four bytes per arena header, including original records;
counting is conditional on the option. This storage cost and changed collection
schedule are real experiment costs. It is not acceptable to credit every speed
change solely to clause selection.

Validation: release and ASan/UBSan unit suites pass, including 4,704 independent
vivification truth-table/RUP cases and five new cutoff checks of the rotating
512-record/top-four selector. Each build also passes 5,922 CLI truth-table,
original-model and text/binary RUP checks, seed 2026090841. The
[validation record](benchmark_results/hot-clause-validation-20260908.json) pins
both binaries and the complete patch. These finite tests are regression evidence.

The [development screen](benchmark_results/hot-clause-screen-20260908.json)
uses four known inputs, two repetitions, eight external wall seconds, seed
2026090842; all timing runs are serial. Baseline, broad inprocessing and hot
selection each verify 4/8 runs, all SAT. Both planning cases remain UNKNOWN.
Mean wall PAR2: baseline 9.693, broad 9.865, hot 9.490 seconds. Battleship median
wall improves 2.393 to 1.744 seconds; ktf is noisy, 4.378 to 4.215 seconds.
No development solve is added.

The [fresh screen](benchmark_results/hot-clause-holdout-20260908.json) uses
[eight previously untested inputs](benchmark_results/hot-clause-holdout-selection-20260908.json),
one per family, 100 KiB–20 MB, seed 2026090844, excluding all recorded JSON
filenames and hashes. Its run seed is 2026090845, one repetition, eight seconds.
A third profile keeps the candidate's header layout but disables hot selection.
All profiles verify the same 2/8 SAT inputs, with zero errors; the other six are
UNKNOWN. Mean wall PAR2 is baseline 12.142, layout control 12.157, hot 12.218.
The puzzle solve is 0.893 / 1.012 / 1.499 seconds respectively. Peak observed RSS
is 290.5 / 290.3 / 415.8 MB; different time-limited search progress prevents
interpreting that ratio as a universal memory overhead. All conclusive answers
are independently model-checked. Neither screen contains a completed UNSAT run.

Decision: archive the prototype; do not retain a speculative option or enlarge
production clause headers. A development gain did not transfer to this small
holdout, and the targeted planning gap remains unresolved. The existing
attribution tools and reproducible experiment remain available. A later attempt
needs a cheaper score representation and evidence that its strengthened clauses
repay probing cost, rather than simply increasing vivification volume.
