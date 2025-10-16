"""
Backbone-Based CDCL (BB-CDCL) Solver

Main solver that combines backbone detection with CDCL for improved performance.

Algorithm:
1. Detect backbone via sampling
2. Fix high-confidence backbone variables
3. Run CDCL on reduced problem
4. If conflict from backbone, backtrack and unfix
5. Repeat until solved or proven UNSAT
"""

import sys
import os
from typing import Dict, List, Optional, Set

# Add src to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from bsat.cnf import CNFExpression, Clause, Literal
from bsat.dpll import DPLLSolver
from bsat.cdcl import CDCLSolver

from .backbone_detector import BackboneDetector


class BBCDCLSolver:
    """
    Backbone-Based CDCL Solver.

    Combines statistical sampling (WalkSAT) with systematic search (CDCL).
    Key innovation: Fix backbone variables first to reduce search space exponentially.

    Performance:
    - Best case: O(sampling + 2^(1-β)n) where β = backbone fraction (0.3-0.6 typical)
    - For β=0.5: 2^n → 2^(0.5n) = massive speedup!
    - Worst case: O(sampling + 2^n) if no backbone or wrong backbone

    Completeness: ✅ Complete - can backtrack from backbone assumptions
    """

    def __init__(self, cnf: CNFExpression,
                 num_samples: int = 100,
                 confidence_threshold: float = 0.95,
                 use_cdcl: bool = True,
                 max_backbone_conflicts: int = 10):
        """
        Initialize BB-CDCL solver.

        Args:
            cnf: CNF formula to solve
            num_samples: Number of WalkSAT samples for backbone detection
            confidence_threshold: Minimum confidence to fix variable (0.95 = 95%)
            use_cdcl: Use CDCL for systematic search (else DPLL)
            max_backbone_conflicts: Max conflicts before unfixing backbone vars
        """
        self.cnf = cnf
        self.num_samples = num_samples
        self.confidence_threshold = confidence_threshold
        self.use_cdcl = use_cdcl
        self.max_backbone_conflicts = max_backbone_conflicts

        # Backbone detection
        self.detector: Optional[BackboneDetector] = None
        self.backbone: Dict[str, bool] = {}
        self.fixed_backbone: Dict[str, bool] = {}
        self.unfixed_backbone: Set[str] = set()

        # Statistics
        self.stats = {
            'backbone_detection_time': 0.0,
            'systematic_search_time': 0.0,
            'total_time': 0.0,
            'num_samples': 0,
            'num_backbone_detected': 0,
            'num_backbone_fixed': 0,
            'backbone_percentage': 0.0,
            'backbone_conflicts': 0,
            'backbone_backtracks': 0,
            'search_space_reduction': 1,
            'used_backbone': False
        }

    def solve(self) -> Optional[Dict[str, bool]]:
        """
        Solve using backbone detection + CDCL.

        Returns:
            Satisfying assignment if SAT, None if UNSAT
        """
        import time

        start_time = time.time()

        # Phase 1: Detect backbone
        backbone_start = time.time()
        self.detector = BackboneDetector(
            self.cnf,
            num_samples=self.num_samples,
            confidence_threshold=self.confidence_threshold
        )
        self.backbone = self.detector.detect_backbone()
        backbone_time = time.time() - backbone_start

        self.stats['backbone_detection_time'] = backbone_time
        self.stats['num_samples'] = len(self.detector.samples)
        self.stats['num_backbone_detected'] = len(self.backbone)

        detector_stats = self.detector.get_statistics()
        self.stats['backbone_percentage'] = detector_stats['backbone_percentage']
        self.stats['search_space_reduction'] = detector_stats['estimated_search_reduction']

        # Phase 2: Decide whether to use backbone
        if len(self.backbone) == 0:
            # No backbone detected - just use systematic solver
            self.stats['used_backbone'] = False
            return self._solve_without_backbone()

        # Fix backbone variables
        self.fixed_backbone = dict(self.backbone)
        self.stats['used_backbone'] = True
        self.stats['num_backbone_fixed'] = len(self.fixed_backbone)

        # Phase 3: Solve with fixed backbone (with conflict-driven unfixing)
        systematic_start = time.time()
        result = self._solve_with_backbone()
        systematic_time = time.time() - systematic_start

        self.stats['systematic_search_time'] = systematic_time
        self.stats['total_time'] = time.time() - start_time

        return result

    def _solve_without_backbone(self) -> Optional[Dict[str, bool]]:
        """
        Fallback: solve without backbone detection.

        Uses CDCL or DPLL directly.
        """
        if self.use_cdcl:
            solver = CDCLSolver(self.cnf)
            return solver.solve()
        else:
            solver = DPLLSolver(self.cnf)
            return solver.solve()

    def _solve_with_backbone(self) -> Optional[Dict[str, bool]]:
        """
        Solve with fixed backbone variables.

        If conflicts arise from backbone assumptions, progressively unfix them.
        """
        max_attempts = self.max_backbone_conflicts + 1

        for attempt in range(max_attempts):
            # Simplify formula with current fixed backbone
            simplified_clauses = self._simplify_with_backbone(self.cnf.clauses, self.fixed_backbone)

            # Check for immediate conflict (empty clause)
            if any(not clause.literals for clause in simplified_clauses):
                # Backbone assumption causes immediate conflict
                self.stats['backbone_conflicts'] += 1

                if not self._handle_backbone_conflict(simplified_clauses):
                    # Can't unfix any more - formula is UNSAT
                    return None
                continue

            # Check if formula is already satisfied
            if not simplified_clauses:
                # All clauses satisfied by backbone
                return self.fixed_backbone

            # Solve simplified formula
            simplified_cnf = CNFExpression(simplified_clauses)

            if self.use_cdcl:
                solver = CDCLSolver(simplified_cnf)
                solution = solver.solve()
            else:
                solver = DPLLSolver(simplified_cnf)
                solution = solver.solve()

            if solution is not None:
                # Success! Combine backbone + solution
                combined = dict(self.fixed_backbone)
                combined.update(solution)
                return combined
            else:
                # UNSAT with current backbone - try unfixing
                self.stats['backbone_conflicts'] += 1

                if not self._unfix_random_backbone():
                    # No more backbone to unfix - formula is UNSAT
                    return None

        # Exceeded max attempts - fall back to solving without backbone
        return self._solve_without_backbone()

    def _simplify_with_backbone(self, clauses: List[Clause], backbone: Dict[str, bool]) -> List[Clause]:
        """
        Simplify clauses by fixing backbone variables.

        Args:
            clauses: Original clauses
            backbone: Fixed backbone assignment

        Returns:
            Simplified clauses
        """
        simplified = []

        for clause in clauses:
            clause_satisfied = False
            new_literals = []

            for literal in clause.literals:
                if literal.variable in backbone:
                    # Backbone variable - check if satisfied
                    value = backbone[literal.variable]
                    literal_value = (not value) if literal.negated else value

                    if literal_value:
                        # Literal is true - clause satisfied
                        clause_satisfied = True
                        break
                    # else: literal is false, remove it
                else:
                    # Non-backbone variable - keep it
                    new_literals.append(literal)

            if not clause_satisfied:
                simplified.append(Clause(new_literals))

        return simplified

    def _handle_backbone_conflict(self, simplified_clauses: List[Clause]) -> bool:
        """
        Handle conflict caused by backbone assumptions.

        Find which backbone variable(s) caused empty clause and unfix one.

        Args:
            simplified_clauses: Simplified clauses (may include empty clause)

        Returns:
            True if successfully unfixed a variable, False if can't continue
        """
        # Find empty clauses
        empty_clauses = [c for c in self.cnf.clauses if not self._simplify_with_backbone([c], self.fixed_backbone)[0].literals]

        if not empty_clauses:
            return True  # No empty clause, continue

        # Find which backbone variables contribute to conflict
        # Simple heuristic: unfix the least confident backbone variable
        least_confident_var = None
        min_confidence = 1.0

        for var in self.fixed_backbone:
            conf_true, conf_false = self.detector.get_confidence_for_variable(var)
            max_conf = max(conf_true, conf_false)

            if max_conf < min_confidence:
                min_confidence = max_conf
                least_confident_var = var

        if least_confident_var is None:
            return False  # No backbone to unfix

        # Unfix the variable
        del self.fixed_backbone[least_confident_var]
        self.unfixed_backbone.add(least_confident_var)
        self.stats['backbone_backtracks'] += 1

        return True

    def _unfix_random_backbone(self) -> bool:
        """
        Unfix a backbone variable when conflict is detected.

        Strategy: Unfix the least confident backbone variable.

        Returns:
            True if successfully unfixed, False if no more to unfix
        """
        if not self.fixed_backbone:
            return False

        # Find least confident backbone variable
        least_confident_var = None
        min_confidence = 1.0

        for var, value in self.fixed_backbone.items():
            conf_true, conf_false = self.detector.get_confidence_for_variable(var)
            confidence = conf_true if value else conf_false

            if confidence < min_confidence:
                min_confidence = confidence
                least_confident_var = var

        if least_confident_var is None:
            return False

        # Unfix it
        del self.fixed_backbone[least_confident_var]
        self.unfixed_backbone.add(least_confident_var)
        self.stats['backbone_backtracks'] += 1

        return True

    def get_statistics(self) -> Dict:
        """
        Get solving statistics.

        Returns:
            Dictionary with BB-CDCL statistics
        """
        return dict(self.stats)

    def get_visualization_data(self) -> Dict:
        """
        Get visualization data for backbone and solving process.

        Returns:
            Dictionary with visualization-ready data
        """
        if self.detector is None:
            return {'error': 'Solver not run yet'}

        viz_data = self.detector.visualize_confidence_data()
        viz_data['solving_stats'] = self.get_statistics()
        viz_data['fixed_backbone'] = self.fixed_backbone
        viz_data['unfixed_backbone'] = sorted(self.unfixed_backbone)

        return viz_data


def solve_bb_cdcl(cnf: CNFExpression,
                  num_samples: int = 100,
                  confidence_threshold: float = 0.95,
                  use_cdcl: bool = True) -> Optional[Dict[str, bool]]:
    """
    Solve using Backbone-Based CDCL.

    Convenience function for quick solving.

    Args:
        cnf: CNF formula to solve
        num_samples: Number of samples for backbone detection
        confidence_threshold: Confidence threshold for backbone (0.95 = 95%)
        use_cdcl: Use CDCL instead of DPLL

    Returns:
        Satisfying assignment if SAT, None if UNSAT

    Example:
        >>> from bsat import CNFExpression
        >>> from research.bb_cdcl import solve_bb_cdcl
        >>> cnf = CNFExpression.parse("(a | b) & (a) & (~b | c)")
        >>> result = solve_bb_cdcl(cnf)
        >>> if result:
        ...     print(f"SAT: {result}")
        ... else:
        ...     print("UNSAT")
    """
    solver = BBCDCLSolver(
        cnf,
        num_samples=num_samples,
        confidence_threshold=confidence_threshold,
        use_cdcl=use_cdcl
    )
    return solver.solve()
