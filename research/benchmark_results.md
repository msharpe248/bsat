# SAT Solver Benchmark Results

Comparison of research solvers vs production solvers on various problem types.

## Summary

**Solvers Tested:**
- **Production**: DPLL, CDCL, Schöning
- **Research**: CoBD-SAT, BB-CDCL, LA-CDCL, CGPM-SAT

**Total Problems**: 8

## Detailed Results

### Modular Problem (3 modules × 3 vars)

| Solver   | Result   |   Time (s) | Speedup   | Decisions   | Conflicts   | Notes   |
|:---------|:---------|-----------:|:----------|:------------|:------------|:--------|
| DPLL     | SAT      |     0.0001 | 0.91×     | -           | -           | -       |
| CDCL     | SAT      |     0.0001 | 1.00×     | -           | -           | -       |
| Schöning | SAT      |     0.0001 | 1.08×     | -           | -           | -       |
| CoBD-SAT | SAT      |     0.0005 | 0.15×     | -           | -           | Q=0.00  |
| BB-CDCL  | SAT      |     0.0028 | 0.03×     | -           | -           | BB=18%  |

### Backbone Problem (15 vars, 50% backbone)

| Solver   | Result   |   Time (s) | Speedup   | Decisions   | Conflicts   | Notes   |
|:---------|:---------|-----------:|:----------|:------------|:------------|:--------|
| DPLL     | SAT      |     0.0001 | 0.52×     | -           | -           | -       |
| CDCL     | SAT      |     0      | 1.00×     | -           | -           | -       |
| Schöning | SAT      |     0.0002 | 0.22×     | -           | -           | -       |
| CoBD-SAT | SAT      |     0.001  | 0.04×     | -           | -           | Q=0.00  |
| BB-CDCL  | SAT      |     0.0073 | 0.01×     | -           | -           | BB=93%  |

### Chain Problem (length=8)

| Solver   | Result   |   Time (s) | Speedup   | Decisions   | Conflicts   | Notes   |
|:---------|:---------|-----------:|:----------|:------------|:------------|:--------|
| DPLL     | SAT      |     0      | 0.88×     | -           | -           | -       |
| CDCL     | SAT      |     0      | 1.00×     | -           | -           | -       |
| Schöning | SAT      |     0.0001 | 0.34×     | -           | -           | -       |
| CoBD-SAT | SAT      |     0.0002 | 0.15×     | -           | -           | Q=0.00  |
| BB-CDCL  | SAT      |     0.0881 | 0.00×     | -           | -           | BB=73%  |

### Circuit Problem (4 gates)

| Solver   | Result   |   Time (s) | Speedup   | Decisions   | Conflicts   | Notes   |
|:---------|:---------|-----------:|:----------|:------------|:------------|:--------|
| DPLL     | SAT      |     0      | 0.92×     | -           | -           | -       |
| CDCL     | SAT      |     0      | 1.00×     | -           | -           | -       |
| Schöning | SAT      |     0      | 1.11×     | -           | -           | -       |
| CoBD-SAT | SAT      |     0.0003 | 0.12×     | -           | -           | Q=0.00  |
| BB-CDCL  | SAT      |     0.0021 | 0.01×     | -           | -           | BB=56%  |

### Random 3-SAT (10 vars, 35 clauses)

| Solver   | Result   |   Time (s) | Speedup   | Decisions   | Conflicts   | Notes   |
|:---------|:---------|-----------:|:----------|:------------|:------------|:--------|
| DPLL     | SAT      |     0.0001 | 103.40×   | -           | -           | -       |
| CDCL     | SAT      |     0.0108 | 1.00×     | -           | -           | -       |
| Schöning | SAT      |     0.0028 | 3.85×     | -           | -           | -       |
| CoBD-SAT | SAT      |     0.0013 | 8.24×     | -           | -           | Q=0.00  |
| BB-CDCL  | SAT      |     0.0565 | 0.19×     | -           | -           | BB=100% |

### Easy Problem

| Solver   | Result   |   Time (s) | Speedup   | Decisions   | Conflicts   | Notes   |
|:---------|:---------|-----------:|:----------|:------------|:------------|:--------|
| DPLL     | SAT      |     0      | 1.02×     | -           | -           | -       |
| CDCL     | SAT      |     0      | 1.00×     | -           | -           | -       |
| Schöning | SAT      |     0      | 1.49×     | -           | -           | -       |
| CoBD-SAT | SAT      |     0.0001 | 0.20×     | -           | -           | Q=0.00  |
| BB-CDCL  | SAT      |     0.0005 | 0.03×     | -           | -           | BB=20%  |

### Strong Backbone (18 vars, 70% backbone)

| Solver   | Result   |   Time (s) | Speedup   | Decisions   | Conflicts   | Notes   |
|:---------|:---------|-----------:|:----------|:------------|:------------|:--------|
| DPLL     | UNSAT    |     0.0001 | 0.25×     | -           | -           | -       |
| CDCL     | UNSAT    |     0      | 1.00×     | -           | -           | -       |
| Schöning | UNSAT    |     0.7039 | 0.00×     | -           | -           | -       |
| CoBD-SAT | UNSAT    |     0.0014 | 0.01×     | -           | -           | Q=0.00  |
| BB-CDCL  | UNSAT    |     6.4443 | 0.00×     | -           | -           | BB=0%   |

### Larger Modular (4 modules × 4 vars)

| Solver   | Result   |   Time (s) | Speedup   | Decisions   | Conflicts   | Notes   |
|:---------|:---------|-----------:|:----------|:------------|:------------|:--------|
| DPLL     | SAT      |     0.0002 | 14.42×    | -           | -           | -       |
| CDCL     | SAT      |     0.0031 | 1.00×     | -           | -           | -       |
| Schöning | SAT      |     0.0002 | 13.79×    | -           | -           | -       |
| CoBD-SAT | SAT      |     0.0013 | 2.38×     | -           | -           | Q=0.00  |
| BB-CDCL  | SAT      |     0.0084 | 0.36×     | -           | -           | BB=17%  |

## Analysis

### Key Findings

**Fastest Solver by Problem Type:**

- **Schöning**: Won on 3 problem(s)
- **CDCL**: Won on 3 problem(s)
- **DPLL**: Won on 2 problem(s)

