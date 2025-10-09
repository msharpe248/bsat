"""
Benchmark SAT Instances for Testing and Performance Evaluation

This module provides a collection of benchmark SAT instances including:
- Random 3SAT instances (varying difficulty)
- Structured instances (pigeon-hole, graph coloring)
- Real-world benchmarks
- Satisfiability phase transition instances

These are used for testing solver correctness and comparing performance.
"""

from bsat import CNFExpression, Clause, Literal
from typing import List, Tuple
import random


def random_3sat(num_vars: int, num_clauses: int, seed: int = None) -> CNFExpression:
    """
    Generate random 3SAT instance.

    Args:
        num_vars: Number of variables
        num_clauses: Number of clauses
        seed: Random seed for reproducibility

    Returns:
        Random 3SAT CNF formula
    """
    if seed is not None:
        random.seed(seed)

    clauses = []
    for _ in range(num_clauses):
        # Pick 3 random distinct variables
        vars_in_clause = random.sample(range(1, num_vars + 1), 3)
        # Randomly negate each literal
        literals = [Literal(f"x{v}", random.choice([True, False])) for v in vars_in_clause]
        clauses.append(Clause(literals))

    return CNFExpression(clauses)


def phase_transition_3sat(num_vars: int, ratio: float = 4.26, seed: int = None) -> CNFExpression:
    """
    Generate 3SAT instance near phase transition (hardest instances).

    The satisfiability phase transition for 3SAT occurs at ratio ≈ 4.26
    (clauses-to-variables ratio). Instances near this ratio are hardest.

    Args:
        num_vars: Number of variables
        ratio: Clauses-to-variables ratio (default 4.26 is phase transition)
        seed: Random seed

    Returns:
        3SAT instance near phase transition
    """
    num_clauses = int(num_vars * ratio)
    return random_3sat(num_vars, num_clauses, seed)


def pigeon_hole(num_pigeons: int) -> CNFExpression:
    """
    Generate pigeon-hole principle instance.

    "Place n+1 pigeons into n holes" - always UNSAT.
    This is a classic hard instance for resolution-based solvers.

    Args:
        num_pigeons: Number of pigeons (holes = num_pigeons - 1)

    Returns:
        CNF encoding pigeon-hole principle
    """
    num_holes = num_pigeons - 1
    clauses = []

    # Variable: p_i_j means "pigeon i in hole j"
    def var(pigeon: int, hole: int) -> str:
        return f"p_{pigeon}_{hole}"

    # Each pigeon must be in at least one hole
    for p in range(num_pigeons):
        clause = Clause([Literal(var(p, h), False) for h in range(num_holes)])
        clauses.append(clause)

    # No two pigeons in the same hole
    for h in range(num_holes):
        for p1 in range(num_pigeons):
            for p2 in range(p1 + 1, num_pigeons):
                clause = Clause([
                    Literal(var(p1, h), True),
                    Literal(var(p2, h), True)
                ])
                clauses.append(clause)

    return CNFExpression(clauses)


def graph_coloring_hard(num_vertices: int, num_colors: int, density: float = 0.5, seed: int = None) -> CNFExpression:
    """
    Generate random graph coloring instance.

    Args:
        num_vertices: Number of vertices
        num_colors: Number of colors (if too few, UNSAT)
        density: Edge density (0.0 to 1.0)
        seed: Random seed

    Returns:
        CNF encoding graph coloring
    """
    if seed is not None:
        random.seed(seed)

    clauses = []

    # Variable: v_i_c means "vertex i has color c"
    def var(vertex: int, color: int) -> str:
        return f"v_{vertex}_{color}"

    # Each vertex has at least one color
    for v in range(num_vertices):
        clause = Clause([Literal(var(v, c), False) for c in range(num_colors)])
        clauses.append(clause)

    # Each vertex has at most one color
    for v in range(num_vertices):
        for c1 in range(num_colors):
            for c2 in range(c1 + 1, num_colors):
                clause = Clause([
                    Literal(var(v, c1), True),
                    Literal(var(v, c2), True)
                ])
                clauses.append(clause)

    # Generate random edges
    edges = []
    for v1 in range(num_vertices):
        for v2 in range(v1 + 1, num_vertices):
            if random.random() < density:
                edges.append((v1, v2))

    # Adjacent vertices have different colors
    for v1, v2 in edges:
        for c in range(num_colors):
            clause = Clause([
                Literal(var(v1, c), True),
                Literal(var(v2, c), True)
            ])
            clauses.append(clause)

    return CNFExpression(clauses)


def xor_chain(length: int, value: bool = True) -> CNFExpression:
    """
    Generate XOR chain instance.

    x1 ⊕ x2 ⊕ ... ⊕ xn = value

    This is easy for XOR-SAT solver but can be hard for DPLL/CDCL.

    Args:
        length: Number of variables in chain
        value: Target value (True = 1, False = 0)

    Returns:
        CNF encoding XOR chain
    """
    clauses = []

    # XOR can be encoded as: (a ⊕ b) = (a ∨ b) ∧ (¬a ∨ ¬b)
    # Build chain: x1 ⊕ x2 = t1, t1 ⊕ x3 = t2, ...

    for i in range(1, length):
        if i == 1:
            # x1 ⊕ x2 = t1
            clauses.append(Clause([Literal("x1", False), Literal("x2", False), Literal("t1", True)]))
            clauses.append(Clause([Literal("x1", False), Literal("x2", True), Literal("t1", False)]))
            clauses.append(Clause([Literal("x1", True), Literal("x2", False), Literal("t1", False)]))
            clauses.append(Clause([Literal("x1", True), Literal("x2", True), Literal("t1", True)]))
        elif i < length - 1:
            # ti ⊕ x(i+1) = t(i+1)
            t_prev = f"t{i-1}"
            x_curr = f"x{i+1}"
            t_curr = f"t{i}"

            clauses.append(Clause([Literal(t_prev, False), Literal(x_curr, False), Literal(t_curr, True)]))
            clauses.append(Clause([Literal(t_prev, False), Literal(x_curr, True), Literal(t_curr, False)]))
            clauses.append(Clause([Literal(t_prev, True), Literal(x_curr, False), Literal(t_curr, False)]))
            clauses.append(Clause([Literal(t_prev, True), Literal(x_curr, True), Literal(t_curr, True)]))
        else:
            # Final: t(n-2) ⊕ xn = value
            t_prev = f"t{i-1}"
            x_last = f"x{length}"

            if value:
                # Result must be True
                clauses.append(Clause([Literal(t_prev, False), Literal(x_last, True)]))
                clauses.append(Clause([Literal(t_prev, True), Literal(x_last, False)]))
            else:
                # Result must be False
                clauses.append(Clause([Literal(t_prev, False), Literal(x_last, False)]))
                clauses.append(Clause([Literal(t_prev, True), Literal(x_last, True)]))

    return CNFExpression(clauses)


def mutilated_chessboard(size: int = 8) -> CNFExpression:
    """
    Generate mutilated chessboard instance (UNSAT).

    Can we tile an 8x8 chessboard with opposite corners removed using 2x1 dominos?
    Answer: No (UNSAT) - classic hard instance.

    Args:
        size: Board size (must be even)

    Returns:
        CNF encoding domino tiling
    """
    clauses = []

    # Variables: d_r_c_o means "domino at (r,c) with orientation o"
    # o = 'h' (horizontal) or 'v' (vertical)

    def var(row: int, col: int, orient: str) -> str:
        return f"d_{row}_{col}_{orient}"

    # Remove opposite corners
    removed = [(0, 0), (size - 1, size - 1)]

    # Each non-removed cell must be covered by exactly one domino
    for r in range(size):
        for c in range(size):
            if (r, c) in removed:
                continue

            # Collect all dominos that could cover this cell
            covering = []

            # Horizontal domino starting here
            if c + 1 < size and (r, c + 1) not in removed:
                covering.append(Literal(var(r, c, 'h'), False))

            # Horizontal domino ending here
            if c - 1 >= 0 and (r, c - 1) not in removed:
                covering.append(Literal(var(r, c - 1, 'h'), False))

            # Vertical domino starting here
            if r + 1 < size and (r + 1, c) not in removed:
                covering.append(Literal(var(r, c, 'v'), False))

            # Vertical domino ending here
            if r - 1 >= 0 and (r - 1, c) not in removed:
                covering.append(Literal(var(r - 1, c, 'v'), False))

            # At least one covering
            if covering:
                clauses.append(Clause(covering))

            # At most one covering (pairwise)
            for i in range(len(covering)):
                for j in range(i + 1, len(covering)):
                    clauses.append(Clause([
                        Literal(covering[i].variable, not covering[i].negated),
                        Literal(covering[j].variable, not covering[j].negated)
                    ]))

    return CNFExpression(clauses)


# Benchmark suite - collection of instances for testing
BENCHMARK_SUITE = {
    # Easy SAT instances
    "easy_sat_1": random_3sat(10, 20, seed=42),
    "easy_sat_2": random_3sat(15, 30, seed=43),

    # Easy UNSAT instances
    "easy_unsat_1": random_3sat(10, 50, seed=44),
    "easy_unsat_2": pigeon_hole(4),  # 4 pigeons, 3 holes

    # Medium instances
    "medium_sat": random_3sat(30, 100, seed=45),
    "medium_unsat": random_3sat(30, 150, seed=46),

    # Phase transition (hardest random 3SAT)
    "phase_transition_20": phase_transition_3sat(20, seed=47),
    "phase_transition_30": phase_transition_3sat(30, seed=48),

    # Structured instances
    "pigeon_hole_5": pigeon_hole(5),  # 5 pigeons, 4 holes (UNSAT)
    "pigeon_hole_6": pigeon_hole(6),  # 6 pigeons, 5 holes (UNSAT)

    # Graph coloring
    "graph_coloring_sat": graph_coloring_hard(8, 4, density=0.3, seed=49),
    "graph_coloring_unsat": graph_coloring_hard(8, 2, density=0.5, seed=50),

    # XOR chains
    "xor_chain_sat": xor_chain(10, value=True),
    "xor_chain_unsat": xor_chain(10, value=False),
}


def get_benchmark(name: str) -> CNFExpression:
    """Get a benchmark instance by name."""
    if name not in BENCHMARK_SUITE:
        raise ValueError(f"Unknown benchmark: {name}. Available: {list(BENCHMARK_SUITE.keys())}")
    return BENCHMARK_SUITE[name]


def list_benchmarks() -> List[str]:
    """List all available benchmarks."""
    return sorted(BENCHMARK_SUITE.keys())


def benchmark_stats(cnf: CNFExpression) -> dict:
    """Get statistics about a benchmark instance."""
    variables = cnf.get_variables()
    clause_sizes = [len(c.literals) for c in cnf.clauses]

    return {
        "num_variables": len(variables),
        "num_clauses": len(cnf.clauses),
        "ratio": len(cnf.clauses) / len(variables) if variables else 0,
        "min_clause_size": min(clause_sizes) if clause_sizes else 0,
        "max_clause_size": max(clause_sizes) if clause_sizes else 0,
        "avg_clause_size": sum(clause_sizes) / len(clause_sizes) if clause_sizes else 0,
    }
