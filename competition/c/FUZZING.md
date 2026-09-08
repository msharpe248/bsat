# Continuous coverage-guided fuzzing

`make fuzzers` builds separate libFuzzer + ASan/UBSan objects with assertions and
allocation-failure injection. It does not reuse production objects. The API target
interprets repeated add-variable, add-clause, solve and assumption operations;
its independent exhaustive oracle checks truth and original-formula SAT models
on at most six variables. Error states must remain UNKNOWN on retry. Feature
combinations cover probing, equivalence, congruence, elimination, BCE, chronological
backtracking, VMTF and frequent inprocessing/reduction.

The parser target builds bounded DIMACS streams from bytes, varies whitespace,
and introduces invalid headers, missing terminators and embedded NULs. It checks
valid formulas against a separate truth table. Both targets can fail one of the
first 256 core allocations. This bounded grammar does not replace the existing
large-header, malformed-token, process-failure or exhaustive allocation tests.

```sh
make -C competition/c -j2 fuzzers
python3 competition/c/tests/seed_fuzz.py /tmp/bsat-fuzz
competition/c/bin/fuzz_api -max_total_time=600 -timeout=10 -max_len=512 \
  -rss_limit_mb=2048 -artifact_prefix=/tmp/bsat-fuzz/ /tmp/bsat-fuzz/api
competition/c/bin/fuzz_dimacs -max_total_time=600 -timeout=10 -max_len=512 \
  -rss_limit_mb=2048 -artifact_prefix=/tmp/bsat-fuzz/ /tmp/bsat-fuzz/dimacs
# Reproduce and reduce an actual failing artifact with the same target:
competition/c/bin/fuzz_api /path/to/crash-artifact
competition/c/bin/fuzz_api -minimize_crash=1 -exact_artifact_path=/tmp/minimized \
  /path/to/crash-artifact
```

CI runs each target for 60 seconds on relevant pushes/PRs and 600 seconds nightly
or on manual dispatch. Corpora persist through the Actions cache; logs, corpora
and failing artifacts are uploaded. A crash fails the job. Preserve the failing
binary/commit, replay the artifact, minimize it, and add a focused regression
before fixing the responsible code. Fuzz executions are bounded test evidence,
not a soundness proof or a percentage of exhaustive coverage.

The local Apple Clang installation lacks its libFuzzer runtime. For local testing
we built the runtime from the official LLVM compiler-rt 21.1.8 release's
`lib/fuzzer/*.cpp` (`clang++ -g -O2 -fno-omit-frame-pointer -std=c++17`, then `ar`).
`FUZZ_ENGINE='/tmp/bsat-libfuzzer/libFuzzer.a -lc++'` overrides only the runtime
link. Linux CI uses its installed Clang runtime. Initial local 61-second campaigns
completed 60,156 API and 409,846 parser executions with no sanitizer/oracle failure;
the subsequent 601-second campaigns added 407,661 API and 3,923,053 parser
executions without a failure (4,800,716 total). Provenance is in
`fuzzing-20260907.json`. Hosted Linux fuzzing also passed in run 34184991907.
No crash minimization was needed.

The correctness workflow explicitly covers Linux GCC and Clang in release and
ASan/UBSan builds, plus macOS Clang in both modes. Hosted results must be checked
separately; defining a matrix does not establish that every configuration passes.
