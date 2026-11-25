# C Solver Production Readiness

Authoritative capability and release-gate summary for the C implementation
(`1.0.0-dev`). Historical experiment reports and milestone logs live in
[`docs/`](docs/); they do not override this summary or
[API_CONTRACT.md](API_CONTRACT.md).

SAT means a satisfying assignment exists; UNSAT means none exists; UNKNOWN
means a budget or cancellation stopped the search. For production acceptance,
independently verify every SAT model against the original input and every UNSAT
certificate against its exact query.

---

## Capability Matrix

| Capability | Current support | Evidence and boundary |
|---|---|---|
| One-shot DIMACS, XOR-CNF & PB solving | CLI (`bin/bsat`), SAT/UNSAT/UNKNOWN, text/binary DRAT & native text/binary LRAT (`--lrat`), XOR-CNF lines (`x <lits> 0`, with `x` as a separate token; the attached CryptoMiniSat spelling `x1 2 3 0` is rejected), `.opb` Pseudo-Boolean decision & optimization | Strict DIMACS parser, model self-check, `validate.py`, `drat-trim` CI, in-tree linear LRAT verifier (`lrat_verify_file` in `src/lrat.c`, exercised by `test_lrat`; no external LRAT checker in CI), `test_xor_and_pb` |
| MaxSAT & higher-level SAT queries | RC2/OLL Weighted Partial MaxSAT (`--maxsat` / `.wcnf`), AllSAT/projected model enumeration (`--models` / `--project`), exact global backbone (`--backbone`), minimal MUS (`--mus`) | Verified against brute-force truth-table oracles and minimality checkers in `test_maxsat` & `test_queries` (release + ASan/UBSan) |
| Multi-core portfolio solving | `-j <N>` / `--threads <N>` (`1..64`), LBD-filtered sharing (`size <= 8, LBD <= 3`), dedicated unit ring, canonical hash dedup, full-default Worker 0 plus 7 named worker profiles (incl. fast-start & WalkSAT-hybridized SAT-hunter), atomic stop | Non-proof one-shot mode; TSan-clean ring buffers (`MODE=tsan`), 8 MiB worker stacks, only logically-equivalent workers export clauses |
| Public C/C++ & Python embedding | Opaque ABI v1 (`bsat.h`, with additive `bsat_freeze`/`melt`, `set_threads`, `set_proof`/`lrat`, `add_xor`/`atmost`/`pb_le`, `backbone`, `mus`), Linux/macOS C11/POSIX, plus zero-dependency Python `ctypes` wrapper (`src/bsat/c_solver.py`) | [API_CONTRACT.md](API_CONTRACT.md), [INSTALLING.md](INSTALLING.md), `test_public_extensions`, `embedding-test`, `package-test`, `tests/test_c_solver.py`; internal `solver.h` layouts are not stable ABI |
| Incremental additions & assumptions | Copied permanent clauses, temporary assumptions, model and sufficient failed core, `solver_freeze`/`solver_melt` | [API_CONTRACT.md](API_CONTRACT.md); destructive inprocessing and non-RUP passes stay disabled on assumption calls |
| Retained learning | Opt-in `BSAT_REUSE_LEARNTS`, including compatible conditional UNSAT and budget slices | [INCREMENTAL_REUSE.md](docs/INCREMENTAL_REUSE.md); cancellation or incompatible state rebuilds from permanent input |
| Certified incremental queries | `BSAT_CERTIFICATES`, alone or with reuse; exact CNF and binary proof export | [RETAINED_CERTIFICATES.md](docs/RETAINED_CERTIFICATES.md); conservative non-destructive options, synchronous export |
| Proof-producing certified probing | Opt-in `BSAT_CERTIFIED_PROBING` (requires `BSAT_CERTIFICATES`) | Bounded RUP units preserve variables and assumptions; other transformations remain disabled |
| IPASIR 1.0 | Installed adapter (`ipasir.h`), assumptions, terminate and learn callbacks | [IPASIR.md](docs/IPASIR.md); adapter may introduce assumption variables |
| Independent concurrent handles | Distinct handles may solve concurrently | [EMBEDDING.md](docs/EMBEDDING.md), TSan and thread-local CPU budget tests; serialize calls per handle |
| Query controls & statistics | Mutable CPU/conflict/decision budgets and latest-query snapshots | CPU limits use solving-thread time; `0` means unlimited; fresh checkpoints in non-BCP inner loops |
| Service limits, quotas & recovery | Cooperative wall/capacity checks, atomic journal-byte ceiling, durable WAL + POSIX `setrlimit` worker starter kit, turnkey SLO acceptance gate | [SERVICE_RESOURCES.md](docs/SERVICE_RESOURCES.md), [SERVICE_RECOVERY.md](docs/SERVICE_RECOVERY.md), [examples/contained_worker.c](examples/contained_worker.c), [scripts/acceptance_gate.py](scripts/acceptance_gate.py) |
| Independent UNSAT verification | DRAT (`drat-trim`) and DRAT-to-LRAT (`cake_lpr`) | [VERIFIED_CHECKING.md](docs/VERIFIED_CHECKING.md); checks the exact CNF, not upstream application encoding |
| Fuzzing & failure injection | LibFuzzer API/parser targets, ASan/UBSan/TSan, deterministic OOM injection | [FUZZING.md](docs/FUZZING.md), [tests/FEATURE_COVERAGE.md](tests/FEATURE_COVERAGE.md) |

---

## Current Verification & Benchmark Evidence

### Automated Verification
- **Unit & integration tests**: `77/77` C tests (including
  `test_differential_engines.c`, `test_gauss_and_amo.c`, `test_xor_and_pb.c`,
  `test_maxsat.c`, `test_queries.c`, `test_lrat.c`, and
  `test_public_extensions.c`) pass across `release`, `diagnostic`, and `debug`
  (ASan + UBSan) builds, plus ThreadSanitizer (`MODE=tsan`), `fault-test`
  (`3,564` deterministic OOM injection cases), `recovery-fault-test`,
  `contained-worker-example` + `test_acceptance_and_containment.py`,
  `embedding-test`, `soak-test`, `package-test`, `application_histories`,
  `check_query_diagnostics`, `check_deadlines` (`0.002 s` max CPU overrun),
  `check_long_search` (`24/24` `drat-trim`), `validate.py` (`9,476` solves with
  `drat-trim`), and `204/204` Python package unit tests (including
  `tests/test_c_solver.py`).
- **CI workflows**: `C solver correctness` (Linux GCC/Clang + macOS Clang
  release/debug + TSan + pinned `drat-trim` / `cake_lpr`), `C solver coverage
  fuzzing`, and `C solver independent incremental integration`.

### Measured Performance (Release, Non-PGO, Apple Silicon)
- **Evidence status (2026-11-25)**: no figure in this section has a checked-in
  artifact under `benchmark_results/` (its newest files are dated 2026-09-10),
  and the Kissat and `drat-trim` rows were not rerun. A rerun of the current
  release build on the cached corpora reproduced the default-mode `bsat` rows:
  `synthetic-hard` 118/118, `satlib-core` 492/492, `satlib-scale` 6,091/6,091,
  `satlib-hard` 914/919 (all `0` errors), and 1-thread SAT Competition 48/150
  (28 SAT, 20 UNSAT, PAR-2 `21.146 s`), one more than recorded below. `-j 4`
  counts vary between runs (26/60 on the 2023 subset in that rerun).
- **Wide-corpus & head-to-head verification** (`scripts/suite_runner.py` &
  `scripts/acceptance_gate.py`, independent full-clause SAT model checks, known
  ground truth, and real `drat-trim` binary DRAT proof checking):
  - **SAT Competition 2023–2025 head-to-head vs Kissat 4.0.4** (150 balanced
    Main Track instances `<= 3 MB`, 15 s timeout, 0 errors, 100% agreement):
    - **`bsat -j 4`**: **51 / 150 solved** (31 SAT, 20 UNSAT), PAR-2 `20.456 s`.
    - **`bsat`** (1-thread): **47 / 150 solved** (27 SAT, 20 UNSAT), PAR-2 `21.314 s` (leads Kissat 4.0.4 by `+5` instances and `1.01 s` PAR-2, winning both SAT and UNSAT; solves `brent_69_0`, `brent_63_0.2`, `brent_63_0.26`, `PRP_30_37`, `par32-4.shuffled`, `par32-2`, `mdp-32-10-sat`, `oddball_24_4`).
    - **`kissat 4.0.4`** (1-thread): **42 / 150 solved** (24 SAT, 18 UNSAT), PAR-2 `22.323 s`.
    - **`bsat --proof --binary-proof` + `drat-trim`**: **38 / 150 solved**, **18 / 18** solved SAT Comp UNSAT proofs independently verified (`0` failures), including `tseitin_grid_n11_m20`, `tseitin_d3_n162`, `bv_ILA_Piccolo_BEQ`, `bv_ILA_Piccolo_JALR`, `clique_n2_k10`, and `oddball_20_5` / `oddball_24_4`, plus `5/5` coupled 48-var in-search mixed-XOR UNSAT benchmarks verified in `0.012–0.018 s` (binary and text DRAT).
  - **`synthetic-hard` + `satlib-core`** (610 instances):
    - Default mode: **118/118** (`synthetic-hard`, PAR-2 `0.039 s`) + **492/492** (`satlib-core`, PAR-2 `0.051 s`), `0` errors.
    - Binary DRAT `--proof` + `drat-trim` (`acceptance_gate.py` with POSIX `setrlimit` containment): **118/118 (100.0%)** on `synthetic-hard` (`PAR-2 = 0.062 s`, `p50 = 0.011 s`, `p95 = 0.161 s`, `max RSS = 16.56 MiB`, `50/50` UNSAT proofs verified including `php_31_30` and `tseitin_v180`) + **482/492 (98.0%)** on `satlib-core` (`169/169` UNSAT proofs verified), `0` failures.
  - **`satlib-scale`** (6,091 instances): **6,091/6,091 (100.0%)**, `0` errors.
  - **`satlib-hard`** (919 instances, 10 s cap): **914/919 (99.5%)**, `0` errors.
- **Frozen industrial & targeted benchmarks**:
  - `cal3-query.cnf` (UNSAT, including verified DRAT proof): **`0.447 s`**
    (`20,294` conflicts when recorded; the current build takes `19,708`
    conflicts without a proof and `31,599` with `--proof`; vs. Kissat `1.045 s`).
  - `16c999d0` (planning, 50k vars / 284k clauses, UNSAT): **`25.68 s`** wall
    with `-j 4` (vs. Kissat `47.33 s`), `54.14 s` single-threaded (`26.1M` props/s).
  - `09cd83e2` (belpyramid, UNSAT): **`0.342 s`** (`8.81M` props/s).
  - `42f1311c` / `tseitin_grid_n11_m20` / `tseitin_d3_n162` (Tseitin XOR, UNSAT): **`0.004 s`–`0.017 s`** (`0` conflicts via GF(2) RREF, `drat-trim`-verified under `--proof`).
  - Mixed XOR+3SAT (`N=140`): **`0.005 s`** SAT / **`0.007 s`** UNSAT via
    dynamic in-search Bird/Han-Jiang GF(2) Gauss.
  - `PHP(12,11)` / `PHP(20,19)` / `PHP(31,30)` (UNSAT): **`0` conflicts** via
    exact symmetry breaking (`PHP(31,30)` solves in **`0.270 s`** with a
    `drat-trim`-verified binary DRAT proof; without `--proof` the current build
    needs `170` conflicts on `PHP(31,30)` and `0` up to `PHP(26,25)`).

### Residual Technical Limitations
- Hard cryptographic S-box / stream-cipher attacks (`asconhash*`) and large
  timetabling/argumentation families (`SC23_Timetable*`, `stb_*`) remain
  challenging single-thread targets (as they also are for single-thread Kissat
  on most of these families).
- In `--proof` mode, live in-search Bird/Han-Jiang GF(2) propagation runs with
  persistent Tseitin XOR-chain proofs on compact matrices (`<= 128` active rows,
  `<= 2,048` cumulative proof-row merges) and falls back cleanly at level 0 to
  standard CDCL on larger/denser XOR matrices to bound proof growth; root GF(2)
  RREF always runs and emits proofs. RAT-emitting passes (SBVA, AMO, symmetry
  breaking, GF(2) Tseitin XOR-chains) stay intentionally disabled in append-only
  RUP-only `BSAT_CERTIFICATES` journals so incremental certificate checking
  requires no RAT witness reconstruction.
- `--lrat` runs CDCL plus the RUP-only inprocessing subset: symmetry breaking,
  SBVA, AMO re-encoding, GF(2) Gauss and gate congruence are not run, so
  instances that depend on them can be far slower or produce very large proofs
  (`PHP(31,30)` does not finish within 120 s under `--lrat`, and a 100-variable
  Tseitin instance from `synthetic-hard` reaches a 60 s limit with a 1.8 GB
  proof, versus well under a second by default or with `--proof`).

### Deployer Responsibilities
- Production deployments must freeze their own workload-specific corpus,
  SLO policy JSON (starting from [scripts/acceptance_policy.example.json](scripts/acceptance_policy.example.json)),
  and host cgroup/container orchestration around [examples/contained_worker.c](examples/contained_worker.c).

---

## Release Gates

`1.0.0-dev` becomes release-ready only against a concrete target workload and
deployment envelope meeting all gates below:

| Gate | Required acceptance evidence |
|---|---|
| Exact candidate integrity | Clean tracked tree, pinned revision/compiler/flags/tool/input hashes, reproducible install and frozen ABI consumers; portable build flags for distribution |
| Correctness CI | All Linux GCC/Clang and macOS Clang release/ASan/UBSan jobs, thread sanitizer, independent integration, and fuzz jobs pass on the release candidate |
| Answer integrity | Every conclusive result in the acceptance corpus is independently verified (models against original CNF, UNSAT certificates against exact queries); UNKNOWN stays unfinished |
| Application semantics | Validate encoding and assumption protocol against representative application histories, including additions after conditional UNSAT or cancellation |
| Performance | Freeze representative holdouts and explicit throughput, tail-latency, memory, and proof-growth limits; compare references serially under matched budgets |
| Failure & recovery | Exercise quota/ENOMEM/worker-death and interrupted replay under the intended supervisor |
| Operational limits | Validate OS-level memory/storage/CPU containment and checker/export resource policy on the target host |
