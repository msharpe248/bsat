"""
SAT Formula Reductions

This module provides reduction functions for transforming SAT formulas into
different forms, particularly k-SAT to 3-SAT reduction.

These reductions are important in complexity theory and can be useful for
preprocessing formulas before solving.
"""

from typing import Dict, List, Tuple
from .cnf import CNFExpression, Clause, Literal


class ReductionStats:
    """Statistics about a reduction operation."""

    def __init__(self):
        self.original_clauses = 0
        self.original_variables = 0
        self.reduced_clauses = 0
        self.reduced_variables = 0
        self.auxiliary_variables = 0
        self.clauses_expanded = 0
        self.max_clause_size_original = 0
        self.max_clause_size_reduced = 0

    def __str__(self):
        return (
            f"ReductionStats(\n"
            f"  Original: {self.original_clauses} clauses, "
            f"{self.original_variables} vars (max clause size: {self.max_clause_size_original})\n"
            f"  Reduced:  {self.reduced_clauses} clauses, "
            f"{self.reduced_variables} vars (max clause size: {self.max_clause_size_reduced})\n"
            f"  Added:    {self.auxiliary_variables} auxiliary variables, "
            f"{self.clauses_expanded} clauses expanded\n"
            f")"
        )


def reduce_to_3sat(cnf: CNFExpression, var_prefix: str = "_aux") -> Tuple[CNFExpression, Dict[str, str], ReductionStats]:
    """
    Reduce a k-SAT formula to 3-SAT by introducing auxiliary variables.

    Any clause with more than 3 literals is split into multiple 3-literal clauses
    using the standard reduction technique:

    Original: (l₁ ∨ l₂ ∨ l₃ ∨ l₄ ∨ ... ∨ lₖ) where k > 3

    Reduced:  (l₁ ∨ l₂ ∨ x₁) ∧
              (¬x₁ ∨ l₃ ∨ x₂) ∧
              (¬x₂ ∨ l₄ ∨ x₃) ∧
              ...
              (¬xₖ₋₃ ∨ lₖ₋₁ ∨ lₖ)

    Clauses with ≤ 3 literals are kept as-is. Clauses with 1 or 2 literals can
    optionally be padded to 3 literals (but this is not necessary for correctness).

    Args:
        cnf: Original CNF formula (may have clauses of any size)
        var_prefix: Prefix for auxiliary variables (default "_aux")

    Returns:
        Tuple of:
        - CNFExpression: The reduced 3-SAT formula
        - Dict[str, str]: Mapping from auxiliary variable names to their meaning
        - ReductionStats: Statistics about the reduction

    Example:
        >>> # 4-SAT clause
        >>> cnf = CNFExpression([
        ...     Clause([Literal('a'), Literal('b'), Literal('c'), Literal('d')])
        ... ])
        >>> reduced, aux_map, stats = reduce_to_3sat(cnf)
        >>> # Result: (a ∨ b ∨ _aux0) ∧ (¬_aux0 ∨ c ∨ d)
    """
    stats = ReductionStats()
    stats.original_clauses = len(cnf.clauses)
    stats.original_variables = len(cnf.get_variables())
    stats.max_clause_size_original = max((len(c.literals) for c in cnf.clauses), default=0)

    new_clauses = []
    aux_map = {}
    aux_counter = 0

    for clause in cnf.clauses:
        clause_size = len(clause.literals)

        if clause_size <= 3:
            # Keep small clauses as-is
            new_clauses.append(clause)
        else:
            # Need to split this clause
            stats.clauses_expanded += 1
            literals = clause.literals

            # Generate auxiliary variables for this clause
            # For k literals, we need k-3 auxiliary variables
            num_aux = clause_size - 3
            aux_vars = []
            for i in range(num_aux):
                aux_name = f"{var_prefix}{aux_counter}"
                aux_vars.append(aux_name)
                aux_map[aux_name] = f"Auxiliary for clause {stats.clauses_expanded} (splits {clause})"
                aux_counter += 1

            # Build the chain of 3-SAT clauses
            # First clause: (l₁ ∨ l₂ ∨ x₁)
            new_clauses.append(Clause([
                literals[0],
                literals[1],
                Literal(aux_vars[0], False)
            ]))

            # Middle clauses: (¬xᵢ ∨ lᵢ₊₂ ∨ xᵢ₊₁)
            for i in range(num_aux - 1):
                new_clauses.append(Clause([
                    Literal(aux_vars[i], True),      # ¬xᵢ
                    literals[i + 2],                  # lᵢ₊₂
                    Literal(aux_vars[i + 1], False)  # xᵢ₊₁
                ]))

            # Last clause: (¬xₖ₋₃ ∨ lₖ₋₁ ∨ lₖ)
            new_clauses.append(Clause([
                Literal(aux_vars[-1], True),  # ¬xₖ₋₃
                literals[-2],                  # lₖ₋₁
                literals[-1]                   # lₖ
            ]))

    reduced_cnf = CNFExpression(new_clauses)

    stats.reduced_clauses = len(new_clauses)
    stats.reduced_variables = len(reduced_cnf.get_variables())
    stats.auxiliary_variables = aux_counter
    stats.max_clause_size_reduced = max((len(c.literals) for c in new_clauses), default=0)

    return reduced_cnf, aux_map, stats


def extract_original_solution(solution: Dict[str, bool], aux_map: Dict[str, str]) -> Dict[str, bool]:
    """
    Extract the original variable assignments from a 3-SAT solution.

    After solving the reduced 3-SAT formula, this function removes auxiliary
    variables to get a solution for the original formula.

    Args:
        solution: Solution to the reduced 3-SAT formula (includes auxiliary vars)
        aux_map: Mapping of auxiliary variables (from reduce_to_3sat)

    Returns:
        Solution containing only original variables

    Example:
        >>> solution_3sat = {'a': True, 'b': False, '_aux0': True, 'c': True}
        >>> aux_map = {'_aux0': 'Auxiliary for clause 1'}
        >>> extract_original_solution(solution_3sat, aux_map)
        {'a': True, 'b': False, 'c': True}
    """
    if solution is None:
        return None

    # Remove auxiliary variables
    return {
        var: val
        for var, val in solution.items()
        if var not in aux_map
    }


def solve_with_reduction(cnf: CNFExpression, solver_func=None, var_prefix: str = "_aux") -> Tuple[Dict[str, bool], ReductionStats]:
    """
    Solve a k-SAT formula by reducing to 3-SAT first, then solving.

    This is a convenience function that:
    1. Reduces the formula to 3-SAT
    2. Solves the reduced formula
    3. Extracts the original variable assignments

    Args:
        cnf: Original k-SAT formula
        solver_func: Solver function to use (default: solve_sat from DPLL)
        var_prefix: Prefix for auxiliary variables

    Returns:
        Tuple of:
        - Dict[str, bool]: Solution for original variables (or None if UNSAT)
        - ReductionStats: Statistics about the reduction

    Example:
        >>> from bsat import CNFExpression, solve_with_reduction
        >>> # 5-SAT formula
        >>> cnf = CNFExpression.parse("(a | b | c | d | e)")
        >>> solution, stats = solve_with_reduction(cnf)
        >>> print(solution)  # Only original variables a, b, c, d, e
        >>> print(stats)     # Shows reduction statistics
    """
    # Import here to avoid circular dependency
    if solver_func is None:
        from .dpll import solve_sat
        solver_func = solve_sat

    # Reduce to 3-SAT
    reduced_cnf, aux_map, stats = reduce_to_3sat(cnf, var_prefix)

    # Solve reduced formula
    reduced_solution = solver_func(reduced_cnf)

    # Extract original solution
    original_solution = extract_original_solution(reduced_solution, aux_map)

    return original_solution, stats


def is_3sat(cnf: CNFExpression) -> bool:
    """
    Check if a formula is already in 3-SAT form (all clauses have ≤ 3 literals).

    Args:
        cnf: CNF formula to check

    Returns:
        True if all clauses have at most 3 literals

    Example:
        >>> cnf = CNFExpression.parse("(a | b | c) & (x | y)")
        >>> is_3sat(cnf)
        True
        >>> cnf = CNFExpression.parse("(a | b | c | d)")
        >>> is_3sat(cnf)
        False
    """
    return all(len(clause.literals) <= 3 for clause in cnf.clauses)


def get_max_clause_size(cnf: CNFExpression) -> int:
    """
    Get the maximum clause size in a CNF formula.

    Args:
        cnf: CNF formula

    Returns:
        Maximum number of literals in any clause (0 for empty formula)

    Example:
        >>> cnf = CNFExpression.parse("(a | b) & (x | y | z) & (p | q | r | s)")
        >>> get_max_clause_size(cnf)
        4
    """
    return max((len(clause.literals) for clause in cnf.clauses), default=0)
