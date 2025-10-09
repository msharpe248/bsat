"""
XOR-SAT Solver

This module implements a polynomial-time solver for XOR-SAT using Gaussian elimination
over GF(2) (the field with two elements: 0 and 1).

XOR-SAT formulas consist of XOR clauses where each clause is satisfied when an odd number
of its literals are true. For example: (x ⊕ y ⊕ ¬z) is satisfied when exactly 1 or 3
of {x, y, ¬z} are true.

Time Complexity: O(n³) where n is the number of variables (due to Gaussian elimination)
Space Complexity: O(n²) for the augmented matrix

References:
- Schaefer, T. J. (1978). "The complexity of satisfiability problems."
  Proceedings of the 10th Annual ACM Symposium on Theory of Computing.
"""

from typing import Dict, Optional, List, Tuple
from .cnf import CNFExpression, Literal


class XORSATSolver:
    """
    Solves XOR-SAT formulas using Gaussian elimination over GF(2).

    Each XOR clause is represented as a linear equation over GF(2):
    - Positive literal x_i corresponds to coefficient 1
    - Negative literal ¬x_i is rewritten: ¬x_i = (1 ⊕ x_i)
    - The clause (x₁ ⊕ ¬x₂ ⊕ x₃) becomes: x₁ ⊕ (1 ⊕ x₂) ⊕ x₃ = 1
      which simplifies to: x₁ ⊕ x₂ ⊕ x₃ = 0 (since we move the constant to RHS)

    We build an augmented matrix [A|b] where:
    - Each row represents an XOR clause
    - Each column represents a variable
    - A[i][j] = 1 if variable j appears in clause i
    - b[i] = parity bit (accounts for negated literals)
    """

    def __init__(self, cnf: CNFExpression):
        """
        Initialize the XOR-SAT solver.

        Args:
            cnf: A CNFExpression representing an XOR formula
        """
        self.cnf = cnf
        self.stats = {
            'gaussian_elimination_steps': 0,
            'back_substitution_steps': 0,
            'num_variables': 0,
            'num_clauses': 0,
            'matrix_rank': 0
        }

    def solve(self) -> Optional[Dict[str, bool]]:
        """
        Solve the XOR-SAT formula using Gaussian elimination.

        Returns:
            A satisfying assignment if one exists, None if UNSAT
        """
        if not self.cnf.clauses:
            return {}

        # Get all variables
        variables = sorted(self.cnf.get_variables())
        if not variables:
            return {}

        self.stats['num_variables'] = len(variables)
        self.stats['num_clauses'] = len(self.cnf.clauses)

        # Create variable index mapping
        var_to_idx = {var: idx for idx, var in enumerate(variables)}

        # Build augmented matrix [A|b]
        matrix = self._build_matrix(var_to_idx)

        # Perform Gaussian elimination
        rank = self._gaussian_elimination(matrix)
        self.stats['matrix_rank'] = rank

        # Check for inconsistency (0 = 1)
        if self._has_contradiction(matrix):
            return None

        # Back substitution to get solution
        assignment = self._back_substitute(matrix, variables)

        return assignment

    def _build_matrix(self, var_to_idx: Dict[str, int]) -> List[List[int]]:
        """
        Build the augmented matrix [A|b] from XOR clauses.

        For each clause, we create a row where:
        - Column j is 1 if variable j appears (positive or negative)
        - The last column (b) is the parity bit accounting for negations

        Example: (x ⊕ ¬y ⊕ z) = 1
        - We want an odd number of literals to be true
        - ¬y contributes a constant 1, so: x ⊕ (1 ⊕ y) ⊕ z = 1
        - Simplifies to: x ⊕ y ⊕ z = 0
        - Row: [1, 1, 1 | 0]

        Args:
            var_to_idx: Mapping from variable names to column indices

        Returns:
            Augmented matrix where each row is [coefficients | constant]
        """
        num_vars = len(var_to_idx)
        matrix = []

        for clause in self.cnf.clauses:
            # Initialize row with zeros
            row = [0] * (num_vars + 1)

            # Count negated literals for parity
            num_negations = sum(1 for lit in clause.literals if lit.negated)

            # XOR clause is satisfied when odd number of literals are true
            # Starting parity is 1 (we want odd number true)
            # Each negation flips the parity
            row[-1] = (1 + num_negations) % 2

            # Set coefficients for variables
            for literal in clause.literals:
                var_idx = var_to_idx[literal.variable]
                row[var_idx] = 1

            matrix.append(row)

        return matrix

    def _gaussian_elimination(self, matrix: List[List[int]]) -> int:
        """
        Perform Gaussian elimination over GF(2) to get row echelon form.

        In GF(2):
        - Addition is XOR: 0+0=0, 0+1=1, 1+0=1, 1+1=0
        - Multiplication is AND: 0*0=0, 0*1=0, 1*0=0, 1*1=1

        Args:
            matrix: Augmented matrix to reduce (modified in place)

        Returns:
            The rank of the matrix
        """
        if not matrix:
            return 0

        num_rows = len(matrix)
        num_cols = len(matrix[0]) - 1  # Exclude the augmented column

        current_row = 0

        for col in range(num_cols):
            self.stats['gaussian_elimination_steps'] += 1

            # Find pivot (a row with 1 in current column)
            pivot_row = None
            for row in range(current_row, num_rows):
                if matrix[row][col] == 1:
                    pivot_row = row
                    break

            # No pivot found, move to next column
            if pivot_row is None:
                continue

            # Swap rows if needed
            if pivot_row != current_row:
                matrix[current_row], matrix[pivot_row] = matrix[pivot_row], matrix[current_row]

            # Eliminate all other 1s in this column (both above and below)
            for row in range(num_rows):
                if row != current_row and matrix[row][col] == 1:
                    # XOR this row with the pivot row
                    for c in range(len(matrix[row])):
                        matrix[row][c] ^= matrix[current_row][c]

            current_row += 1

        return current_row

    def _has_contradiction(self, matrix: List[List[int]]) -> bool:
        """
        Check if the matrix has a contradiction (0 = 1).

        A contradiction occurs when we have a row with all zeros in the
        coefficient columns but a 1 in the constant column.

        Args:
            matrix: The matrix in row echelon form

        Returns:
            True if there's a contradiction (UNSAT), False otherwise
        """
        for row in matrix:
            # Check if all coefficients are 0 but constant is 1
            if all(row[i] == 0 for i in range(len(row) - 1)) and row[-1] == 1:
                return True
        return False

    def _back_substitute(self, matrix: List[List[int]], variables: List[str]) -> Dict[str, bool]:
        """
        Perform back substitution to extract a solution.

        For free variables (those without a pivot row), we assign False.
        For pivot variables, we compute their value from the equation.

        Args:
            matrix: The matrix in reduced row echelon form
            variables: List of variable names in order

        Returns:
            A satisfying assignment
        """
        assignment = {}
        num_vars = len(variables)

        # Initialize all variables to False (free variables)
        for var in variables:
            assignment[var] = False

        # Process each row to set pivot variable values
        for row in matrix:
            self.stats['back_substitution_steps'] += 1

            # Find the pivot column (first non-zero coefficient)
            pivot_col = None
            for col in range(num_vars):
                if row[col] == 1:
                    pivot_col = col
                    break

            # If no pivot, this is a zero row (skip it)
            if pivot_col is None:
                continue

            # Calculate value for pivot variable
            # pivot_var ⊕ (sum of other vars) = constant
            # So: pivot_var = constant ⊕ (sum of other vars)
            value = row[-1]  # Start with constant

            for col in range(pivot_col + 1, num_vars):
                if row[col] == 1:
                    # XOR with the value of this variable
                    value ^= (1 if assignment[variables[col]] else 0)

            assignment[variables[pivot_col]] = bool(value)

        return assignment


def solve_xorsat(cnf: CNFExpression) -> Optional[Dict[str, bool]]:
    """
    Solve an XOR-SAT formula.

    XOR-SAT formulas consist of clauses where literals are combined with XOR.
    Each clause is satisfied when an odd number of its literals are true.

    This is a polynomial-time algorithm using Gaussian elimination over GF(2).

    Args:
        cnf: A CNFExpression representing an XOR formula

    Returns:
        A satisfying assignment if one exists, None if UNSAT

    Example:
        >>> from bsat import CNFExpression, solve_xorsat
        >>> # (x ⊕ y) ∧ (y ⊕ z) ∧ (x ⊕ z)
        >>> # This is UNSAT because it requires x=y, y=z, but x≠z
        >>> formula = CNFExpression([
        ...     Clause([Literal('x', False), Literal('y', False)]),
        ...     Clause([Literal('y', False), Literal('z', False)]),
        ...     Clause([Literal('x', False), Literal('z', False)])
        ... ])
        >>> result = solve_xorsat(formula)
        >>> print(result)  # None (UNSAT)
    """
    solver = XORSATSolver(cnf)
    return solver.solve()


def get_xorsat_stats(cnf: CNFExpression) -> Dict:
    """
    Solve XOR-SAT and return statistics about the solving process.

    Args:
        cnf: A CNFExpression representing an XOR formula

    Returns:
        Dictionary containing solution and statistics
    """
    solver = XORSATSolver(cnf)
    solution = solver.solve()

    return {
        'solution': solution,
        'satisfiable': solution is not None,
        'stats': solver.stats
    }
