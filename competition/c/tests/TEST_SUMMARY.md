# C solver test summary

The maintained coverage inventory and reproducible commands are in
[FEATURE_COVERAGE.md](FEATURE_COVERAGE.md). The previous 2025 feature-count and
percentage summary was stale and has been replaced.

The Makefile discovers `test_*.c` automatically, with separate release/sanitizer
builds. Coverage includes allocation-failure injection, model/proof validation,
stateful and real-circuit histories, resource controls, worker recovery, ABI and
threading checks, and benchmark-harness regressions. Counts describe exercised
cases, not a correctness percentage.

See [Production readiness](../PRODUCTION_READINESS.md) for the authoritative
capability matrix, current evidence limits and release gates. Earlier reports
retain their historical counts and measurements.
