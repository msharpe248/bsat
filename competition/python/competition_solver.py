#!/usr/bin/env python3
"""
Competition-Compatible SAT Solver

Wrapper around cdcl_optimized.py that provides SAT competition-compatible I/O:
- Reads DIMACS CNF files
- Outputs solutions in competition format
- Supports command-line interface
- Reports statistics to stderr

Usage:
    python competition_solver.py <input.cnf>
    python competition_solver.py <input.cnf> --output <solution.txt>
    python competition_solver.py <input.cnf> --max-conflicts 100000
    python competition_solver.py <input.cnf> --verbose
"""

import sys
import os
import argparse
import time

# Add src to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))

from bsat.dimacs import read_dimacs_file
from bsat.cnf import CNFExpression
import cdcl_optimized


def solve_competition_format(cnf_file: str,
                             max_conflicts: int = 1000000,
                             verbose: bool = False,
                             output_file: str = None):
    """
    Solve a DIMACS CNF file and output in competition format.

    Competition output format:
    - Status line: "s SATISFIABLE" or "s UNSATISFIABLE"
    - Solution (if SAT): "v <lit1> <lit2> ... <litn> 0"
    - Comments: "c <message>" (to stderr if verbose)

    Args:
        cnf_file: Path to DIMACS CNF file
        max_conflicts: Maximum conflicts before timeout
        verbose: Print statistics to stderr
        output_file: Output file (default: stdout)
    """
    # Read DIMACS file
    if verbose:
        print(f"c Reading {cnf_file}...", file=sys.stderr)

    try:
        cnf = read_dimacs_file(cnf_file)
    except Exception as e:
        print(f"c ERROR: Failed to read CNF file: {e}", file=sys.stderr)
        sys.exit(1)

    num_vars = len(cnf.get_variables())
    num_clauses = len(cnf.clauses)

    if verbose:
        print(f"c Instance: {num_vars} variables, {num_clauses} clauses", file=sys.stderr)
        print(f"c Solver: CDCL with two-watched literals, LBD, adaptive restarts, phase saving", file=sys.stderr)
        print(f"c Max conflicts: {max_conflicts}", file=sys.stderr)

    # Create solver
    # Two-watched literals now FIXED and enabled (50-100× faster propagation)
    solver = cdcl_optimized.CDCLSolver(
        cnf,
        use_watched_literals=True,  # ✅ FIXED - soundness bugs resolved
        phase_saving=True,
        restart_strategy='glucose',
        restart_postponing=True,
        adaptive_random_phase=True,  # Week 7: Auto-enable when stuck
    )

    # Solve
    if verbose:
        print("c Solving...", file=sys.stderr)

    start_time = time.time()
    result = solver.solve(max_conflicts=max_conflicts)
    elapsed_time = time.time() - start_time

    # Prepare output
    output_lines = []

    if result is None:
        # UNSAT or timeout
        if solver.stats.conflicts >= max_conflicts:
            output_lines.append("s UNKNOWN")
            if verbose:
                print(f"c TIMEOUT after {solver.stats.conflicts} conflicts", file=sys.stderr)
        else:
            output_lines.append("s UNSATISFIABLE")
            if verbose:
                print(f"c UNSATISFIABLE", file=sys.stderr)
    else:
        # SAT
        output_lines.append("s SATISFIABLE")

        # Convert solution to DIMACS format (integers)
        # Variables are named like 'x1', 'x2', ... or just '1', '2', ...
        # We need to extract the integer and apply polarity
        solution_literals = []

        # Get sorted variable names to ensure consistent ordering
        sorted_vars = sorted(result.keys())

        for var_name in sorted_vars:
            # Extract integer from variable name
            # Handle both 'x1' and '1' formats
            if var_name.isdigit():
                var_int = int(var_name)
            else:
                # Extract digits from variable name (e.g., 'x1' -> 1)
                var_int = int(''.join(filter(str.isdigit, var_name)))

            # Apply polarity
            if result[var_name]:
                solution_literals.append(var_int)
            else:
                solution_literals.append(-var_int)

        # Format solution lines (max 80 chars per line, as per convention)
        solution_str = "v "
        for lit in solution_literals:
            lit_str = str(lit) + " "
            if len(solution_str) + len(lit_str) > 78:
                output_lines.append(solution_str)
                solution_str = "v " + lit_str
            else:
                solution_str += lit_str

        # Add final line with terminating 0
        solution_str += "0"
        output_lines.append(solution_str)

        if verbose:
            print(f"c SATISFIABLE", file=sys.stderr)

    # Statistics (to stderr if verbose)
    if verbose:
        stats = solver.stats
        print(f"c", file=sys.stderr)
        print(f"c Statistics:", file=sys.stderr)
        print(f"c   Time: {elapsed_time:.3f}s", file=sys.stderr)
        print(f"c   Decisions: {stats.decisions}", file=sys.stderr)
        print(f"c   Conflicts: {stats.conflicts}", file=sys.stderr)
        print(f"c   Restarts: {stats.restarts}", file=sys.stderr)
        print(f"c   Learned clauses: {stats.learned_clauses}", file=sys.stderr)
        print(f"c   Glue clauses (LBD≤2): {stats.glue_clauses}", file=sys.stderr)

        if solver.adaptive_enabled:
            print(f"c   Adaptive random phase: ENABLED (detected stuck state)", file=sys.stderr)
        else:
            print(f"c   Adaptive random phase: not enabled", file=sys.stderr)

    # Output solution
    if output_file:
        with open(output_file, 'w') as f:
            f.write('\n'.join(output_lines) + '\n')
        if verbose:
            print(f"c Solution written to {output_file}", file=sys.stderr)
    else:
        # Print to stdout
        for line in output_lines:
            print(line)


def main():
    """Main entry point for competition solver."""
    parser = argparse.ArgumentParser(
        description='Competition SAT Solver - CDCL with adaptive optimizations',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python competition_solver.py instance.cnf
  python competition_solver.py instance.cnf --verbose
  python competition_solver.py instance.cnf --max-conflicts 100000
  python competition_solver.py instance.cnf --output solution.txt

Output Format (Competition Standard):
  s SATISFIABLE        - Instance is satisfiable
  s UNSATISFIABLE      - Instance is unsatisfiable
  s UNKNOWN            - Timeout/resource limit reached
  v <lits> 0          - Solution literals (if SAT)
  c <message>         - Comments (stderr only with --verbose)
        """
    )

    parser.add_argument('cnf_file', help='DIMACS CNF file to solve')
    parser.add_argument('--max-conflicts', type=int, default=1000000,
                       help='Maximum conflicts before timeout (default: 1000000)')
    parser.add_argument('--output', '-o', help='Output file (default: stdout)')
    parser.add_argument('--verbose', '-v', action='store_true',
                       help='Print statistics and progress to stderr')

    args = parser.parse_args()

    # Solve and output
    solve_competition_format(
        args.cnf_file,
        max_conflicts=args.max_conflicts,
        verbose=args.verbose,
        output_file=args.output
    )


if __name__ == '__main__':
    main()
