# Fresh-input VMTF/congruence confirmation

This experiment freezes a two-by-two comparison of VMTF and congruence. All four
profiles retain chronology, equivalence processing with a 100-million-work
budget, and alternating search. Each run has a 12-second solving-thread CPU
limit and a 16-second external wall limit, with two seeded serial repetitions.
The binary and exact options are pinned before timing. No local builds, tests,
fuzzers or other solver workloads run concurrently with the experiment.

The first six inputs come from the existing seeded distinct-family selector,
with a 50 KB–5 MB size range. Selection excludes recorded filenames and content
hashes from 445 JSON history files. Two additional argumentation inputs use a
separate seeded file shuffle, the same historical exclusions and a 50 KB–20 MB
range. There is no answer-based selection. The resulting eight inputs span
seven families; exact selection policy, history hashes and input hashes are in
`benchmark_results/profile-confirm-inputs-20260908.json`.

This is held-out confirmation of an existing profile hypothesis. It is a small,
size-bounded sample, not a SAT Competition score or a claim about all families.
Primary acceptance is independently verified completion within the common
budget. SAT models are checked against original input; UNSAT requires the
DRAT-to-LRAT/CakeML chain. UNKNOWN proof prefixes are never certificates.

The timing policy is `benchmark_results/profile-confirm-policy-20260908.json`
and the complete run record is `profile-confirm-20260908.json`.

All 64 runs pass the exact-grid, frozen-option/binary/input and acceptance audit;
all 445 selection-history hashes still match. There are eight verified SAT
answers, 56 UNKNOWN and no errors.

| Profile | Verified / 16 | Mean PAR-2 |
| --- | ---: | ---: |
| Full | 0 | 32.000 s |
| Omit VMTF | 4 | 25.664 s |
| Omit congruence | 0 | 32.000 s |
| Omit both | 4 | 25.647 s |

Both profiles without VMTF solve the same fresh argumentation input in
6.452–6.600 seconds and the unknown-cases input in 6.672–6.767 seconds, in both
repetitions. All VMTF-enabled runs on those inputs are UNKNOWN. The remaining
six inputs are unresolved by every profile. Congruence changes no completion
in this sample; the small timing difference between the two winners is not a
useful claim. Maximum RSS across the screen is higher for the winners.

This supports using the previously identified no-VMTF option combination on
additional workloads, including a second family. It does not support promoting
other options globally, disabling congruence generally, or claiming improved
propagation throughput: this is a change in search effectiveness. Defaults stay
unchanged (VMTF is already opt-in). The pinned binary precedes the subsequent
conditional-reuse extension; these timings are not relabeled as that revision.
