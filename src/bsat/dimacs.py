"""
DIMACS CNF Format Support

The DIMACS format is the standard format for SAT solver input/output,
used in SAT competitions and by industrial SAT solvers.

Format specification:
- Comments start with 'c'
- Problem line: 'p cnf <num_vars> <num_clauses>'
- Each clause: space-separated literals, terminated with 0
- Variables are numbered 1, 2, 3, ...
- Negation: negative number (e.g., -3 means ¬x3)

Example:
    c This is a comment
    p cnf 3 2
    1 -2 3 0
    -1 2 0

Represents: (x1 ∨ ¬x2 ∨ x3) ∧ (¬x1 ∨ x2)
"""

from typing import Dict, List, Optional, TextIO, Tuple
from pathlib import Path
from .cnf import CNFExpression, Clause, Literal


class DIMACSParseError(Exception):
    """Exception raised when DIMACS file parsing fails."""
    pass


def parse_dimacs(content: str) -> CNFExpression:
    """
    Parse DIMACS CNF format string into CNFExpression.

    Args:
        content: String containing DIMACS format CNF

    Returns:
        CNFExpression parsed from DIMACS format

    Raises:
        DIMACSParseError: If format is invalid

    Example:
        >>> dimacs = '''
        ... c Sample CNF
        ... p cnf 3 2
        ... 1 -2 0
        ... 2 3 0
        ... '''
        >>> cnf = parse_dimacs(dimacs)
        >>> len(cnf.clauses)
        2
    """
    lines = content.strip().split('\n')
    clauses = []
    num_vars = None
    num_clauses = None
    var_map = {}  # Map DIMACS variable numbers to our variable names

    for line_num, line in enumerate(lines, 1):
        line = line.strip()

        # Skip empty lines
        if not line:
            continue

        # Comment line
        if line.startswith('c'):
            continue

        # Problem line
        if line.startswith('p'):
            parts = line.split()
            if len(parts) != 4 or parts[1] != 'cnf':
                raise DIMACSParseError(
                    f"Line {line_num}: Invalid problem line '{line}'. "
                    f"Expected format: 'p cnf <num_vars> <num_clauses>'"
                )
            try:
                num_vars = int(parts[2])
                num_clauses = int(parts[3])
            except ValueError:
                raise DIMACSParseError(
                    f"Line {line_num}: Invalid numbers in problem line"
                )
            continue

        # Clause line
        if num_vars is None:
            raise DIMACSParseError(
                f"Line {line_num}: Clause found before problem line"
            )

        # Parse literals
        try:
            tokens = [int(x) for x in line.split()]
        except ValueError:
            raise DIMACSParseError(
                f"Line {line_num}: Invalid literal in clause '{line}'"
            )

        if not tokens or tokens[-1] != 0:
            raise DIMACSParseError(
                f"Line {line_num}: Clause must end with 0"
            )

        # Remove trailing 0
        tokens = tokens[:-1]

        if not tokens:
            # Empty clause (always false) - valid in DIMACS
            continue

        # Convert to our Literal format
        literals = []
        for lit in tokens:
            if lit == 0:
                raise DIMACSParseError(
                    f"Line {line_num}: 0 can only appear at end of clause"
                )

            var_num = abs(lit)
            if var_num > num_vars:
                raise DIMACSParseError(
                    f"Line {line_num}: Variable {var_num} exceeds declared {num_vars}"
                )

            # Create variable name (x1, x2, x3, ...)
            if var_num not in var_map:
                var_map[var_num] = f"x{var_num}"

            var_name = var_map[var_num]
            negated = (lit < 0)
            literals.append(Literal(var_name, negated))

        clauses.append(Clause(literals))

    # Verify we got expected number of clauses
    if num_clauses is not None and len(clauses) != num_clauses:
        # Warning, but not error - some files have incorrect counts
        pass

    return CNFExpression(clauses)


def read_dimacs_file(filepath: str) -> CNFExpression:
    """
    Read DIMACS CNF file.

    Args:
        filepath: Path to DIMACS .cnf file

    Returns:
        CNFExpression parsed from file

    Example:
        >>> cnf = read_dimacs_file('problem.cnf')
    """
    path = Path(filepath)
    if not path.exists():
        raise FileNotFoundError(f"File not found: {filepath}")

    with open(path, 'r') as f:
        content = f.read()

    return parse_dimacs(content)


def to_dimacs(cnf: CNFExpression, comments: Optional[List[str]] = None) -> str:
    """
    Convert CNFExpression to DIMACS format string.

    Args:
        cnf: CNF formula to convert
        comments: Optional list of comment lines

    Returns:
        String in DIMACS CNF format

    Example:
        >>> from bsat import CNFExpression, Clause, Literal
        >>> cnf = CNFExpression([
        ...     Clause([Literal('x1', False), Literal('x2', True)]),
        ...     Clause([Literal('x2', False), Literal('x3', False)])
        ... ])
        >>> dimacs_str = to_dimacs(cnf, comments=['Example formula'])
        >>> print(dimacs_str)
        c Example formula
        p cnf 3 2
        1 -2 0
        2 3 0
    """
    lines = []

    # Add comments
    if comments:
        for comment in comments:
            lines.append(f"c {comment}")

    # Build variable mapping
    variables = sorted(cnf.get_variables())
    var_to_num = {var: i + 1 for i, var in enumerate(variables)}
    num_vars = len(variables)
    num_clauses = len(cnf.clauses)

    # Problem line
    lines.append(f"p cnf {num_vars} {num_clauses}")

    # Clauses
    for clause in cnf.clauses:
        literals = []
        for lit in clause.literals:
            var_num = var_to_num[lit.variable]
            if lit.negated:
                literals.append(f"-{var_num}")
            else:
                literals.append(str(var_num))
        literals.append("0")
        lines.append(" ".join(literals))

    return "\n".join(lines) + "\n"


def write_dimacs_file(cnf: CNFExpression, filepath: str,
                      comments: Optional[List[str]] = None):
    """
    Write CNFExpression to DIMACS file.

    Args:
        cnf: CNF formula to write
        filepath: Output file path
        comments: Optional list of comment lines

    Example:
        >>> write_dimacs_file(cnf, 'output.cnf', comments=['Generated by BSAT'])
    """
    dimacs_str = to_dimacs(cnf, comments)
    path = Path(filepath)

    with open(path, 'w') as f:
        f.write(dimacs_str)


def parse_dimacs_solution(content: str) -> Optional[Dict[str, bool]]:
    """
    Parse DIMACS solution format (SAT solver output).

    DIMACS solution format:
        s SATISFIABLE
        v 1 -2 3 0

    Or:
        s UNSATISFIABLE

    Args:
        content: String containing DIMACS solution

    Returns:
        Dictionary mapping variable names to values, or None if UNSAT

    Example:
        >>> solution_str = '''
        ... s SATISFIABLE
        ... v 1 -2 3 0
        ... '''
        >>> sol = parse_dimacs_solution(solution_str)
        >>> sol['x1']
        True
        >>> sol['x2']
        False
    """
    lines = content.strip().split('\n')

    satisfiable = None
    assignments = {}

    for line in lines:
        line = line.strip()
        if not line:
            continue

        # Status line
        if line.startswith('s'):
            if 'SATISFIABLE' in line and 'UNSATISFIABLE' not in line:
                satisfiable = True
            elif 'UNSATISFIABLE' in line:
                satisfiable = False
                return None

        # Variable assignment line
        if line.startswith('v'):
            tokens = line[1:].strip().split()
            for token in tokens:
                try:
                    lit = int(token)
                except ValueError:
                    continue

                if lit == 0:
                    break

                var_num = abs(lit)
                var_name = f"x{var_num}"
                assignments[var_name] = (lit > 0)

    return assignments if satisfiable else None


def solution_to_dimacs(solution: Dict[str, bool],
                       satisfiable: bool = True) -> str:
    """
    Convert solution to DIMACS format.

    Args:
        solution: Variable assignments
        satisfiable: Whether formula is satisfiable

    Returns:
        DIMACS solution format string

    Example:
        >>> sol = {'x1': True, 'x2': False, 'x3': True}
        >>> dimacs = solution_to_dimacs(sol)
        >>> print(dimacs)
        s SATISFIABLE
        v 1 -2 3 0
    """
    if not satisfiable:
        return "s UNSATISFIABLE\n"

    lines = ["s SATISFIABLE"]

    # Sort variables by number for consistent output
    def var_num(var_name):
        # Extract number from variable name (e.g., 'x5' -> 5)
        try:
            return int(var_name[1:])
        except (ValueError, IndexError):
            return 0

    sorted_vars = sorted(solution.keys(), key=var_num)

    # Build assignment line
    literals = []
    for var in sorted_vars:
        num = var_num(var)
        if solution[var]:
            literals.append(str(num))
        else:
            literals.append(f"-{num}")

    # DIMACS solution lines should be at most 80 characters
    # Split into multiple 'v' lines if needed
    current_line = []
    current_length = 2  # 'v '

    for lit in literals:
        lit_len = len(lit) + 1  # +1 for space
        if current_length + lit_len > 78:  # Leave room for ' 0'
            current_line.append('0')
            lines.append('v ' + ' '.join(current_line))
            current_line = []
            current_length = 2

        current_line.append(lit)
        current_length += lit_len

    # Add final line
    if current_line:
        current_line.append('0')
        lines.append('v ' + ' '.join(current_line))

    return '\n'.join(lines) + '\n'
