# Verified UNSAT acceptance

`tests/verified_check.py` snapshots the original CNF and DRAT proof, runs pinned
DRAT-to-LRAT conversion, then accepts only an exit-zero `s VERIFIED UNSAT` from
cake_lpr on that CNF and LRAT. A converter success alone never establishes final
acceptance. SAT still requires independent model checking.

Build and use:

```sh
python3 competition/c/tests/build_verified_checker.py --directory /tmp/cake-lpr
BSAT_DRAT_TRIM=/path/to/drat-trim BSAT_CAKE_LPR=/tmp/cake-lpr/cake_lpr \
  competition/c/tests/verified_check.py input.cnf proof.drat --artifacts /tmp/checks
```

The wrapper can be supplied as `benchmark.py --checker`; the same environment
variables select its tools. Each stage has a 600-second timeout by default; allow
for both stages in an enclosing benchmark checker timeout. Optional artifact
retention preserves the original snapshots, LRAT, commands, hashes and stage logs.
Timeouts, crashes, invalid conversions and invalid final status fail closed.

`build_verified_checker.py` pins upstream revision
`a36874a8b750b43fe4b385b8ddbf5b033e46a3fa` and checks source hashes before compiling
its distributed x86-64 or ARM64 assembly with the C runtime. It does not replay
the HOL4/CakeML proofs. The assurance scope and trusted runtime assumptions are
those of [upstream cake_lpr](https://github.com/tanyongkiam/cake_lpr/tree/a36874a8b750b43fe4b385b8ddbf5b033e46a3fa).
This is a verified certificate checker path, not formal verification of BSAT,
the compiler, operating system, hardware or the Python orchestration.

Local validation is recorded in `verified-checking-20260907.json`:

- Six valid text/binary chains, including empty-clause and contradictory-unit
  inputs, pass. Corrupt LRAT, an LRAT bound to the wrong original CNF, and an
  invalid DRAT on a satisfiable input are rejected. Strict status/exit tests pass.
- All 24 existing long-search/metamorphic cases pass; SAT models are independently
  checked, and UNSAT goes through conversion and cake_lpr.
- The existing hardware-model-checking binary certificate passes on the original
  160,903-variable, 399,948-clause input. That proof predates binary buffering;
  this check demonstrates large-certificate acceptance, not a new solver timing.

CI builds the pinned checker and runs the real positive/negative chain tests in
every C solver platform/build configuration. Existing broader DRAT checks remain.

The September 8 assurance milestone adds explicit conditional-query snapshots,
per-stage wall/CPU costs, cumulative child RSS, and timeout-group cleanup. Three
current BSAT industrial UNSAT certificates pass the chain; see
`ASSURANCE_EXPANSION.md` and its hash-pinned receipts.

## Larger certificates

The defaults remain a 512 MB CakeML heap and 128 MB stack. Larger certificates
can require more of both. Use explicit `--checker-heap-mb` /
`--checker-stack-mb` settings (or `BSAT_CAKE_HEAP_MB` / `BSAT_CAKE_STACK_MB` for
the CLI wrapper), preserve verification failures, and budget checker resources
separately from solving. See [the reproduced resource limit and fix](CHECKER_RESOURCE_SCALING.md).
