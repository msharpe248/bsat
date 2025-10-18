#!/usr/bin/env python3
"""
SAT Solver Benchmark Tool

Compares research solvers with production solvers on various problem types.

Research Solvers:
- Original Suite: CoBD-SAT, BB-CDCL, LA-CDCL, CGPM-SAT
- New Suite: TPM-SAT, SSTA-SAT, VPL-SAT, CQP-SAT, MAB-SAT, CCG-SAT, HAS-SAT, CEGP-SAT

Production Solvers: CDCL, DPLL, Schöning

Usage:
    # Single problem from command line
    python benchmark.py --formula "(a | b) & (~a | c)"

    # Problems from file
    python benchmark.py --file problems.txt

    # Generate random problems
    python benchmark.py --random --num-vars 20 --num-clauses 80 --count 10

    # All benchmarks (comprehensive)
    python benchmark.py --all
"""

import sys
import os
import argparse
import time
import random
from typing import Dict, List, Optional, Tuple
from dataclasses import dataclass
from tabulate import tabulate

# Add paths
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from bsat import CNFExpression
from bsat.dpll import DPLLSolver
from bsat.cdcl import CDCLSolver
from bsat.schoening import SchoeningSolver

# Original research suite
from cobd_sat import CoBDSATSolver
from bb_cdcl import BBCDCLSolver
from la_cdcl import LACDCLSolver
from cgpm_sat import CGPMSolver

# New research suite
from tpm_sat import TPMSATSolver
from ssta_sat import SSTASATSolver
from vpl_sat import VPLSATSolver
from cqp_sat import CQPSATSolver
from mab_sat import MABSATSolver
from ccg_sat import CCGSATSolver
from has_sat import HASSATSolver
from cegp_sat import CEGPSATSolver


@dataclass
class BenchmarkResult:
    """Result from running a single solver on a problem."""
    solver_name: str
    satisfiable: Optional[bool]
    time_seconds: float
    decisions: int
    conflicts: int
    error: Optional[str] = None

    # Solver-specific metrics
    extra_metrics: Dict = None

    def __post_init__(self):
        if self.extra_metrics is None:
            self.extra_metrics = {}


class ProblemGenerator:
    """Generate random SAT problems of various types."""

    @staticmethod
    def random_3sat(num_vars: int, num_clauses: int, seed: int = None) -> CNFExpression:
        """Generate random 3-SAT problem."""
        if seed is not None:
            random.seed(seed)

        variables = [f"v{i}" for i in range(num_vars)]
        clauses = []

        for _ in range(num_clauses):
            # Choose 3 random variables
            chosen = random.sample(variables, min(3, num_vars))

            # Random negations
            literals = []
            for var in chosen:
                neg = random.choice([True, False])
                literals.append(f"~{var}" if neg else var)

            clause = f"({' | '.join(literals)})"
            clauses.append(clause)

        formula = " & ".join(clauses)
        return CNFExpression.parse(formula)

    @staticmethod
    def modular_problem(num_modules: int, module_size: int, interface_vars: int, seed: int = None) -> CNFExpression:
        """Generate problem with modular structure (good for CoBD-SAT)."""
        if seed is not None:
            random.seed(seed)

        clauses = []
        all_vars = []

        # Create interface variables
        interface = [f"i{i}" for i in range(interface_vars)]
        all_vars.extend(interface)

        # Create modules
        for m in range(num_modules):
            # Module-local variables
            local_vars = [f"m{m}_v{i}" for i in range(module_size)]
            all_vars.extend(local_vars)

            # Internal module clauses (tight coupling)
            for i in range(module_size * 2):
                chosen = random.sample(local_vars, min(3, len(local_vars)))
                literals = [random.choice([v, f"~{v}"]) for v in chosen]
                clauses.append(f"({' | '.join(literals)})")

            # Interface connections (sparse coupling)
            for i in range(interface_vars):
                ivar = interface[i]
                lvar = random.choice(local_vars)
                clauses.append(f"({ivar} | {lvar})")

        formula = " & ".join(clauses)
        return CNFExpression.parse(formula)

    @staticmethod
    def backbone_problem(num_vars: int, backbone_fraction: float = 0.5, seed: int = None) -> CNFExpression:
        """Generate problem with known backbone (good for BB-CDCL)."""
        if seed is not None:
            random.seed(seed)

        num_backbone = int(num_vars * backbone_fraction)

        # Backbone variables (forced values)
        backbone_vars = [f"b{i}" for i in range(num_backbone)]
        backbone_values = {var: random.choice([True, False]) for var in backbone_vars}

        # Free variables
        free_vars = [f"f{i}" for i in range(num_vars - num_backbone)]

        clauses = []

        # Force backbone variables
        for var, value in backbone_values.items():
            if value:
                clauses.append(f"({var})")
            else:
                clauses.append(f"(~{var})")

            # Add implications to reinforce backbone
            for _ in range(2):
                other = random.choice(backbone_vars + free_vars)
                if other != var:
                    clauses.append(f"(~{var} | {other})")

        # Free variable clauses
        for _ in range(len(free_vars) * 2):
            chosen = random.sample(free_vars, min(3, len(free_vars)))
            literals = [random.choice([v, f"~{v}"]) for v in chosen]
            clauses.append(f"({' | '.join(literals)})")

        formula = " & ".join(clauses)
        return CNFExpression.parse(formula)

    @staticmethod
    def chain_problem(length: int) -> CNFExpression:
        """Generate chain problem with long propagation (good for LA-CDCL)."""
        clauses = []

        # Chain: v0 -> v1 -> v2 -> ... -> vN
        clauses.append(f"(v0)")  # Start

        for i in range(length - 1):
            # vi implies v(i+1)
            clauses.append(f"(~v{i} | v{i+1})")

        # Some choices at the end
        for i in range(3):
            clauses.append(f"(v{length-1} | e{i})")

        formula = " & ".join(clauses)
        return CNFExpression.parse(formula)

    @staticmethod
    def circuit_problem(num_gates: int, seed: int = None) -> CNFExpression:
        """Generate circuit-like problem (good for CGPM-SAT)."""
        if seed is not None:
            random.seed(seed)

        clauses = []

        # Input variables
        inputs = [f"i{i}" for i in range(num_gates * 2)]

        # Gates (AND/OR)
        for g in range(num_gates):
            gate_var = f"g{g}"
            in1 = random.choice(inputs + [f"g{i}" for i in range(g)])
            in2 = random.choice(inputs + [f"g{i}" for i in range(g)])

            gate_type = random.choice(['and', 'or'])

            if gate_type == 'and':
                # g = in1 AND in2
                clauses.append(f"(~{in1} | ~{in2} | {gate_var})")
                clauses.append(f"({in1} | ~{gate_var})")
                clauses.append(f"({in2} | ~{gate_var})")
            else:
                # g = in1 OR in2
                clauses.append(f"({in1} | {in2} | ~{gate_var})")
                clauses.append(f"(~{in1} | {gate_var})")
                clauses.append(f"(~{in2} | {gate_var})")

        # Fix some inputs
        for i in range(min(3, len(inputs))):
            clauses.append(f"({inputs[i]})")

        formula = " & ".join(clauses)
        return CNFExpression.parse(formula)


class SolverBenchmark:
    """Benchmark runner for SAT solvers."""

    def __init__(self, timeout: float = 10.0):
        """
        Initialize benchmark runner.

        Args:
            timeout: Maximum time per solver in seconds
        """
        self.timeout = timeout
        self.results: List[Tuple[str, List[BenchmarkResult]]] = []

    def run_solver(self, solver_name: str, cnf: CNFExpression) -> BenchmarkResult:
        """
        Run a single solver on a problem.

        Args:
            solver_name: Name of solver
            cnf: CNF formula

        Returns:
            BenchmarkResult
        """
        try:
            start_time = time.perf_counter()  # High precision timer

            if solver_name == "DPLL":
                solver = DPLLSolver(cnf)
                result = solver.solve()
                elapsed = time.perf_counter() - start_time

                return BenchmarkResult(
                    solver_name=solver_name,
                    satisfiable=result is not None,
                    time_seconds=elapsed,
                    decisions=0,  # DPLL doesn't track
                    conflicts=0
                )

            elif solver_name == "CDCL":
                solver = CDCLSolver(cnf)
                result = solver.solve()
                elapsed = time.perf_counter() - start_time

                return BenchmarkResult(
                    solver_name=solver_name,
                    satisfiable=result is not None,
                    time_seconds=elapsed,
                    decisions=0,  # Would need to track
                    conflicts=0
                )

            elif solver_name == "Schöning":
                solver = SchoeningSolver(cnf)
                result = solver.solve()
                elapsed = time.perf_counter() - start_time

                return BenchmarkResult(
                    solver_name=solver_name,
                    satisfiable=result is not None,
                    time_seconds=elapsed,
                    decisions=0,
                    conflicts=0
                )

            elif solver_name == "CoBD-SAT":
                solver = CoBDSATSolver(cnf)
                result = solver.solve()
                elapsed = time.perf_counter() - start_time

                stats = solver.get_statistics()
                return BenchmarkResult(
                    solver_name=solver_name,
                    satisfiable=result is not None,
                    time_seconds=elapsed,
                    decisions=0,
                    conflicts=0,
                    extra_metrics={
                        'modularity': stats.get('modularity_score', 0),
                        'communities': stats.get('num_communities', 0),
                        'used_decomp': stats.get('used_decomposition', False)
                    }
                )

            elif solver_name == "BB-CDCL":
                solver = BBCDCLSolver(cnf, num_samples=50)
                result = solver.solve()
                elapsed = time.perf_counter() - start_time

                stats = solver.get_statistics()
                return BenchmarkResult(
                    solver_name=solver_name,
                    satisfiable=result is not None,
                    time_seconds=elapsed,
                    decisions=0,
                    conflicts=stats.get('backbone_conflicts', 0),
                    extra_metrics={
                        'backbone_pct': stats.get('backbone_percentage', 0),
                        'used_backbone': stats.get('used_backbone', False)
                    }
                )

            elif solver_name == "LA-CDCL":
                solver = LACDCLSolver(cnf, lookahead_depth=2, num_candidates=5)
                result = solver.solve()
                elapsed = time.perf_counter() - start_time

                stats = solver.get_statistics()
                return BenchmarkResult(
                    solver_name=solver_name,
                    satisfiable=result is not None,
                    time_seconds=elapsed,
                    decisions=stats.get('decisions_made', 0),
                    conflicts=stats.get('conflicts_total', 0),
                    extra_metrics={
                        'lookahead_pct': stats.get('lookahead_percentage', 0),
                        'overhead_pct': stats.get('lookahead_overhead_percentage', 0)
                    }
                )

            elif solver_name == "CGPM-SAT":
                solver = CGPMSolver(cnf, graph_weight=0.5)
                result = solver.solve()
                elapsed = time.perf_counter() - start_time

                stats = solver.get_statistics()
                graph_stats = stats.get('graph_statistics', {})
                return BenchmarkResult(
                    solver_name=solver_name,
                    satisfiable=result is not None,
                    time_seconds=elapsed,
                    decisions=stats.get('decisions_made', 0),
                    conflicts=0,
                    extra_metrics={
                        'graph_influence': stats.get('graph_influence_rate', 0),
                        'graph_density': graph_stats.get('graph_density', 0)
                    }
                )

            elif solver_name == "TPM-SAT":
                solver = TPMSATSolver(cnf, window_size=5)
                result = solver.solve()
                elapsed = time.perf_counter() - start_time

                stats = solver.get_pattern_statistics()
                return BenchmarkResult(
                    solver_name=solver_name,
                    satisfiable=result is not None,
                    time_seconds=elapsed,
                    decisions=0,
                    conflicts=0,
                    extra_metrics={
                        'patterns': stats.get('patterns_found', 0),
                        'anti_patterns': stats.get('anti_patterns_found', 0)
                    }
                )

            elif solver_name == "SSTA-SAT":
                solver = SSTASATSolver(cnf, num_samples=50)
                result = solver.solve()
                elapsed = time.perf_counter() - start_time

                stats = solver.get_topology_statistics()
                return BenchmarkResult(
                    solver_name=solver_name,
                    satisfiable=result is not None,
                    time_seconds=elapsed,
                    decisions=0,
                    conflicts=0,
                    extra_metrics={
                        'solutions_sampled': stats.get('solutions_sampled', 0),
                        'clusters': stats.get('clusters_found', 0)
                    }
                )

            elif solver_name == "VPL-SAT":
                solver = VPLSATSolver(cnf, use_phase_learning=True, strategy='hybrid')
                result = solver.solve()
                elapsed = time.perf_counter() - start_time

                stats = solver.get_phase_statistics()
                return BenchmarkResult(
                    solver_name=solver_name,
                    satisfiable=result is not None,
                    time_seconds=elapsed,
                    decisions=0,
                    conflicts=0,
                    extra_metrics={
                        'learned_phases': stats.get('learned_phases_used', 0)
                    }
                )

            elif solver_name == "CQP-SAT":
                solver = CQPSATSolver(cnf, use_quality_prediction=True)
                result = solver.solve()
                elapsed = time.perf_counter() - start_time

                stats = solver.get_quality_statistics()
                return BenchmarkResult(
                    solver_name=solver_name,
                    satisfiable=result is not None,
                    time_seconds=elapsed,
                    decisions=0,
                    conflicts=0,
                    extra_metrics={
                        'glue_clauses': stats.get('glue_clauses_kept', 0)
                    }
                )

            elif solver_name == "MAB-SAT":
                solver = MABSATSolver(cnf, use_mab=True, exploration_constant=1.4)
                result = solver.solve()
                elapsed = time.perf_counter() - start_time

                stats = solver.get_mab_statistics()
                return BenchmarkResult(
                    solver_name=solver_name,
                    satisfiable=result is not None,
                    time_seconds=elapsed,
                    decisions=0,
                    conflicts=0,
                    extra_metrics={
                        'ucb1_decisions': stats.get('ucb1_decisions', 0)
                    }
                )

            elif solver_name == "CCG-SAT":
                solver = CCGSATSolver(cnf, use_causality=True, old_age_threshold=5000)
                result = solver.solve()
                elapsed = time.perf_counter() - start_time

                stats = solver.get_causality_statistics()
                return BenchmarkResult(
                    solver_name=solver_name,
                    satisfiable=result is not None,
                    time_seconds=elapsed,
                    decisions=0,
                    conflicts=0,
                    extra_metrics={
                        'causality_restarts': stats.get('causality_restarts', 0),
                        'root_causes': stats.get('root_causes_detected', 0)
                    }
                )

            elif solver_name == "HAS-SAT":
                solver = HASSATSolver(cnf, use_abstraction=True, num_levels=2)
                result = solver.solve()
                elapsed = time.perf_counter() - start_time

                stats = solver.get_abstraction_statistics()
                return BenchmarkResult(
                    solver_name=solver_name,
                    satisfiable=result is not None,
                    time_seconds=elapsed,
                    decisions=0,
                    conflicts=0,
                    extra_metrics={
                        'levels_solved': stats.get('levels_solved', 0),
                        'refinements': stats.get('refinements', 0)
                    }
                )

            elif solver_name == "CEGP-SAT":
                solver = CEGPSATSolver(cnf, use_evolution=True, evolution_frequency=500)
                result = solver.solve()
                elapsed = time.perf_counter() - start_time

                stats = solver.get_evolution_statistics()
                return BenchmarkResult(
                    solver_name=solver_name,
                    satisfiable=result is not None,
                    time_seconds=elapsed,
                    decisions=0,
                    conflicts=0,
                    extra_metrics={
                        'evolutions': stats.get('evolutions', 0),
                        'evolved_clauses': stats.get('evolved_clauses', 0)
                    }
                )

            else:
                return BenchmarkResult(
                    solver_name=solver_name,
                    satisfiable=None,
                    time_seconds=0,
                    decisions=0,
                    conflicts=0,
                    error=f"Unknown solver: {solver_name}"
                )

        except Exception as e:
            elapsed = time.perf_counter() - start_time
            return BenchmarkResult(
                solver_name=solver_name,
                satisfiable=None,
                time_seconds=elapsed,
                decisions=0,
                conflicts=0,
                error=str(e)
            )

    def benchmark_problem(self, problem_name: str, cnf: CNFExpression,
                         solvers: List[str]) -> List[BenchmarkResult]:
        """
        Run all solvers on a problem.

        Args:
            problem_name: Name/description of problem
            cnf: CNF formula
            solvers: List of solver names to run

        Returns:
            List of BenchmarkResults
        """
        print(f"\nBenchmarking: {problem_name}")
        print(f"  Variables: {len(set(lit.variable for clause in cnf.clauses for lit in clause.literals))}")
        print(f"  Clauses: {len(cnf.clauses)}")

        results = []

        for solver_name in solvers:
            print(f"  Running {solver_name}...", end=" ", flush=True)
            result = self.run_solver(solver_name, cnf)
            results.append(result)

            if result.error:
                print(f"ERROR: {result.error}")
            else:
                sat_str = "SAT" if result.satisfiable else "UNSAT"
                print(f"{sat_str} in {result.time_seconds:.4f}s")

        self.results.append((problem_name, results))
        return results

    def print_summary_table(self):
        """Print summary table of all results."""
        print("\n" + "=" * 100)
        print("BENCHMARK SUMMARY")
        print("=" * 100)

        for problem_name, results in self.results:
            print(f"\n{problem_name}")
            print("-" * 100)

            # Build table
            headers = ["Solver", "Result", "Time (s)", "Speedup", "Decisions", "Conflicts", "Notes"]
            rows = []

            # Find baseline (CDCL) time
            baseline_time = next((r.time_seconds for r in results if r.solver_name == "CDCL"), None)

            for result in results:
                if result.error:
                    rows.append([
                        result.solver_name,
                        "ERROR",
                        f"{result.time_seconds:.4f}",
                        "-",
                        "-",
                        "-",
                        result.error[:30]
                    ])
                else:
                    sat_str = "SAT" if result.satisfiable else "UNSAT"

                    # Calculate speedup vs baseline
                    speedup = "-"
                    if baseline_time and baseline_time > 0 and result.time_seconds > 0:
                        speedup = f"{baseline_time / result.time_seconds:.2f}×"

                    # Extra metrics
                    notes = []
                    if result.extra_metrics:
                        if 'modularity' in result.extra_metrics:
                            notes.append(f"Q={result.extra_metrics['modularity']:.2f}")
                        if 'backbone_pct' in result.extra_metrics:
                            notes.append(f"BB={result.extra_metrics['backbone_pct']:.0f}%")
                        if 'lookahead_pct' in result.extra_metrics:
                            notes.append(f"LA={result.extra_metrics['lookahead_pct']:.0f}%")
                        if 'graph_influence' in result.extra_metrics:
                            notes.append(f"GI={result.extra_metrics['graph_influence']:.0f}%")

                    rows.append([
                        result.solver_name,
                        sat_str,
                        f"{result.time_seconds:.4f}",
                        speedup,
                        result.decisions if result.decisions > 0 else "-",
                        result.conflicts if result.conflicts > 0 else "-",
                        ", ".join(notes) if notes else "-"
                    ])

            print(tabulate(rows, headers=headers, tablefmt="grid"))

    def export_markdown_report(self, filename: str):
        """
        Export results as markdown report.

        Args:
            filename: Output markdown file
        """
        with open(filename, 'w') as f:
            f.write("# SAT Solver Benchmark Results\n\n")
            f.write("Comparison of research solvers vs production solvers on various problem types.\n\n")

            # Summary statistics
            f.write("## Summary\n\n")
            f.write("**Solvers Tested:**\n")
            f.write("- **Production**: DPLL, CDCL, Schöning (3 solvers)\n")
            f.write("- **Original Research**: CoBD-SAT, BB-CDCL, LA-CDCL, CGPM-SAT (4 solvers)\n")
            f.write("- **New Research**: TPM-SAT, SSTA-SAT, VPL-SAT, CQP-SAT, MAB-SAT, CCG-SAT, HAS-SAT, CEGP-SAT (8 solvers)\n")
            f.write("- **Total Solvers**: 15\n\n")

            f.write(f"**Total Problems**: {len(self.results)}\n\n")

            # Per-problem results
            f.write("## Detailed Results\n\n")

            for problem_name, results in self.results:
                f.write(f"### {problem_name}\n\n")

                # Find the winner (fastest solver)
                valid_results = [r for r in results if not r.error and r.time_seconds > 0]
                winner = min(valid_results, key=lambda r: r.time_seconds) if valid_results else None

                # Sort results by time (fastest first)
                sorted_results = sorted(results, key=lambda r: r.time_seconds if not r.error else float('inf'))

                # Build markdown table
                headers = ["Solver", "Result", "Time (s)", "Speedup", "Decisions", "Conflicts", "Notes"]
                rows = []

                baseline_time = next((r.time_seconds for r in results if r.solver_name == "CDCL"), None)

                for result in sorted_results:
                    if result.error:
                        solver_name = result.solver_name
                        rows.append([
                            solver_name,
                            "ERROR",
                            f"{result.time_seconds:.4f}",
                            "-",
                            "-",
                            "-",
                            result.error[:30]
                        ])
                    else:
                        # Mark winner clearly
                        solver_name = result.solver_name
                        if winner and result.solver_name == winner.solver_name:
                            solver_name = f"👑 **{result.solver_name}**"

                        sat_str = "SAT" if result.satisfiable else "UNSAT"
                        speedup = "-"
                        if baseline_time and baseline_time > 0 and result.time_seconds > 0:
                            speedup = f"{baseline_time / result.time_seconds:.2f}×"

                        notes = []
                        if result.extra_metrics:
                            if 'modularity' in result.extra_metrics:
                                notes.append(f"Q={result.extra_metrics['modularity']:.2f}")
                            if 'backbone_pct' in result.extra_metrics:
                                notes.append(f"BB={result.extra_metrics['backbone_pct']:.0f}%")
                            if 'lookahead_pct' in result.extra_metrics:
                                notes.append(f"LA={result.extra_metrics['lookahead_pct']:.0f}%")
                            if 'graph_influence' in result.extra_metrics:
                                notes.append(f"GI={result.extra_metrics['graph_influence']:.0f}%")

                        # Add WINNER note for fastest solver
                        if winner and result.solver_name == winner.solver_name:
                            notes.insert(0, "**WINNER**")

                        rows.append([
                            solver_name,
                            sat_str,
                            f"{result.time_seconds:.4f}",
                            speedup,
                            result.decisions if result.decisions > 0 else "-",
                            result.conflicts if result.conflicts > 0 else "-",
                            ", ".join(notes) if notes else "-"
                        ])

                f.write(tabulate(rows, headers=headers, tablefmt="pipe"))
                f.write("\n\n")

            # Analysis
            f.write("## Analysis\n\n")
            self._write_analysis(f)

    def _write_analysis(self, f):
        """Write analysis section to markdown file."""
        f.write("### Key Findings\n\n")

        # Analyze which solver won on which problems
        solver_wins = {}
        for problem_name, results in self.results:
            # Find fastest solver
            valid_results = [r for r in results if not r.error and r.time_seconds > 0]
            if valid_results:
                fastest = min(valid_results, key=lambda r: r.time_seconds)
                solver_wins[fastest.solver_name] = solver_wins.get(fastest.solver_name, 0) + 1

        f.write("**Fastest Solver by Problem Type:**\n\n")
        for solver, count in sorted(solver_wins.items(), key=lambda x: x[1], reverse=True):
            f.write(f"- **{solver}**: Won on {count} problem(s)\n")
        f.write("\n")


def main():
    """Main benchmark runner."""
    parser = argparse.ArgumentParser(description="SAT Solver Benchmark Tool")

    # Input options
    parser.add_argument('--formula', type=str, help='Single formula to test')
    parser.add_argument('--file', type=str, help='File with formulas (one per line)')
    parser.add_argument('--random', action='store_true', help='Generate random problems')
    parser.add_argument('--all', action='store_true', help='Run comprehensive benchmark suite')

    # Random generation options
    parser.add_argument('--num-vars', type=int, default=20, help='Number of variables for random problems')
    parser.add_argument('--num-clauses', type=int, default=80, help='Number of clauses for random problems')
    parser.add_argument('--count', type=int, default=5, help='Number of random problems to generate')

    # Solver selection options
    parser.add_argument('--all-solvers', action='store_true', help='Test all 15 solvers (production + all research)')
    parser.add_argument('--new-suite', action='store_true', help='Test only new research suite (8 solvers)')
    parser.add_argument('--original-suite', action='store_true', help='Test only original research suite (4 solvers)')

    # Output options
    parser.add_argument('--output', type=str, default='benchmark_results.md', help='Output markdown file')
    parser.add_argument('--timeout', type=float, default=10.0, help='Timeout per solver (seconds)')

    args = parser.parse_args()

    # Solvers to test
    # Production solvers
    production_solvers = ["DPLL", "CDCL", "Schöning"]

    # Original research suite
    original_research = ["CoBD-SAT", "BB-CDCL", "LA-CDCL", "CGPM-SAT"]

    # New research suite
    new_research = ["TPM-SAT", "SSTA-SAT", "VPL-SAT", "CQP-SAT", "MAB-SAT", "CCG-SAT", "HAS-SAT", "CEGP-SAT"]

    # All solvers (for comprehensive benchmarks)
    all_solvers_list = production_solvers + original_research + new_research

    # Select solvers based on command-line arguments
    if args.all_solvers:
        solvers = all_solvers_list
        print(f"Running ALL 15 solvers (3 production + 4 original + 8 new)")
    elif args.new_suite:
        solvers = production_solvers + new_research
        print(f"Running NEW research suite (3 production + 8 new)")
    elif args.original_suite:
        solvers = production_solvers + original_research
        print(f"Running ORIGINAL research suite (3 production + 4 original)")
    else:
        # Default: production + original research (for backward compatibility)
        solvers = production_solvers + original_research
        print(f"Running default solvers (3 production + 4 original). Use --all-solvers for all 15.")

    print(f"Solvers: {', '.join(solvers)}\n")

    # Create benchmark runner
    benchmark = SolverBenchmark(timeout=args.timeout)

    if args.formula:
        # Single formula from command line
        cnf = CNFExpression.parse(args.formula)
        benchmark.benchmark_problem("Command Line Formula", cnf, solvers)

    elif args.file:
        # Formulas from file
        with open(args.file, 'r') as f:
            for i, line in enumerate(f):
                line = line.strip()
                if line and not line.startswith('#'):
                    cnf = CNFExpression.parse(line)
                    benchmark.benchmark_problem(f"Formula {i+1} from {args.file}", cnf, solvers)

    elif args.random:
        # Generate random problems
        print(f"Generating {args.count} random 3-SAT problems...")
        for i in range(args.count):
            cnf = ProblemGenerator.random_3sat(args.num_vars, args.num_clauses, seed=i)
            benchmark.benchmark_problem(f"Random 3-SAT #{i+1}", cnf, solvers)

    elif args.all:
        # Comprehensive benchmark suite
        print("Running comprehensive benchmark suite...")

        # 1. Modular problems (good for CoBD-SAT)
        for i, (modules, size, interface) in enumerate([(3, 5, 2), (4, 6, 3), (5, 4, 2)]):
            cnf = ProblemGenerator.modular_problem(modules, size, interface, seed=i)
            benchmark.benchmark_problem(f"Modular Problem (M={modules}, S={size}, I={interface})", cnf, solvers)

        # 2. Backbone problems (good for BB-CDCL)
        for i, (vars, bb_frac) in enumerate([(20, 0.3), (25, 0.5), (30, 0.7)]):
            cnf = ProblemGenerator.backbone_problem(vars, bb_frac, seed=i)
            benchmark.benchmark_problem(f"Backbone Problem (V={vars}, BB={bb_frac*100:.0f}%)", cnf, solvers)

        # 3. Chain problems (good for LA-CDCL)
        for length in [10, 15, 20]:
            cnf = ProblemGenerator.chain_problem(length)
            benchmark.benchmark_problem(f"Chain Problem (Length={length})", cnf, solvers)

        # 4. Circuit problems (good for CGPM-SAT)
        for i, gates in enumerate([5, 7, 10]):
            cnf = ProblemGenerator.circuit_problem(gates, seed=i)
            benchmark.benchmark_problem(f"Circuit Problem (Gates={gates})", cnf, solvers)

        # 5. Random 3-SAT (baseline comparison)
        for i, (vars, clauses) in enumerate([(15, 60), (20, 80), (25, 100)]):
            cnf = ProblemGenerator.random_3sat(vars, clauses, seed=i)
            benchmark.benchmark_problem(f"Random 3-SAT (V={vars}, C={clauses})", cnf, solvers)

    else:
        parser.print_help()
        return 1

    # Print summary
    benchmark.print_summary_table()

    # Export markdown report
    benchmark.export_markdown_report(args.output)
    print(f"\n✅ Markdown report saved to: {args.output}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
