"""
Clause Bidder Agents for MARKET-SAT

Each clause acts as a bidding agent in the auction for variable assignments.
Clauses compete to acquire satisfying literals based on their budgets and valuations.

Inspired by auction theory and mechanism design.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import Dict, Optional, Tuple
from bsat.cnf import Clause, Literal


class ClauseBidder:
    """
    Bidding agent representing a clause in the SAT auction.

    Each clause:
    - Has a budget to spend on acquiring literals
    - Values literals based on clause size (unit clauses value highest)
    - Bids on the literal that maximizes consumer surplus (value - price)
    - Becomes satisfied when it acquires at least one literal
    """

    def __init__(self, clause: Clause, clause_id: int):
        """
        Initialize clause bidder.

        Args:
            clause: The SAT clause this agent represents
            clause_id: Unique identifier for this clause
        """
        self.clause = clause
        self.clause_id = clause_id

        # Economic state
        self.budget = self._compute_initial_budget()
        self.spent = 0.0
        self.allocated_literal: Optional[Literal] = None

        # Bidding state
        self.current_bid: Optional[Tuple[Literal, float]] = None
        self.is_satisfied = False

    def _compute_initial_budget(self) -> float:
        """
        Compute initial budget based on clause difficulty.

        Unit clauses (size 1) get infinite budget - they MUST win.
        Longer clauses get less budget proportional to their flexibility.

        Returns:
            Initial budget allocation
        """
        clause_size = len(self.clause.literals)

        if clause_size == 0:
            return 0.0  # Empty clause (shouldn't happen in valid CNF)

        if clause_size == 1:
            # Unit clause - critical priority
            return float('inf')

        if clause_size == 2:
            # Binary clause - high priority
            return 100.0

        # Longer clauses - inversely proportional to size
        # More options = less budget needed
        return 100.0 / clause_size

    def valuation(self, literal: Literal) -> float:
        """
        How much this clause values acquiring this literal.

        Valuation represents the benefit of satisfying the clause
        with this particular literal.

        Args:
            literal: Literal to value

        Returns:
            Valuation (higher = more valuable)
        """
        clause_size = len(self.clause.literals)

        # Check if literal actually satisfies this clause
        if literal not in self.clause.literals:
            return 0.0  # Not in clause

        # Smaller clauses value each literal more highly
        # (fewer alternatives = each option more critical)
        if clause_size == 1:
            return 1000.0  # Must have this

        if clause_size == 2:
            return 100.0  # Very valuable

        # Diminishing value for larger clauses
        return 100.0 / clause_size

    def can_afford(self, price: float) -> bool:
        """
        Check if clause can afford a literal at given price.

        Args:
            price: Price of literal

        Returns:
            True if affordable within budget
        """
        return price <= (self.budget - self.spent)

    def consumer_surplus(self, literal: Literal, price: float) -> float:
        """
        Compute consumer surplus for acquiring literal at price.

        Consumer surplus = valuation - price

        Higher surplus = better deal for this clause.

        Args:
            literal: Literal to acquire
            price: Current price

        Returns:
            Consumer surplus (can be negative if overpriced)
        """
        return self.valuation(literal) - price

    def choose_best_literal(self, prices: Dict[Tuple[str, bool], float]) -> Optional[Literal]:
        """
        Choose the literal that maximizes consumer surplus.

        Strategy: Bid on the literal with highest (valuation - price)
        among affordable options.

        Args:
            prices: Current prices for all (variable, value) pairs

        Returns:
            Best literal to bid on, or None if nothing affordable
        """
        best_literal = None
        best_surplus = float('-inf')

        for literal in self.clause.literals:
            # Get price for this literal
            var = literal.variable
            value = not literal.negated  # True if positive literal
            price = prices.get((var, value), 1.0)

            # Can afford it?
            if not self.can_afford(price):
                continue

            # Compute surplus
            surplus = self.consumer_surplus(literal, price)

            # Best option so far?
            if surplus > best_surplus:
                best_surplus = surplus
                best_literal = literal

        # Only bid if surplus is positive (good deal)
        if best_surplus > 0:
            return best_literal
        else:
            return None  # Nothing worth buying at current prices

    def place_bid(self, literal: Literal, price: float) -> bool:
        """
        Place bid on literal at given price.

        Args:
            literal: Literal to bid on
            price: Bid price

        Returns:
            True if bid placed successfully
        """
        if not self.can_afford(price):
            return False

        self.current_bid = (literal, price)
        return True

    def win_auction(self, literal: Literal, final_price: float):
        """
        Clause wins auction for literal.

        Args:
            literal: Won literal
            final_price: Final price paid
        """
        self.allocated_literal = literal
        self.spent = final_price
        self.is_satisfied = True

    def lose_auction(self):
        """Clause loses current auction (didn't win)."""
        self.current_bid = None
        # Stay unsatisfied

    def boost_budget(self, factor: float = 1.1):
        """
        Increase budget to help struggling clauses.

        Args:
            factor: Multiplicative budget increase
        """
        if self.budget != float('inf'):
            self.budget *= factor

    def get_statistics(self) -> dict:
        """Get statistics about this bidder."""
        return {
            'clause_id': self.clause_id,
            'clause_size': len(self.clause.literals),
            'budget': self.budget,
            'spent': self.spent,
            'is_satisfied': self.is_satisfied,
            'allocated_literal': str(self.allocated_literal) if self.allocated_literal else None,
        }

    def __repr__(self):
        status = "SAT" if self.is_satisfied else "UNSAT"
        return f"ClauseBidder(id={self.clause_id}, size={len(self.clause.literals)}, {status})"
