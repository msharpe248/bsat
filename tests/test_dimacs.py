"""
Tests for DIMACS format support
"""

import unittest
import tempfile
import os
from pathlib import Path

import sys
sys.path.insert(0, str(Path(__file__).parent.parent))

from bsat import CNFExpression, Clause, Literal
from bsat.dimacs import (
    parse_dimacs, to_dimacs, read_dimacs_file, write_dimacs_file,
    parse_dimacs_solution, solution_to_dimacs, DIMACSParseError
)


class TestDIMACSParsing(unittest.TestCase):
    """Test DIMACS format parsing."""

    def test_simple_formula(self):
        """Test parsing simple DIMACS formula."""
        dimacs = """
        c Simple formula
        p cnf 3 2
        1 -2 3 0
        -1 2 0
        """
        cnf = parse_dimacs(dimacs)

        self.assertEqual(len(cnf.clauses), 2)
        self.assertEqual(len(cnf.clauses[0].literals), 3)
        self.assertEqual(len(cnf.clauses[1].literals), 2)

    def test_with_comments(self):
        """Test parsing with multiple comments."""
        dimacs = """
        c This is a comment
        c Another comment
        p cnf 2 1
        c Comment between clauses
        1 2 0
        """
        cnf = parse_dimacs(dimacs)
        self.assertEqual(len(cnf.clauses), 1)

    def test_empty_lines(self):
        """Test parsing with empty lines."""
        dimacs = """
        p cnf 2 1

        1 2 0

        """
        cnf = parse_dimacs(dimacs)
        self.assertEqual(len(cnf.clauses), 1)

    def test_variable_numbering(self):
        """Test that variables are correctly numbered."""
        dimacs = """
        p cnf 3 1
        1 -2 3 0
        """
        cnf = parse_dimacs(dimacs)
        clause = cnf.clauses[0]

        # Check literals
        self.assertEqual(clause.literals[0].variable, 'x1')
        self.assertFalse(clause.literals[0].negated)

        self.assertEqual(clause.literals[1].variable, 'x2')
        self.assertTrue(clause.literals[1].negated)

        self.assertEqual(clause.literals[2].variable, 'x3')
        self.assertFalse(clause.literals[2].negated)

    def test_unit_clause(self):
        """Test parsing unit clauses."""
        dimacs = """
        p cnf 2 2
        1 0
        -2 0
        """
        cnf = parse_dimacs(dimacs)
        self.assertEqual(len(cnf.clauses), 2)
        self.assertEqual(len(cnf.clauses[0].literals), 1)
        self.assertEqual(len(cnf.clauses[1].literals), 1)

    def test_large_variables(self):
        """Test parsing with large variable numbers."""
        dimacs = """
        p cnf 100 1
        1 -50 100 0
        """
        cnf = parse_dimacs(dimacs)
        self.assertEqual(len(cnf.clauses), 1)
        vars = cnf.get_variables()
        self.assertIn('x1', vars)
        self.assertIn('x50', vars)
        self.assertIn('x100', vars)


class TestDIMACSErrors(unittest.TestCase):
    """Test DIMACS error handling."""

    def test_no_problem_line(self):
        """Test error when problem line is missing."""
        dimacs = """
        1 2 0
        """
        with self.assertRaises(DIMACSParseError):
            parse_dimacs(dimacs)

    def test_invalid_problem_line(self):
        """Test error on invalid problem line."""
        dimacs = """
        p sat 3 2
        1 2 0
        """
        with self.assertRaises(DIMACSParseError):
            parse_dimacs(dimacs)

    def test_missing_zero_terminator(self):
        """Test error when clause doesn't end with 0."""
        dimacs = """
        p cnf 3 1
        1 2 3
        """
        with self.assertRaises(DIMACSParseError):
            parse_dimacs(dimacs)

    def test_variable_exceeds_declared(self):
        """Test error when variable number exceeds declared."""
        dimacs = """
        p cnf 3 1
        1 2 5 0
        """
        with self.assertRaises(DIMACSParseError):
            parse_dimacs(dimacs)

    def test_invalid_literal(self):
        """Test error on non-integer literal."""
        dimacs = """
        p cnf 3 1
        1 abc 3 0
        """
        with self.assertRaises(DIMACSParseError):
            parse_dimacs(dimacs)

    def test_zero_in_middle(self):
        """Test error when 0 appears in middle of clause."""
        dimacs = """
        p cnf 3 1
        1 0 2 3 0
        """
        with self.assertRaises(DIMACSParseError):
            parse_dimacs(dimacs)


class TestDIMACSGeneration(unittest.TestCase):
    """Test DIMACS format generation."""

    def test_simple_output(self):
        """Test generating simple DIMACS output."""
        cnf = CNFExpression([
            Clause([Literal('x1', False), Literal('x2', True)]),
            Clause([Literal('x2', False), Literal('x3', False)])
        ])

        dimacs = to_dimacs(cnf)

        # Parse it back
        cnf2 = parse_dimacs(dimacs)
        self.assertEqual(len(cnf2.clauses), 2)

    def test_with_comments(self):
        """Test generating DIMACS with comments."""
        cnf = CNFExpression([
            Clause([Literal('x1', False)])
        ])

        dimacs = to_dimacs(cnf, comments=['Test formula', 'Generated by unit test'])

        self.assertIn('c Test formula', dimacs)
        self.assertIn('c Generated by unit test', dimacs)

    def test_roundtrip(self):
        """Test parse → generate → parse roundtrip."""
        original = """
        p cnf 4 3
        1 -2 3 0
        -1 4 0
        2 -3 -4 0
        """

        cnf1 = parse_dimacs(original)
        dimacs = to_dimacs(cnf1)
        cnf2 = parse_dimacs(dimacs)

        # Should have same structure
        self.assertEqual(len(cnf1.clauses), len(cnf2.clauses))
        for i in range(len(cnf1.clauses)):
            self.assertEqual(len(cnf1.clauses[i].literals),
                           len(cnf2.clauses[i].literals))

    def test_variable_ordering(self):
        """Test that variables are consistently ordered."""
        cnf = CNFExpression([
            Clause([Literal('x3', False), Literal('x1', True), Literal('x2', False)])
        ])

        dimacs = to_dimacs(cnf)
        # Variables should be mapped consistently (x1=1, x2=2, x3=3)
        self.assertIn('p cnf 3 1', dimacs)


class TestDIMACSFileIO(unittest.TestCase):
    """Test DIMACS file reading and writing."""

    def test_write_and_read_file(self):
        """Test writing and reading DIMACS file."""
        cnf = CNFExpression([
            Clause([Literal('x1', False), Literal('x2', True)]),
            Clause([Literal('x2', False)])
        ])

        with tempfile.TemporaryDirectory() as tmpdir:
            filepath = os.path.join(tmpdir, 'test.cnf')

            # Write
            write_dimacs_file(cnf, filepath, comments=['Test file'])

            # Read back
            cnf2 = read_dimacs_file(filepath)

            self.assertEqual(len(cnf.clauses), len(cnf2.clauses))

    def test_read_nonexistent_file(self):
        """Test error when reading nonexistent file."""
        with self.assertRaises(FileNotFoundError):
            read_dimacs_file('/nonexistent/file.cnf')


class TestDIMACSSolution(unittest.TestCase):
    """Test DIMACS solution format."""

    def test_parse_sat_solution(self):
        """Test parsing SAT solution."""
        solution = """
        s SATISFIABLE
        v 1 -2 3 0
        """
        sol = parse_dimacs_solution(solution)

        self.assertIsNotNone(sol)
        self.assertTrue(sol['x1'])
        self.assertFalse(sol['x2'])
        self.assertTrue(sol['x3'])

    def test_parse_unsat_solution(self):
        """Test parsing UNSAT solution."""
        solution = """
        s UNSATISFIABLE
        """
        sol = parse_dimacs_solution(solution)
        self.assertIsNone(sol)

    def test_parse_multiline_solution(self):
        """Test parsing solution split across multiple lines."""
        solution = """
        s SATISFIABLE
        v 1 -2 3 0
        v 4 -5 0
        """
        sol = parse_dimacs_solution(solution)

        self.assertIsNotNone(sol)
        self.assertEqual(len(sol), 5)
        self.assertTrue(sol['x1'])
        self.assertTrue(sol['x4'])
        self.assertFalse(sol['x5'])

    def test_generate_sat_solution(self):
        """Test generating SAT solution."""
        sol = {'x1': True, 'x2': False, 'x3': True}
        dimacs = solution_to_dimacs(sol, satisfiable=True)

        self.assertIn('s SATISFIABLE', dimacs)
        self.assertIn('v', dimacs)
        self.assertIn('1', dimacs)
        self.assertIn('-2', dimacs)
        self.assertIn('3', dimacs)
        self.assertIn('0', dimacs)

    def test_generate_unsat_solution(self):
        """Test generating UNSAT solution."""
        dimacs = solution_to_dimacs({}, satisfiable=False)
        self.assertEqual(dimacs.strip(), 's UNSATISFIABLE')

    def test_solution_roundtrip(self):
        """Test solution generate → parse roundtrip."""
        original = {'x1': True, 'x2': False, 'x5': True, 'x10': False}
        dimacs = solution_to_dimacs(original)
        parsed = parse_dimacs_solution(dimacs)

        for var, val in original.items():
            self.assertEqual(parsed[var], val)


class TestDIMACSWithSolvers(unittest.TestCase):
    """Test DIMACS integration with solvers."""

    def test_solve_dimacs_formula(self):
        """Test solving a formula read from DIMACS."""
        from bsat import solve_sat

        dimacs = """
        c Simple SAT formula
        p cnf 3 3
        1 2 0
        -1 3 0
        -2 -3 0
        """

        cnf = parse_dimacs(dimacs)
        solution = solve_sat(cnf)

        self.assertIsNotNone(solution)
        self.assertTrue(cnf.evaluate(solution))

    def test_solve_and_export_solution(self):
        """Test solving and exporting solution in DIMACS format."""
        from bsat import solve_cdcl

        dimacs = """
        p cnf 4 4
        1 2 3 0
        -1 -2 0
        -2 -3 0
        -3 -4 0
        """

        cnf = parse_dimacs(dimacs)
        solution = solve_cdcl(cnf)

        if solution:
            # Export solution to DIMACS
            sol_dimacs = solution_to_dimacs(solution)

            # Verify it can be parsed
            parsed_sol = parse_dimacs_solution(sol_dimacs)
            self.assertIsNotNone(parsed_sol)

    def test_pigeon_hole_dimacs(self):
        """Test pigeon-hole problem in DIMACS format."""
        from bsat import solve_sat

        # 3 pigeons, 2 holes (UNSAT)
        dimacs = """
        c Pigeon-hole: 3 pigeons, 2 holes
        p cnf 6 9
        c Each pigeon in at least one hole
        1 2 0
        3 4 0
        5 6 0
        c At most one pigeon per hole
        -1 -3 0
        -1 -5 0
        -3 -5 0
        -2 -4 0
        -2 -6 0
        -4 -6 0
        """

        cnf = parse_dimacs(dimacs)
        solution = solve_sat(cnf)
        self.assertIsNone(solution)


if __name__ == '__main__':
    unittest.main()
