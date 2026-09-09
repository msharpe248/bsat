# Incremental performance follow-up — 2026-09-09

Authorized scope: broaden Linux confirmation of the portable reduction order,
remeasure retained certified cal3, and implement a targeted change only when
controlled evidence identifies a bottleneck. Update documentation and commit and
push each milestone.

1. Freeze a broader Linux regression corpus and same-runner baseline/current
   comparison before timing. Eight named families, lexicographically first local
   SAT Competition 2025 filename no larger than 2 MB in each: battleship,
   belpyramid-puzzle, hamiltonian, coloring, cryptography, argumentation,
   scheduling, tseitin-formulas. Selection uses names and sizes, not outcomes.
   These are existing repository datasets; no pristine holdout claim.
   Two repetitions, 30 process CPU / 45 wall seconds, seed 2026090910;
   independent model or DRAT/LRAT/CakeML checking (600 seconds per stage,
   2048 MB heap / 512 MB stack). Compare pinned pre-sort commit 67d80b5
   against current default with portable -O3 -flto on the same Linux runner.
   Gate: no invalid answer, no checked solve loss, aggregate CPU PAR2 regression
   at most 5%. UNKNOWN remains unfinished. Inspect per-family regressions even
   if aggregate passes. This screen is smaller than deployment acceptance.
2. Remeasure cal3 public-facade retained uncertified/certified histories at 60
   query CPU seconds, with independent retained CaDiCaL and checked witnesses.
   Then compare fresh certified state and journal-free diagnostic execution
   under the same budget to separate retained search and certificate costs.
3. Select one implementation hypothesis from this evidence, validate soundness
   and repeat frozen confirmation. Reject unsuccessful experiments explicitly;
   do not change defaults merely to complete the milestone.

Status: corpus/harness preparation and retained baseline in progress.
