"""
Sudoku Solver using SAT Encoding

Sudoku is a constraint satisfaction problem that can be elegantly encoded as SAT.
For a 9x9 Sudoku, we need to ensure:
1. Each cell contains exactly one number (1-9)
2. Each row contains all numbers 1-9
3. Each column contains all numbers 1-9
4. Each 3x3 box contains all numbers 1-9

This demonstrates how SAT solvers can solve popular puzzles efficiently.
"""

from bsat import CNFExpression, Clause, Literal, solve_sat, solve_cdcl
from typing import List, Dict, Optional


def encode_sudoku(size: int = 9, clues: Optional[Dict[tuple, int]] = None) -> CNFExpression:
    """
    Encode Sudoku as SAT problem.

    Variables: x_r_c_n means "cell (row r, col c) contains number n"

    Constraints:
    1. Each cell has at least one number
    2. Each cell has at most one number
    3. Each row contains each number exactly once
    4. Each column contains each number exactly once
    5. Each box contains each number exactly once
    6. Clues (given numbers) are fixed

    Args:
        size: Grid size (default 9 for standard Sudoku)
        clues: Dictionary mapping (row, col) -> number for given clues

    Returns:
        CNF formula encoding the Sudoku puzzle
    """
    clauses = []
    box_size = int(size ** 0.5)  # 3 for 9x9 Sudoku

    def var_name(row: int, col: int, num: int) -> str:
        """Create variable name for cell-number assignment."""
        return f"x_{row}_{col}_{num}"

    # Constraint 1: Each cell has at least one number
    for r in range(size):
        for c in range(size):
            clause = Clause([Literal(var_name(r, c, n), False) for n in range(1, size + 1)])
            clauses.append(clause)

    # Constraint 2: Each cell has at most one number
    for r in range(size):
        for c in range(size):
            for n1 in range(1, size + 1):
                for n2 in range(n1 + 1, size + 1):
                    # Cell (r,c) cannot be both n1 and n2
                    clause = Clause([
                        Literal(var_name(r, c, n1), True),
                        Literal(var_name(r, c, n2), True)
                    ])
                    clauses.append(clause)

    # Constraint 3: Each row contains each number exactly once
    for r in range(size):
        for n in range(1, size + 1):
            # At least one cell in row r contains n
            clause = Clause([Literal(var_name(r, c, n), False) for c in range(size)])
            clauses.append(clause)

            # At most one cell in row r contains n
            for c1 in range(size):
                for c2 in range(c1 + 1, size):
                    clause = Clause([
                        Literal(var_name(r, c1, n), True),
                        Literal(var_name(r, c2, n), True)
                    ])
                    clauses.append(clause)

    # Constraint 4: Each column contains each number exactly once
    for c in range(size):
        for n in range(1, size + 1):
            # At least one cell in column c contains n
            clause = Clause([Literal(var_name(r, c, n), False) for r in range(size)])
            clauses.append(clause)

            # At most one cell in column c contains n
            for r1 in range(size):
                for r2 in range(r1 + 1, size):
                    clause = Clause([
                        Literal(var_name(r1, c, n), True),
                        Literal(var_name(r2, c, n), True)
                    ])
                    clauses.append(clause)

    # Constraint 5: Each box contains each number exactly once
    for box_row in range(box_size):
        for box_col in range(box_size):
            for n in range(1, size + 1):
                # Cells in this box
                box_cells = [
                    (box_row * box_size + dr, box_col * box_size + dc)
                    for dr in range(box_size)
                    for dc in range(box_size)
                ]

                # At least one cell in box contains n
                clause = Clause([Literal(var_name(r, c, n), False) for r, c in box_cells])
                clauses.append(clause)

                # At most one cell in box contains n
                for i in range(len(box_cells)):
                    for j in range(i + 1, len(box_cells)):
                        r1, c1 = box_cells[i]
                        r2, c2 = box_cells[j]
                        clause = Clause([
                            Literal(var_name(r1, c1, n), True),
                            Literal(var_name(r2, c2, n), True)
                        ])
                        clauses.append(clause)

    # Constraint 6: Clues (given numbers)
    if clues:
        for (row, col), num in clues.items():
            clause = Clause([Literal(var_name(row, col, num), False)])
            clauses.append(clause)

    return CNFExpression(clauses)


def decode_sudoku(solution: Dict[str, bool], size: int = 9) -> List[List[int]]:
    """
    Decode SAT solution to Sudoku grid.

    Args:
        solution: SAT solution
        size: Grid size

    Returns:
        2D list representing the solved Sudoku grid
    """
    grid = [[0 for _ in range(size)] for _ in range(size)]

    for r in range(size):
        for c in range(size):
            for n in range(1, size + 1):
                var = f"x_{r}_{c}_{n}"
                if var in solution and solution[var]:
                    grid[r][c] = n
                    break

    return grid


def print_sudoku(grid: List[List[int]], title: str = "Sudoku"):
    """Pretty print Sudoku grid."""
    size = len(grid)
    box_size = int(size ** 0.5)

    print(f"\n{title}:")
    print("  " + " ".join([str(i) for i in range(size)]))
    for i, row in enumerate(grid):
        if i % box_size == 0 and i != 0:
            print("  " + "-" * (size * 2 + box_size - 1))
        row_str = ""
        for j, cell in enumerate(row):
            if j % box_size == 0 and j != 0:
                row_str += "| "
            row_str += (str(cell) if cell != 0 else ".") + " "
        print(f"{i} {row_str}")


def verify_sudoku(grid: List[List[int]]) -> bool:
    """Verify that a Sudoku solution is valid."""
    size = len(grid)
    box_size = int(size ** 0.5)

    # Check rows
    for row in grid:
        if len(set(row)) != size or min(row) < 1 or max(row) > size:
            return False

    # Check columns
    for c in range(size):
        col = [grid[r][c] for r in range(size)]
        if len(set(col)) != size:
            return False

    # Check boxes
    for box_row in range(box_size):
        for box_col in range(box_size):
            box = []
            for dr in range(box_size):
                for dc in range(box_size):
                    box.append(grid[box_row * box_size + dr][box_col * box_size + dc])
            if len(set(box)) != size:
                return False

    return True


def example1_easy_sudoku():
    """Example 1: Easy Sudoku puzzle."""
    print("=" * 60)
    print("Example 1: Easy Sudoku")
    print("=" * 60)

    # Easy puzzle with many clues
    clues = {
        (0, 0): 5, (0, 1): 3, (0, 4): 7,
        (1, 0): 6, (1, 3): 1, (1, 4): 9, (1, 5): 5,
        (2, 1): 9, (2, 2): 8, (2, 7): 6,
        (3, 0): 8, (3, 4): 6, (3, 8): 3,
        (4, 0): 4, (4, 3): 8, (4, 5): 3, (4, 8): 1,
        (5, 0): 7, (5, 4): 2, (5, 8): 6,
        (6, 1): 6, (6, 6): 2, (6, 7): 8,
        (7, 3): 4, (7, 4): 1, (7, 5): 9, (7, 8): 5,
        (8, 4): 8, (8, 7): 7, (8, 8): 9
    }

    # Create initial grid for display
    initial = [[0] * 9 for _ in range(9)]
    for (r, c), n in clues.items():
        initial[r][c] = n

    print_sudoku(initial, "Initial Puzzle")

    # Encode and solve
    print("\nEncoding puzzle as SAT...")
    cnf = encode_sudoku(size=9, clues=clues)
    print(f"CNF has {len(cnf.clauses)} clauses")

    print("Solving...")
    solution = solve_sat(cnf)

    if solution:
        grid = decode_sudoku(solution, size=9)
        print_sudoku(grid, "Solution")
        print(f"\nValid? {verify_sudoku(grid)}")
    else:
        print("No solution found!")


def example2_hard_sudoku():
    """Example 2: Harder Sudoku puzzle (fewer clues)."""
    print("\n" + "=" * 60)
    print("Example 2: Hard Sudoku")
    print("=" * 60)

    # Harder puzzle - fewer clues
    clues = {
        (0, 0): 8, (0, 2): 3, (0, 3): 6,
        (1, 1): 7, (1, 4): 9, (1, 6): 2,
        (2, 0): 6, (2, 4): 1, (2, 5): 8,
        (3, 2): 1, (3, 5): 6, (3, 7): 4,
        (4, 1): 9, (4, 3): 1, (4, 5): 2, (4, 7): 7,
        (5, 1): 4, (5, 3): 3, (5, 6): 9,
        (6, 3): 7, (6, 4): 6, (6, 8): 5,
        (7, 2): 9, (7, 4): 3, (7, 7): 6,
        (8, 5): 4, (8, 6): 7, (8, 8): 2
    }

    initial = [[0] * 9 for _ in range(9)]
    for (r, c), n in clues.items():
        initial[r][c] = n

    print_sudoku(initial, "Initial Puzzle")

    cnf = encode_sudoku(size=9, clues=clues)
    print(f"\nCNF has {len(cnf.clauses)} clauses")

    print("Solving with CDCL...")
    from bsat import get_cdcl_stats
    solution, stats = get_cdcl_stats(cnf)

    if solution:
        grid = decode_sudoku(solution, size=9)
        print_sudoku(grid, "Solution")
        print(f"\nValid? {verify_sudoku(grid)}")
        print(f"\nCDCL Stats:")
        print(f"  Decisions: {stats.decisions}")
        print(f"  Propagations: {stats.propagations}")
        print(f"  Conflicts: {stats.conflicts}")
    else:
        print("No solution found!")


def example3_minimal_sudoku():
    """Example 3: Minimal Sudoku (17 clues - theoretical minimum)."""
    print("\n" + "=" * 60)
    print("Example 3: Minimal Sudoku (17 clues)")
    print("=" * 60)

    # One of the minimal Sudoku puzzles (17 clues)
    clues = {
        (0, 0): 1, (0, 6): 2,
        (1, 1): 3, (1, 4): 4,
        (2, 2): 5, (2, 7): 6,
        (3, 3): 7, (3, 5): 8,
        (4, 1): 9, (4, 7): 1,
        (5, 3): 2, (5, 5): 3,
        (6, 1): 4, (6, 6): 5,
        (7, 4): 6, (7, 7): 7,
        (8, 2): 8
    }

    initial = [[0] * 9 for _ in range(9)]
    for (r, c), n in clues.items():
        initial[r][c] = n

    print(f"\nNote: 17 is the proven minimum number of clues for a unique Sudoku")
    print_sudoku(initial, "Initial Puzzle (17 clues)")

    cnf = encode_sudoku(size=9, clues=clues)
    solution = solve_cdcl(cnf)

    if solution:
        grid = decode_sudoku(solution, size=9)
        print_sudoku(grid, "Solution")
        print(f"\nValid? {verify_sudoku(grid)}")


def example4_4x4_sudoku():
    """Example 4: Smaller 4x4 Sudoku."""
    print("\n" + "=" * 60)
    print("Example 4: 4x4 Sudoku")
    print("=" * 60)

    # 4x4 Sudoku (numbers 1-4, 2x2 boxes)
    clues = {
        (0, 1): 3,
        (1, 0): 3, (1, 3): 2,
        (2, 0): 1, (2, 3): 4,
        (3, 2): 2
    }

    initial = [[0] * 4 for _ in range(4)]
    for (r, c), n in clues.items():
        initial[r][c] = n

    print_sudoku(initial, "Initial 4x4 Puzzle")

    cnf = encode_sudoku(size=4, clues=clues)
    print(f"\nCNF has {len(cnf.clauses)} clauses")

    solution = solve_sat(cnf)

    if solution:
        grid = decode_sudoku(solution, size=4)
        print_sudoku(grid, "Solution")
        print(f"\nValid? {verify_sudoku(grid)}")


def example5_empty_sudoku():
    """Example 5: Generate a valid Sudoku from empty grid."""
    print("\n" + "=" * 60)
    print("Example 5: Generate Valid Sudoku")
    print("=" * 60)

    print("\nGenerating a valid 4x4 Sudoku from scratch...")
    print("(no clues - finding any valid solution)")

    cnf = encode_sudoku(size=4, clues=None)
    solution = solve_sat(cnf)

    if solution:
        grid = decode_sudoku(solution, size=4)
        print_sudoku(grid, "Generated 4x4 Sudoku")
        print(f"\nValid? {verify_sudoku(grid)}")


if __name__ == '__main__':
    example1_easy_sudoku()
    example2_hard_sudoku()
    example3_minimal_sudoku()
    example4_4x4_sudoku()
    example5_empty_sudoku()

    print("\n" + "=" * 60)
    print("All Sudoku examples completed!")
    print("=" * 60)
