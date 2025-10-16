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
| DPLL     | SAT      |     0.0001 | 0.94×     | -           | -           | -       |
| CDCL     | SAT      |     0.0001 | 1.00×     | -           | -           | -       |
| Schöning | SAT      |     0.0001 | 0.63×     | -           | -           | -       |
| CoBD-SAT | SAT      |     0.0003 | 0.29×     | -           | -           | Q=0.00  |
| BB-CDCL  | SAT      |     0.0029 | 0.03×     | -           | -           | BB=18%  |
| LA-CDCL  | SAT      |     0.0016 | 0.05×     | 4           | -           | LA=100% |
| CGPM-SAT | SAT      |     0.0003 | 0.24×     | 8           | -           | GI=100% |

### Backbone Problem (15 vars, 50% backbone)

| Solver   | Result   |   Time (s) | Speedup   | Decisions   | Conflicts   | Notes   |
|:---------|:---------|-----------:|:----------|:------------|:------------|:--------|
| DPLL     | SAT      |     0.0001 | 0.64×     | -           | -           | -       |
| CDCL     | SAT      |     0      | 1.00×     | -           | -           | -       |
| Schöning | SAT      |     0.0004 | 0.10×     | -           | -           | -       |
| CoBD-SAT | SAT      |     0.0005 | 0.09×     | -           | -           | Q=0.00  |
| BB-CDCL  | SAT      |     0.007  | 0.01×     | -           | -           | BB=93%  |
| LA-CDCL  | SAT      |     0.0001 | 0.33×     | 1           | -           | LA=100% |
| CGPM-SAT | SAT      |     0.0005 | 0.09×     | 1           | -           | GI=100% |

### Chain Problem (length=8, good for LA-CDCL)

| Solver   | Result   |   Time (s) | Speedup   | Decisions   | Conflicts   | Notes   |
|:---------|:---------|-----------:|:----------|:------------|:------------|:--------|
| DPLL     | SAT      |     0      | 0.90×     | -           | -           | -       |
| CDCL     | SAT      |     0      | 1.00×     | -           | -           | -       |
| Schöning | SAT      |     0.0001 | 0.32×     | -           | -           | -       |
| CoBD-SAT | SAT      |     0.0002 | 0.11×     | -           | -           | Q=0.00  |
| BB-CDCL  | SAT      |     0.0828 | 0.00×     | -           | -           | BB=73%  |
| LA-CDCL  | SAT      |     0.0001 | 0.16×     | 3           | -           | LA=100% |
| CGPM-SAT | SAT      |     0.0002 | 0.10×     | 3           | -           | GI=100% |

### Circuit Problem (4 gates, good for CGPM-SAT)

| Solver   | Result   |   Time (s) | Speedup   | Decisions   | Conflicts   | Notes   |
|:---------|:---------|-----------:|:----------|:------------|:------------|:--------|
| DPLL     | SAT      |     0      | 0.98×     | -           | -           | -       |
| CDCL     | SAT      |     0      | 1.00×     | -           | -           | -       |
| Schöning | SAT      |     0      | 1.10×     | -           | -           | -       |
| CoBD-SAT | SAT      |     0.0002 | 0.21×     | -           | -           | Q=0.00  |
| BB-CDCL  | SAT      |     0.002  | 0.02×     | -           | -           | BB=56%  |
| LA-CDCL  | SAT      |     0.0001 | 0.24×     | 1           | -           | LA=100% |
| CGPM-SAT | SAT      |     0.0002 | 0.19×     | 2           | -           | GI=100% |

### Random 3-SAT (10 vars, 35 clauses)

| Solver   | Result   |   Time (s) | Speedup   | Decisions   | Conflicts   | Notes   |
|:---------|:---------|-----------:|:----------|:------------|:------------|:--------|
| DPLL     | SAT      |     0.0001 | 96.37×    | -           | -           | -       |
| CDCL     | SAT      |     0.0098 | 1.00×     | -           | -           | -       |
| Schöning | SAT      |     0.0008 | 12.84×    | -           | -           | -       |
| CoBD-SAT | SAT      |     0.0004 | 24.77×    | -           | -           | Q=0.00  |
| BB-CDCL  | SAT      |     0.0499 | 0.20×     | -           | -           | BB=100% |
| LA-CDCL  | SAT      |     0.0012 | 7.90×     | 4           | 3           | LA=100% |
| CGPM-SAT | SAT      |     0.0005 | 18.28×    | 8           | -           | GI=100% |

### Random 3-SAT (12 vars, 40 clauses - was hanging)

| Solver   | Result   |   Time (s) | Speedup   | Decisions   | Conflicts   | Notes   |
|:---------|:---------|-----------:|:----------|:------------|:------------|:--------|
| DPLL     | SAT      |     0.0001 | 1180.30×  | -           | -           | -       |
| CDCL     | UNSAT    |     0.1652 | 1.00×     | -           | -           | -       |
| Schöning | SAT      |     0.0011 | 145.77×   | -           | -           | -       |
| CoBD-SAT | SAT      |     0.0006 | 267.30×   | -           | -           | Q=0.00  |
| BB-CDCL  | SAT      |     0.0998 | 1.66×     | -           | -           | BB=92%  |
| LA-CDCL  | SAT      |     0.0018 | 90.76×    | 6           | 3           | LA=100% |
| CGPM-SAT | SAT      |     0.0008 | 219.39×   | 8           | -           | GI=100% |

### Easy Problem (shows overhead)

| Solver   | Result   |   Time (s) | Speedup   | Decisions   | Conflicts   | Notes   |
|:---------|:---------|-----------:|:----------|:------------|:------------|:--------|
| DPLL     | SAT      |     0      | 1.00×     | -           | -           | -       |
| CDCL     | SAT      |     0      | 1.00×     | -           | -           | -       |
| Schöning | SAT      |     0      | 1.05×     | -           | -           | -       |
| CoBD-SAT | SAT      |     0.0001 | 0.23×     | -           | -           | Q=0.00  |
| BB-CDCL  | SAT      |     0.0005 | 0.03×     | -           | -           | BB=20%  |
| LA-CDCL  | SAT      |     0.0001 | 0.22×     | 2           | -           | LA=100% |
| CGPM-SAT | SAT      |     0.0001 | 0.25×     | 4           | -           | GI=100% |

### Strong Backbone (18 vars, 70% backbone)

| Solver   | Result   |   Time (s) | Speedup   | Decisions   | Conflicts   | Notes   |
|:---------|:---------|-----------:|:----------|:------------|:------------|:--------|
| DPLL     | UNSAT    |     0.0001 | 0.30×     | -           | -           | -       |
| CDCL     | UNSAT    |     0      | 1.00×     | -           | -           | -       |
| Schöning | UNSAT    |     0.6754 | 0.00×     | -           | -           | -       |
| CoBD-SAT | UNSAT    |     0.0005 | 0.04×     | -           | -           | Q=0.00  |
| BB-CDCL  | UNSAT    |     0.0001 | 0.21×     | -           | -           | BB=0%   |
| LA-CDCL  | UNSAT    |     0      | 0.44×     | -           | 1           | LA=0%   |
| CGPM-SAT | UNSAT    |     0.0001 | 0.26×     | -           | -           | GI=0%   |

## Analysis

### Key Findings

**Fastest Solver by Problem Type:**

- **CDCL**: Won on 4 problem(s)
- **Schöning**: Won on 2 problem(s)
- **DPLL**: Won on 2 problem(s)

