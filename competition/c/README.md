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
make all test                                            # Release build + 73 unit tests
make all test MODE=debug                                 # Debug build (ASan + UBSan)
make all test MODE=diagnostic                            # Build with opt-in detailed search counters
make shared                                              # Build libbsat shared library (also used by Python bsat.solve_c)
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
./bin/bsat input.cnf                          # Default one-shot solve (DIMACS CNF or XOR-CNF)
./bin/bsat -j 4 input.cnf                     # 4-thread clause-sharing portfolio
./bin/bsat --proof proof.drat input.cnf       # Text DRAT proof
./bin/bsat --binary-proof --proof p.drat in.cnf
./bin/bsat --lrat proof.lrat input.cnf        # Native linearly checkable text LRAT proof
./bin/bsat --lrat p.blrat --binary-lrat in.cnf # Native binary LRAT proof
./bin/bsat problem.wcnf                       # Core-guided Weighted Partial MaxSAT (RC2/OLL)
./bin/bsat problem.opb                        # Pseudo-Boolean decision or optimization (.opb)
./bin/bsat --models all --project 1,2,3 in.cnf # AllSAT / projected model enumeration
./bin/bsat --backbone in.cnf                  # Exact global backbone literals
./bin/bsat --mus in.cnf                       # Minimal Unsatisfiable Subset (MUS)
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

**DRAT & Native LRAT proofs**: Text (chunked decimal) and binary DRAT are supported across
search, minimization, block shrinking, OTFS, probing/HBR, probing intersection,
multi-gate congruence (2-input AND/XOR/ITE + N-ary AND + `MAJ3` + `XOR3`),
Tarjan SCC substitution, Mini-Kitten SAT sweeping, global backbone extraction,
subsumption/SSR, gate-aware and semantic Mini-CDCL definition BVE, binary
transitive reduction, BCE, bounded factoring (`--factor`), prefix-sharing
vivification, and **DRAT/RAT proof-producing** SBVA (`--sbva`), AMO Sinz
sequential-counter re-encoding, exact syntactic symmetry breaking (`--sym`),
Root GF(2) XOR-chain RREF, and bounded live in-search Bird/Han-Jiang GF(2)
propagation (`src/gauss.c`, compact matrices `<= 128` rows / `<= 2,048`
cumulative proof-row merges with automatic level-0 CDCL fallback above budget).
For direct linear-time verification without `drat-trim`, `--lrat` /
`--binary-lrat` (`src/lrat.c`) emits native text and binary LRAT proofs with
exact 64-bit clause IDs, trail-ordered antecedent hints, buffered deletions,
and GC ID relocation (with zero overhead when disabled). All RAT/non-RUP passes
stay disabled in append-only `BSAT_CERTIFICATES` journals so standard
incremental RUP/DRAT/LRAT checkers remain strictly valid.

---

## Architecture & Current Defaults

Default one-shot solving uses:

- **Search & Branching**: Tick-based alternation (`--alternating`) between
  **Focused mode** (VMTF conflict-recency queue, fast/slow Biere–Fröhlich LBD
  EMA restarts, trail reuse, saved phases) and **Stable mode** (4-ary heap
  VSIDS with dynamic decay `0.80 → 0.95`, reluctant doubling restarts, separated
  `target_phase[]`), with chronological backtracking (`--chrono`,
  `--chrono-levels 100`), reason-side activity bumping (`num_vars >= 300`), and
  level-0 6-step Kissat-style `B/W/I/B/W/O` rephasing
  (`--rephase-interval 1000`).
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
  3. Preprocessing & periodic iterative fixpoint gate congruence closure
     (2-input AND/XOR/ITE up to `800k` clauses + N-ary AND + 3-input
     `MAJ3`/`XOR3` full-adder gates) and model-guided global backbone extraction
  4. In-place Tarjan SCC equivalence substitution
  5. Global 64-bit gate-DAG simulation + dual-cone embedded Mini-Kitten CDCL SAT
     sweeping with RUP lemma projection
  6. Forward subsumption and self-subsuming resolution (SSR)
  7. Gate-aware cascading BVE (syntactic AND/XOR/ITE + semantic Mini-CDCL
     UNSAT-core definitions) with compact model reconstruction
  8. Post-BVE binary transitive reduction
  9. O(n) prefix-sharing clause vivification
  10. Active-clause-gated, tick-capped Kissat-style ProbSAT phase seeding
      (`(1 + break)^(-2.3)`, low-energy basin gating)
- **Proof-Producing Structural Engines** (all emit standard DRAT/RAT under
  `--proof`, using monotonic proof-only auxiliary variable IDs where temporary
  variables are needed; skipped in append-only `BSAT_CERTIFICATES` journals,
  assumption calls, and `BSAT_REUSE_LEARNTS`):
  - **SBVA** (`--sbva`, `src/sbva.c`): Arbitrary-length Structured Bounded
    Variable Addition with 3-hop structural tie-breaking and RAT-valid DRAT
    emission.
  - **AMO Cliques** (`src/inprocess.c`): Linear Sinz sequential-counter
    re-encoding for `>=80%`-binary formulas with temporary backward RAT
    definitions + 1-step RUP blocking binaries.
  - **Exact Syntactic Symmetry Breaking** (`--sym`, `src/symmetry.c`): Exact
    clause hash set over level-0-reduced clauses, verified involution
    automorphisms, variable transposition chains, and BreakID-style
    disjoint-support matrix row-interchange breaking; in `--proof` mode emits
    DRAT/RAT proofs via O(n) single-pair conditional-swap peeling + failed-literal
    RUP (mutex columns) and insertion-sort conditional-swap chains (non-mutex
    columns).
  - **GF(2) Gaussian Elimination** (`src/gauss.c`): 2..7-var XOR extraction with
    subsumed-XOR recovery, Tseitin XOR-chain stitching, connected-component root
    Gauss-Jordan RREF (emitting linear Tseitin XOR-chain addition DRAT proofs
    from RREF history bitmasks under `--proof`), and live in-search
    Bird/Han-Jiang two-watched-column matrices (with persistent row Tseitin
    XOR-chain DRAT proofs on compact matrices `<= 128` rows / `<= 2,048`
    cumulative proof-row merges, falling back cleanly at level 0 above budget).
- **Multi-Core Clause-Sharing Portfolio** (`-j <N>` / `--threads <N>`,
  `src/portfolio.c`, non-proof mode): Worker 0 runs full BSAT (imports only);
  Workers `1..N-1` run 8 diversified logically-equivalent profiles (including a
  zero-preprocessing fast-start profile on Worker 2, with no destructive
  BVE/BCE/SBVA/symmetry) sharing LBD-filtered learned clauses
  (`size <= 8, LBD <= 3`, plus all units/binaries) through a dedicated unit
  broadcast ring + mutex-guarded clause ring with 64-bit canonical hash
  deduplication, 8 MiB worker stacks, and immediate atomic cancellation on first
  winner.
- **Deployment & Acceptance Starter Kit**:
  - [scripts/acceptance_gate.py](scripts/acceptance_gate.py) +
    [scripts/acceptance_policy.example.json](scripts/acceptance_policy.example.json):
    turnkey candidate integrity (git rev + binary/checker SHA-256), POSIX
    `setrlimit` child containment (`RLIMIT_CPU`, `RLIMIT_FSIZE`,
    `RLIMIT_DATA`/`RLIMIT_AS`), peak RSS (`wait4`), `drat-trim` verification,
    and numerical SLO evaluation (`min_solve_rate`, `max_bad`, `max_p95_sec`,
    `max_par2_sec`, `max_rss_mb`, `max_proof_mb`).
  - [examples/contained_worker.c](examples/contained_worker.c)
    (`make contained-worker-example`): POSIX supervised worker with a durable
    `fsync()`'d CRC-framed append-only WAL (torn-tail resilient), `setrlimit`
    worker containment, SIGKILL recovery, and certified query export.
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
