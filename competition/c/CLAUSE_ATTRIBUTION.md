# Clause scan attribution

`make MODE=diagnostic` adds per-clause saturating 32-bit scan, long-unit and
analysis-expansion counters, and a split 64-bit learned-record birth clock.
Splitting avoids unaligned 64-bit accesses in the four-byte-aligned arena.
Original records use epoch zero. The diagnostic conflict clock accumulates over
retained queries; reconstructed solvers start a new epoch. Strengthening creates
a new learned record. Garbage collection copies all metadata. Normal release
and debug clause headers remain 16 bytes, without these counter increments.

With `--accounting`, aggregate scan counters distinguish original/learned origin,
age below 100, 100–999 and at least 1,000 conflicts, and whether a clause had
already produced a long-clause unit or been expanded as an analysis reason.
These aggregates include subsequently deleted clauses. The top ten live clauses
report current CRefs, size, origin, scans, units, analyses and birth; this list
excludes deleted clauses and must not be interpreted as a lifetime ranking.
Counters saturate per record at UINT32_MAX. CRefs are snapshot locations, not
stable identifiers. Neither a conflict's initial clause nor binary propagation
is counted as an analysis expansion or long-unit event.

The [fixed-work report](benchmark_results/clause-attribution-20260908.json)
records commands, binary/input hashes and all output for two known planning
inputs, 100,000 conflicts each. Both return UNKNOWN; this is attribution, not a
solve-rate or speed benchmark. Diagnostic headers and instrumentation perturb
memory, collection and timing, so comparisons with ordinary wall times are invalid.

| Planning input prefix | Original / learned scans | Scans after unit/analysis use | Age >= 1,000 |
| --- | --- | --- | --- |
| 16c999 | 39.80% / 60.20% | 92.93% | 46.83% |
| 21b173 | 14.01% / 85.99% | 91.81% | 22.74% |

The largest surviving individual scan counts belong to original clauses, even
though learned clauses dominate aggregate work. This survivor bias matters:
the top-live list alone would suggest a different target. Test bounded selection
of frequently scanned learned clauses first, preserving productive clauses while
attempting independently justified literal removal. These data do not establish
that strengthening will pay for itself.

Release, debug ASan/UBSan and diagnostic unit suites pass. GC tests now derive
record sizes from the header and validate nonzero diagnostic metadata across
512 deletion patterns. Counter conservation checks cover origin, age and reuse.
The [production trace guard](benchmark_results/attribution-default-traces-20260908.json)
finds identical counters and binary proof prefixes before/after at 1,000 conflicts
on both planning cases; incomplete prefixes are not UNSAT certificates.
