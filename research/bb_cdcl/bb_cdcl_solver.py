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
                 num_samples: Optional[int] = None,
                 confidence_threshold: float = 0.95,
                 use_cdcl: bool = True,
                 max_backbone_conflicts: int = 10,
                 adaptive_sampling: bool = True):
        """
        Initialize BB-CDCL solver.

        Args:
            cnf: CNF formula to solve
            num_samples: Number of WalkSAT samples (None = adaptive based on problem)
            confidence_threshold: Minimum confidence to fix variable (0.95 = 95%)
            use_cdcl: Use CDCL for systematic search (else DPLL)
            max_backbone_conflicts: Max conflicts before unfixing backbone vars
            adaptive_sampling: Automatically adjust sample count based on problem difficulty
        """
        self.cnf = cnf
        self.num_samples_requested = num_samples
        self.confidence_threshold = confidence_threshold
        self.use_cdcl = use_cdcl
        self.max_backbone_conflicts = max_backbone_conflicts
        self.adaptive_sampling = adaptive_sampling

        # Statistics (must be initialized before adaptive sampling)
        self.stats = {
            'backbone_detection_time': 0.0,
            'systematic_search_time': 0.0,
            'total_time': 0.0,
            'num_samples': 0,
            'num_samples_adaptive': False,
            'problem_difficulty': 'unknown',
            'num_backbone_detected': 0,
            'num_backbone_fixed': 0,
            'backbone_percentage': 0.0,
            'backbone_conflicts': 0,
            'backbone_backtracks': 0,
            'search_space_reduction': 1,
            'used_backbone': False
        }

        # Determine actual sample count (adaptive or fixed)
        if num_samples is not None:
            self.num_samples = num_samples
        elif adaptive_sampling:
            self.num_samples = self._compute_adaptive_sample_count()
        else:
            self.num_samples = 100  # Default fallback

        # Backbone detection
        self.detector: Optional[BackboneDetector] = None
        self.backbone: Dict[str, bool] = {}
        self.fixed_backbone: Dict[str, bool] = {}
        self.unfixed_backbone: Set[str] = set()

    def _compute_adaptive_sample_count(self) -> int:
        """
        Compute adaptive sample count based on problem difficulty.

        Heuristics:
        - Easy problems (small size, low clause/var ratio): 10-20 samples
        - Medium problems: 30-50 samples
        - Hard problems (large size, high clause/var ratio): 80-120 samples

        Returns:
            Optimal number of samples for this problem
        """
        num_vars = len(self.cnf.get_variables())
        num_clauses = len(self.cnf.clauses)

        if num_vars == 0:
            return 10  # Trivial problem

        clause_var_ratio = num_clauses / num_vars

        # Difficulty scoring
        # Easy: < 20 vars AND ratio < 3.5
        # Medium: 20-50 vars OR ratio 3.5-4.5
        # Hard: > 50 vars OR ratio > 4.5

        if num_vars < 20 and clause_var_ratio < 3.5:
            # Easy problem
            difficulty = 'easy'
            samples = max(10, min(20, int(num_vars)))  # 10-20 samples
        elif num_vars > 50 or clause_var_ratio > 4.5:
            # Hard problem
            difficulty = 'hard'
            samples = max(80, min(120, int(20 + num_vars * 1.5)))  # 80-120 samples
        else:
            # Medium problem
            difficulty = 'medium'
            samples = max(30, min(50, int(10 + num_vars)))  # 30-50 samples

        # Store difficulty info in stats
        self.stats['problem_difficulty'] = difficulty
        self.stats['num_samples_adaptive'] = True

        return samples

    def solve(self) -> Optional[Dict[str, bool]]:
        """
        Solve using backbone detection + CDCL.

        Returns:
            Satisfying assignment if SAT, None if UNSAT
        """
        import time
        import signal

        start_time = time.time()

        # Phase 0: Quick UNSAT check (CRITICAL FIX!)
        # Before expensive sampling, try quick DPLL with timeout
        # If UNSAT is proven quickly, skip sampling entirely
        quick_unsat_start = time.time()
        is_quickly_unsat = self._quick_unsat_check(timeout_ms=10)
        quick_check_time = time.time() - quick_unsat_start

        if is_quickly_unsat:
            # UNSAT detected quickly! Skip expensive sampling
            self.stats['quick_unsat_detected'] = True
            self.stats['quick_check_time'] = quick_check_time
            self.stats['backbone_detection_time'] = 0.0
            self.stats['systematic_search_time'] = 0.0
            self.stats['total_time'] = time.time() - start_time
            self.stats['num_samples'] = 0
            return None

        self.stats['quick_unsat_detected'] = False
        self.stats['quick_check_time'] = quick_check_time

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

    def _quick_unsat_check(self, timeout_ms: int = 10) -> bool:
        """
        Quick UNSAT check with timeout.

        Tries to prove UNSAT quickly using DPLL with a short timeout.
        If proven UNSAT quickly, returns True.
        Otherwise (timeout or SAT), returns False.

        Args:
            timeout_ms: Timeout in milliseconds

        Returns:
            True if UNSAT proven quickly, False otherwise
        """
        import signal

        class TimeoutException(Exception):
            pass

        def timeout_handler(signum, frame):
            raise TimeoutException()

        # Set up timeout (convert ms to seconds)
        old_handler = signal.signal(signal.SIGALRM, timeout_handler)
        signal.setitimer(signal.ITIMER_REAL, timeout_ms / 1000.0)

        try:
            solver = DPLLSolver(self.cnf)
            result = solver.solve()

            # Cancel timeout
            signal.setitimer(signal.ITIMER_REAL, 0)
            signal.signal(signal.SIGALRM, old_handler)

            # If UNSAT proven, return True
            return result is None

        except TimeoutException:
            # Timeout - couldn't prove quickly
            signal.setitimer(signal.ITIMER_REAL, 0)
            signal.signal(signal.SIGALRM, old_handler)
            return False

        except Exception:
            # Any other error - assume not quickly provable
            signal.setitimer(signal.ITIMER_REAL, 0)
            signal.signal(signal.SIGALRM, old_handler)
            return False

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
                  num_samples: Optional[int] = None,
                  confidence_threshold: float = 0.95,
                  use_cdcl: bool = True,
                  adaptive_sampling: bool = True) -> Optional[Dict[str, bool]]:
    """
    Solve using Backbone-Based CDCL.

    Convenience function for quick solving.

    Args:
        cnf: CNF formula to solve
        num_samples: Number of samples for backbone detection (None = adaptive)
        confidence_threshold: Confidence threshold for backbone (0.95 = 95%)
        use_cdcl: Use CDCL instead of DPLL
        adaptive_sampling: Automatically adjust samples based on problem difficulty

    Returns:
        Satisfying assignment if SAT, None if UNSAT

    Example:
        >>> from bsat import CNFExpression
        >>> from research.bb_cdcl import solve_bb_cdcl
        >>> cnf = CNFExpression.parse("(a | b) & (a) & (~b | c)")
        >>> result = solve_bb_cdcl(cnf)  # Uses adaptive sampling
        >>> if result:
        ...     print(f"SAT: {result}")
        ... else:
        ...     print("UNSAT")
    """
    solver = BBCDCLSolver(
        cnf,
        num_samples=num_samples,
        confidence_threshold=confidence_threshold,
        use_cdcl=use_cdcl,
        adaptive_sampling=adaptive_sampling
    )
    return solver.solve()
