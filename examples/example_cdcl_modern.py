"""
Examples demonstrating the modern CDCL solver.

`CDCLSolver` shows conflict-driven clause learning in its simplest form.
`ModernCDCLSolver` adds the techniques every competitive solver relies on:
two-watched literals, learned-clause minimization, phase saving, LBD-based
clause deletion and dynamic restarts. Most can be switched off, so these
examples measure what each one contributes.
"""

import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent / "tests"))

from bsat import (
    CNFExpression,
    ModernCDCLSolver, solve_modern_cdcl, get_modern_cdcl_stats,
    get_cdcl_stats,
)
from benchmarks import pigeon_hole, phase_transition_3sat


def example1_basic_usage():
    """Example 1: Basic usage."""
    print("=" * 60)
    print("Example 1: Basic Usage")
    print("=" * 60)

    formula = "(x | y | z) & (~x | y | ~z) & (x | ~y | z) & (~x | ~y)"
    cnf = CNFExpression.parse(formula)
    print(f"\nFormula: {cnf}")

    result = solve_modern_cdcl(cnf)
    print(f"Solution: {result}")
    print(f"Verifies: {cnf.evaluate(result)}")


def example2_statistics():
    """Example 2: Statistics."""
    print("\n" + "=" * 60)
    print("Example 2: Statistics")
    print("=" * 60)

    cnf = pigeon_hole(6)
    print(f"\nPigeonhole: 6 pigeons, 5 holes "
          f"({len(cnf.get_variables())} variables, {len(cnf.clauses)} clauses)")

    result, stats = get_modern_cdcl_stats(cnf)
    print(f"Result: {'SAT' if result else 'UNSAT'}")
    print(stats)


def example3_basic_vs_modern():
    """Example 3: The same algorithm, with and without the engineering."""
    print("\n" + "=" * 60)
    print("Example 3: Basic CDCL vs Modern CDCL")
    print("=" * 60)

    print(f"\n{'Instance':<28} {'Basic CDCL':>22} {'Modern CDCL':>22}")
    print("-" * 74)
    instances = [
        ("Pigeonhole 6 into 5", pigeon_hole(6)),
        ("Pigeonhole 7 into 6", pigeon_hole(7)),
        ("Random 3-SAT, 60 vars", phase_transition_3sat(60, seed=1)),
        ("Random 3-SAT, 80 vars", phase_transition_3sat(80, seed=2)),
    ]
    for name, cnf in instances:
        start = time.perf_counter()
        _, basic = get_cdcl_stats(cnf)
        basic_time = time.perf_counter() - start

        start = time.perf_counter()
        _, modern = get_modern_cdcl_stats(cnf)
        modern_time = time.perf_counter() - start

        print(f"{name:<28} "
              f"{basic.conflicts:>8} confl {basic_time:>6.2f}s "
              f"{modern.conflicts:>8} confl {modern_time:>6.2f}s")

    print("\nBoth learn clauses from conflicts. The difference in time comes mostly")
    print("from two-watched literals: the basic solver rescans every clause after")
    print("each assignment, the modern one only looks at clauses that might be unit.")


def example4_watched_literals():
    """Example 4: How much work two-watched literals avoid."""
    print("\n" + "=" * 60)
    print("Example 4: Two-Watched Literals")
    print("=" * 60)

    cnf = phase_transition_3sat(100, seed=3)
    solver = ModernCDCLSolver(cnf)
    solver.solve()
    stats = solver.stats

    total_clauses = len(solver.clauses) + stats.learned_clauses
    assignments = stats.decisions + stats.propagations
    naive_visits = assignments * total_clauses

    print(f"\nRandom 3-SAT with 100 variables: {stats.status}")
    print(f"  Assignments made:              {assignments}")
    print(f"  Clauses inspected:             {stats.clause_visits}")
    print(f"  Skipped via blocking literal:  {stats.blocker_skips}")
    print(f"  Per assignment:                {stats.clause_visits / assignments:.1f} clauses")
    print(f"\nScanning every clause after every assignment would have taken")
    print(f"up to {naive_visits} clause inspections "
          f"({naive_visits / max(stats.clause_visits, 1):.0f}x more).")


def example5_ablation():
    """Example 5: Switching techniques off one at a time."""
    print("\n" + "=" * 60)
    print("Example 5: What Each Technique Contributes")
    print("=" * 60)

    cnf = pigeon_hole(8)
    print(f"\nPigeonhole 8 into 7 "
          f"({len(cnf.get_variables())} variables, {len(cnf.clauses)} clauses)\n")

    configurations = [
        ("All techniques (default)", {}),
        ("No clause minimization", {"minimize_learned": False}),
        ("No phase saving", {"phase_saving": False}),
        ("No clause deletion", {"reduce_db": False}),
        ("Luby restarts", {"restart_strategy": "luby"}),
        ("No restarts", {"restart_strategy": "none"}),
    ]

    print(f"{'Configuration':<28} {'Conflicts':>9} {'Avg len':>8} {'Deleted':>8} "
          f"{'Restarts':>8} {'Time':>7}")
    print("-" * 74)
    for name, options in configurations:
        start = time.perf_counter()
        _, stats = get_modern_cdcl_stats(cnf, **options)
        elapsed = time.perf_counter() - start
        average = stats.learned_literals / max(stats.learned_clauses, 1)
        print(f"{name:<28} {stats.conflicts:>9} {average:>8.1f} {stats.deleted_clauses:>8} "
              f"{stats.restarts:>8} {elapsed:>6.2f}s")

    print("\n'Avg len' is the average learned clause length. Results on a single")
    print("instance are noisy: a technique that helps on average can hurt here.")


def example6_lbd():
    """Example 6: LBD of learned clauses."""
    print("\n" + "=" * 60)
    print("Example 6: Literal Block Distance (LBD)")
    print("=" * 60)

    cnf = phase_transition_3sat(80, seed=2)
    solver = ModernCDCLSolver(cnf, reduce_db=False)
    solver.solve()

    histogram = {}
    for clause in solver.learned:
        histogram[clause.lbd] = histogram.get(clause.lbd, 0) + 1

    print(f"\nLearned {len(solver.learned)} clauses. LBD = number of distinct decision")
    print("levels among a clause's literals at the moment it was learned.\n")
    largest = max(histogram.values())
    for lbd in sorted(histogram):
        bar = "#" * max(1, round(40 * histogram[lbd] / largest))
        note = "  <- 'glue' clauses, never deleted" if lbd == 2 else ""
        print(f"  LBD {lbd:>2}: {histogram[lbd]:>5} {bar}{note}")


def example7_restart_strategies():
    """Example 7: Restart strategies."""
    print("\n" + "=" * 60)
    print("Example 7: Restart Strategies")
    print("=" * 60)

    print(f"\n{'Instance':<24} {'glucose':>16} {'luby':>16} {'none':>16}")
    print("-" * 74)
    for name, cnf in [("Pigeonhole 8 into 7", pigeon_hole(8)),
                      ("Random 3-SAT, 120 vars", phase_transition_3sat(120, seed=1)),
                      ("Random 3-SAT, 120 vars", phase_transition_3sat(120, seed=3))]:
        cells = []
        for strategy in ("glucose", "luby", "none"):
            _, stats = get_modern_cdcl_stats(cnf, restart_strategy=strategy)
            cells.append(f"{stats.conflicts:>6} c {stats.restarts:>4} r")
        print(f"{name:<24} {cells[0]:>16} {cells[1]:>16} {cells[2]:>16}")

    print("\nc = conflicts, r = restarts. Glucose restarts when recently learned")
    print("clauses have a worse (higher) LBD than the average over the whole run.")
    print("On small random 3-SAT that rarely happens, so 'glucose' and 'none' can")
    print("make exactly the same run; the policy matters on structured instances.")


if __name__ == '__main__':
    example1_basic_usage()
    example2_statistics()
    example3_basic_vs_modern()
    example4_watched_literals()
    example5_ablation()
    example6_lbd()
    example7_restart_strategies()
