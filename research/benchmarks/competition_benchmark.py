#!/usr/bin/env python3
"""
SAT Competition-Style Benchmark Runner

Generates and runs benchmarks similar to SAT competitions:
- Random 3-SAT at various sizes
- Random 3-SAT at phase transition (r ≈ 4.26)
- Graph coloring problems
- Structured problems

Outputs results in a format comparable to competition results.
"""

import sys
import os
import time
import random
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from bsat import CNFExpression, Clause, Literal
from bsat.dpll import DPLLSolver
from bsat.cdcl import CDCLSolver
from benchmark import ProblemGenerator


@dataclass
class BenchmarkInstance:
    """A benchmark instance with metadata."""
    name: str
    num_vars: int
    num_clauses: int
    category: str
    expected_result: Optional[str]  # "SAT", "UNSAT", or None (unknown)
    cnf: CNFExpression
    competition_time: Optional[float] = None  # Published competition time (if known)
    competition_solver: Optional[str] = None  # Solver that achieved competition_time


class CompetitionBenchmarkGenerator:
    """Generate competition-style benchmarks."""

    @staticmethod
    def random_3sat(n: int, ratio: float = 4.26, seed: int = None) -> CNFExpression:
        """
        Generate random 3-SAT instance.

        Args:
            n: Number of variables
            ratio: Clause-to-variable ratio (4.26 is phase transition for 3-SAT)
            seed: Random seed
        """
        if seed is not None:
            random.seed(seed)

        m = int(n * ratio)
        variables = [f"v{i}" for i in range(1, n + 1)]
        clauses = []

        for _ in range(m):
            # Choose 3 random variables
            chosen = random.sample(variables, 3)

            # Random negations
            literals = []
            for var in chosen:
                if random.random() < 0.5:
                    literals.append(f"~{var}")
                else:
                    literals.append(var)

            clause_str = f"({' | '.join(literals)})"
            clauses.append(clause_str)

        formula = " & ".join(clauses)
        return CNFExpression.parse(formula)

    @staticmethod
    def graph_coloring(n: int, edges: List[Tuple[int, int]], k: int) -> CNFExpression:
        """
        Generate graph k-coloring SAT encoding.

        Args:
            n: Number of nodes
            edges: List of edges [(u, v), ...]
            k: Number of colors
        """
        clauses = []

        # At least one color per node
        for i in range(1, n + 1):
            clause = " | ".join([f"x_{i}_{c}" for c in range(1, k + 1)])
            clauses.append(f"({clause})")

        # At most one color per node
        for i in range(1, n + 1):
            for c1 in range(1, k + 1):
                for c2 in range(c1 + 1, k + 1):
                    clauses.append(f"(~x_{i}_{c1} | ~x_{i}_{c2})")

        # Adjacent nodes must have different colors
        for u, v in edges:
            for c in range(1, k + 1):
                clauses.append(f"(~x_{u}_{c} | ~x_{v}_{c})")

        formula = " & ".join(clauses)
        return CNFExpression.parse(formula)

    @staticmethod
    def pigeon_hole(n: int) -> CNFExpression:
        """
        Generate pigeon hole problem: n+1 pigeons, n holes (UNSAT).

        Args:
            n: Number of holes (will create n+1 pigeons)
        """
        clauses = []
        pigeons = n + 1
        holes = n

        # Each pigeon must be in some hole
        for p in range(1, pigeons + 1):
            clause = " | ".join([f"p{p}_h{h}" for h in range(1, holes + 1)])
            clauses.append(f"({clause})")

        # No two pigeons in the same hole
        for h in range(1, holes + 1):
            for p1 in range(1, pigeons + 1):
                for p2 in range(p1 + 1, pigeons + 1):
                    clauses.append(f"(~p{p1}_h{h} | ~p{p2}_h{h})")

        formula = " & ".join(clauses)
        return CNFExpression.parse(formula)


def generate_benchmark_suite() -> List[BenchmarkInstance]:
    """Generate a suite of competition-style benchmarks."""
    benchmarks = []

    # Category 1: Small random 3-SAT (satisfiable)
    for size in [15, 20, 25]:  # Smaller sizes
        cnf = CompetitionBenchmarkGenerator.random_3sat(size, ratio=4.0, seed=42)
        benchmarks.append(BenchmarkInstance(
            name=f"random-3sat-{size}v-SAT",
            num_vars=size,
            num_clauses=int(size * 4.0),
            category="Random 3-SAT (Under-constrained)",
            expected_result="SAT",
            cnf=cnf
        ))

    # Category 2: Random 3-SAT at phase transition
    for size in [15, 20, 25, 30]:  # Smaller sizes
        cnf = CompetitionBenchmarkGenerator.random_3sat(size, ratio=4.26, seed=42)
        benchmarks.append(BenchmarkInstance(
            name=f"random-3sat-{size}v-phase-transition",
            num_vars=size,
            num_clauses=int(size * 4.26),
            category="Random 3-SAT (Phase Transition)",
            expected_result=None,  # Unknown
            cnf=cnf
        ))

    # Category 3: Random 3-SAT (over-constrained, likely UNSAT)
    for size in [15, 20]:  # Smaller sizes
        cnf = CompetitionBenchmarkGenerator.random_3sat(size, ratio=6.0, seed=42)
        benchmarks.append(BenchmarkInstance(
            name=f"random-3sat-{size}v-overconstrained",
            num_vars=size,
            num_clauses=int(size * 6.0),
            category="Random 3-SAT (Over-constrained)",
            expected_result=None,  # Likely UNSAT but not guaranteed
            cnf=cnf
        ))

    # Category 4: Graph coloring (SAT and UNSAT)
    # Triangle graph, 3-coloring (SAT)
    cnf = CompetitionBenchmarkGenerator.graph_coloring(3, [(1, 2), (2, 3), (1, 3)], k=3)
    benchmarks.append(BenchmarkInstance(
        name="graph-3-coloring-triangle-SAT",
        num_vars=9,  # 3 nodes × 3 colors
        num_clauses=len(cnf.clauses),
        category="Graph Coloring",
        expected_result="SAT",
        cnf=cnf
    ))

    # Triangle graph, 2-coloring (UNSAT)
    cnf = CompetitionBenchmarkGenerator.graph_coloring(3, [(1, 2), (2, 3), (1, 3)], k=2)
    benchmarks.append(BenchmarkInstance(
        name="graph-3-coloring-triangle-UNSAT",
        num_vars=6,  # 3 nodes × 2 colors
        num_clauses=len(cnf.clauses),
        category="Graph Coloring",
        expected_result="UNSAT",
        cnf=cnf
    ))

    # Larger graph
    edges = [(i, j) for i in range(1, 6) for j in range(i + 1, 6) if (i + j) % 3 == 0]
    cnf = CompetitionBenchmarkGenerator.graph_coloring(5, edges, k=3)
    benchmarks.append(BenchmarkInstance(
        name="graph-5-node-3-coloring",
        num_vars=15,  # 5 nodes × 3 colors
        num_clauses=len(cnf.clauses),
        category="Graph Coloring",
        expected_result=None,
        cnf=cnf
    ))

    # Category 5: Pigeon hole (classic UNSAT)
    for n in [4, 5, 6]:  # Smaller sizes - pigeon hole grows very fast
        cnf = CompetitionBenchmarkGenerator.pigeon_hole(n)
        benchmarks.append(BenchmarkInstance(
            name=f"pigeonhole-{n+1}pigeons-{n}holes-UNSAT",
            num_vars=(n + 1) * n,  # (n+1 pigeons) × (n holes)
            num_clauses=len(cnf.clauses),
            category="Pigeon Hole",
            expected_result="UNSAT",
            cnf=cnf
        ))

    return benchmarks


def run_competition_benchmarks(timeout: float = 60.0):
    """Run all solvers on competition-style benchmarks."""
    print("=" * 100)
    print("SAT COMPETITION-STYLE BENCHMARK")
    print("=" * 100)
    print()
    print("Generating benchmark suite...")

    benchmarks = generate_benchmark_suite()

    print(f"Generated {len(benchmarks)} benchmark instances")
    print()

    # Solvers to test
    solvers = [
        ("CDCL", lambda cnf: CDCLSolver(cnf)),
        ("DPLL", lambda cnf: DPLLSolver(cnf)),
    ]

    # Add research solvers
    try:
        from cobd_sat import CoBDSATSolver
        solvers.append(("CoBD-SAT", lambda cnf: CoBDSATSolver(cnf)))
    except:
        pass

    try:
        from bb_cdcl import BBCDCLSolver
        solvers.append(("BB-CDCL", lambda cnf: BBCDCLSolver(cnf, num_samples=50)))
    except:
        pass

    try:
        from la_cdcl import LACDCLSolver
        solvers.append(("LA-CDCL", lambda cnf: LACDCLSolver(cnf, lookahead_depth=2, num_candidates=5)))
    except:
        pass

    try:
        from cgpm_sat import CGPMSolver
        solvers.append(("CGPM-SAT", lambda cnf: CGPMSolver(cnf, graph_weight=0.5)))
    except:
        pass

    # Add new research suite
    try:
        from tpm_sat import TPMSATSolver
        solvers.append(("TPM-SAT", lambda cnf: TPMSATSolver(cnf, window_size=5)))
    except:
        pass

    try:
        from ssta_sat import SSTASATSolver
        solvers.append(("SSTA-SAT", lambda cnf: SSTASATSolver(cnf, num_samples=50)))
    except:
        pass

    try:
        from vpl_sat import VPLSATSolver
        solvers.append(("VPL-SAT", lambda cnf: VPLSATSolver(cnf, use_phase_learning=True)))
    except:
        pass

    try:
        from cqp_sat import CQPSATSolver
        solvers.append(("CQP-SAT", lambda cnf: CQPSATSolver(cnf, use_quality_prediction=True)))
    except:
        pass

    try:
        from mab_sat import MABSATSolver
        solvers.append(("MAB-SAT", lambda cnf: MABSATSolver(cnf, use_mab=True, exploration_constant=1.4)))
    except:
        pass

    try:
        from ccg_sat import CCGSATSolver
        solvers.append(("CCG-SAT", lambda cnf: CCGSATSolver(cnf, use_causality=True)))
    except:
        pass

    try:
        from has_sat import HASSATSolver
        solvers.append(("HAS-SAT", lambda cnf: HASSATSolver(cnf, use_abstraction=True)))
    except:
        pass

    try:
        from cegp_sat import CEGPSATSolver
        solvers.append(("CEGP-SAT", lambda cnf: CEGPSATSolver(cnf, use_evolution=True, evolution_frequency=100)))
    except:
        pass

    # Results storage
    results = {}

    # Run benchmarks
    for benchmark in benchmarks:
        print(f"\n{'=' * 100}")
        print(f"Benchmark: {benchmark.name}")
        print(f"{'=' * 100}")
        print(f"  Category: {benchmark.category}")
        print(f"  Variables: {benchmark.num_vars}")
        print(f"  Clauses: {benchmark.num_clauses}")
        print(f"  Ratio: {benchmark.num_clauses / benchmark.num_vars:.2f}")
        if benchmark.expected_result:
            print(f"  Expected: {benchmark.expected_result}")
        print()

        benchmark_results = []

        for solver_name, solver_factory in solvers:
            try:
                print(f"  {solver_name:15s} ... ", end="", flush=True)

                start = time.perf_counter()
                solver = solver_factory(benchmark.cnf)
                result = solver.solve()
                elapsed = time.perf_counter() - start

                # Timeout check
                if elapsed > timeout:
                    print(f"TIMEOUT (>{timeout}s)")
                    benchmark_results.append({
                        'solver': solver_name,
                        'time': timeout,
                        'result': 'TIMEOUT'
                    })
                    continue

                result_str = "SAT" if result is not None else "UNSAT"

                # Check if result matches expected
                status = ""
                if benchmark.expected_result:
                    if result_str == benchmark.expected_result:
                        status = "✅ "
                    else:
                        status = "❌ WRONG! "

                print(f"{status}{result_str:5s} in {elapsed:8.4f}s")

                benchmark_results.append({
                    'solver': solver_name,
                    'time': elapsed,
                    'result': result_str
                })

            except KeyboardInterrupt:
                print(f"INTERRUPTED")
                benchmark_results.append({
                    'solver': solver_name,
                    'time': timeout,
                    'result': 'INTERRUPTED'
                })
                break
            except Exception as e:
                print(f"ERROR: {e}")
                benchmark_results.append({
                    'solver': solver_name,
                    'time': timeout,
                    'result': 'ERROR'
                })

        results[benchmark.name] = {
            'benchmark': benchmark,
            'results': benchmark_results
        }

    # Print summary
    print("\n" + "=" * 100)
    print("COMPETITION BENCHMARK SUMMARY")
    print("=" * 100)

    # Group by category
    categories = {}
    for name, data in results.items():
        cat = data['benchmark'].category
        if cat not in categories:
            categories[cat] = []
        categories[cat].append((name, data))

    for category, benchmarks in categories.items():
        print(f"\n{category}")
        print("-" * 100)

        for name, data in benchmarks:
            benchmark = data['benchmark']
            results_list = data['results']

            print(f"\n{name}")
            print(f"  {benchmark.num_vars} vars, {benchmark.num_clauses} clauses (ratio {benchmark.num_clauses / benchmark.num_vars:.2f})")

            # Find fastest
            fastest = min(results_list, key=lambda r: r['time'] if r['result'] != 'ERROR' else float('inf'))

            for r in sorted(results_list, key=lambda x: x['time']):
                marker = "🏆 " if r == fastest and r['result'] != 'ERROR' else "   "
                speedup = fastest['time'] / r['time'] if r['time'] > 0 and r['result'] != 'ERROR' else 1.0
                print(f"  {marker}{r['solver']:15s}: {r['result']:5s}  {r['time']:8.4f}s  ({speedup:.2f}×)")


def main():
    """Main entry point."""
    run_competition_benchmarks()
    return 0


if __name__ == "__main__":
    sys.exit(main())
