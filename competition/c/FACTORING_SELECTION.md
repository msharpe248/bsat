# Factoring coverage and selectivity

Factoring now considers two shared residual columns, covering profitable 3-by-2
ternary rectangles that the former three-column seed threshold missed. Each such
rectangle replaces six clauses with five. The proof construction and rule that
all replacements precede deletions are unchanged.

`--factor-min-gain N` requires at least N net clauses removed per rectangle;
default 1 preserves the existing positive-saving policy. Zero still requires a
strict saving. For thresholds above 1, a linear seed scan bounds possible gain
using live column count D and maximum static reverse degree R:
`(D-1)*(R-1)-1`. Deletions can only make this bound looser, so it cannot reject a
seed capable of meeting the threshold. Unpromising seeds avoid the more expensive
two-hop intersection scan. `Factoring candidates` and `Factoring pruned` report
attempted seeds and these early rejections, respectively.

The initial frozen development experiment compares old factoring, current
default factoring, thresholds 32/128 and no factoring. On the known argumentation
case, threshold 32 takes 3.132 seconds, versus 11.573 for current default factoring,
10.792 for old factoring and 4.725 without factoring. It uses the same 1,024 fresh
variables but removes 39,632 net clauses versus 32,068 at threshold 1. It performs
more selection work, so the lower total time is not a claim that preprocessing
itself became faster. Threshold 128 spends its full work allowance, introduces
no variables and takes 5.972 seconds; stricter is not uniformly better.

On the original reg-n instance, threshold 32 preserves all 60 auxiliary variables
and the exact old proof hash, finishing UNSAT in 2.580 seconds. Control reaches
its 15-CPU-second limit without an answer. All conclusive results are checked
independently. These are local development measurements, not new global defaults.

The separate 12-trial confirmation includes the known argumentation case, a
second reg-n instance, and two additional argumentation instances. Threshold 32
repeats the known SAT win at 2.867 seconds (threshold 1: 10.625; control: 4.796).
Both thresholds solve the second reg-n UNSAT in 0.51 seconds; control times out.
All profiles time out on both additional argumentation cases. The selective
profile prunes 284 seeds on one of those inputs, but that does not yield a solved
count gain. All five conclusive confirmation answers pass independent checks;
the campaign records no errors. Threshold 32 stays an explicit development option.

Validation includes 2,048 additional signed exhaustive projection/gain-boundary
cases (9,536 total factoring cases), all release and ASan/UBSan unit suites,
3,127 sanitizer allocation failures, and 288 signed text/binary certificates per
build, including interrupted prefixes and gain-based rejection. Updated core
fuzzing completes 11,611 executions in 31 seconds without a finding.

Records: `benchmark_results/factor-selection-20260908.json`, the separate
`factor-selection-confirm-20260908.json` campaign, and
`factor-select-certificates-{release,debug}-20260908.json`. Factoring and its
selective profile remain opt-in; larger/recursive residual factoring remains
outside this bounded implementation.
