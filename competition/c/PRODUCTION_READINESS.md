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
| One-shot DIMACS solving | CLI (`bin/bsat`), SAT/UNSAT/UNKNOWN, text or binary DRAT | Strict DIMACS parser, model self-check, `validate.py` oracle/RUP suite, independent `drat-trim` CI |
| Multi-core portfolio solving | `-j <N>` / `--threads <N>` (`1..64`), LBD-filtered sharing (`size <= 8, LBD <= 3`), dedicated unit ring, canonical hash dedup, 8 profiles, atomic stop | Non-proof one-shot mode; TSan-clean ring buffers (`MODE=tsan`), 8 MiB worker stacks, only logically-equivalent workers export clauses |
| Public C/C++ & Python embedding | Opaque ABI v1 (`bsat.h`), Linux and macOS C11/POSIX, plus zero-dependency Python `ctypes` wrapper (`src/bsat/c_solver.py`) | [API_CONTRACT.md](API_CONTRACT.md), [INSTALLING.md](INSTALLING.md), `tests/test_c_solver.py`; internal `solver.h` layouts are not stable ABI |
| Incremental additions & assumptions | Copied permanent clauses, temporary assumptions, model and sufficient failed core, `solver_freeze`/`solver_melt` | [API_CONTRACT.md](API_CONTRACT.md); destructive inprocessing and non-RUP passes stay disabled on assumption calls |
| Retained learning | Opt-in `BSAT_REUSE_LEARNTS`, including compatible conditional UNSAT and budget slices | [INCREMENTAL_REUSE.md](docs/INCREMENTAL_REUSE.md); cancellation or incompatible state rebuilds from permanent input |
| Certified incremental queries | `BSAT_CERTIFICATES`, alone or with reuse; exact CNF and binary proof export | [RETAINED_CERTIFICATES.md](docs/RETAINED_CERTIFICATES.md); conservative non-destructive options, synchronous export |
| Proof-producing certified probing | Opt-in `BSAT_CERTIFIED_PROBING` (requires `BSAT_CERTIFICATES`) | Bounded RUP units preserve variables and assumptions; other transformations remain disabled |
| IPASIR 1.0 | Installed adapter (`ipasir.h`), assumptions, terminate and learn callbacks | [IPASIR.md](docs/IPASIR.md); adapter may introduce assumption variables |
| Independent concurrent handles | Distinct handles may solve concurrently | [EMBEDDING.md](docs/EMBEDDING.md), TSan and thread-local CPU budget tests; serialize calls per handle |
| Query controls & statistics | Mutable CPU/conflict/decision budgets and latest-query snapshots | CPU limits use solving-thread time; `0` means unlimited; fresh checkpoints in non-BCP inner loops |
| Service limits, quotas & recovery | Cooperative wall/capacity checks, atomic journal-byte ceiling, killed-worker replay examples | [SERVICE_RESOURCES.md](docs/SERVICE_RESOURCES.md), [SERVICE_RECOVERY.md](docs/SERVICE_RECOVERY.md); tested ENOMEM and SIGKILL recovery, not a durable transaction service |
| Independent UNSAT verification | DRAT (`drat-trim`) and DRAT-to-LRAT (`cake_lpr`) | [VERIFIED_CHECKING.md](docs/VERIFIED_CHECKING.md); checks the exact CNF, not upstream application encoding |
| Fuzzing & failure injection | LibFuzzer API/parser targets, ASan/UBSan/TSan, deterministic OOM injection | [FUZZING.md](docs/FUZZING.md), [tests/FEATURE_COVERAGE.md](tests/FEATURE_COVERAGE.md) |

---

## Current Verification & Benchmark Evidence

### Automated Verification
- **Unit & integration tests**: `68/68` C tests (including
  `test_differential_engines.c` across baseline, full, aggressive BCE/elim/SBVA,
  and `-j 4` portfolio) pass across `release`, `diagnostic`, and `debug`
  (ASan + UBSan) builds, plus ThreadSanitizer (`MODE=tsan`), `fault-test`
  (`3,526` deterministic OOM injection cases), `recovery-fault-test`,
  `embedding-test`, `soak-test`, `package-test`, `application_histories`,
  `check_query_diagnostics`, `check_deadlines` (`0.000 s` release / `0.004 s`
  debug max CPU overrun), `check_process_failures`, `validate.py` with
  `drat-trim`, and `203/203` Python package unit tests (including
  `tests/test_c_solver.py`).
- **CI workflows**: `C solver correctness` (Linux GCC/Clang + macOS Clang
  release/debug + TSan + pinned `drat-trim` / `cake_lpr`), `C solver coverage
  fuzzing`, and `C solver independent incremental integration`.

### Measured Performance (Release, Non-PGO, Apple Silicon)
- **Wide-corpus & head-to-head verification** (`scripts/suite_runner.py`,
  independent full-clause SAT model checks, known ground truth, and real
  `drat-trim` binary DRAT proof checking):
  - **SAT Competition 2023–2025 head-to-head vs Kissat 4.0.4** (150 balanced
    Main Track instances `<= 3 MB`, 15 s timeout, 0 errors, 100% agreement):
    - **`bsat -j 4`**: **43 / 150 solved** (23 SAT, 20 UNSAT), PAR-2 `21.914 s`, median `0.150 s`.
    - **`kissat 4.0.4`** (1-thread): **40 / 150 solved** (23 SAT, 17 UNSAT), PAR-2 `22.547 s`, median `0.891 s`.
    - **`bsat`** (1-thread): **39 / 150 solved** (20 SAT, 19 UNSAT), PAR-2 `22.726 s`, median `0.196 s`.
    - **`bsat --proof --binary-proof` + `drat-trim`**: **16 / 16** solved SAT Comp UNSAT proofs independently verified (`0` failures), including `bv_ILA_Piccolo_BEQ` (`1.54 s`), `bv_ILA_Piccolo_JALR` (`2.89 s`), `clique_n2_k10` (SBVA RAT), and `oddball_20_5` / `oddball_24_4`.
  - **`synthetic-hard` + `satlib-core`** (610 instances):
    - Default mode: **118/118** (`synthetic-hard`, PAR-2 `0.039 s`) + **492/492** (`satlib-core`, PAR-2 `0.051 s`), `0` errors.
    - Binary DRAT `--proof` + `drat-trim`: **206/206** solved UNSAT proofs (including proof-producing SBVA on `php_13_12.cnf` in `2.58 s`) independently verified, `0` failures.
  - **`satlib-scale`** (6,091 instances): **6,091/6,091 (100.0%)**, `0` errors.
  - **`satlib-hard`** (919 instances, 10 s cap): **914/919 (99.5%)**, `0` errors.
- **Frozen industrial & targeted benchmarks**:
  - `cal3-query.cnf` (UNSAT, including verified DRAT proof): **`0.447 s`**
    (`20,294` conflicts, vs. Kissat `1.045 s`).
  - `16c999d0` (planning, 50k vars / 284k clauses, UNSAT): **`25.68 s`** wall
    with `-j 4` (vs. Kissat `47.33 s`), `54.14 s` single-threaded (`26.1M` props/s).
  - `09cd83e2` (belpyramid, UNSAT): **`0.342 s`** (`8.81M` props/s).
  - `42f1311c` (Tseitin XOR, UNSAT): **`0.004 s`** (`0` conflicts via GF(2) RREF).
  - Mixed XOR+3SAT (`N=140`): **`0.005 s`** SAT / **`0.007 s`** UNSAT via
    dynamic in-search Bird/Han-Jiang GF(2) Gauss.
  - `PHP(12,11)` / `PHP(20,19)` / `PHP(30,29)` (UNSAT): **`0.062 s`** (`0`) /
    **`0.006 s`** (`0`) / **`0.055 s`** (`80` conflicts) via exact BreakID
    symmetry breaking.

### Known Open Gaps
- Hard cryptographic S-box / stream-cipher attacks, GF(2) tensor-rank (`brent_*`),
  and large scheduling/argumentation instances remain open performance targets.
- Non-RUP engines without DRAT emission (GF(2) Gauss, symmetry breaking, AMO
  re-encoding) remain disabled in `--proof` and `BSAT_CERTIFICATES` modes (SBVA
  emits RAT-valid DRAT in `--proof` mode and stays disabled in append-only
  `BSAT_CERTIFICATES` journals).
- Deployment-specific acceptance corpora, numerical SLOs, durable supervision,
  and host OS containment policies remain application-level work.

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
