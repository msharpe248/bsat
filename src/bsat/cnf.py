"""Data structures and methods for working with CNF (Conjunctive Normal Form) expressions."""

import json
import re
from typing import Set, Dict, List, Tuple
from itertools import product


class Literal:
    """Represents a literal in a CNF expression (a variable or its negation)."""

    def __init__(self, variable: str, negated: bool = False):
        """
        Create a literal.

        Args:
            variable: The variable name (e.g., 'x', 'y', 'p1')
            negated: Whether this literal is negated
        """
        self.variable = variable
        self.negated = negated

    def __str__(self) -> str:
        """Return string representation using typical logical notation."""
        return f"¬{self.variable}" if self.negated else self.variable

    def __repr__(self) -> str:
        return f"Literal({self.variable!r}, {self.negated})"

    def __eq__(self, other) -> bool:
        if not isinstance(other, Literal):
            return False
        return self.variable == other.variable and self.negated == other.negated

    def __hash__(self) -> int:
        return hash((self.variable, self.negated))

    def evaluate(self, assignment: Dict[str, bool]) -> bool:
        """
        Evaluate the literal given a variable assignment.

        Args:
            assignment: Dictionary mapping variable names to boolean values

        Returns:
            The boolean value of this literal
        """
        value = assignment.get(self.variable, False)
        return not value if self.negated else value

    def to_dict(self) -> dict:
        """Convert to dictionary for JSON serialization."""
        return {"variable": self.variable, "negated": self.negated}

    @classmethod
    def from_dict(cls, data: dict) -> 'Literal':
        """Create a Literal from a dictionary."""
        return cls(data["variable"], data["negated"])


class Clause:
    """Represents a clause in a CNF expression (disjunction of literals)."""

    def __init__(self, literals: List[Literal]):
        """
        Create a clause.

        Args:
            literals: List of literals in this clause
        """
        self.literals = literals

    def __str__(self) -> str:
        """Return string representation using typical logical notation."""
        if not self.literals:
            return "⊥"  # Empty clause (always false)
        if len(self.literals) == 1:
            return str(self.literals[0])
        return "(" + " ∨ ".join(str(lit) for lit in self.literals) + ")"

    def __repr__(self) -> str:
        return f"Clause({self.literals!r})"

    def __eq__(self, other) -> bool:
        if not isinstance(other, Clause):
            return False
        return set(self.literals) == set(other.literals)

    def __hash__(self) -> int:
        return hash(frozenset(self.literals))

    def evaluate(self, assignment: Dict[str, bool]) -> bool:
        """
        Evaluate the clause given a variable assignment.

        Args:
            assignment: Dictionary mapping variable names to boolean values

        Returns:
            True if at least one literal is true, False otherwise
        """
        if not self.literals:
            return False
        return any(lit.evaluate(assignment) for lit in self.literals)

    def get_variables(self) -> Set[str]:
        """Get all variables appearing in this clause."""
        return {lit.variable for lit in self.literals}

    def to_dict(self) -> dict:
        """Convert to dictionary for JSON serialization."""
        return {"literals": [lit.to_dict() for lit in self.literals]}

    @classmethod
    def from_dict(cls, data: dict) -> 'Clause':
        """Create a Clause from a dictionary."""
        literals = [Literal.from_dict(lit_data) for lit_data in data["literals"]]
        return cls(literals)


class CNFExpression:
    """Represents a CNF (Conjunctive Normal Form) expression."""

    def __init__(self, clauses: List[Clause]):
        """
        Create a CNF expression.

        Args:
            clauses: List of clauses (conjunction of clauses)
        """
        self.clauses = clauses

    def __str__(self) -> str:
        """Return string representation using typical logical notation."""
        if not self.clauses:
            return "⊤"  # Empty CNF (always true)
        if len(self.clauses) == 1:
            return str(self.clauses[0])
        return " ∧ ".join(str(clause) for clause in self.clauses)

    def __repr__(self) -> str:
        return f"CNFExpression({self.clauses!r})"

    def __eq__(self, other) -> bool:
        """
        Check structural equality (not logical equivalence).
        For logical equivalence, use is_equivalent().
        """
        if not isinstance(other, CNFExpression):
            return False
        return set(self.clauses) == set(other.clauses)

    def evaluate(self, assignment: Dict[str, bool]) -> bool:
        """
        Evaluate the CNF expression given a variable assignment.

        Args:
            assignment: Dictionary mapping variable names to boolean values

        Returns:
            True if all clauses are true, False otherwise
        """
        if not self.clauses:
            return True
        return all(clause.evaluate(assignment) for clause in self.clauses)

    def get_variables(self) -> Set[str]:
        """Get all variables appearing in this CNF expression."""
        variables = set()
        for clause in self.clauses:
            variables.update(clause.get_variables())
        return variables

    def generate_truth_table(self) -> List[Tuple[Dict[str, bool], bool]]:
        """
        Generate the complete truth table for this expression.

        Returns:
            List of tuples (assignment, result) for all possible variable assignments
        """
        variables = sorted(self.get_variables())
        if not variables:
            return [({}, self.evaluate({}))]

        truth_table = []
        for values in product([False, True], repeat=len(variables)):
            assignment = dict(zip(variables, values))
            result = self.evaluate(assignment)
            truth_table.append((assignment, result))

        return truth_table

    def print_truth_table(self) -> None:
        """Print a formatted truth table for this expression."""
        variables = sorted(self.get_variables())

        if not variables:
            print("No variables in expression")
            print(f"Result: {self.evaluate({})}")
            return

        # Print header
        header = " | ".join(variables) + " | Result"
        separator = "-" * len(header)
        print(separator)
        print(header)
        print(separator)

        # Print rows
        for assignment, result in self.generate_truth_table():
            values = [str(int(assignment[var])) for var in variables]
            result_str = str(int(result))
            row = " | ".join(values) + " | " + result_str
            print(row)

        print(separator)

    def is_equivalent(self, other: 'CNFExpression') -> bool:
        """
        Check if two CNF expressions are logically equivalent using truth tables.

        Args:
            other: Another CNF expression to compare with

        Returns:
            True if the expressions are logically equivalent, False otherwise
        """
        if not isinstance(other, CNFExpression):
            return False

        # Get all variables from both expressions
        all_variables = sorted(self.get_variables() | other.get_variables())

        if not all_variables:
            # Both expressions have no variables
            return self.evaluate({}) == other.evaluate({})

        # Check all possible assignments
        for values in product([False, True], repeat=len(all_variables)):
            assignment = dict(zip(all_variables, values))
            if self.evaluate(assignment) != other.evaluate(assignment):
                return False

        return True

    def to_dict(self) -> dict:
        """Convert to dictionary for JSON serialization."""
        return {"clauses": [clause.to_dict() for clause in self.clauses]}

    def to_json(self, indent: int = 2) -> str:
        """
        Convert to JSON string.

        Args:
            indent: Indentation level for pretty printing

        Returns:
            JSON string representation
        """
        return json.dumps(self.to_dict(), indent=indent)

    @classmethod
    def from_dict(cls, data: dict) -> 'CNFExpression':
        """Create a CNF expression from a dictionary."""
        clauses = [Clause.from_dict(clause_data) for clause_data in data["clauses"]]
        return cls(clauses)

    @classmethod
    def from_json(cls, json_str: str) -> 'CNFExpression':
        """
        Create a CNF expression from a JSON string.

        Args:
            json_str: JSON string representation

        Returns:
            CNF expression object
        """
        data = json.loads(json_str)
        return cls.from_dict(data)

    @classmethod
    def parse(cls, expression: str) -> 'CNFExpression':
        """
        Parse a CNF expression from a string using typical logical notation.

        Supports:
        - Variables: alphanumeric names (x, y, p1, etc.)
        - Negation: ¬, ~, !, NOT
        - Disjunction (OR): ∨, |, OR
        - Conjunction (AND): ∧, &, AND
        - Parentheses for grouping

        Examples:
            "(x ∨ y) ∧ (¬x ∨ z)"
            "(a | b) & (~a | c)"
            "(p OR q) AND (NOT p OR r)"

        Args:
            expression: String representation of the CNF expression

        Returns:
            CNF expression object
        """
        # Normalize the expression
        expr = expression.strip()

        # Replace various notation styles with standard symbols
        expr = re.sub(r'\bNOT\b', '¬', expr, flags=re.IGNORECASE)
        expr = re.sub(r'\bOR\b', '∨', expr, flags=re.IGNORECASE)
        expr = re.sub(r'\bAND\b', '∧', expr, flags=re.IGNORECASE)
        expr = expr.replace('~', '¬')
        expr = expr.replace('!', '¬')
        expr = expr.replace('|', '∨')
        expr = expr.replace('&', '∧')

        # Split by conjunction (AND)
        clause_strs = re.split(r'\s*∧\s*', expr)

        clauses = []
        for clause_str in clause_strs:
            clause_str = clause_str.strip()

            # Remove outer parentheses if present
            if clause_str.startswith('(') and clause_str.endswith(')'):
                clause_str = clause_str[1:-1].strip()

            # Split by disjunction (OR)
            literal_strs = re.split(r'\s*∨\s*', clause_str)

            literals = []
            for lit_str in literal_strs:
                lit_str = lit_str.strip()

                # Check for negation
                negated = False
                if lit_str.startswith('¬'):
                    negated = True
                    lit_str = lit_str[1:].strip()

                # Extract variable name
                match = re.match(r'^([a-zA-Z_][a-zA-Z0-9_]*)$', lit_str)
                if not match:
                    raise ValueError(f"Invalid variable name: {lit_str}")

                variable = match.group(1)
                literals.append(Literal(variable, negated))

            clauses.append(Clause(literals))

        return cls(clauses)
