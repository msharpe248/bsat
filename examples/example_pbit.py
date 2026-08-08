"""
Examples demonstrating the p-bit (probabilistic bit) SAT solver.

A p-bit is a bit that fluctuates between 0 and 1 with probability
controlled by its input: P(m=1) = σ(2·I). Networks of p-bits perform
Gibbs sampling of a Boltzmann distribution P(m) ∝ exp(−β·E(m)). Taking
E = number of unsatisfied clauses makes the ground states exactly the
satisfying assignments, and annealing the inverse temperature β turns
the sampler into simulated annealing.

References:
- Camsari, Faria, Sutton, Datta. "Stochastic p-bits for invertible
  logic". Physical Review X 7, 031014 (2017).
- Aadit et al. "Massively parallel probabilistic computing with sparse
  Ising machines". Nature Electronics 5, 460-468 (2022).
"""

import random

from bsat import (
    CNFExpression,
    Clause,
    Literal,
    PBitSolver,
    solve_pbit,
    get_pbit_stats,
    solve_walksat,
    get_schoening_stats,
)


def sparkline(values, width=60):
    """Render an energy trajectory as a compact text trace."""
    if not values:
        return "(empty)"
    if len(values) > width:
        step = len(values) / width
        values = [values[int(i * step)] for i in range(width)]
    peak = max(max(values), 1)
    chars = " ▁▂▃▄▅▆▇█"
    return ''.join(chars[min(8, round(8 * v / peak))] for v in values)


def random_3sat(n_vars, n_clauses, rng):
    """Generate a random 3SAT instance."""
    variables = [f"v{i}" for i in range(n_vars)]
    clauses = []
    for _ in range(n_clauses):
        chosen = rng.sample(variables, 3)
        clauses.append(Clause([Literal(v, rng.random() < 0.5) for v in chosen]))
    return CNFExpression(clauses)


def example_basic():
    """Basic usage: solve a small formula."""
    print("=" * 60)
    print("Example 1: Basic Usage")
    print("=" * 60)

    cnf = CNFExpression.parse("(a | b | c) & (~a | b) & (a | ~c) & (~b | c)")
    print(f"Formula: {cnf}")

    solution = solve_pbit(cnf, seed=42)
    if solution:
        print(f"SAT: {solution}")
        print(f"Verified: {cnf.evaluate(solution)}")
    else:
        print("No solution found (but may exist)")
    print()


def example_statistics():
    """Inspect solver statistics."""
    print("=" * 60)
    print("Example 2: Statistics")
    print("=" * 60)

    cnf = CNFExpression.parse(
        "(a | b | c) & (~a | ~b | c) & (a | ~b | ~c) & (~a | b | ~c) & (a | b | ~c)"
    )
    print(f"Formula: {cnf}")

    solution, stats = get_pbit_stats(cnf, seed=42)
    print(f"Solution: {solution}")
    print(stats)
    print()


def example_beta_schedule():
    """The effect of the annealing schedule on the energy trajectory."""
    print("=" * 60)
    print("Example 3: Why Annealing Matters (β-schedule effect)")
    print("=" * 60)

    rng = random.Random(7)
    cnf = random_3sat(15, 63, rng)  # near the 3SAT phase transition
    print(f"Random 3SAT: {len(cnf.get_variables())} variables, {len(cnf.clauses)} clauses")
    print("Energy trace = #unsatisfied clauses after each sweep (lower is better)\n")

    runs = [
        ("Constant low β = 0.2  (hot: random sampler)", dict(beta_min=0.2, beta_max=0.2)),
        ("Constant high β = 10  (cold: greedy descent)", dict(beta_min=10.0, beta_max=10.0)),
        ("Annealed β: 0.2 → 6.0 (geometric schedule)", dict(beta_min=0.2, beta_max=6.0)),
    ]
    for label, params in runs:
        solution, stats = get_pbit_stats(cnf, sweeps=150, restarts=1, seed=42, **params)
        outcome = "SOLVED" if solution else f"stuck at energy {stats.final_energy}"
        print(f"{label}")
        print(f"  [{sparkline(stats.energy_history)}] {outcome}")
    print()
    print("Hot sampling never settles, greedy descent can get stuck in a")
    print("local minimum; annealing explores first, then settles.")
    print()


def example_phase_transition():
    """Random 3SAT near the m/n ≈ 4.26 phase transition."""
    print("=" * 60)
    print("Example 4: Random 3SAT Near the Phase Transition")
    print("=" * 60)

    rng = random.Random(3)
    n = 20
    m = int(n * 4.26)
    cnf = random_3sat(n, m, rng)
    print(f"Instance: n={n} variables, m={m} clauses (m/n ≈ 4.26)")

    solution, stats = get_pbit_stats(cnf, sweeps=2000, restarts=20, seed=42)
    if solution:
        print(f"SAT after {stats.restarts} restart(s), {stats.sweeps} sweeps")
        print(f"Verified: {cnf.evaluate(solution)}")
    else:
        print(f"No solution found; best energy reached: {stats.best_energy}")
        print("(instance may be UNSAT — the p-bit solver cannot prove it)")
    print()


def example_comparison():
    """Compare against the other incomplete solvers in bsat."""
    print("=" * 60)
    print("Example 5: P-Bit vs WalkSAT vs Schöning")
    print("=" * 60)

    rng = random.Random(11)
    cnf = random_3sat(15, 60, rng)
    print(f"Instance: 15 variables, 60 clauses\n")

    solution, stats = get_pbit_stats(cnf, seed=42)
    print(f"P-bit:    {'SAT' if solution else 'not found'} "
          f"({stats.updates} updates, {stats.flips} flips)")

    ws = solve_walksat(cnf, seed=42)
    print(f"WalkSAT:  {'SAT' if ws else 'not found'}")

    sol_s, stats_s = get_schoening_stats(cnf, seed=42)
    print(f"Schöning: {'SAT' if sol_s else 'not found'} "
          f"({stats_s.tries} tries, {stats_s.total_flips} flips)")
    print()
    print("All three are incomplete randomized solvers. P-bit does full")
    print("Gibbs sweeps over all variables; WalkSAT and Schöning walk on")
    print("unsatisfied clauses only.")
    print()


def example_unsat():
    """Behavior on unsatisfiable formulas."""
    print("=" * 60)
    print("Example 6: UNSAT Behavior")
    print("=" * 60)

    cnf = CNFExpression.parse("(x | y) & (x | ~y) & (~x | y) & (~x | ~y)")
    print(f"Formula: {cnf} (unsatisfiable)")

    solution, stats = get_pbit_stats(cnf, sweeps=100, restarts=5, seed=42)
    print(f"Result: {solution}")
    print(f"Best energy reached: {stats.best_energy} (never 0)")
    print()
    print("IMPORTANT: The p-bit solver is INCOMPLETE — None means 'no")
    print("solution found', not a proof of UNSAT. Use solve_cdcl() or")
    print("solve_sat() when UNSAT must be proven.")
    print()


if __name__ == '__main__':
    print("P-Bit (Probabilistic Bit) SAT Solver Examples")
    print("Gibbs sampling / simulated annealing inspired by p-bit hardware")
    print()
    example_basic()
    example_statistics()
    example_beta_schedule()
    example_phase_transition()
    example_comparison()
    example_unsat()
