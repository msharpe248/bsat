# SAT Competition 2025 Benchmarks

This directory is the expected location for benchmark instances from the **SAT Competition 2025**.

**The instances are not included in the repository and there is no download script for them here** — a fresh checkout contains only this README. See [Downloading Benchmarks](#downloading-benchmarks). The dataset figures below describe the full set as originally downloaded and cannot be checked against a fresh checkout.

## Source

**Official Website**: https://satcompetition.github.io/2025/

These benchmarks are the **official test instances** used in the 2025 International SAT Competition, representing state-of-the-art challenging SAT problems from various domains.

## Dataset Overview

- **Total Instances**: 399 CNF files
- **Families**: 99 different problem families
- **Format**: DIMACS CNF
- **Size Range**: 7.9 KB to 7.0 GB per file
- **Difficulty**: Extremely challenging - designed to stress-test industrial solvers

## Problem Families

The benchmarks are organized into 99 subdirectories by problem family:

### Examples of Families

- `algorithm-equivalence-checking/` - Hardware and algorithm verification
- `argumentation/` - Abstract argumentation frameworks
- `battleship/` - Battleship puzzle instances
- `coloring/` - Graph coloring problems
- `cryptography/` - Cryptographic problems
- `fpga-routing/` - FPGA routing problems
- `hamiltonian/` - Hamiltonian path/cycle problems
- `hardware-model-checking/` - Hardware verification
- `md5-equivalence-checking/` - MD5 hash verification
- `pigeon-hole/` - Pigeonhole principle instances
- `scheduling/` - Scheduling problems
- `tseitin-formulas/` - Tseitin transformation instances
- ... and 87 more families

## Instance Characteristics

### File Sizes
- **Smallest**: 7.9 KB (subset-cardinality family)
- **Largest**: 7.0 GB (md5-equivalence-checking family)
- **Median**: ~300 KB

### Problem Sizes
- **Variables**: 149 to 10,000,000+
- **Clauses**: 606 to 100,000,000+
- **Clause/Variable Ratios**: Varies widely by family

## Difficulty Level

⚠️ **WARNING**: These instances are **extremely challenging**

- Designed to push **industrial SAT solvers** (MiniSat, Glucose, CryptoMiniSat, etc.) to their limits
- Many instances require **hours or days** even for state-of-the-art solvers
- Basic DPLL/CDCL implementations will **timeout on most instances**
- Only a handful of the smallest instances (< 200 variables) are solvable by simple implementations

## Usage

### For Testing Limits
These benchmarks are useful for:
- Testing solver scalability
- Understanding what real-world SAT problems look like
- Comparing against industrial-strength solvers
- Identifying areas for optimization

### For Development/Learning
**Not recommended** for:
- Basic solver validation (use `simple_tests` instead)
- Performance comparison of basic implementations (use `medium_tests` instead)
- Quick benchmarking (most will timeout)

## Downloading Benchmarks

The CNF files are **not checked into git** due to their size (54 GB total).

### Full Set (manual)
There is no script in this repository that downloads the full set (the
`download_all_benchmarks.py` previously referenced here does not exist).
Obtain the instances from https://satcompetition.github.io/2025/ and place the
uncompressed files as `<family>/<instance>.cnf` in this directory; that is the
layout `research/benchmarks/run_all_benchmarks.py` and
`research/benchmarks/create_small_test_set.py` expect.

### Size-Filtered Sample (scripted)
For a small, labelled sample of the 2025 main track, use the on-demand corpus
fetcher from the repository root:

```bash
python3 competition/c/scripts/fetch_corpora.py fetch satcomp-2025 --limit 60 --max-mb 2
python3 competition/c/scripts/suite_runner.py --corpus satcomp-2025 --timeout 30 --workers 2
```

This downloads compressed `.cnf.xz` files into the gitignored
`dataset/corpora/satcomp-2025/` (not into this directory) for use with
`suite_runner.py` and the C solver; the Python benchmark scripts below do not
read that location.

## Running Benchmarks

Requires the instances to be downloaded into this directory first. From the repository root:

```bash
cd research/benchmarks

# Run with 120s timeout (expect many timeouts)
./run_all_benchmarks.py ../../dataset/sat_competition2025 -t 120

# Run only on specific solvers
./run_all_benchmarks.py ../../dataset/sat_competition2025 -t 120 -s CDCL CGPM-SAT

# Test on smallest instances only (create the subset first with ./create_small_test_set.py)
./run_all_benchmarks.py ../../dataset/sat_competition2025_small -t 60
```

## Expected Results for Basic Solvers

Based on testing with basic DPLL, CDCL, and research solver implementations:

- **Instances < 200 variables**: Some will solve in seconds
- **Instances 200-500 variables**: Most will timeout (> 30s)
- **Instances > 500 variables**: Nearly all will timeout
- **Large instances (> 10,000 variables)**: Will timeout immediately or run out of memory

The **CGPM-SAT** solver (graph-based heuristics) shows the best performance but still struggles with larger competition instances.

## Comparison with Generated Tests

For meaningful benchmarking of basic solvers, use the generated test suites:

| Dataset | Solvability | Best For |
|---------|-------------|----------|
| `simple_tests` | All solvable | Quick validation |
| `medium_tests` | Mostly solvable | Performance comparison |
| `sat_competition2025_small` | Some timeout | Testing limits |
| `sat_competition2025` | Most timeout | Aspirational goals |

## Credits

- **SAT Competition 2025**: https://satcompetition.github.io/2025/
- **Benchmark Database**: https://benchmark-database.de/
- **Competition Organizers**: International SAT Competition organizing committee

## License

These benchmarks are provided by the SAT Competition organizers. Please refer to the official website for licensing and usage terms.

## Directory Structure

Layout once the full set has been downloaded (only `README.md` is present in a fresh checkout):

```
sat_competition2025/
├── README.md                                    # This file
├── algorithm-equivalence-checking/
│   └── *.cnf                                   # 7 instances
├── argumentation/
│   └── *.cnf                                   # 20 instances
├── battleship/
│   └── *.cnf                                   # 5 instances
├── ...                                         # 96 more families
└── tseitin-formulas/
    └── *.cnf                                   # Various instances
```

Total: **99 families, 399 instances**
