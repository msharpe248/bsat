# Allocate conditional-equivalence storage only when used

Retained: lazy allocation of the congruence branch/join table saves **64 MiB** of
requested temporary storage on both tested argumentation and reg-n inputs. It is
allocated on the first conditional-equivalence insertion. Hash capacity, slot
order, gate construction and bounded scan work remain unchanged. The empty-table
scan still charges its original work; this change is about memory, not a new
preprocessing policy.

Twelve serial, randomized, instrumented runs compare the previous binary with the
candidate: three development inputs, two repetitions per binary, 5,000 conflicts,
binary proofs and an external 120-second limit. Every selected work counter and
proof prefix hash matches across both binaries and repetitions. All runs stop at
the conflict limit with UNKNOWN; their proof prefixes are not UNSAT certificates.
No solved-count gain is inferred from this diagnostic.

| Input | Old temporary capacity | New capacity | Old process RSS | New process RSS |
| --- | ---: | ---: | ---: | ---: |
| argumentation | 101,402,360 | 34,293,496 | 204.1–204.6 MB | 136.8–136.9 MB |
| reg-n | 100,681,732 | 33,572,868 | 204.1–204.2 MB | 132.6–137.3 MB |
| at-least-two-sol | 480,641,932 | 480,641,932 | 1,053.6–1,064.7 MB | 1,058.4–1,058.5 MB |

The largest input uses conditional joins, so this allocation is necessary there.
Its reconstruction overlap and retained watch storage remain memory targets.
Short instrumented CPU timings vary; this is a demonstrated memory reduction,
not a general speedup claim. Requested temporary savings and RSS are distinct.

All 50 C tests and allocation-failure regressions pass in release and ASan/UBSan.
A new binary-only congruence fixture enforces a sub-100-KB temporary-storage ceiling
that the previous eager table exceeds. The 24 structured long-search cases retain
independent model/proof acceptance. Existing congruence tests cover actual AND,
XOR and conditional/ITE joins. The benchmark artifact/checker regressions also
pass after adding proof hashes to result rows, enabling matched-prefix audits.

Reproduction: `benchmark_results/lazy-congruence-policy-20260908.json` freezes
inputs, binaries, options, limits and seed; `lazy-congruence-work-20260908.json`
contains every run and `lazy-congruence-audit-20260908.json` checks counters and
proof bytes. Validation is in `lazy-congruence-validation-20260908.json`.
