# Problem Encodings

How to encode real-world problems as SAT formulas. This guide shows practical applications of SAT solving with complete examples.

## Table of Contents

1. [Overview](#overview)
2. [Graph Coloring](#graph-coloring)
3. [Sudoku](#sudoku)
4. [N-Queens](#n-queens)
5. [General Encoding Principles](#general-encoding-principles)

---

## Overview

Many real-world problems can be encoded as Boolean satisfiability (SAT) problems. Once encoded, we can use SAT solvers to find solutions automatically.

**Why encode problems as SAT?**
- ✅ Leverage decades of SAT solver optimization
- ✅ Automatic solving - no custom algorithms needed
- ✅ Proof of optimality (for UNSAT cases)
- ✅ Works for NP-complete problems

**Common encoding pattern:**
1. Define Boolean variables representing decisions
2. Add clauses for each constraint
3. Solve using SAT solver
4. Decode solution back to original problem

---

## Graph Coloring

**Problem**: Assign colors to graph vertices such that no adjacent vertices share the same color.

**Applications**:
- Register allocation in compilers
- Scheduling (time slots = colors, conflicts = edges)
- Frequency assignment in wireless networks
- Map coloring

### Encoding

**Variables**: `x_v_c` = "vertex v has color c"

**Constraints**:
```
1. Each vertex has at least one color:
   (x_v_0 ∨ x_v_1 ∨ ... ∨ x_v_k)

2. Each vertex has at most one color:
   (¬x_v_i ∨ ¬x_v_j) for all i ≠ j

3. Adjacent vertices have different colors:
   (¬x_u_c ∨ ¬x_v_c) for each edge (u,v) and color c
```

### Example: Triangle Graph

```python
from examples.encodings.graph_coloring import encode_graph_coloring, decode_coloring
from bsat import solve_sat

# Triangle: needs 3 colors
edges = [(0, 1), (1, 2), (2, 0)]
cnf = encode_graph_coloring(edges, num_vertices=3, num_colors=3)

solution = solve_sat(cnf)
if solution:
    coloring = decode_coloring(solution, num_vertices=3, num_colors=3)
    print(f"Coloring: {coloring}")  # e.g., {0: 0, 1: 1, 2: 2}
```

### Finding Chromatic Number

Try k=1, k=2, k=3, ... until SAT:

```python
for k in range(1, num_vertices + 1):
    cnf = encode_graph_coloring(edges, num_vertices, num_colors=k)
    if solve_sat(cnf):
        print(f"Chromatic number: {k}")
        break
```

**See**: [examples/encodings/graph_coloring.py](../examples/encodings/graph_coloring.py)

---

## Sudoku

**Problem**: Fill 9×9 grid with digits 1-9 such that each row, column, and 3×3 box contains each digit exactly once.

**Applications**:
- Constraint satisfaction problems
- Logic puzzles
- Timetabling

### Encoding

**Variables**: `x_r_c_n` = "cell (row r, col c) contains number n"

**Constraints**:
```
1. Each cell has exactly one number:
   - At least one: (x_r_c_1 ∨ x_r_c_2 ∨ ... ∨ x_r_c_9)
   - At most one: (¬x_r_c_i ∨ ¬x_r_c_j) for i ≠ j

2. Each row contains each number exactly once:
   - Similar clauses across columns

3. Each column contains each number exactly once:
   - Similar clauses across rows

4. Each 3×3 box contains each number exactly once:
   - Similar clauses within each box

5. Given clues are fixed:
   - (x_r_c_n) for each clue
```

### Example: Solving Sudoku

```python
from examples.encodings.sudoku import encode_sudoku, decode_sudoku, print_sudoku
from bsat import solve_cdcl

# Define clues as (row, col) -> number
clues = {
    (0, 0): 5, (0, 1): 3, (0, 4): 7,
    (1, 0): 6, (1, 3): 1, (1, 4): 9,
    # ... more clues
}

cnf = encode_sudoku(size=9, clues=clues)
solution = solve_cdcl(cnf)

if solution:
    grid = decode_sudoku(solution, size=9)
    print_sudoku(grid)
```

### Performance Notes

- Easy Sudoku: ~1000 clauses, solves instantly
- Hard Sudoku: Same encoding, CDCL handles efficiently
- Minimal (17 clues): Still solvable in < 1 second

**See**: [examples/encodings/sudoku.py](../examples/encodings/sudoku.py)

---

## N-Queens

**Problem**: Place N queens on N×N chessboard such that no two queens attack each other (same row, column, or diagonal).

**Applications**:
- Constraint satisfaction
- Backtracking algorithms benchmark
- Parallel processing task assignment

### Encoding

**Variables**: `q_r_c` = "queen at row r, column c"

**Constraints**:
```
1. Exactly one queen per row:
   - At least one: (q_r_0 ∨ q_r_1 ∨ ... ∨ q_r_N)
   - At most one: (¬q_r_i ∨ ¬q_r_j) for i ≠ j

2. At most one queen per column:
   (¬q_i_c ∨ ¬q_j_c) for i ≠ j

3. At most one queen per diagonal (/):
   (¬q_i_j ∨ ¬q_k_l) if i-j == k-l and (i,j) ≠ (k,l)

4. At most one queen per anti-diagonal (\\):
   (¬q_i_j ∨ ¬q_k_l) if i+j == k+l and (i,j) ≠ (k,l)
```

### Example: 8-Queens

```python
from examples.encodings.n_queens import encode_n_queens, decode_queens, print_board
from bsat import solve_cdcl

cnf = encode_n_queens(n=8)
solution = solve_cdcl(cnf)

if solution:
    queens = decode_queens(solution, n=8)
    print_board(queens, n=8)
    # Prints board with Q's showing queen positions
```

### Interesting Facts

- **2-Queens & 3-Queens**: Impossible (UNSAT)
- **4-Queens**: 2 distinct solutions
- **8-Queens**: 92 distinct solutions
- **N-Queens**: Solutions exist for all N ≥ 4

**See**: [examples/encodings/n_queens.py](../examples/encodings/n_queens.py)

---

## General Encoding Principles

### 1. Choose Variables Carefully

**Good variable design**:
- Each variable represents a clear decision
- Variable names are descriptive (`x_row_col_value` not `x123`)
- Minimize total variables when possible

### 2. Encode "Exactly One" Efficiently

For "exactly one of {x₁, x₂, ..., xₙ} is true":

```python
# At least one
clauses.append(Clause([Literal(x_i, False) for i in range(n)]))

# At most one (pairwise)
for i in range(n):
    for j in range(i + 1, n):
        clauses.append(Clause([Literal(x_i, True), Literal(x_j, True)]))
```

This uses O(n²) clauses. For large n, consider sequential encoding or commander variables.

### 3. Leverage Problem Structure

**Symmetry breaking**:
- Add clauses to eliminate symmetric solutions
- Reduces search space

**Example for N-Queens**:
```python
# Force first queen in first half of first row
clauses.append(Clause([Literal(f"q_0_{c}", False) for c in range(n//2)]))
```

### 4. Test Incrementally

1. Start with small instances (4×4 Sudoku, not 9×9)
2. Verify encoding correctness
3. Scale up

### 5. Choose the Right Solver

| Problem Size | Recommended Solver |
|-------------|-------------------|
| Small (< 100 vars) | `solve_sat()` (DPLL) |
| Medium (100-1000 vars) | `solve_cdcl()` |
| Large (> 1000 vars) | `solve_cdcl()` or try `solve_walksat()` |

---

## Encoding Complexity

### Variables

| Problem | Variables | Formula |
|---------|-----------|---------|
| **Graph Coloring** | O(V·k) | V vertices, k colors |
| **Sudoku** | O(n³) | n×n grid |
| **N-Queens** | O(n²) | n×n board |

### Clauses

| Problem | Clauses | Breakdown |
|---------|---------|-----------|
| **Graph Coloring** | O(V·k² + E·k) | "At most one" + edge constraints |
| **Sudoku** | O(n⁴) | Row/col/box constraints |
| **N-Queens** | O(n³) | Diagonal constraints |

---

## Advanced Techniques

### 1. Optimization via Binary Search

Find minimum colors needed:

```python
def find_chromatic_number(edges, num_vertices):
    low, high = 1, num_vertices
    best = high

    while low <= high:
        mid = (low + high) // 2
        cnf = encode_graph_coloring(edges, num_vertices, num_colors=mid)

        if solve_sat(cnf):
            best = mid
            high = mid - 1  # Try fewer colors
        else:
            low = mid + 1   # Need more colors

    return best
```

### 2. Incremental SAT

Add clues incrementally:

```python
# Start with base Sudoku encoding
cnf = encode_sudoku(size=9, clues={})

# Add clues one at a time
for (r, c), n in clues.items():
    # Add unit clause for this clue
    cnf.clauses.append(Clause([Literal(f"x_{r}_{c}_{n}", False)]))

solution = solve_cdcl(cnf)
```

### 3. UNSAT Cores

If no solution, find minimal unsatisfiable core (which constraints conflict).

---

## Examples

Run the complete examples:

```bash
# Graph coloring
python examples/encodings/graph_coloring.py

# Sudoku solver
python examples/encodings/sudoku.py

# N-Queens
python examples/encodings/n_queens.py
```

---

## Further Reading

- [CNF Data Structures](cnf.md)
- [CDCL Solver](cdcl-solver.md) - Best for these problems
- [Theory & Complexity](theory.md)
- [Reading List](reading-list.md) - Papers on SAT encoding

---

## Next Steps

**Try encoding your own problems:**
1. Identify decisions (variables)
2. List constraints (clauses)
3. Encode using BSAT
4. Solve and decode

**More problem ideas:**
- Boolean circuit SAT
- Hamiltonian path
- Job shop scheduling
- Bin packing
- 3-SAT to Vertex Cover reduction
