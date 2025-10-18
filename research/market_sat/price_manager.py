"""
Price Manager for MARKET-SAT

Manages prices for variable assignments and implements price discovery through
tatonnement process (iterative price adjustment toward equilibrium).

Inspired by Walrasian general equilibrium theory.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from typing import Dict, Tuple, List
from collections import defaultdict


class PriceManager:
    """
    Manages prices for variable assignments in the SAT auction.

    Implements Walrasian tatonnement:
    - Prices rise when excess demand
    - Prices fall when excess supply
    - Equilibrium when supply = demand for all goods
    """

    def __init__(self, variables: List[str], initial_price: float = 1.0):
        """
        Initialize price manager.

        Args:
            variables: List of all variables in CNF
            initial_price: Starting price for all assignments
        """
        self.variables = variables

        # Prices: (variable, value) -> price
        # Example: ('x1', True) -> 1.5
        self.prices: Dict[Tuple[str, bool], float] = {}

        # Initialize all prices
        for var in variables:
            self.prices[(var, True)] = initial_price
            self.prices[(var, False)] = initial_price

        # Demand tracking
        self.demand: Dict[Tuple[str, bool], int] = defaultdict(int)

        # Supply (always 1 - each variable assigned once)
        self.supply = 1

        # Price adjustment parameters
        self.learning_rate = 0.1
        self.min_price = 0.01
        self.max_price = 1000.0

    def get_price(self, variable: str, value: bool) -> float:
        """
        Get current price for variable assignment.

        Args:
            variable: Variable name
            value: Assignment value (True/False)

        Returns:
            Current price
        """
        return self.prices.get((variable, value), 1.0)

    def reset_demand(self):
        """Reset demand counts for new auction round."""
        self.demand.clear()

    def register_demand(self, variable: str, value: bool):
        """
        Register demand for a variable assignment.

        Args:
            variable: Variable name
            value: Desired value
        """
        self.demand[(variable, value)] += 1

    def get_demand(self, variable: str, value: bool) -> int:
        """
        Get current demand for variable assignment.

        Args:
            variable: Variable name
            value: Assignment value

        Returns:
            Number of clauses demanding this assignment
        """
        return self.demand.get((variable, value), 0)

    def compute_excess_demand(self, variable: str, value: bool) -> int:
        """
        Compute excess demand for variable assignment.

        Excess demand = demand - supply
        Positive = shortage (need to raise price)
        Negative = surplus (can lower price)
        Zero = market clears

        Args:
            variable: Variable name
            value: Assignment value

        Returns:
            Excess demand
        """
        demand = self.get_demand(variable, value)
        return demand - self.supply

    def update_prices(self):
        """
        Update prices based on excess demand (tatonnement process).

        Classical price adjustment rule:
            dP/dt = α * ED(P)

        Where:
            P = price
            ED = excess demand
            α = learning rate (speed of adjustment)
        """
        for var in self.variables:
            for value in [True, False]:
                # Compute excess demand
                excess = self.compute_excess_demand(var, value)

                # Price adjustment
                if excess > 0:
                    # Excess demand - raise price
                    adjustment = self.learning_rate * excess
                    self.prices[(var, value)] += adjustment
                elif excess < 0:
                    # Excess supply - lower price
                    adjustment = self.learning_rate * abs(excess)
                    self.prices[(var, value)] -= adjustment
                # else: excess == 0, market clears, don't change price

                # Keep prices bounded
                self.prices[(var, value)] = max(
                    self.min_price,
                    min(self.prices[(var, value)], self.max_price)
                )

    def is_equilibrium(self) -> bool:
        """
        Check if market has reached equilibrium.

        Equilibrium condition:
        For each variable, exactly one value has demand = supply (1),
        and the other value has demand = 0.

        Returns:
            True if market at equilibrium
        """
        for var in self.variables:
            demand_true = self.get_demand(var, True)
            demand_false = self.get_demand(var, False)

            # Equilibrium: one value demanded, other not
            equilibrium_condition = (
                (demand_true == 1 and demand_false == 0) or
                (demand_true == 0 and demand_false == 1)
            )

            if not equilibrium_condition:
                return False  # At least one variable not at equilibrium

        return True  # All variables at equilibrium

    def get_clearing_assignment(self) -> Dict[str, bool]:
        """
        Extract variable assignment from market-clearing prices.

        For each variable, assign to the value with positive demand.

        Returns:
            Variable assignment
        """
        assignment = {}

        for var in self.variables:
            demand_true = self.get_demand(var, True)
            demand_false = self.get_demand(var, False)

            if demand_true > 0:
                assignment[var] = True
            elif demand_false > 0:
                assignment[var] = False
            else:
                # No demand for either (shouldn't happen at equilibrium)
                # Assign arbitrarily
                assignment[var] = True

        return assignment

    def get_price_summary(self) -> dict:
        """Get summary of current prices."""
        return {
            'average_price': sum(self.prices.values()) / len(self.prices),
            'max_price': max(self.prices.values()),
            'min_price': min(self.prices.values()),
            'num_variables': len(self.variables),
        }

    def get_market_state(self) -> dict:
        """Get complete market state for debugging."""
        state = {}

        for var in self.variables:
            state[var] = {
                'price_true': self.get_price(var, True),
                'price_false': self.get_price(var, False),
                'demand_true': self.get_demand(var, True),
                'demand_false': self.get_demand(var, False),
                'excess_true': self.compute_excess_demand(var, True),
                'excess_false': self.compute_excess_demand(var, False),
            }

        return state

    def __repr__(self):
        at_equilibrium = "EQUILIBRIUM" if self.is_equilibrium() else "ADJUSTING"
        return f"PriceManager({len(self.variables)} vars, {at_equilibrium})"
