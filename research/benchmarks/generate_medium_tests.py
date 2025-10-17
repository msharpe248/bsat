#!/usr/bin/env python3
"""
Generate medium-difficulty SAT instances for benchmarking.

Creates ~50 instances that are challenging but solvable,
ranging from easy (fast) to moderately hard (takes a few seconds).
"""

import os
import random
from pathlib import Path


def generate_random_3sat(num_vars, num_clauses, seed=None):
    """Generate a random 3-SAT instance."""
    if seed is not None:
        random.seed(seed)

    lines = []
    lines.append(f"c Random 3-SAT instance")
    lines.append(f"c Variables: {num_vars}, Clauses: {num_clauses}, Ratio: {num_clauses/num_vars:.2f}")
    lines.append(f"c Seed: {seed}")
    lines.append(f"p cnf {num_vars} {num_clauses}")

    for _ in range(num_clauses):
        vars_in_clause = random.sample(range(1, num_vars + 1), 3)
        literals = [v if random.random() > 0.5 else -v for v in vars_in_clause]
        lines.append(" ".join(map(str, literals)) + " 0")

    return "\n".join(lines)


def generate_random_ksat(num_vars, num_clauses, k, seed=None):
    """Generate a random k-SAT instance."""
    if seed is not None:
        random.seed(seed)

    lines = []
    lines.append(f"c Random {k}-SAT instance")
    lines.append(f"c Variables: {num_vars}, Clauses: {num_clauses}, K: {k}")
    lines.append(f"c Seed: {seed}")
    lines.append(f"p cnf {num_vars} {num_clauses}")

    for _ in range(num_clauses):
        clause_size = min(k, num_vars)
        vars_in_clause = random.sample(range(1, num_vars + 1), clause_size)
        literals = [v if random.random() > 0.5 else -v for v in vars_in_clause]
        lines.append(" ".join(map(str, literals)) + " 0")

    return "\n".join(lines)


def generate_satisfiable_instance(num_vars, clauses_per_var=2):
    """Generate a definitely satisfiable instance with a known solution."""
    # Create a satisfying assignment
    assignment = {i: random.choice([True, False]) for i in range(1, num_vars + 1)}

    lines = []
    lines.append(f"c Satisfiable instance (solution exists)")
    lines.append(f"c Variables: {num_vars}")
    lines.append(f"p cnf {num_vars} {num_vars * clauses_per_var}")

    # Generate clauses that are satisfied by the assignment
    for var in range(1, num_vars + 1):
        for _ in range(clauses_per_var):
            # Pick 2 other random variables
            other_vars = random.sample([v for v in range(1, num_vars + 1) if v != var], 2)

            # Make sure at least one literal is satisfied
            if assignment[var]:
                lit = var
            else:
                lit = -var

            clause = [lit] + [v if random.random() > 0.5 else -v for v in other_vars]
            random.shuffle(clause)
            lines.append(" ".join(map(str, clause)) + " 0")

    return "\n".join(lines)


def generate_medium_test_suite():
    """Generate a medium-difficulty test suite with ~50 instances."""

    dest_dir = Path("../../dataset/medium_tests")

    if dest_dir.exists():
        import shutil
        print(f"Removing existing {dest_dir}...")
        shutil.rmtree(dest_dir)

    dest_dir.mkdir(parents=True)

    print("Generating medium-difficulty test suite...")
    print("Target: ~50 instances, varying difficulty")
    print()

    instances = []
    seed_base = 1000

    # Easy warm-up (10-30 vars) - 10 instances
    print("Easy instances (10-30 vars) - fast to solve:")
    for i, num_vars in enumerate(range(10, 31, 2), 1):  # 10, 12, 14, ..., 30
        ratio = 4.2  # Below phase transition - easier
        num_clauses = int(num_vars * ratio)

        dimacs = generate_random_3sat(num_vars, num_clauses, seed=seed_base + i)
        filename = f"easy_3sat_v{num_vars:03d}_c{num_clauses:04d}.cnf"
        filepath = dest_dir / filename

        with open(filepath, 'w') as f:
            f.write(dimacs)

        print(f"  {filename}")
        instances.append(filename)

    print()

    # Medium instances (40-80 vars) - 20 instances
    print("Medium instances (40-80 vars) - moderate challenge:")
    for i, num_vars in enumerate(range(40, 81, 2), 1):  # 40, 42, 44, ..., 80
        ratio = 4.26  # Near phase transition - moderate
        num_clauses = int(num_vars * ratio)

        dimacs = generate_random_3sat(num_vars, num_clauses, seed=seed_base + 100 + i)
        filename = f"medium_3sat_v{num_vars:03d}_c{num_clauses:04d}.cnf"
        filepath = dest_dir / filename

        with open(filepath, 'w') as f:
            f.write(dimacs)

        print(f"  {filename}")
        instances.append(filename)

    print()

    # Harder instances (90-120 vars) - 10 instances
    print("Harder instances (90-120 vars) - takes a few seconds:")
    for i, num_vars in enumerate(range(90, 121, 3), 1):  # 90, 93, 96, ..., 120
        ratio = 4.27  # At phase transition - harder
        num_clauses = int(num_vars * ratio)

        dimacs = generate_random_3sat(num_vars, num_clauses, seed=seed_base + 200 + i)
        filename = f"hard_3sat_v{num_vars:03d}_c{num_clauses:04d}.cnf"
        filepath = dest_dir / filename

        with open(filepath, 'w') as f:
            f.write(dimacs)

        print(f"  {filename}")
        instances.append(filename)

    print()

    # Mixed k-SAT instances - 5 instances
    print("Mixed k-SAT instances (different clause sizes):")
    k_sat_configs = [
        (30, 4, 4.5),   # 4-SAT
        (40, 4, 4.8),
        (50, 5, 5.2),   # 5-SAT
        (60, 4, 4.5),
        (70, 5, 5.0),
    ]

    for i, (num_vars, k, ratio) in enumerate(k_sat_configs, 1):
        num_clauses = int(num_vars * ratio)

        dimacs = generate_random_ksat(num_vars, num_clauses, k, seed=seed_base + 300 + i)
        filename = f"mixed_{k}sat_v{num_vars:03d}_c{num_clauses:04d}.cnf"
        filepath = dest_dir / filename

        with open(filepath, 'w') as f:
            f.write(dimacs)

        print(f"  {filename}")
        instances.append(filename)

    print()

    # Satisfiable instances with known solutions - 5 instances
    print("Satisfiable instances (guaranteed SAT):")
    for i, num_vars in enumerate([40, 60, 80, 100, 120], 1):
        dimacs = generate_satisfiable_instance(num_vars, clauses_per_var=3)
        filename = f"sat_known_v{num_vars:03d}.cnf"
        filepath = dest_dir / filename

        with open(filepath, 'w') as f:
            f.write(dimacs)

        print(f"  {filename}")
        instances.append(filename)

    print()
    print(f"Generated {len(instances)} test instances")
    print()

    # Create family directory structure for benchmark runner
    suite_dir = dest_dir / "medium_suite"
    suite_dir.mkdir()

    print("Organizing into family directory...")
    for instance_file in dest_dir.glob("*.cnf"):
        instance_file.rename(suite_dir / instance_file.name)

    print(f"  Moved {len(instances)} files to {suite_dir}")
    print()
    print("=" * 80)
    print(f"Created medium test suite: {dest_dir}")
    print(f"  Total instances: {len(instances)}")
    print(f"  Difficulty range: Easy (10 vars) → Hard (120 vars)")
    print(f"  Expected solve time: <0.1s to ~10s per instance")
    print()
    print("Run with:")
    print(f"  cd research/benchmarks")
    print(f"  ./run_all_benchmarks.py ../../dataset/medium_tests -t 30 -s DPLL CDCL")
    print()
    print("Quick test on one file:")
    print(f"  python3 -c 'import sys; sys.path.insert(0, \"../../src\"); from bsat.dimacs import read_dimacs_file; from bsat.cdcl import CDCLSolver; import time; cnf = read_dimacs_file(\"{suite_dir}/hard_3sat_v120_c512.cnf\"); start = time.time(); result = CDCLSolver(cnf).solve(); print(f\"Result: {{\\\"SAT\\\" if result else \\\"UNSAT\\\"}} in {{time.time()-start:.2f}}s\")'")


if __name__ == "__main__":
    generate_medium_test_suite()
