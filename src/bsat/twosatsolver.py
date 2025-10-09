"""Algorithms for solving SAT problems."""

from typing import Dict, List, Optional, Set, Tuple
from collections import defaultdict, deque
from .cnf import CNFExpression, Clause, Literal


def is_2sat(cnf: CNFExpression) -> bool:
    """
    Check if a CNF formula is in 2SAT form (all clauses have exactly 2 literals).

    Args:
        cnf: CNF expression to check

    Returns:
        True if all clauses have exactly 2 literals, False otherwise
    """
    return all(len(clause.literals) == 2 for clause in cnf.clauses)


class TwoSATSolver:
    """
    Solves 2SAT problems using the implication graph and strongly connected components (SCC).

    Algorithm:
    1. Build an implication graph where each literal points to its implications
       For clause (a ∨ b), we have implications: ¬a → b and ¬b → a
    2. Find strongly connected components using Kosaraju's algorithm
    3. Check if any variable and its negation are in the same SCC (unsatisfiable)
    4. If satisfiable, assign values based on reverse topological order of SCCs

    Time Complexity: O(n + m) where n is the number of variables and m is the number of clauses
    Space Complexity: O(n + m)
    """

    def __init__(self, cnf: CNFExpression):
        """
        Initialize the 2SAT solver.

        Args:
            cnf: CNF expression to solve

        Raises:
            ValueError: If the CNF is not a 2SAT instance (has clauses with != 2 literals)
        """
        self.cnf = cnf
        self._validate_2sat()
        self.variables = sorted(cnf.get_variables())
        self.implication_graph = defaultdict(list)
        self.reverse_graph = defaultdict(list)
        self._build_implication_graph()

    def _validate_2sat(self):
        """Validate that all clauses have exactly 2 literals."""
        for i, clause in enumerate(self.cnf.clauses):
            if len(clause.literals) != 2:
                raise ValueError(
                    f"Clause {i} has {len(clause.literals)} literals. "
                    f"2SAT requires exactly 2 literals per clause. "
                    f"Found: {clause}"
                )

    def _literal_key(self, variable: str, negated: bool) -> str:
        """Create a unique key for a literal."""
        return f"{'~' if negated else ''}{variable}"

    def _parse_literal_key(self, key: str) -> Tuple[str, bool]:
        """Parse a literal key back to variable and negated flag."""
        if key.startswith('~'):
            return key[1:], True
        return key, False

    def _negate_key(self, key: str) -> str:
        """Return the negation of a literal key."""
        if key.startswith('~'):
            return key[1:]
        return f"~{key}"

    def _build_implication_graph(self):
        """
        Build the implication graph from the CNF formula.

        For each clause (a ∨ b), add edges:
        - ¬a → b (if a is false, b must be true)
        - ¬b → a (if b is false, a must be true)
        """
        for clause in self.cnf.clauses:
            lit1, lit2 = clause.literals

            # Create literal keys
            key1 = self._literal_key(lit1.variable, lit1.negated)
            key2 = self._literal_key(lit2.variable, lit2.negated)

            # Add implications: ¬lit1 → lit2 and ¬lit2 → lit1
            neg_key1 = self._negate_key(key1)
            neg_key2 = self._negate_key(key2)

            # Forward graph
            self.implication_graph[neg_key1].append(key2)
            self.implication_graph[neg_key2].append(key1)

            # Reverse graph (for Kosaraju's algorithm)
            self.reverse_graph[key2].append(neg_key1)
            self.reverse_graph[key1].append(neg_key2)

    def _kosaraju_first_dfs(self, node: str, visited: Set[str], stack: List[str]):
        """First DFS pass for Kosaraju's algorithm (on original graph)."""
        visited.add(node)

        for neighbor in self.implication_graph[node]:
            if neighbor not in visited:
                self._kosaraju_first_dfs(neighbor, visited, stack)

        stack.append(node)

    def _kosaraju_second_dfs(self, node: str, visited: Set[str], component: List[str]):
        """Second DFS pass for Kosaraju's algorithm (on reverse graph)."""
        visited.add(node)
        component.append(node)

        for neighbor in self.reverse_graph[node]:
            if neighbor not in visited:
                self._kosaraju_second_dfs(neighbor, visited, component)

    def _find_sccs(self) -> List[List[str]]:
        """
        Find strongly connected components using Kosaraju's algorithm.

        Returns:
            List of SCCs, where each SCC is a list of literal keys
        """
        # Get all nodes (literals)
        all_nodes = set()
        for clause in self.cnf.clauses:
            for lit in clause.literals:
                key = self._literal_key(lit.variable, lit.negated)
                all_nodes.add(key)
                all_nodes.add(self._negate_key(key))

        # First DFS pass: fill stack with finish times
        visited = set()
        stack = []

        for node in all_nodes:
            if node not in visited:
                self._kosaraju_first_dfs(node, visited, stack)

        # Second DFS pass: find SCCs in reverse topological order
        visited = set()
        sccs = []

        while stack:
            node = stack.pop()
            if node not in visited:
                component = []
                self._kosaraju_second_dfs(node, visited, component)
                sccs.append(component)

        return sccs

    def is_satisfiable(self) -> bool:
        """
        Check if the 2SAT formula is satisfiable.

        Returns:
            True if satisfiable, False otherwise
        """
        sccs = self._find_sccs()

        # Create mapping from literal to SCC index
        literal_to_scc = {}
        for i, scc in enumerate(sccs):
            for literal in scc:
                literal_to_scc[literal] = i

        # Check if any variable and its negation are in the same SCC
        for var in self.variables:
            pos_key = self._literal_key(var, False)
            neg_key = self._literal_key(var, True)

            if pos_key in literal_to_scc and neg_key in literal_to_scc:
                if literal_to_scc[pos_key] == literal_to_scc[neg_key]:
                    return False

        return True

    def solve(self) -> Optional[Dict[str, bool]]:
        """
        Solve the 2SAT problem and return a satisfying assignment if one exists.

        Returns:
            Dictionary mapping variables to boolean values if satisfiable,
            None if unsatisfiable
        """
        sccs = self._find_sccs()

        # Create mapping from literal to SCC index
        literal_to_scc = {}
        for i, scc in enumerate(sccs):
            for literal in scc:
                literal_to_scc[literal] = i

        # Check satisfiability and build assignment
        assignment = {}

        for var in self.variables:
            pos_key = self._literal_key(var, False)
            neg_key = self._literal_key(var, True)

            # Get SCC indices (default to -1 if not in graph)
            pos_scc = literal_to_scc.get(pos_key, -1)
            neg_scc = literal_to_scc.get(neg_key, -1)

            # If variable and its negation are in same SCC, unsatisfiable
            if pos_scc == neg_scc and pos_scc != -1:
                return None

            # Assign value based on SCC ordering
            # SCCs are in reverse topological order, so higher index = earlier in topo order
            # We want to set literals in later SCCs to true
            if pos_scc == -1 and neg_scc == -1:
                # Variable doesn't appear in any clause, can be anything
                assignment[var] = False
            elif pos_scc == -1:
                # Only negation appears, set to false
                assignment[var] = False
            elif neg_scc == -1:
                # Only positive appears, set to true
                assignment[var] = True
            else:
                # Both appear: set to true if positive literal is in earlier SCC (higher index)
                assignment[var] = pos_scc > neg_scc

        return assignment

    def get_all_solutions(self) -> List[Dict[str, bool]]:
        """
        Get all satisfying assignments for the 2SAT formula.

        Note: This can be exponential in the worst case, but often there are
        relatively few solutions in practice.

        Returns:
            List of all satisfying assignments
        """
        if not self.is_satisfiable():
            return []

        solutions = []
        sccs = self._find_sccs()

        # Create mapping from literal to SCC index
        literal_to_scc = {}
        for i, scc in enumerate(sccs):
            for literal in scc:
                literal_to_scc[literal] = i

        # Find free variables (those not constrained by SCCs)
        free_vars = []
        constrained_assignment = {}

        for var in self.variables:
            pos_key = self._literal_key(var, False)
            neg_key = self._literal_key(var, True)

            pos_scc = literal_to_scc.get(pos_key, -1)
            neg_scc = literal_to_scc.get(neg_key, -1)

            if pos_scc == neg_scc and pos_scc != -1:
                # Unsatisfiable - should not happen if is_satisfiable() returned True
                return []
            elif pos_scc == -1 and neg_scc == -1:
                # Free variable
                free_vars.append(var)
            elif pos_scc == neg_scc:  # both -1
                free_vars.append(var)
            else:
                # Constrained by SCC structure
                if pos_scc == -1:
                    constrained_assignment[var] = False
                elif neg_scc == -1:
                    constrained_assignment[var] = True
                elif pos_scc != neg_scc:
                    # Can vary, but one choice may be preferred
                    free_vars.append(var)

        # Generate all combinations for free variables
        from itertools import product

        for values in product([False, True], repeat=len(free_vars)):
            assignment = constrained_assignment.copy()
            for var, val in zip(free_vars, values):
                assignment[var] = val

            # Verify this assignment satisfies the formula
            if self.cnf.evaluate(assignment):
                solutions.append(assignment)

        return solutions


def solve_2sat(cnf: CNFExpression) -> Optional[Dict[str, bool]]:
    """
    Convenience function to solve a 2SAT problem.

    Args:
        cnf: CNF expression with exactly 2 literals per clause

    Returns:
        Dictionary mapping variables to boolean values if satisfiable,
        None if unsatisfiable

    Raises:
        ValueError: If the CNF is not a valid 2SAT instance

    Example:
        >>> expr = CNFExpression.parse("(x ∨ y) ∧ (¬x ∨ z) ∧ (¬y ∨ z)")
        >>> solution = solve_2sat(expr)
        >>> if solution:
        ...     print(f"Satisfiable: {solution}")
        ... else:
        ...     print("Unsatisfiable")
    """
    solver = TwoSATSolver(cnf)
    return solver.solve()


def is_2sat_satisfiable(cnf: CNFExpression) -> bool:
    """
    Check if a 2SAT problem is satisfiable.

    Args:
        cnf: CNF expression with exactly 2 literals per clause

    Returns:
        True if satisfiable, False otherwise

    Raises:
        ValueError: If the CNF is not a valid 2SAT instance
    """
    solver = TwoSATSolver(cnf)
    return solver.is_satisfiable()
