#!/usr/bin/env python3
"""
Generate simple, solvable SAT instances for testing solvers.

Creates instances that are challenging but actually solvable by
basic DPLL/CDCL implementations.
"""

import os
import random
from pathlib import Path


def generate_random_3sat(num_vars, num_clauses, seed=None):
    """Generate a random 3-SAT instance.

    Args:
        num_vars: Number of variables
        num_clauses: Number of clauses
        seed: Random seed for reproducibility

    Returns:
        DIMACS string
    """
    if seed is not None:
        random.seed(seed)

    lines = []
    lines.append(f"c Random 3-SAT instance")
    lines.append(f"c Variables: {num_vars}, Clauses: {num_clauses}")
    lines.append(f"c Seed: {seed}")
    lines.append(f"p cnf {num_vars} {num_clauses}")

    for _ in range(num_clauses):
        # Pick 3 random variables
        vars_in_clause = random.sample(range(1, num_vars + 1), 3)

        # Randomly negate each literal
        literals = [v if random.random() > 0.5 else -v for v in vars_in_clause]

        lines.append(" ".join(map(str, literals)) + " 0")

    return "\n".join(lines)


def generate_satisfiable_instance(num_vars):
    """Generate a definitely satisfiable instance.

    Creates an instance with a known solution.
    """
    # Create a satisfying assignment
    assignment = {i: random.choice([True, False]) for i in range(1, num_vars + 1)}

    lines = []
    lines.append(f"c Satisfiable instance (solution exists)")
    lines.append(f"p cnf {num_vars} {num_vars * 2}")

    # Generate clauses that are satisfied by the assignment
    for var in range(1, num_vars + 1):
        # Create clauses using this variable
        for _ in range(2):
            # Pick 2 other random variables
            other_vars = random.sample([v for v in range(1, num_vars + 1) if v != var], 2)

            # Make sure at least one literal is satisfied
            if assignment[var]:
                lit = var  # Positive literal will be satisfied
            else:
                lit = -var  # Negative literal will be satisfied

            # Add other literals (may or may not be satisfied)
            clause = [lit] + [v if random.random() > 0.5 else -v for v in other_vars]
            random.shuffle(clause)

            lines.append(" ".join(map(str, clause)) + " 0")

    return "\n".join(lines)


def generate_test_suite():
    """Generate a suite of test instances."""

    dest_dir = Path("../../dataset/simple_tests")

    if dest_dir.exists():
        import shutil
        print(f"Removing existing {dest_dir}...")
        shutil.rmtree(dest_dir)

    dest_dir.mkdir(parents=True)

    print("Generating simple test instances...")
    print()

    instances = []

    # Very easy instances (5-10 variables)
    print("Easy instances (5-10 vars):")
    for i, num_vars in enumerate([5, 7, 10], 1):
        ratio = 4.3  # Classic ratio for 3-SAT
        num_clauses = int(num_vars * ratio)

        # Random 3-SAT
        dimacs = generate_random_3sat(num_vars, num_clauses, seed=100 + i)
        filename = f"random3sat_v{num_vars}_c{num_clauses}.cnf"
        filepath = dest_dir / filename

        with open(filepath, 'w') as f:
            f.write(dimacs)

        print(f"  {filename}")
        instances.append(filename)

    print()

    # Medium instances (15-30 variables)
    print("Medium instances (15-30 vars):")
    for i, num_vars in enumerate([15, 20, 30], 1):
        ratio = 4.3
        num_clauses = int(num_vars * ratio)

        # Random 3-SAT
        dimacs = generate_random_3sat(num_vars, num_clauses, seed=200 + i)
        filename = f"random3sat_v{num_vars}_c{num_clauses}.cnf"
        filepath = dest_dir / filename

        with open(filepath, 'w') as f:
            f.write(dimacs)

        print(f"  {filename}")
        instances.append(filename)

    print()

    # Definitely satisfiable instances
    print("Satisfiable instances (known solution):")
    for i, num_vars in enumerate([10, 20, 30], 1):
        dimacs = generate_satisfiable_instance(num_vars)
        filename = f"sat_v{num_vars}.cnf"
        filepath = dest_dir / filename

        with open(filepath, 'w') as f:
            f.write(dimacs)

        print(f"  {filename}")
        instances.append(filename)

    print()
    print(f"Generated {len(instances)} test instances in {dest_dir}")
    print()
    print("Run with:")
    print(f"  cd {dest_dir}")
    print(f"  # Test single file:")
    print(f"  python3 -c 'import sys; sys.path.insert(0, \"../../src\"); from bsat.dimacs import read_dimacs_file; from bsat.cdcl import CDCLSolver; cnf = read_dimacs_file(\"{instances[0]}\"); print(CDCLSolver(cnf).solve())'")
    print()
    print(f"  # Or benchmark all:")
    print(f"  # (Note: run_all_benchmarks expects family directories, so create one)")
    print(f"  mkdir -p simple_suite && mv *.cnf simple_suite/")
    print(f"  cd ../../research/benchmarks")
    print(f"  ./run_all_benchmarks.py ../../dataset/simple_tests -t 10")


if __name__ == "__main__":
    generate_test_suite()
