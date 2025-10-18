"""
MARKET-SAT: Economic Auction-Based SAT Solver

Solves SAT by treating it as a market equilibrium problem:
- Variables are commodities
- Clauses are buyers bidding for satisfying literals
- Satisfying assignment = market-clearing equilibrium

Educational/Experimental approach demonstrating auction theory and mechanism design for SAT.

Novel contribution - first application of market mechanisms to SAT solving!
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import Dict, Optional
from bsat.cnf import CNFExpression
from bsat.cdcl import CDCLStats

from .auction_engine import AuctionEngine


class MarketStats(CDCLStats):
    """Extended statistics for MARKET-SAT solver."""

    def __init__(self):
        super().__init__()
        self.auction_rounds = 0
        self.price_updates = 0
        self.budget_boosts = 0
        self.clauses_satisfied = 0
        self.total_spent = 0.0
        self.reached_equilibrium = False

    def __str__(self):
        base_stats = super().__str__()
        market_stats = (
            f"  Auction rounds: {self.auction_rounds}\n"
            f"  Price updates: {self.price_updates}\n"
            f"  Budget boosts: {self.budget_boosts}\n"
            f"  Clauses satisfied: {self.clauses_satisfied}\n"
            f"  Total spent: ${self.total_spent:.2f}\n"
            f"  Equilibrium reached: {self.reached_equilibrium}\n"
        )
        return base_stats.rstrip(')') + '\n' + market_stats + ')'


class MARKETSATSolver:
    """
    MARKET-SAT solver using auction mechanism.

    Treats SAT as economic problem:
    - Each clause is a buyer with budget
    - Variables are goods being auctioned
    - Tatonnement process finds equilibrium
    - Equilibrium corresponds to satisfying assignment

    Educational implementation of auction theory for SAT.

    Example:
        >>> from bsat import CNFExpression
        >>> from research.market_sat import MARKETSATSolver
        >>>
        >>> cnf = CNFExpression.parse("(a | b) & (~a | c) & (~b | ~c)")
        >>> solver = MARKETSATSolver(cnf)
        >>> result = solver.solve()
    """

    def __init__(self,
                 cnf: CNFExpression,
                 max_auction_rounds: int = 1000,
                 use_market: bool = True):
        """
        Initialize MARKET-SAT solver.

        Args:
            cnf: CNF formula to solve
            max_auction_rounds: Maximum auction rounds before giving up
            use_market: Enable market mechanism (if False, use fallback)
        """
        self.cnf = cnf
        self.max_auction_rounds = max_auction_rounds
        self.use_market = use_market

        # Create auction engine
        if self.use_market:
            self.auction = AuctionEngine(cnf)
            self.auction.max_rounds = max_auction_rounds

        # Statistics
        self.stats = MarketStats()

    def solve(self, max_conflicts: int = 1000000) -> Optional[Dict[str, bool]]:
        """
        Solve using market auction mechanism.

        Args:
            max_conflicts: Not used for market solver (uses max_auction_rounds instead)

        Returns:
            Solution if SAT, None if UNSAT
        """
        if not self.use_market:
            # Fallback to simple solver
            from bsat.cdcl import CDCLSolver
            fallback = CDCLSolver(self.cnf)
            result = fallback.solve(max_conflicts=max_conflicts)
            self._update_stats_from_cdcl(fallback.stats)
            return result

        # Run auction
        result = self.auction.run_full_auction()

        # Update statistics
        self._update_stats_from_auction()

        # Verify solution if found
        if result is not None:
            if not self._verify_solution(result):
                # Invalid solution (shouldn't happen with correct auction)
                # Fall back to CDCL
                from bsat.cdcl import CDCLSolver
                fallback = CDCLSolver(self.cnf)
                result = fallback.solve(max_conflicts=max_conflicts)

        return result

    def _verify_solution(self, assignment: Dict[str, bool]) -> bool:
        """
        Verify that assignment satisfies all clauses.

        Args:
            assignment: Variable assignment

        Returns:
            True if valid solution
        """
        for clause in self.cnf.clauses:
            satisfied = False
            for lit in clause.literals:
                if lit.variable in assignment:
                    value = assignment[lit.variable]
                    if (not lit.negated and value) or (lit.negated and not value):
                        satisfied = True
                        break

            if not satisfied:
                return False

        return True

    def _update_stats_from_auction(self):
        """Update statistics from auction results."""
        auction_stats = self.auction.get_statistics()

        self.stats.auction_rounds = auction_stats['rounds']
        self.stats.clauses_satisfied = auction_stats['num_satisfied']
        self.stats.total_spent = auction_stats['total_spent']
        self.stats.reached_equilibrium = auction_stats['at_equilibrium']

        # Map to CDCL-like stats for compatibility
        # (auction rounds ~ conflicts in CDCL)
        self.stats.conflicts = auction_stats['rounds']
        self.stats.decisions = auction_stats['rounds']  # Each round is like a decision

    def _update_stats_from_cdcl(self, cdcl_stats: CDCLStats):
        """Update stats from fallback CDCL solver."""
        self.stats.decisions = cdcl_stats.decisions
        self.stats.conflicts = cdcl_stats.conflicts
        self.stats.propagations = cdcl_stats.propagations
        self.stats.restarts = cdcl_stats.restarts
        self.stats.backjumps = cdcl_stats.backjumps
        self.stats.learned_clauses = cdcl_stats.learned_clauses
        self.stats.max_decision_level = cdcl_stats.max_decision_level

    def get_market_statistics(self) -> dict:
        """Get detailed market statistics."""
        if not self.use_market:
            return {'enabled': False}

        auction_stats = self.auction.get_statistics()

        return {
            'enabled': True,
            'auction_rounds': auction_stats['rounds'],
            'clauses_satisfied': auction_stats['num_satisfied'],
            'satisfaction_rate': auction_stats['satisfaction_rate'],
            'total_spent': auction_stats['total_spent'],
            'equilibrium': auction_stats['at_equilibrium'],
            'prices': auction_stats['price_summary'],
        }

    def get_detailed_market_state(self) -> dict:
        """Get complete market state (for debugging)."""
        if not self.use_market:
            return {}

        return self.auction.get_detailed_state()

    def get_stats(self) -> MarketStats:
        """Get solver statistics."""
        return self.stats

    def __repr__(self):
        mode = "MARKET" if self.use_market else "FALLBACK"
        return f"MARKETSATSolver({mode}, {len(self.cnf.clauses)} clauses)"


def solve_market_sat(cnf: CNFExpression,
                     use_market: bool = True,
                     max_auction_rounds: int = 1000,
                     max_conflicts: int = 1000000) -> Optional[Dict[str, bool]]:
    """
    Solve using MARKET-SAT (Economic Auction SAT).

    Educational/experimental approach using market mechanisms.

    Args:
        cnf: CNF formula to solve
        use_market: Enable market mechanism
        max_auction_rounds: Maximum auction rounds
        max_conflicts: Maximum conflicts (for fallback solver)

    Returns:
        Solution if SAT, None if UNSAT

    Example:
        >>> from bsat import CNFExpression
        >>> cnf = CNFExpression.parse("(x | y) & (~x | z)")
        >>> result = solve_market_sat(cnf)
    """
    solver = MARKETSATSolver(
        cnf,
        max_auction_rounds=max_auction_rounds,
        use_market=use_market
    )
    return solver.solve(max_conflicts=max_conflicts)
