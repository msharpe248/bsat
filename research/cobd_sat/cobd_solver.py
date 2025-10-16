"""
Community-Based Decomposition SAT (CoBD-SAT) Solver

Main solver implementation that decomposes SAT problems based on community
structure and coordinates sub-problem solving.

Algorithm:
1. Detect communities in variable-clause graph
2. Identify interface variables connecting communities
3. Enumerate interface variable assignments
4. Solve each community independently given interface assignments
5. Combine solutions
"""

import sys
import os
from typing import Dict, List, Optional, Set, Tuple
from collections import defaultdict
from itertools import product

# Add src to path to import bsat modules
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from bsat.cnf import CNFExpression, Clause, Literal
from bsat.dpll import DPLLSolver

from .community_detector import CommunityDetector


class CoBDSATSolver:
    """
    Community-Based Decomposition SAT Solver.

    Decomposes SAT problems based on community structure in the variable-clause
    interaction graph. Solves communities semi-independently and coordinates
    through interface variables.

    Key Features:
    - Automatic community detection
    - Independent sub-problem solving
    - Interface variable coordination
    - Fallback to standard DPLL if no structure found

    Performance:
    - Best case: O(k * 2^(n/k)) for k communities with balanced decomposition
    - Worst case: O(2^n) for no community structure
    - Modularity Q > 0.3 typically shows speedup
    """

    def __init__(self, cnf: CNFExpression,
                 min_communities: int = 2,
                 max_communities: int = 8,
                 max_interface_assignments: int = 1000,
                 use_cdcl: bool = False):
        """
        Initialize CoBD-SAT solver.

        Args:
            cnf: CNFExpression to solve
            min_communities: Minimum communities to detect (default 2)
            max_communities: Maximum communities to detect (default 8)
            max_interface_assignments: Max interface variable combinations to try (default 1000)
            use_cdcl: Use CDCL instead of DPLL for sub-problems (default False)
        """
        self.cnf = cnf
        self.min_communities = min_communities
        self.max_communities = max_communities
        self.max_interface_assignments = max_interface_assignments
        self.use_cdcl = use_cdcl

        # Community detection
        self.detector = CommunityDetector(cnf)
        self.communities: Dict[str, int] = {}
        self.interface_variables: Set[str] = set()
        self.community_formulas: Dict[int, CNFExpression] = {}

        # Statistics
        self.stats = {
            'num_communities': 0,
            'modularity': 0.0,
            'num_interface_vars': 0,
            'interface_percentage': 0.0,
            'interface_assignments_tried': 0,
            'community_solve_attempts': 0,
            'used_decomposition': False,
            'fallback_reason': None
        }

    def solve(self) -> Optional[Dict[str, bool]]:
        """
        Solve the SAT problem using community-based decomposition.

        Returns:
            Satisfying assignment if SAT, None if UNSAT
        """
        # Detect communities
        self.communities = self.detector.detect_communities(
            min_communities=self.min_communities,
            max_communities=self.max_communities
        )

        # Get statistics
        community_stats = self.detector.get_statistics()
        self.stats.update(community_stats)

        # Identify interface variables
        self.interface_variables = self.detector.identify_interface_variables()

        # Decide whether to use decomposition
        if not self._should_use_decomposition():
            return self._fallback_solve()

        # Use decomposition
        self.stats['used_decomposition'] = True

        # Build community sub-formulas
        self._build_community_formulas()

        # Try interface variable assignments
        return self._solve_with_decomposition()

    def _should_use_decomposition(self) -> bool:
        """
        Decide whether decomposition is beneficial.

        Heuristics:
        - Need at least 2 communities
        - Modularity should be > 0.2 (some structure)
        - Interface variables should be < 50% (otherwise overhead too high)
        - Each community should have some variables (not trivial split)
        """
        num_communities = self.stats['num_communities']
        modularity = self.stats['modularity']
        interface_pct = self.stats['interface_percentage']

        if num_communities < 2:
            self.stats['fallback_reason'] = 'Too few communities detected'
            return False

        if modularity < 0.2:
            self.stats['fallback_reason'] = f'Low modularity ({modularity:.3f} < 0.2)'
            return False

        if interface_pct > 50:
            self.stats['fallback_reason'] = f'Too many interface variables ({interface_pct:.1f}%)'
            return False

        # Check that all communities have variables
        vars_per_community = self.stats['vars_per_community']
        if any(count == 0 for count in vars_per_community.values()):
            self.stats['fallback_reason'] = 'Empty community detected'
            return False

        # Check interface assignment space
        num_interface = len(self.interface_variables)
        if 2 ** num_interface > self.max_interface_assignments:
            self.stats['fallback_reason'] = f'Too many interface assignments (2^{num_interface} > {self.max_interface_assignments})'
            return False

        return True

    def _fallback_solve(self) -> Optional[Dict[str, bool]]:
        """
        Fallback to standard solver when decomposition is not beneficial.

        Uses CDCL if enabled (faster with learning), else DPLL.
        """
        if self.use_cdcl:
            from bsat.cdcl import CDCLSolver
            solver = CDCLSolver(self.cnf)
        else:
            solver = DPLLSolver(self.cnf)
        return solver.solve()

    def _build_community_formulas(self):
        """
        Build separate CNF formulas for each community.

        Each community formula contains:
        - Clauses whose variables are primarily in that community
        - Interface variables treated as parameters
        """
        var_communities = self.detector.get_variable_communities()
        clause_communities = self.detector.get_clause_communities()

        # Group clauses by community
        clauses_by_community = defaultdict(list)
        for idx, clause in enumerate(self.cnf.clauses):
            clause_id = f"C{idx}"
            if clause_id in clause_communities:
                comm_id = clause_communities[clause_id]
                clauses_by_community[comm_id].append(clause)

        # Build CNF for each community
        for comm_id, clauses in clauses_by_community.items():
            self.community_formulas[comm_id] = CNFExpression(clauses)

    def _solve_with_decomposition(self) -> Optional[Dict[str, bool]]:
        """
        Solve using community decomposition.

        Algorithm:
        1. Enumerate assignments to interface variables
        2. For each interface assignment:
           a. Solve each community independently given that assignment
           b. If all communities SAT, combine and return solution
        3. If no interface assignment works, return UNSAT
        """
        # Enumerate interface variable assignments
        interface_vars = sorted(self.interface_variables)
        num_interface = len(interface_vars)

        if num_interface == 0:
            # No interface variables - communities are independent
            # Each community should be satisfiable
            return self._solve_independent_communities()

        # Try each interface assignment
        for interface_values in product([False, True], repeat=num_interface):
            self.stats['interface_assignments_tried'] += 1

            interface_assignment = dict(zip(interface_vars, interface_values))

            # Try to solve all communities with this interface assignment
            solution = self._try_interface_assignment(interface_assignment)

            if solution is not None:
                return solution

        # No interface assignment worked - UNSAT
        return None

    def _solve_independent_communities(self) -> Optional[Dict[str, bool]]:
        """
        Solve communities that have no interface variables (fully independent).

        This is the ideal case - each community can be solved completely independently.
        """
        combined_solution = {}

        for comm_id, formula in self.community_formulas.items():
            self.stats['community_solve_attempts'] += 1

            solver = DPLLSolver(formula)
            solution = solver.solve()

            if solution is None:
                # This community is UNSAT
                return None

            combined_solution.update(solution)

        return combined_solution

    def _try_interface_assignment(self, interface_assignment: Dict[str, bool]) -> Optional[Dict[str, bool]]:
        """
        Try solving all communities given a fixed interface assignment.

        Args:
            interface_assignment: Fixed values for interface variables

        Returns:
            Combined solution if all communities SAT, None otherwise
        """
        combined_solution = dict(interface_assignment)

        for comm_id, formula in self.community_formulas.items():
            self.stats['community_solve_attempts'] += 1

            # Simplify formula given interface assignment
            simplified_clauses = self._simplify_clauses(formula.clauses, interface_assignment)

            # Check for empty clause (conflict with interface assignment)
            if any(not clause.literals for clause in simplified_clauses):
                return None  # This interface assignment doesn't work

            # Check if already satisfied
            if not simplified_clauses:
                # All clauses satisfied by interface assignment
                continue

            # Solve simplified community formula
            simplified_formula = CNFExpression(simplified_clauses)
            solver = DPLLSolver(simplified_formula)
            solution = solver.solve()

            if solution is None:
                # Community UNSAT with this interface assignment
                return None

            # Add community solution to combined solution
            combined_solution.update(solution)

        return combined_solution

    def _simplify_clauses(self, clauses: List[Clause], assignment: Dict[str, bool]) -> List[Clause]:
        """
        Simplify clauses given a partial assignment (interface variables).

        Args:
            clauses: List of clauses to simplify
            assignment: Partial assignment (interface variables)

        Returns:
            Simplified clauses with interface variables removed
        """
        simplified = []

        for clause in clauses:
            # Check if clause is satisfied or simplify it
            clause_satisfied = False
            new_literals = []

            for literal in clause.literals:
                if literal.variable in assignment:
                    # Interface variable - check if it satisfies the literal
                    value = assignment[literal.variable]
                    literal_value = (not value) if literal.negated else value

                    if literal_value:
                        # This literal is true - clause is satisfied
                        clause_satisfied = True
                        break
                    # Else: literal is false, don't include it
                else:
                    # Non-interface variable - keep it
                    new_literals.append(literal)

            if not clause_satisfied:
                simplified.append(Clause(new_literals))

        return simplified

    def get_statistics(self) -> Dict:
        """
        Get solving statistics.

        Returns:
            Dictionary with statistics about decomposition and solving
        """
        return dict(self.stats)

    def get_visualization_data(self) -> Dict:
        """
        Get data for visualizing the decomposition.

        Returns:
            Dictionary with community graph data and statistics
        """
        viz_data = self.detector.visualize_graph_data()
        viz_data['solving_stats'] = self.get_statistics()
        viz_data['interface_variables'] = sorted(self.interface_variables)

        # Add community formulas info
        community_info = {}
        for comm_id, formula in self.community_formulas.items():
            community_info[comm_id] = {
                'num_clauses': len(formula.clauses),
                'variables': sorted(formula.get_variables())
            }
        viz_data['community_formulas'] = community_info

        return viz_data


def solve_cobd_sat(cnf: CNFExpression,
                   min_communities: int = 2,
                   max_communities: int = 8) -> Optional[Dict[str, bool]]:
    """
    Solve a SAT problem using Community-Based Decomposition.

    Convenience function for quick solving.

    Args:
        cnf: CNFExpression to solve
        min_communities: Minimum communities to detect
        max_communities: Maximum communities to detect

    Returns:
        Satisfying assignment if SAT, None if UNSAT

    Example:
        >>> from bsat import CNFExpression
        >>> from research.cobd_sat import solve_cobd_sat
        >>> cnf = CNFExpression.parse("(a | b) & (b | c) & (c | d)")
        >>> result = solve_cobd_sat(cnf)
        >>> if result:
        ...     print(f"SAT: {result}")
        ... else:
        ...     print("UNSAT")
    """
    solver = CoBDSATSolver(cnf, min_communities=min_communities, max_communities=max_communities)
    return solver.solve()
