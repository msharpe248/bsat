# Large-input allocation lifetimes

Three changes reduce retained or overlapping storage: release the congruence
clause index before ITE expansion, begin each watch list at four entries instead
of sixteen, and transfer the original input buffer when an incremental rebuild
commits successfully. Failure before that transfer leaves the old input owned
by the original instance. Equivalence reconstruction already transferred input;
the new input transfer specifically fixes the ordinary incremental rebuild.

The frozen 12-run comparison uses two repetitions, three existing inputs, both
binaries, the same explicit search profile, binary proof output and a fixed
1,000-conflict budget. All runs return UNKNOWN with identical selected search
counters and identical proof prefixes. These prefixes are trace guards, not
accepted UNSAT certificates. No other local build/test/solver runs alongside
the timed comparison.

On the 622,818-variable input:

| Measurement | Before | After |
| --- | ---: | ---: |
| Peak process RSS, two runs | 1,058.42 / 1,058.46 MB | 934.99 / 938.48 MB |
| Congruence temporary capacity peak | 480.64 MB | 459.67 MB |
| Reconstruction owned-capacity overlap | 737.86 MB | 610.99 MB |
| Retained watch capacity at cutoff | 140.69 MB | 89.37 MB |
| Total retained owned capacity at cutoff | 396.56 MB | 345.24 MB |

MB is decimal. Peak RSS falls about 11%; owned capacities exclude allocator and
runtime overhead. Wall times are 2.737/2.673 seconds before and 2.744/2.257 after.
Two short repetitions do not establish a speedup. The smaller reg-n case uses
about 3.5 MB more RSS despite equal owned temporary capacity, illustrating
allocator effects; argumentation RSS is slightly lower. This bounded search
does not replace the earlier 120-second largest-input evaluation.

Release and ASan/UBSan suites pass, including original-input ownership across
20 rebuilds, watch growth, hard incremental histories, and 2,580 injected
allocation failures per build. Independent validation passes 11,316 formula
solves and 24 longer structured/metamorphic model/proof cases. Detailed records
are `benchmark_results/memory-lifetimes*-20260908.json`; additional 20,000-conflict
search/proof guards are in `memory-trace-20260908.json` and its frozen policy.
All 24 additional runs match status, counters and proof bytes. Timing is mixed:
diagnosis text-proof runs take 1.85/1.95 seconds of CPU after versus 1.70/1.66
before, while binary/no-proof results are closer or faster. The memory reduction
therefore carries a possible workload-dependent throughput tradeoff; it is not
advertised as a general speed improvement.
