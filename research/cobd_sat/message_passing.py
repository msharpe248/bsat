"""
Message Passing for Community Coordination

This module implements message passing between communities to coordinate
their solving efforts. Instead of enumerating all interface variable assignments,
we use constraint propagation to narrow down possibilities.

Key Concepts:
- Messages: Constraints on interface variables sent between communities
- Beliefs: Current probability distribution over interface variable values
- Convergence: Iterative refinement until beliefs stabilize
"""

from typing import Dict, List, Set, Tuple, Optional
from collections import defaultdict
import sys
import os

# Add src to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from bsat.cnf import CNFExpression, Clause, Literal


class Message:
    """
    Represents a message about interface variable constraints.

    A message contains information about which values of interface variables
    are compatible with a community being satisfiable.
    """

    def __init__(self, from_community: int, to_community: int, variable: str):
        """
        Initialize a message.

        Args:
            from_community: Source community ID
            to_community: Target community ID
            variable: Interface variable this message is about
        """
        self.from_community = from_community
        self.to_community = to_community
        self.variable = variable
        # Beliefs: probability that variable=True is compatible
        self.belief_true = 0.5  # Initially neutral
        self.belief_false = 0.5

    def update_beliefs(self, true_compatible: bool, false_compatible: bool):
        """
        Update beliefs based on compatibility check.

        Args:
            true_compatible: Is variable=True compatible with sender?
            false_compatible: Is variable=False compatible with sender?
        """
        if true_compatible and not false_compatible:
            self.belief_true = 1.0
            self.belief_false = 0.0
        elif false_compatible and not true_compatible:
            self.belief_true = 0.0
            self.belief_false = 1.0
        elif true_compatible and false_compatible:
            self.belief_true = 0.5
            self.belief_false = 0.5
        else:  # Neither compatible - conflict!
            self.belief_true = 0.0
            self.belief_false = 0.0

    def is_forced(self) -> Optional[bool]:
        """
        Check if message forces a specific value.

        Returns:
            True if forces True, False if forces False, None if no forcing
        """
        if self.belief_true == 1.0 and self.belief_false == 0.0:
            return True
        elif self.belief_true == 0.0 and self.belief_false == 1.0:
            return False
        return None

    def indicates_conflict(self) -> bool:
        """Check if message indicates a conflict (no value works)."""
        return self.belief_true == 0.0 and self.belief_false == 0.0


class MessagePasser:
    """
    Coordinates communities through iterative message passing.

    This implements a belief propagation-like algorithm where communities
    exchange information about their constraints on interface variables.
    """

    def __init__(self, community_formulas: Dict[int, CNFExpression],
                 interface_variables: Set[str],
                 variable_communities: Dict[str, int],
                 clause_communities: Dict[str, int]):
        """
        Initialize message passer.

        Args:
            community_formulas: CNF formula for each community
            interface_variables: Set of interface variables
            variable_communities: Mapping of variables to their primary community
            clause_communities: Mapping of clause IDs to communities
        """
        self.community_formulas = community_formulas
        self.interface_variables = interface_variables
        self.variable_communities = variable_communities
        self.clause_communities = clause_communities

        # Message state
        self.messages: Dict[Tuple[int, int, str], Message] = {}
        self._initialize_messages()

        # Beliefs about interface variables (aggregated from messages)
        self.interface_beliefs: Dict[str, Tuple[float, float]] = {}  # var -> (belief_true, belief_false)

    def _initialize_messages(self):
        """
        Initialize messages between communities.

        For each interface variable, create messages from each community
        that uses it to all other communities that use it.
        """
        # Find which communities each interface variable connects
        var_to_communities = defaultdict(set)
        for var in self.interface_variables:
            # Find all communities this variable appears in
            # (through the clauses they contain)
            for comm_id, formula in self.community_formulas.items():
                if var in formula.get_variables():
                    var_to_communities[var].add(comm_id)

        # Create messages
        for var, communities in var_to_communities.items():
            communities_list = list(communities)
            # Each community sends message to each other community
            for from_comm in communities_list:
                for to_comm in communities_list:
                    if from_comm != to_comm:
                        msg = Message(from_comm, to_comm, var)
                        self.messages[(from_comm, to_comm, var)] = msg

    def propagate(self, max_iterations: int = 10) -> Dict[str, Optional[bool]]:
        """
        Run message passing to determine interface variable assignments.

        Iteratively refines beliefs about interface variables by having
        communities send messages about what values are compatible.

        Args:
            max_iterations: Maximum iterations to run

        Returns:
            Dictionary mapping interface variables to forced values
            (None if not forced)
        """
        for iteration in range(max_iterations):
            # Update all messages
            changed = self._update_all_messages()

            # Aggregate beliefs
            self._aggregate_beliefs()

            # Check for convergence
            if not changed:
                break

        # Extract forced assignments
        forced_assignments = {}
        for var in self.interface_variables:
            forced_value = self._get_forced_value(var)
            forced_assignments[var] = forced_value

        return forced_assignments

    def _update_all_messages(self) -> bool:
        """
        Update all messages based on current formula state.

        Returns:
            True if any message changed, False otherwise
        """
        changed = False

        for (from_comm, to_comm, var), msg in self.messages.items():
            # Test if variable=True is compatible with from_community
            true_compatible = self._is_compatible(from_comm, var, True)

            # Test if variable=False is compatible with from_community
            false_compatible = self._is_compatible(from_comm, var, False)

            # Update message beliefs
            old_beliefs = (msg.belief_true, msg.belief_false)
            msg.update_beliefs(true_compatible, false_compatible)
            new_beliefs = (msg.belief_true, msg.belief_false)

            if old_beliefs != new_beliefs:
                changed = True

        return changed

    def _is_compatible(self, community_id: int, variable: str, value: bool) -> bool:
        """
        Check if a variable assignment is compatible with a community.

        This is a simplified check: we see if the assignment immediately
        leads to an empty clause in the community formula.

        Args:
            community_id: Community to check
            variable: Variable to assign
            value: Value to assign

        Returns:
            True if assignment is compatible (doesn't create immediate conflict)
        """
        if community_id not in self.community_formulas:
            return True

        formula = self.community_formulas[community_id]
        assignment = {variable: value}

        # Simplify and check for empty clause
        for clause in formula.clauses:
            clause_satisfied = False
            all_false = True

            for literal in clause.literals:
                if literal.variable == variable:
                    # Check if this literal is satisfied or falsified
                    lit_value = (not value) if literal.negated else value
                    if lit_value:
                        clause_satisfied = True
                        break
                    # else: literal is false, continue
                else:
                    # Other variable in clause
                    all_false = False

            # If clause has only the variable and it's false, conflict
            if all_false and not clause_satisfied:
                return False

        return True

    def _aggregate_beliefs(self):
        """
        Aggregate messages to compute beliefs about interface variables.

        For each interface variable, combine messages from all communities.
        """
        self.interface_beliefs = {}

        for var in self.interface_variables:
            # Collect all messages about this variable
            messages_about_var = [
                msg for (from_c, to_c, v), msg in self.messages.items()
                if v == var
            ]

            if not messages_about_var:
                # No messages - neutral belief
                self.interface_beliefs[var] = (0.5, 0.5)
                continue

            # Aggregate: If any message forces a value, use that
            # Otherwise, average the beliefs
            forced_true = any(msg.belief_true == 1.0 and msg.belief_false == 0.0
                            for msg in messages_about_var)
            forced_false = any(msg.belief_true == 0.0 and msg.belief_false == 1.0
                             for msg in messages_about_var)

            if forced_true and not forced_false:
                self.interface_beliefs[var] = (1.0, 0.0)
            elif forced_false and not forced_true:
                self.interface_beliefs[var] = (0.0, 1.0)
            elif forced_true and forced_false:
                # Conflict! Both values forced
                self.interface_beliefs[var] = (0.0, 0.0)
            else:
                # Average beliefs
                avg_true = sum(msg.belief_true for msg in messages_about_var) / len(messages_about_var)
                avg_false = sum(msg.belief_false for msg in messages_about_var) / len(messages_about_var)
                self.interface_beliefs[var] = (avg_true, avg_false)

    def _get_forced_value(self, variable: str) -> Optional[bool]:
        """
        Get forced value for a variable based on aggregated beliefs.

        Args:
            variable: Variable to check

        Returns:
            True if forced to True, False if forced to False, None otherwise
        """
        if variable not in self.interface_beliefs:
            return None

        belief_true, belief_false = self.interface_beliefs[variable]

        if belief_true == 1.0 and belief_false == 0.0:
            return True
        elif belief_true == 0.0 and belief_false == 1.0:
            return False
        else:
            return None

    def has_conflict(self) -> bool:
        """
        Check if message passing detected a conflict.

        Returns:
            True if any variable has no compatible value
        """
        return any(msg.indicates_conflict() for msg in self.messages.values())

    def get_statistics(self) -> Dict:
        """
        Get statistics about message passing.

        Returns:
            Dictionary with message passing statistics
        """
        num_forced = sum(1 for var in self.interface_variables
                        if self._get_forced_value(var) is not None)

        num_conflicts = sum(1 for msg in self.messages.values()
                           if msg.indicates_conflict())

        return {
            'num_messages': len(self.messages),
            'num_interface_vars': len(self.interface_variables),
            'num_forced': num_forced,
            'forced_percentage': 100.0 * num_forced / len(self.interface_variables) if self.interface_variables else 0,
            'num_conflicts': num_conflicts,
            'has_conflict': self.has_conflict()
        }


def test_message_passing():
    """
    Simple test of message passing functionality.
    """
    from bsat import CNFExpression

    # Create a simple formula with two communities
    # Community 1: (a | b) & (a | c)
    # Community 2: (c | d) & (d | e)
    # Interface variable: c
    cnf = CNFExpression.parse("(a | b) & (a | c) & (c | d) & (d | e)")

    # Manually create communities (in practice, this would come from detector)
    formula1 = CNFExpression.parse("(a | b) & (a | c)")
    formula2 = CNFExpression.parse("(c | d) & (d | e)")

    community_formulas = {0: formula1, 1: formula2}
    interface_variables = {'c'}
    variable_communities = {'a': 0, 'b': 0, 'c': 0, 'd': 1, 'e': 1}
    clause_communities = {'C0': 0, 'C1': 0, 'C2': 1, 'C3': 1}

    # Run message passing
    passer = MessagePasser(
        community_formulas,
        interface_variables,
        variable_communities,
        clause_communities
    )

    forced = passer.propagate()
    stats = passer.get_statistics()

    print("Forced assignments:", forced)
    print("Statistics:", stats)


if __name__ == "__main__":
    test_message_passing()
