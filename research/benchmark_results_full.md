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
| DPLL     | SAT      |     0.0001 | 0.98×     | -           | -           | -       |
| CDCL     | SAT      |     0.0001 | 1.00×     | -           | -           | -       |
| Schöning | SAT      |     0.0001 | 1.37×     | -           | -           | -       |
| CoBD-SAT | SAT      |     0.0005 | 0.19×     | -           | -           | Q=0.00  |
| BB-CDCL  | SAT      |     0.0026 | 0.03×     | -           | -           | BB=18%  |
| LA-CDCL  | SAT      |     0.0018 | 0.05×     | 5           | -           | LA=100% |
| CGPM-SAT | SAT      |     0.0004 | 0.23×     | 8           | -           | GI=100% |

### Backbone Problem (15 vars, 50% backbone)

| Solver   | Result   |   Time (s) | Speedup   | Decisions   | Conflicts   | Notes   |
|:---------|:---------|-----------:|:----------|:------------|:------------|:--------|
| DPLL     | SAT      |     0.0001 | 0.59×     | -           | -           | -       |
| CDCL     | SAT      |     0      | 1.00×     | -           | -           | -       |
| Schöning | SAT      |     0.0002 | 0.16×     | -           | -           | -       |
| CoBD-SAT | SAT      |     0.0011 | 0.03×     | -           | -           | Q=0.00  |
| BB-CDCL  | SAT      |     0.0072 | 0.01×     | -           | -           | BB=93%  |
| LA-CDCL  | SAT      |     0.0001 | 0.27×     | 1           | -           | LA=100% |
| CGPM-SAT | SAT      |     0.0005 | 0.07×     | 1           | -           | GI=100% |

### Chain Problem (length=8, good for LA-CDCL)

| Solver   | Result   |   Time (s) | Speedup   | Decisions   | Conflicts   | Notes   |
|:---------|:---------|-----------:|:----------|:------------|:------------|:--------|
| DPLL     | SAT      |     0      | 0.86×     | -           | -           | -       |
| CDCL     | SAT      |     0      | 1.00×     | -           | -           | -       |
| Schöning | SAT      |     0.0001 | 0.17×     | -           | -           | -       |
| CoBD-SAT | SAT      |     0.0001 | 0.13×     | -           | -           | Q=0.00  |
| BB-CDCL  | SAT      |     0.0838 | 0.00×     | -           | -           | BB=73%  |
| LA-CDCL  | SAT      |     0.0001 | 0.15×     | 3           | -           | LA=100% |
| CGPM-SAT | SAT      |     0.0002 | 0.09×     | 3           | -           | GI=100% |

### Circuit Problem (4 gates, good for CGPM-SAT)

| Solver   | Result   |   Time (s) | Speedup   | Decisions   | Conflicts   | Notes   |
|:---------|:---------|-----------:|:----------|:------------|:------------|:--------|
| DPLL     | SAT      |     0      | 0.87×     | -           | -           | -       |
| CDCL     | SAT      |     0      | 1.00×     | -           | -           | -       |
| Schöning | SAT      |     0.0001 | 0.26×     | -           | -           | -       |
| CoBD-SAT | SAT      |     0.0003 | 0.10×     | -           | -           | Q=0.00  |
| BB-CDCL  | SAT      |     0.0019 | 0.02×     | -           | -           | BB=56%  |
| LA-CDCL  | SAT      |     0.0001 | 0.25×     | 1           | -           | LA=100% |
| CGPM-SAT | SAT      |     0.0002 | 0.20×     | 2           | -           | GI=100% |

### Random 3-SAT (10 vars, 35 clauses)

| Solver   | Result   |   Time (s) | Speedup   | Decisions   | Conflicts   | Notes   |
|:---------|:---------|-----------:|:----------|:------------|:------------|:--------|
| DPLL     | SAT      |     0.0001 | 101.27×   | -           | -           | -       |
| CDCL     | SAT      |     0.0099 | 1.00×     | -           | -           | -       |
| Schöning | SAT      |     0.0001 | 165.17×   | -           | -           | -       |
| CoBD-SAT | SAT      |     0.0011 | 9.22×     | -           | -           | Q=0.00  |
| BB-CDCL  | SAT      |     0.0503 | 0.20×     | -           | -           | BB=100% |
| LA-CDCL  | SAT      |     0.001  | 10.35×    | 3           | 2           | LA=100% |
| CGPM-SAT | SAT      |     0.0006 | 15.95×    | 7           | -           | GI=100% |

### Random 3-SAT (12 vars, 40 clauses - was hanging)

| Solver   | Result   |   Time (s) | Speedup   | Decisions   | Conflicts   | Notes   |
|:---------|:---------|-----------:|:----------|:------------|:------------|:--------|
| DPLL     | SAT      |     0.0001 | 1222.42×  | -           | -           | -       |
| CDCL     | UNSAT    |     0.1722 | 1.00×     | -           | -           | -       |
| Schöning | SAT      |     0.0127 | 13.56×    | -           | -           | -       |
| CoBD-SAT | SAT      |     0.0015 | 112.44×   | -           | -           | Q=0.00  |
| BB-CDCL  | SAT      |     0.1003 | 1.72×     | -           | -           | BB=92%  |
| LA-CDCL  | SAT      |     0.0007 | 250.33×   | 3           | -           | LA=100% |
| CGPM-SAT | SAT      |     0.0009 | 193.07×   | 8           | -           | GI=100% |

### Easy Problem (shows overhead)

| Solver   | Result   |   Time (s) | Speedup   | Decisions   | Conflicts   | Notes   |
|:---------|:---------|-----------:|:----------|:------------|:------------|:--------|
| DPLL     | SAT      |     0      | 0.87×     | -           | -           | -       |
| CDCL     | SAT      |     0      | 1.00×     | -           | -           | -       |
| Schöning | SAT      |     0      | 1.00×     | -           | -           | -       |
| CoBD-SAT | SAT      |     0.0001 | 0.20×     | -           | -           | Q=0.00  |
| BB-CDCL  | SAT      |     0.0005 | 0.03×     | -           | -           | BB=20%  |
| LA-CDCL  | SAT      |     0.0001 | 0.24×     | 2           | -           | LA=100% |
| CGPM-SAT | SAT      |     0.0001 | 0.20×     | 4           | -           | GI=100% |

### Strong Backbone (18 vars, 70% backbone)

| Solver   | Result   |   Time (s) | Speedup   | Decisions   | Conflicts   | Notes   |
|:---------|:---------|-----------:|:----------|:------------|:------------|:--------|
| DPLL     | UNSAT    |     0.0001 | 0.30×     | -           | -           | -       |
| CDCL     | UNSAT    |     0      | 1.00×     | -           | -           | -       |
| Schöning | UNSAT    |     0.6798 | 0.00×     | -           | -           | -       |
| CoBD-SAT | UNSAT    |     0.0009 | 0.02×     | -           | -           | Q=0.00  |
| BB-CDCL  | UNSAT    |     6.1964 | 0.00×     | -           | -           | BB=0%   |
| LA-CDCL  | UNSAT    |     0.0001 | 0.38×     | -           | 1           | LA=0%   |
| CGPM-SAT | UNSAT    |     0.0001 | 0.26×     | -           | -           | GI=0%   |

## Analysis

### Key Findings

**Fastest Solver by Problem Type:**

- **CDCL**: Won on 5 problem(s)
- **Schöning**: Won on 2 problem(s)
- **DPLL**: Won on 1 problem(s)

