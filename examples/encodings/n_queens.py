"""
N-Queens Problem Encoding to SAT

Place N queens on an NxN chessboard such that no two queens attack each other.
Queens attack horizontally, vertically, and diagonally.

This is a classic constraint satisfaction problem demonstrating:
- Backtracking avoidance through SAT encoding
- Symmetry in solutions
- Scalability of SAT solvers
"""

from bsat import CNFExpression, Clause, Literal, solve_sat, solve_cdcl
from typing import List, Dict


def encode_n_queens(n: int) -> CNFExpression:
    """
    Encode N-Queens problem as SAT.

    Variables: q_r_c means "queen at row r, column c"

    Constraints:
    1. At least one queen per row
    2. At most one queen per row
    3. At most one queen per column
    4. At most one queen per diagonal (/)
    5. At most one queen per anti-diagonal (\\)

    Args:
        n: Board size (n x n)

    Returns:
        CNF formula encoding N-Queens
    """
    clauses = []

    def var(row: int, col: int) -> str:
        return f"q_{row}_{col}"

    # Constraint 1 & 2: Exactly one queen per row
    for r in range(n):
        # At least one
        clauses.append(Clause([Literal(var(r, c), False) for c in range(n)]))

        # At most one
        for c1 in range(n):
            for c2 in range(c1 + 1, n):
                clauses.append(Clause([
                    Literal(var(r, c1), True),
                    Literal(var(r, c2), True)
                ]))

    # Constraint 3: At most one queen per column
    for c in range(n):
        for r1 in range(n):
            for r2 in range(r1 + 1, n):
                clauses.append(Clause([
                    Literal(var(r1, c), True),
                    Literal(var(r2, c), True)
                ]))

    # Constraint 4: At most one queen per diagonal (/)
    for d in range(-(n - 1), n):
        positions = [(r, r - d) for r in range(n) if 0 <= r - d < n]
        for i in range(len(positions)):
            for j in range(i + 1, len(positions)):
                r1, c1 = positions[i]
                r2, c2 = positions[j]
                clauses.append(Clause([
                    Literal(var(r1, c1), True),
                    Literal(var(r2, c2), True)
                ]))

    # Constraint 5: At most one queen per anti-diagonal (\\)
    for d in range(2 * n - 1):
        positions = [(r, d - r) for r in range(n) if 0 <= d - r < n]
        for i in range(len(positions)):
            for j in range(i + 1, len(positions)):
                r1, c1 = positions[i]
                r2, c2 = positions[j]
                clauses.append(Clause([
                    Literal(var(r1, c1), True),
                    Literal(var(r2, c2), True)
                ]))

    return CNFExpression(clauses)


def decode_queens(solution: Dict[str, bool], n: int) -> List[Tuple[int, int]]:
    """Decode SAT solution to queen positions."""
    queens = []
    for r in range(n):
        for c in range(n):
            var = f"q_{r}_{c}"
            if var in solution and solution[var]:
                queens.append((r, c))
    return queens


def print_board(queens: List[Tuple[int, int]], n: int):
    """Pretty print chessboard with queens."""
    board = [['.' for _ in range(n)] for _ in range(n)]
    for r, c in queens:
        board[r][c] = 'Q'

    print("\n  " + " ".join(str(i) for i in range(n)))
    for i, row in enumerate(board):
        print(f"{i} " + " ".join(row))


def verify_queens(queens: List[Tuple[int, int]], n: int) -> bool:
    """Verify that queens don't attack each other."""
    if len(queens) != n:
        return False

    for i, (r1, c1) in enumerate(queens):
        for j, (r2, c2) in enumerate(queens):
            if i != j:
                # Same row, column, or diagonal?
                if r1 == r2 or c1 == c2 or abs(r1 - r2) == abs(c1 - c2):
                    return False
    return True


def example1_4queens():
    """Example 1: 4-Queens."""
    print("=" * 60)
    print("Example 1: 4-Queens")
    print("=" * 60)

    n = 4
    cnf = encode_n_queens(n)
    print(f"\n{n}-Queens problem")
    print(f"CNF has {len(cnf.clauses)} clauses")

    solution = solve_sat(cnf)
    if solution:
        queens = decode_queens(solution, n)
        print(f"\nSolution found: {queens}")
        print_board(queens, n)
        print(f"\nValid? {verify_queens(queens, n)}")
    else:
        print("No solution!")


def example2_8queens():
    """Example 2: Classic 8-Queens."""
    print("\n" + "=" * 60)
    print("Example 2: 8-Queens (Classic)")
    print("=" * 60)

    n = 8
    cnf = encode_n_queens(n)
    print(f"\n{n}-Queens problem")
    print(f"CNF has {len(cnf.clauses)} clauses")
    print("Note: 8-Queens has 92 distinct solutions")

    solution = solve_cdcl(cnf)
    if solution:
        queens = decode_queens(solution, n)
        print(f"\nOne solution: {queens}")
        print_board(queens, n)
        print(f"\nValid? {verify_queens(queens, n)}")


def example3_impossible():
    """Example 3: 2-Queens and 3-Queens (impossible)."""
    print("\n" + "=" * 60)
    print("Example 3: Impossible Cases")
    print("=" * 60)

    for n in [2, 3]:
        print(f"\n{n}-Queens:")
        cnf = encode_n_queens(n)
        solution = solve_sat(cnf)
        print(f"  Solvable? {solution is not None}")
        if solution is None:
            print(f"  (No way to place {n} non-attacking queens on {n}x{n} board)")


def example4_larger():
    """Example 4: Larger board with CDCL."""
    print("\n" + "=" * 60)
    print("Example 4: 12-Queens with CDCL")
    print("=" * 60)

    n = 12
    cnf = encode_n_queens(n)
    print(f"\n{n}-Queens problem")
    print(f"CNF has {len(cnf.clauses)} clauses")

    from bsat import get_cdcl_stats
    solution, stats = get_cdcl_stats(cnf)

    if solution:
        queens = decode_queens(solution, n)
        print(f"\nSolution found!")
        print_board(queens, n)
        print(f"\nValid? {verify_queens(queens, n)}")
        print(f"\nCDCL Stats:")
        print(f"  Decisions: {stats.decisions}")
        print(f"  Conflicts: {stats.conflicts}")


if __name__ == '__main__':
    example1_4queens()
    example2_8queens()
    example3_impossible()
    example4_larger()

    print("\n" + "=" * 60)
    print("All N-Queens examples completed!")
    print("=" * 60)
