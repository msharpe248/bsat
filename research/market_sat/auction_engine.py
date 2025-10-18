"""
Auction Engine for MARKET-SAT

Coordinates the auction process where clauses bid on variable assignments.
Implements simultaneous ascending auction mechanism.

Inspired by spectrum auctions and combinatorial auctions.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import Dict, List, Optional
from bsat.cnf import CNFExpression, Literal

from .clause_agents import ClauseBidder
from .price_manager import PriceManager


class AuctionEngine:
    """
    Auction mechanism for SAT solving.

    Runs simultaneous ascending auction where:
    - All variables auctioned in parallel
    - Clauses bid on literals that satisfy them
    - Prices adjust based on competition
    - Equilibrium = satisfying assignment
    """

    def __init__(self, cnf: CNFExpression):
        """
        Initialize auction engine.

        Args:
            cnf: CNF formula to solve
        """
        self.cnf = cnf
        self.variables = list(cnf.get_variables())

        # Create bidding agents (one per clause)
        self.bidders: List[ClauseBidder] = []
        for i, clause in enumerate(cnf.clauses):
            bidder = ClauseBidder(clause, clause_id=i)
            self.bidders.append(bidder)

        # Create price manager
        self.price_manager = PriceManager(self.variables)

        # Auction state
        self.round_number = 0
        self.max_rounds = 1000

    def run_auction_round(self):
        """
        Run one round of the auction.

        Process:
        1. Each clause chooses best literal to bid on (given prices)
        2. Register all bids (demand)
        3. Check if equilibrium reached
        4. Update prices based on excess demand
        5. Help struggling clauses (budget boosts)
        """
        self.round_number += 1

        # Reset demand from previous round
        self.price_manager.reset_demand()

        # Phase 1: Bidders choose literals
        for bidder in self.bidders:
            # Get current prices
            prices = self._get_all_prices()

            # Choose best affordable literal
            chosen_literal = bidder.choose_best_literal(prices)

            if chosen_literal is not None:
                # Register demand for this literal
                var = chosen_literal.variable
                value = not chosen_literal.negated  # True if positive literal

                self.price_manager.register_demand(var, value)

                # Place bid
                price = self.price_manager.get_price(var, value)
                bidder.place_bid(chosen_literal, price)
            else:
                # Clause couldn't afford anything - mark unsatisfied
                bidder.lose_auction()

    def _get_all_prices(self) -> Dict:
        """Get current prices for all (variable, value) pairs."""
        prices = {}
        for var in self.variables:
            prices[(var, True)] = self.price_manager.get_price(var, True)
            prices[(var, False)] = self.price_manager.get_price(var, False)
        return prices

    def allocate_winners(self):
        """
        Allocate literals to winning bidders.

        For each variable:
        - If multiple clauses want same assignment, they all get it (non-rivalrous good)
        - Update each bidder's allocation status
        """
        for bidder in self.bidders:
            if bidder.current_bid is not None:
                literal, price = bidder.current_bid

                # Check if this literal's assignment was selected
                var = literal.variable
                value = not literal.negated

                demand = self.price_manager.get_demand(var, value)

                if demand > 0:
                    # This assignment has demand - allocate to bidder
                    bidder.win_auction(literal, price)
                else:
                    # No demand for this assignment
                    bidder.lose_auction()

    def check_all_satisfied(self) -> bool:
        """
        Check if all clauses are satisfied.

        Returns:
            True if all clauses have acquired satisfying literals
        """
        return all(bidder.is_satisfied for bidder in self.bidders)

    def boost_struggling_clauses(self):
        """
        Give budget boosts to unsatisfied clauses.

        This helps clauses that are "priced out" of the market.
        Similar to subsidies in real markets.
        """
        for bidder in self.bidders:
            if not bidder.is_satisfied:
                # Give 10% budget increase
                bidder.boost_budget(factor=1.1)

    def run_full_auction(self) -> Optional[Dict[str, bool]]:
        """
        Run complete auction process until equilibrium or timeout.

        Returns:
            Variable assignment if SAT, None if UNSAT or timeout
        """
        for round_num in range(self.max_rounds):
            # Run auction round
            self.run_auction_round()

            # Allocate to winners
            self.allocate_winners()

            # Check if equilibrium reached
            if self.price_manager.is_equilibrium():
                # Equilibrium reached!

                # Are all clauses satisfied?
                if self.check_all_satisfied():
                    # Success - extract assignment
                    assignment = self.price_manager.get_clearing_assignment()
                    return assignment
                else:
                    # Equilibrium but some clauses unsatisfied
                    # This means UNSAT (no affordable allocation exists)
                    return None

            # Update prices based on excess demand
            self.price_manager.update_prices()

            # Help struggling clauses
            self.boost_struggling_clauses()

        # Didn't converge within max rounds
        # Try to extract best effort solution
        if self.check_all_satisfied():
            return self.price_manager.get_clearing_assignment()
        else:
            return None  # Likely UNSAT or need more rounds

    def get_statistics(self) -> dict:
        """Get auction statistics."""
        num_satisfied = sum(1 for b in self.bidders if b.is_satisfied)
        total_spent = sum(b.spent for b in self.bidders if b.spent != float('inf'))

        return {
            'rounds': self.round_number,
            'num_clauses': len(self.bidders),
            'num_satisfied': num_satisfied,
            'satisfaction_rate': num_satisfied / len(self.bidders) if self.bidders else 0,
            'total_spent': total_spent,
            'at_equilibrium': self.price_manager.is_equilibrium(),
            'price_summary': self.price_manager.get_price_summary(),
        }

    def get_detailed_state(self) -> dict:
        """Get detailed auction state for debugging."""
        return {
            'round': self.round_number,
            'market': self.price_manager.get_market_state(),
            'bidders': [b.get_statistics() for b in self.bidders],
        }

    def __repr__(self):
        status = "EQUILIBRIUM" if self.price_manager.is_equilibrium() else f"Round {self.round_number}"
        satisfied = sum(1 for b in self.bidders if b.is_satisfied)
        return f"AuctionEngine({satisfied}/{len(self.bidders)} satisfied, {status})"
