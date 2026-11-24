# BSAT C Solver

BSAT is a C11/POSIX CDCL SAT solver with a tick-budgeted inprocessing engine,
dynamic GF(2) Gaussian elimination, exact syntactic symmetry breaking,
multi-core clause-sharing portfolio solving, DRAT proof logging, and an opaque
ABI-v1 C/C++ embedding library (plus IPASIR 1.0).

Start with [PRODUCTION_READINESS.md](PRODUCTION_READINESS.md) for the
capability matrix and release gates, [FEATURES.md](FEATURES.md) for the full
option and default reference, and [API_CONTRACT.md](API_CONTRACT.md) for
embedding rules.

---

## Build and Verify

Requires a C11/POSIX compiler, `make`, and Python 3 for validation and
benchmarking.

```sh
cd competition/c
make all test                                            # Release build + 67 unit tests
make all test MODE=debug                                 # Debug build (ASan + UBSan)
make all test MODE=diagnostic                            # Build with opt-in detailed search counters
make pgo                                                 # Profile-guided optimization (Clang/LLVM)
python3 tests/validate.py --solver bin/bsat --cases 200  # Independent oracle, model & RUP check
```

Build artifacts are separated by mode (`release`, `debug`, `diagnostic`, `tsan`,
`profile`). Additional verification targets:
- `make fault-test` / `make recovery-fault-test`: deterministic OOM allocation-failure testing
- `make embedding-test soak-test package-test`: ABI v1 shared library, soak, and staged install checks
- `make MODE=tsan bin/tests/tsan/test_gauss_and_amo && ./bin/tests/tsan/test_gauss_and_amo`: ThreadSanitizer check for concurrent handles and `-j N` portfolio solving

See [tests/FEATURE_COVERAGE.md](tests/FEATURE_COVERAGE.md) for the test
coverage index.

---

## Run

```sh
./bin/bsat input.cnf                          # Default one-shot solve
./bin/bsat -j 4 input.cnf                     # 4-thread clause-sharing portfolio
./bin/bsat --proof proof.drat input.cnf       # Text DRAT proof
./bin/bsat --binary-proof --proof p.drat in.cnf
/path/to/drat-trim input.cnf proof.drat
```

**Exit codes**: `10` SAT, `20` UNSAT, `0` UNKNOWN / resource limit, `1` input,
I/O, or internal error. Every returned SAT model is verified against the
retained original input before returning.

**DIMACS input**: Uses a 64 KiB buffered reader (`src/dimacs.c`) enforcing
strict DIMACS syntax—one `p cnf <vars> <clauses>` header, in-range literals,
exact clause count, zero-terminated clauses (which may span or share lines),
and rejection of embedded NULs or integer overflow. Duplicate literals and
tautologies are normalized automatically. Empty formulas are SAT; empty clauses
are UNSAT.

**DRAT proofs**: Text (chunked decimal) and binary DRAT are supported across
search, minimization, block shrinking, OTFS, probing/HBR, probing intersection,
gate congruence, Tarjan SCC substitution, Mini-Kitten SAT sweeping, global
backbone extraction, subsumption/SSR, gate-aware BVE, binary transitive
reduction, BCE, bounded factoring (`--factor`), and prefix-sharing vivification.
Non-RUP passes (GF(2) Gaussian elimination, exact symmetry breaking, AMO
sequential-counter re-encoding, and SBVA) are automatically disabled whenever
`--proof` or `BSAT_CERTIFICATES` is active so standard DRAT/LRAT checkers
remain strictly valid.

---

## Architecture & Current Defaults

Default one-shot solving uses:

- **Search & Branching**: Tick-based alternation (`--alternating`) between
  **Focused mode** (VMTF conflict-recency queue, fast/slow Biere–Fröhlich LBD
  EMA restarts, trail reuse, saved phases) and **Stable mode** (4-ary heap
  VSIDS with dynamic decay `0.80 → 0.95`, reluctant doubling restarts, target
  phases), with chronological backtracking (`--chrono`, `--chrono-levels 100`),
  reason-side activity bumping (`num_vars >= 300`), and 6-step Kissat-style
  target rephasing (`--rephase-interval 1000`).
- **Conflict Analysis & Clause DB**: 1-UIP learning, 3-state binary-aware DFS
  minimization (`--minimize-budget 10000`), block-level all-UIP shrinking
  (`--shrink`), binary-resolution minimization (`--binary-minimize`), on-the-fly
  self-subsumption (OTFS), dynamic LBD updates (`--dynamic-lbd`), and a 3-tier
  learned database (`--protect-used`, `--reduce-interval 1000`,
  `--reduce-increment 100`) with portable deterministic tie-breaking and lazy
  watch sweeping.
- **Hot-Loop BCP**: Literal-indexed `lit_values[]` shadow array during search,
  compact 16-byte `ClauseHeader`, binary-first watch partitioning, single-branch
  tagged/implicit binary dispatch, 1-ahead long-clause prefetching, and two-pass
  circular watch scanning (`--no-circular` to disable).
- **Tick-Budgeted Inprocessing** (`--inprocess`, `--inprocess-interval 10000`):
  1. Root cleanup and unit propagation
  2. Failed-literal probing, Hyper-Binary Resolution (HBR), and RUP-certified
     double-lookahead probing intersection
  3. Preprocessing & periodic gate congruence closure (2-input AND/XOR/ITE +
     3..8-input N-ary AND) and model-guided global backbone extraction
  4. In-place Tarjan SCC equivalence substitution
  5. Global 64-bit gate-DAG simulation + dual-cone embedded Mini-Kitten CDCL SAT
     sweeping with RUP lemma projection
  6. Forward subsumption and self-subsuming resolution (SSR)
  7. Gate-aware cascading BVE (AND/XOR/ITE) with compact model reconstruction
  8. Post-BVE binary transitive reduction
  9. O(n) prefix-sharing clause vivification
  10. Kissat-style ProbSAT polynomial-weighted phase seeding (`(1 + break)^(-2.3)`)
      with a 4,000-flip preprocessing warmup on small/medium formulas
- **Specialized Non-Proof Engines** (one-shot non-proof mode only):
  - **Dynamic GF(2) Gaussian Elimination** (`src/gauss.c`): 2..7-var XOR
    extraction with subsumed-XOR recovery, Tseitin XOR-chain stitching, root
    Gauss-Jordan RREF, and live Bird/Han-Jiang two-watched-column matrices
    during CDCL search with zero-undo backtracking and temporary 1-UIP reasons.
  - **Exact Syntactic Symmetry Breaking** (`--sym`, `src/symmetry.c`): Exact
    clause hash set over level-0-reduced clauses, verified involution
    automorphisms, variable transposition chains, and BreakID-style disjoint-support
    matrix row-interchange lex-leader constraints.
  - **AMO Cliques & SBVA** (`src/inprocess.c`, `--sbva` / `src/sbva.c`):
    Linear Sinz sequential-counter re-encoding for `>=80%`-binary formulas and
    arbitrary-length Structured Bounded Variable Addition with 3-hop structural
    tie-breaking.
- **Multi-Core Clause-Sharing Portfolio** (`-j <N>` / `--threads <N>`,
  `src/portfolio.c`, non-proof mode): Worker 0 runs full BSAT (imports only);
  Workers `1..N-1` run diversified logically-equivalent profiles (no destructive
  BVE/BCE/SBVA/symmetry) exporting short learned clauses (`size <= 4`) through a
  mutex-guarded ring buffer with immediate atomic cancellation on first winner.
- **Incremental Protection**: Destructive inprocessing and non-RUP passes are
  automatically skipped under assumption queries, `BSAT_CERTIFICATES`, and
  `BSAT_REUSE_LEARNTS`. Internal callers can also pin variables against
  elimination/substitution/BCE/symmetry via `solver_freeze` / `solver_melt` /
  `solver_is_frozen`.

Run `./bin/bsat --help` or see [FEATURES.md](FEATURES.md) for all flags and
defaults.

---

## On-Demand Wide Benchmark & Proving Suites

For testing BSAT across hundreds to thousands of known SAT/UNSAT problems beyond
the fast check-in suite, use [scripts/fetch_corpora.py](scripts/fetch_corpora.py)
and [scripts/suite_runner.py](scripts/suite_runner.py) (cached in gitignored
`dataset/corpora/`):

```sh
cd competition/c
make wide-list                                  # Show offline, SATLIB & SAT Comp 2023-2025 packs
make wide-fetch PACK="synthetic-hard satlib-core"
make wide-test CORPUS=satlib-core TIMEOUT=10 WORKERS=4

# Fetch balanced size-filtered SAT Competition 2024 instances with known SAT/UNSAT labels:
python3 scripts/fetch_corpora.py fetch satcomp-2024 --limit 60 --max-mb 2

# Compare single-thread vs 4-thread portfolio side by side:
python3 scripts/suite_runner.py --corpus satcomp-2024 --timeout 20 --workers 2 \
    --solver bsat=bin/bsat --solver "bsat-j4=bin/bsat -j 4" --output /tmp/wide_results.json
```

`suite_runner.py` transparently decompresses `.cnf.gz`/`.cnf.xz`/`.cnf.bz2`/`.cnf.zst`,
independently checks every SAT model against all clauses of the original CNF,
validates ground-truth labels and cross-solver agreement, and supports external
DRAT verification via `--proof --checker /path/to/drat-trim`.

For strict serial PAR-2 benchmarking with SHA-256-pinned manifests and `drat-trim`
checking, see [tests/benchmark.py](tests/benchmark.py).

---

## C & IPASIR Embedding API

Include [include/bsat.h](include/bsat.h) and build with `make shared` (or
`make install` — see [INSTALLING.md](INSTALLING.md)). The opaque ABI-v1
library provides:

- Independent concurrent handles (serialize calls per handle; distinct handles
  run safely on separate threads with thread-local CPU limits and no library
  signal handlers)
- Permanent clause addition, temporary assumptions, model and failed-core
  queries, cooperative termination callbacks, and per-query limits/statistics
- Opt-in `BSAT_REUSE_LEARNTS` for retaining learned clauses across compatible
  incremental queries
- Opt-in `BSAT_CERTIFICATES` (and optional `BSAT_CERTIFIED_PROBING`) with
  `bsat_export_query` for exporting exact query CNFs and proof journals without
  re-solving
- Standard IPASIR 1.0 adapter via [include/ipasir.h](include/ipasir.h)

See [API_CONTRACT.md](API_CONTRACT.md) for full ownership, error-poisoning, and
concurrency rules. `include/solver.h` is an internal development interface with
no stable ABI.

---

## Key Design Lessons Retained

- **Portable reduction ordering**: `src/reduction_sort.c` uses a fixed typed
  sort instead of platform `qsort` so tied learned-clause scores produce
  identical reduction decisions across Linux and macOS.
- **16-byte `ClauseHeader`**: Compacting headers further or dropping activity
  tracking did not improve throughput; the 16-byte layout with separate dense
  `values[]` / `lit_values[]` arrays and 4-byte trail entries is retained.
- **Fresh deadline checkpoints in non-BCP loops**: Long inner loops that do not
  increment `s->work` rapidly (local search, ProbSAT phase seeding, and
  inter-pass inprocessing boundaries) poll `solver_budget_exhausted_now()` so
  short CPU deadlines hold even in unoptimized sanitizer builds.
- **Conservative incremental/certified boundaries**: Destructive simplification
  and non-RUP passes stay strictly isolated from assumption queries,
  `BSAT_CERTIFICATES`, and `BSAT_REUSE_LEARNTS` sessions, and only
  logically-equivalent portfolio workers export clauses.

Historical experiment reports, rejected prototypes, and milestone analyses are
archived in [`docs/`](docs/).
