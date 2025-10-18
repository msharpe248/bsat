#!/usr/bin/env python3
"""
Run All SAT Competition Benchmarks

Systematically tests all solvers on all competition benchmarks,
with configurable timeouts and comprehensive result tracking.

Supported Solvers:
- Production: DPLL, CDCL
- Original Research: CoBD-SAT, BB-CDCL, LA-CDCL, CGPM-SAT
- New Research: TPM-SAT, SSTA-SAT, VPL-SAT, CQP-SAT, MAB-SAT, CCG-SAT, HAS-SAT, CEGP-SAT

Usage:
    ./run_all_benchmarks.py                    # Run all benchmarks with 120s timeout
    ./run_all_benchmarks.py -t 60              # Use 60s timeout
    ./run_all_benchmarks.py -s DPLL CDCL       # Only run specific solvers
    ./run_all_benchmarks.py -s TPM-SAT MAB-SAT # Run new research solvers
"""

import sys
import os
import time
import argparse
import multiprocessing
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Optional

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from bsat import CNFExpression
from bsat.dimacs import read_dimacs_file
from bsat.dpll import DPLLSolver
from bsat.cdcl import CDCLSolver

# Import research solvers
try:
    from cobd_sat import CoBDSATSolver
    HAS_COBD = True
except:
    HAS_COBD = False

try:
    from bb_cdcl import BBCDCLSolver
    HAS_BB = True
except:
    HAS_BB = False

try:
    from la_cdcl import LACDCLSolver
    HAS_LA = True
except:
    HAS_LA = False

try:
    from cgpm_sat import CGPMSolver
    HAS_CGPM = True
except:
    HAS_CGPM = False

# Import new research suite
try:
    from tpm_sat import TPMSATSolver
    HAS_TPM = True
except:
    HAS_TPM = False

try:
    from ssta_sat import SSTASATSolver
    HAS_SSTA = True
except:
    HAS_SSTA = False

try:
    from vpl_sat import VPLSATSolver
    HAS_VPL = True
except:
    HAS_VPL = False

try:
    from cqp_sat import CQPSATSolver
    HAS_CQP = True
except:
    HAS_CQP = False

try:
    from mab_sat import MABSATSolver
    HAS_MAB = True
except:
    HAS_MAB = False

try:
    from ccg_sat import CCGSATSolver
    HAS_CCG = True
except:
    HAS_CCG = False

try:
    from has_sat import HASSATSolver
    HAS_HAS = True
except:
    HAS_HAS = False

try:
    from cegp_sat import CEGPSATSolver
    HAS_CEGP = True
except:
    HAS_CEGP = False


def _run_solver_worker(solver_name, cnf_file_path, result_queue):
    """Worker function to run solver in separate process."""
    try:
        # Load CNF from file in worker process
        from bsat.dimacs import read_dimacs_file
        cnf = read_dimacs_file(cnf_file_path)

        # Import solvers inside worker to avoid pickling issues
        from bsat.dpll import DPLLSolver
        from bsat.cdcl import CDCLSolver

        # Import research solvers if available
        try:
            from cobd_sat import CoBDSATSolver
        except:
            CoBDSATSolver = None

        try:
            from bb_cdcl import BBCDCLSolver
        except:
            BBCDCLSolver = None

        try:
            from la_cdcl import LACDCLSolver
        except:
            LACDCLSolver = None

        try:
            from cgpm_sat import CGPMSolver
        except:
            CGPMSolver = None

        # Import new research suite if available
        try:
            from tpm_sat import TPMSATSolver
        except:
            TPMSATSolver = None

        try:
            from ssta_sat import SSTASATSolver
        except:
            SSTASATSolver = None

        try:
            from vpl_sat import VPLSATSolver
        except:
            VPLSATSolver = None

        try:
            from cqp_sat import CQPSATSolver
        except:
            CQPSATSolver = None

        try:
            from mab_sat import MABSATSolver
        except:
            MABSATSolver = None

        try:
            from ccg_sat import CCGSATSolver
        except:
            CCGSATSolver = None

        try:
            from has_sat import HASSATSolver
        except:
            HASSATSolver = None

        try:
            from cegp_sat import CEGPSATSolver
        except:
            CEGPSATSolver = None

        # Create solver based on name
        if solver_name == "DPLL":
            solver = DPLLSolver(cnf)
        elif solver_name == "CDCL":
            solver = CDCLSolver(cnf)
        elif solver_name == "CoBD-SAT" and CoBDSATSolver:
            solver = CoBDSATSolver(cnf)
        elif solver_name == "BB-CDCL" and BBCDCLSolver:
            solver = BBCDCLSolver(cnf, num_samples=50)
        elif solver_name == "LA-CDCL" and LACDCLSolver:
            solver = LACDCLSolver(cnf, lookahead_depth=2, num_candidates=5)
        elif solver_name == "CGPM-SAT" and CGPMSolver:
            solver = CGPMSolver(cnf, graph_weight=0.5)
        elif solver_name == "TPM-SAT" and TPMSATSolver:
            solver = TPMSATSolver(cnf, use_patterns=True, max_pattern_length=3)
        elif solver_name == "SSTA-SAT" and SSTASATSolver:
            solver = SSTASATSolver(cnf, use_topology=True, hop_limit=3)
        elif solver_name == "VPL-SAT" and VPLSATSolver:
            solver = VPLSATSolver(cnf, use_phase_learning=True, decay_factor=0.95)
        elif solver_name == "CQP-SAT" and CQPSATSolver:
            solver = CQPSATSolver(cnf, use_lbd=True, glue_limit=30)
        elif solver_name == "MAB-SAT" and MABSATSolver:
            solver = MABSATSolver(cnf, use_ucb=True, exploration_constant=0.5)
        elif solver_name == "CCG-SAT" and CCGSATSolver:
            solver = CCGSATSolver(cnf, use_causality=True, graph_depth=5)
        elif solver_name == "HAS-SAT" and HASSATSolver:
            solver = HASSATSolver(cnf, use_abstraction=True, abstraction_threshold=100)
        elif solver_name == "CEGP-SAT" and CEGPSATSolver:
            solver = CEGPSATSolver(cnf, use_evolution=True, evolution_frequency=100)
        else:
            result_queue.put(("ERROR", f"Unknown solver: {solver_name}", 0.0))
            return

        start = time.perf_counter()
        result = solver.solve()
        elapsed = time.perf_counter() - start

        result_str = "SAT" if result is not None else "UNSAT"
        result_queue.put(("SUCCESS", result_str, elapsed))
    except Exception as e:
        result_queue.put(("ERROR", str(e), 0.0))


def parse_dimacs_header(filepath):
    """Quickly parse just the DIMACS header to get variable and clause counts."""
    with open(filepath, 'r') as f:
        for line in f:
            line = line.strip()
            if line.startswith('p cnf'):
                parts = line.split()
                if len(parts) >= 4:
                    num_vars = int(parts[2])
                    num_clauses = int(parts[3])
                    return num_vars, num_clauses
    return None, None


def get_all_solvers():
    """Get all available solvers."""
    solvers = [
        ("DPLL", lambda cnf: DPLLSolver(cnf)),
        ("CDCL", lambda cnf: CDCLSolver(cnf)),
    ]

    if HAS_COBD:
        solvers.append(("CoBD-SAT", lambda cnf: CoBDSATSolver(cnf)))
    if HAS_BB:
        solvers.append(("BB-CDCL", lambda cnf: BBCDCLSolver(cnf, num_samples=50)))
    if HAS_LA:
        solvers.append(("LA-CDCL", lambda cnf: LACDCLSolver(cnf, lookahead_depth=2, num_candidates=5)))
    if HAS_CGPM:
        solvers.append(("CGPM-SAT", lambda cnf: CGPMSolver(cnf, graph_weight=0.5)))

    # New research suite
    if HAS_TPM:
        solvers.append(("TPM-SAT", lambda cnf: TPMSATSolver(cnf, use_patterns=True, max_pattern_length=3)))
    if HAS_SSTA:
        solvers.append(("SSTA-SAT", lambda cnf: SSTASATSolver(cnf, use_topology=True, hop_limit=3)))
    if HAS_VPL:
        solvers.append(("VPL-SAT", lambda cnf: VPLSATSolver(cnf, use_phase_learning=True, decay_factor=0.95)))
    if HAS_CQP:
        solvers.append(("CQP-SAT", lambda cnf: CQPSATSolver(cnf, use_lbd=True, glue_limit=30)))
    if HAS_MAB:
        solvers.append(("MAB-SAT", lambda cnf: MABSATSolver(cnf, use_ucb=True, exploration_constant=0.5)))
    if HAS_CCG:
        solvers.append(("CCG-SAT", lambda cnf: CCGSATSolver(cnf, use_causality=True, graph_depth=5)))
    if HAS_HAS:
        solvers.append(("HAS-SAT", lambda cnf: HASSATSolver(cnf, use_abstraction=True, abstraction_threshold=100)))
    if HAS_CEGP:
        solvers.append(("CEGP-SAT", lambda cnf: CEGPSATSolver(cnf, use_evolution=True, evolution_frequency=100)))

    return solvers


def run_solver_with_timeout(solver_name, solver_factory, cnf_file_path, timeout_seconds):
    """Run a solver with timeout using multiprocessing."""
    result_queue = multiprocessing.Queue()
    process = multiprocessing.Process(
        target=_run_solver_worker,
        args=(solver_name, cnf_file_path, result_queue)
    )

    process.start()
    process.join(timeout=timeout_seconds)

    if process.is_alive():
        # Timeout occurred - terminate the process
        process.terminate()
        process.join()
        return "TIMEOUT", timeout_seconds, "TIMEOUT"

    # Get result from queue
    if not result_queue.empty():
        status, result, elapsed = result_queue.get()
        if status == "SUCCESS":
            return result, elapsed, None
        else:
            return "ERROR", 0.0, result
    else:
        return "ERROR", 0.0, "No result returned"


def run_all_benchmarks(dataset_dir, timeout_seconds, solver_names=None):
    """Run all benchmarks."""
    # Create results directory (in dataset/results/ to keep benchmark dirs clean)
    timestamp = datetime.now().strftime("%Y-%m-%d-%H-%M-%S")
    dataset_parent = Path(dataset_dir).parent
    results_base = dataset_parent / "results"
    results_base.mkdir(exist_ok=True)
    results_dir = results_base / f"results-{timestamp}"
    results_dir.mkdir(exist_ok=True)

    print("=" * 100)
    print(f"SAT COMPETITION BENCHMARK RUNNER")
    print("=" * 100)
    print(f"Dataset: {dataset_dir}")
    print(f"Results: {results_dir}")
    print(f"Timeout: {timeout_seconds}s")
    print()

    # Get solvers
    all_solvers = get_all_solvers()
    if solver_names:
        all_solvers = [(name, factory) for name, factory in all_solvers if name in solver_names]

    print(f"Solvers: {', '.join([name for name, _ in all_solvers])}")
    print()

    # Find all family directories
    dataset_path = Path(dataset_dir)
    family_dirs = [d for d in dataset_path.iterdir() if d.is_dir() and not d.name.startswith('.') and not d.name.startswith('results-')]
    family_dirs.sort()

    print(f"Found {len(family_dirs)} families")
    print()

    # Master summary data
    master_results = []

    # Process each family
    for family_idx, family_dir in enumerate(family_dirs, 1):
        family_name = family_dir.name

        print(f"[{family_idx}/{len(family_dirs)}] Processing family: {family_name}")
        print("=" * 100)

        # Find all CNF files in this family
        cnf_files = list(family_dir.glob("*.cnf"))
        cnf_files.sort()

        if not cnf_files:
            print(f"  No CNF files found, skipping...")
            print()
            continue

        print(f"  Found {len(cnf_files)} CNF files")
        print()

        # Create family results file
        family_results_file = results_dir / f"{family_name}.results.txt"
        family_summary_data = []

        with open(family_results_file, 'w') as f:
            f.write(f"Family: {family_name}\n")
            f.write(f"Timeout: {timeout_seconds}s\n")
            f.write(f"Files: {len(cnf_files)}\n")
            f.write("=" * 100 + "\n\n")

            # Process each CNF file
            for file_idx, cnf_file in enumerate(cnf_files, 1):
                print(f"  [{file_idx}/{len(cnf_files)}] {cnf_file.name}")

                # Parse DIMACS header quickly (without loading full file)
                try:
                    num_vars, num_clauses = parse_dimacs_header(str(cnf_file))
                    if num_vars is None or num_clauses is None:
                        raise Exception("Could not parse DIMACS header")
                except Exception as e:
                    print(f"    ERROR parsing header: {e}")
                    f.write(f"File: {cnf_file.name}\n")
                    f.write(f"  ERROR: {e}\n\n")
                    continue

                print(f"    Variables: {num_vars}, Clauses: {num_clauses}")

                # Write file header
                f.write(f"File: {cnf_file.name}\n")
                f.write(f"  Variables: {num_vars}\n")
                f.write(f"  Clauses: {num_clauses}\n")
                f.write(f"  Ratio: {num_clauses / num_vars:.2f}\n")
                f.write(f"\n")

                # Run each solver
                file_results = []
                for solver_name, solver_factory in all_solvers:
                    print(f"    {solver_name:15s} ... ", end="", flush=True)

                    result, elapsed, error = run_solver_with_timeout(
                        solver_name, solver_factory, str(cnf_file), timeout_seconds
                    )

                    if error == "TIMEOUT":
                        print(f"TIMEOUT (>{timeout_seconds}s)")
                    elif error:
                        print(f"ERROR: {error}")
                    else:
                        print(f"{result:5s} in {elapsed:10.4f}s")

                    file_results.append({
                        'solver': solver_name,
                        'result': result,
                        'time': elapsed,
                        'error': error
                    })

                # Rank solvers (only non-timeout/non-error ones)
                valid_results = [r for r in file_results if r['error'] is None]
                valid_results.sort(key=lambda x: x['time'])

                # Assign ranks
                ranks = {}
                for rank, r in enumerate(valid_results, 1):
                    ranks[r['solver']] = rank

                # Write results to file
                for r in file_results:
                    rank_str = f"RANK={ranks.get(r['solver'], 'N/A')}" if r['error'] is None else r['error']
                    time_str = f"{r['time']:.4f}s" if r['error'] is None else "N/A"
                    f.write(f"  {r['solver']:15s} {r['result']:7s} {time_str:12s} {rank_str}\n")

                    # Add to summary data
                    family_summary_data.append({
                        'family': family_name,
                        'file': cnf_file.name,
                        'variables': num_vars,
                        'clauses': num_clauses,
                        'solver': r['solver'],
                        'result': r['result'],
                        'time': r['time'],
                        'rank': ranks.get(r['solver'], -1) if r['error'] is None else -1
                    })

                    master_results.append({
                        'family': family_name,
                        'file': cnf_file.name,
                        'variables': num_vars,
                        'clauses': num_clauses,
                        'solver': r['solver'],
                        'result': r['result'],
                        'time': r['time'],
                        'rank': ranks.get(r['solver'], -1) if r['error'] is None else -1
                    })

                f.write("\n")
                print()

        # Generate family summary
        generate_family_summary(family_name, family_summary_data, results_dir)
        print()

    # Generate master summary
    generate_master_summary(master_results, results_dir, all_solvers)

    print("=" * 100)
    print("COMPLETE")
    print("=" * 100)
    print(f"Results saved to: {results_dir}")
    print()


def generate_family_summary(family_name, data, results_dir):
    """Generate per-family summary."""
    summary_file = results_dir / f"{family_name}.summary.txt"

    with open(summary_file, 'w') as f:
        f.write(f"Summary: {family_name}\n")
        f.write("=" * 100 + "\n\n")

        # Count rankings
        solver_ranks = {}
        for row in data:
            solver = row['solver']
            rank = row['rank']

            if solver not in solver_ranks:
                solver_ranks[solver] = {'1st': 0, '2nd': 0, '3rd': 0, 'timeout': 0, 'error': 0}

            if rank == 1:
                solver_ranks[solver]['1st'] += 1
            elif rank == 2:
                solver_ranks[solver]['2nd'] += 1
            elif rank == 3:
                solver_ranks[solver]['3rd'] += 1
            elif rank == -1:
                if row['result'] == 'TIMEOUT':
                    solver_ranks[solver]['timeout'] += 1
                else:
                    solver_ranks[solver]['error'] += 1

        # Write rankings
        f.write("Solver Rankings:\n")
        for solver in sorted(solver_ranks.keys()):
            ranks = solver_ranks[solver]
            f.write(f"  {solver:15s}: 1st={ranks['1st']:3d}  2nd={ranks['2nd']:3d}  3rd={ranks['3rd']:3d}  "
                   f"Timeout={ranks['timeout']:3d}  Error={ranks['error']:3d}\n")

        f.write("\n")

    print(f"  Family summary: {summary_file.name}")


def generate_master_summary(data, results_dir, solvers):
    """Generate master summary CSV."""
    summary_file = results_dir / "master_summary.csv"

    with open(summary_file, 'w') as f:
        # Header
        f.write("family,file,variables,clauses,solver,result,time,rank\n")

        # Data
        for row in data:
            f.write(f"{row['family']},{row['file']},{row['variables']},{row['clauses']},"
                   f"{row['solver']},{row['result']},{row['time']:.4f},{row['rank']}\n")

    print(f"Master summary: {summary_file.name}")

    # Also generate overall rankings
    rankings_file = results_dir / "overall_rankings.txt"

    solver_ranks = {}
    for row in data:
        solver = row['solver']
        rank = row['rank']

        if solver not in solver_ranks:
            solver_ranks[solver] = {'1st': 0, '2nd': 0, '3rd': 0, 'timeout': 0, 'error': 0, 'total': 0}

        solver_ranks[solver]['total'] += 1

        if rank == 1:
            solver_ranks[solver]['1st'] += 1
        elif rank == 2:
            solver_ranks[solver]['2nd'] += 1
        elif rank == 3:
            solver_ranks[solver]['3rd'] += 1
        elif rank == -1:
            if row['result'] == 'TIMEOUT':
                solver_ranks[solver]['timeout'] += 1
            else:
                solver_ranks[solver]['error'] += 1

    with open(rankings_file, 'w') as f:
        f.write("Overall Solver Rankings\n")
        f.write("=" * 100 + "\n\n")

        for solver in sorted(solver_ranks.keys()):
            ranks = solver_ranks[solver]
            f.write(f"{solver:15s}: 1st={ranks['1st']:4d}  2nd={ranks['2nd']:4d}  3rd={ranks['3rd']:4d}  "
                   f"Timeout={ranks['timeout']:4d}  Error={ranks['error']:4d}  Total={ranks['total']:4d}\n")

    print(f"Overall rankings: {rankings_file.name}")


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(description="Run all SAT competition benchmarks")
    parser.add_argument('dataset_dir', nargs='?',
                       default=os.path.join(os.path.dirname(__file__), '../../dataset/sat_competition2025'),
                       help='Dataset directory (default: ../../dataset/sat_competition2025)')
    parser.add_argument('-t', '--timeout', type=int, default=120,
                       help='Timeout per solver in seconds (default: 120)')
    parser.add_argument('-s', '--solvers', nargs='+',
                       help='Specific solvers to run (default: all)')

    args = parser.parse_args()

    if not os.path.exists(args.dataset_dir):
        print(f"Error: Dataset directory not found: {args.dataset_dir}")
        return 1

    run_all_benchmarks(args.dataset_dir, args.timeout, args.solvers)
    return 0


if __name__ == "__main__":
    sys.exit(main())
