# Cancellation boundary regressions

Review after the frozen ablation found two callback contract defects:

1. A one-shot cancellation callback could consume the private portfolio time
   slice and request UNKNOWN. The portfolio saw only `interrupted` and elapsed
   time, interpreted the request as slice expiration, and started another attempt.
   If the callback then returned zero, the second attempt could return an answer.
2. The final CPU deadline check ran before the final callback. CPU consumed by
   that callback could therefore escape the limit on an otherwise cached result.

The solver now records explicit cancellation separately from timeout, preserves
that reason through incomplete rebuilds, and refuses portfolio fallback after a
cancellation request. It checks the deadline after the terminal callback. Retry
starts a fresh cancellation state; the application still must reset/disable its
own request. These fix cancellation/resource semantics, not a demonstrated wrong
Boolean answer: the old answers in the reproducer were mathematically correct
but should have been withheld as UNKNOWN.

A standalone reproducer fails both checks on the old binary (failure mask 3):
portfolio returns SAT after two attempts, and cached UNSAT returns beyond the CPU
limit. The same reproducer passes after the fix (mask 0): UNKNOWN after one
attempt, and UNKNOWN after terminal callback CPU expiration. Permanent regressions
are in `test_cancellation.c`.

Release and ASan/UBSan each pass all 52 C test executables, 2,016 allocation
failures, the 12,000-query concurrent shared-library client and 16,384-query soak.
Final deadline, longer certificate and fixed-work evidence is retained in
`benchmark_results/cancellation-*-20260908.json` and the matching work-policy file.
The earlier extended hosted fuzz/soak and ablation records retain their original
binary/revision pins; they are not relabeled as post-fix measurements.

All 69 release deadline cases pass (maximum observed CPU overrun 0.001 s), as do
24 longer structured cases with independently checked models/proofs. Eight
initial fixed-work runs preserve counters and binary-proof prefix hashes. Their
short diagnosis timings suggested higher CPU, so a separately frozen five-repeat
confirmation was run and retained rather than discarding the initial sample.
All 20 confirmation runs preserve the same traces. Diagnosis median CPU is
1.674 s before / 1.587 s fixed, with broadly overlapping ranges; station-repacking
is 0.236 / 0.242 s. The confirmation does not reproduce a consistent diagnosis
slowdown. These noisy samples support no general speed claim for the correctness
fix. Both protocols and every measurement remain available.
