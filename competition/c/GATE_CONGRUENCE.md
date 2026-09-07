# Experimental gate congruence

Hardware-model-checking input `7bdf3e5401cc263951003b569bcd689c.cnf`
contains 160,903 variables and 399,948 clauses. The default solver timed out
after 60 solving CPU seconds. A bounded gate-congruence pass followed by
equivalence substitution produced an independently checked UNSAT proof in
19.13 process CPU seconds with focused search. Alternating search took 17.44
seconds and alternating VMTF took 11.94 seconds. These initial measurements
are single runs on a development input; they do not establish general speedups.
Raw results are in `benchmark_results/congruence-long-20260907.json`.

A subsequent matched comparison (seed 20261227, two repetitions, 60 solving CPU
seconds and 70 external wall seconds) separates preprocessing from branching:

| Profile | Verified solves | Process CPU seconds |
| --- | ---: | --- |
| Prior alternating VMTF | 0/2 | Both timed out at 60 |
| Prior equivalence substitution + alternating VMTF | 0/2 | Both timed out at 60 |
| Congruence + equivalence substitution + alternating VMTF | 2/2 | 12.140, 12.916 |

The SCC allowance is 100,000,000 work units in both profiles that enable it.
Proof checking occurs outside solver timing. Jobs run serially without concurrent
builds or validation. This establishes a repeatable gain on this development
input, not general parity with Kissat. See `congruence-matched-20260907.json`.

A four-family development guard (one repetition, seed 20261228, 15 solving CPU
seconds, 20 external wall seconds) solved 1/4 with each matched profile and
reported no errors. Both proved belpyramid UNSAT (old 2.666 CPU seconds, new
2.512); battleship, KTF and random-circuits remained UNKNOWN. That small timing
difference is not sufficient evidence of a speedup. Peak RSS increased on KTF
from 63.9 MB to 114.8 MB and on belpyramid from 48.8 MB to 55.1 MB. These are
whole-run measurements, including time-dependent search allocation. They
reinforce the decision to retain congruence as opt-in. See
`congruence-guard-20260907.json`. Broader held-out evaluation remains outstanding.

```
bin/bsat --congruence --equiv --equiv-budget 100000000 \
  --alternating --vmtf --time 60 --proof answer.drat input.cnf
drat-trim input.cnf answer.drat
```

The pass is disabled by default. `--congruence-budget` defaults to 100,000,000
work units; zero skips the pass. `--equiv` remains a separate option: congruence
learns consequences, while the existing SCC pass substitutes equivalent
variables. Both passes retain their own work allowance. Mandatory root
propagation can exceed the optional work allowance, but obeys the CPU deadline.

## Algorithm and soundness contract

The pass indexes root-reduced binary and ternary clauses, strengthens ternaries
using complementary binary clauses, and extracts complete two-input AND, XOR
and ITE definitions. Existing complementary binary pairs seed signed aliases.
ITE extraction retains alternative conditional definitions. Exact normalized
gate keys, signed union-find representatives and constant simplifications
identify further aliases. Hash collisions require exact-key comparison.

Every new clause must pass reverse unit propagation against the current formula
before insertion, including conditional intermediate clauses used to prove an
alias. Both implications must be proved before merging representatives. Proof
output precedes insertion. Partial valid lemmas can survive a budget cutoff;
an unproved alias cannot. Temporary RUP assignments preserve saved search
phases. Root implications are propagated at level zero before and after each
insertion, so temporary backtracking cannot discard root consequences.

The immutable input is never extended with learned consequences. SAT models
are checked against that input, and UNSAT benchmark answers count only after
external DRAT verification. A timeout is UNKNOWN. Runtime RUP checks and the
test suite provide evidence, not a formal verification of the implementation.

This is a bounded initial pass, skipped under assumptions and after elimination
state exists. Temporary gate storage is capped at four times the initial clause
count. Budget or storage limits may leave useful gates undiscovered. The pass
does not implement arbitrary-width gates or repeated inprocessing closure.
Intermediate proof clauses currently remain in the core and can increase
memory and subsequent search costs.

## Validation

Release and ASan/UBSan builds each passed all 45 C test executables. The new
tests exhaust signed gate patterns, aliases, constants, cyclic definitions,
conditional alternatives, strengthening and budget cutoffs. Each added clause
in the small-model tests is checked against original satisfying assignments.
Direct RUP regressions check rejection, saved phases, immutable input and root
propagation before and after insertion.

Each build also passed 4,212 independent validator solves (81 configurations,
seed 20261226), checking truth-table answers, original models, text/binary RUP
proofs and external DRAT certificates. Each passed 57 CPU-deadline cases.
Five deliberate mutations are caught: trusting unproved clauses, omitting either
root propagation step, changing saved phases, and discarding alternative ITE
branches. The post-insertion propagation mutation initially survived; a direct
entailed-unit implication-chain regression now detects it.

Twelve default trace cases match the prior binary and the zero-budget option
in status, return code, 24 selected counters and proof bytes. Isolated PGO
generate/use builds both exercised congruence and emitted verified UNSAT proofs.
See the `congruence-default-traces`, `congruence-mutations` and `congruence-pgo`
JSON records for commands and hashes. The combined `congruence-validation`
record preserves the build logs, independent-validation summaries, deadline
results and final source hashes. Five prior prototypes have independent U0
patches against `2cee3bc`, checked with `git apply --check --unidiff-zero`;
`congruence-prototypes` pins their source and patch hashes.

## Research and development history

The comparison uses Kissat 4.0.4, pinned at
`8af8e56f174b778aef3aa45af9f739b2a5f492c2`. Its
[congruence implementation](https://github.com/arminbiere/kissat/blob/8af8e56f174b778aef3aa45af9f739b2a5f492c2/src/congruence.c)
provided the extraction and simplification reference. On this hardware input,
disabling its congruence pass, any of AND/XOR/ITE extraction, or binary
strengthening changed a roughly one-second verified solve into a ten-second
timeout. Those ablations motivated this implementation; they do not establish
that congruence helps every family. Kissat solved the random-circuit comparison
without needing this pass.

The `circuit-*` and successive `congruence-*` benchmark records preserve these
development experiments. Early prototypes timed out without completed answers.
The initial prototype's temporary RUP handling had a root-propagation defect,
fixed before retention. Subsequent versions added binary seeds, alternative ITE
definitions, strengthening and phase preservation. Current preprocessing
substitutes 50,852 variables on the hardware target, but still makes vastly more
search decisions than Kissat. Closing that remaining search gap and obtaining
broader, longer-budget held-out evidence remain necessary for competition
performance.
