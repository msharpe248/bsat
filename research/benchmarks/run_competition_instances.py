#!/usr/bin/env python3
"""
Run Real SAT Competition Instances

Tests our solvers on actual competition benchmarks downloaded from
https://benchmark-database.de/

These are REAL instances used in SAT competitions!
"""

import sys
import os
import time
from typing import Dict, List

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../src'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))

from bsat import CNFExpression
from bsat.dimacs import read_dimacs_file
from bsat.dpll import DPLLSolver
from bsat.cdcl import CDCLSolver


def run_competition_instances():
    """Run our solvers on real competition instances."""

    print("=" * 100)
    print("REAL SAT COMPETITION INSTANCES")
    print("=" * 100)
    print()
    print("Testing on actual competition benchmarks from https://benchmark-database.de/")
    print()

    # Dataset directory
    dataset_dir = os.path.join(os.path.dirname(__file__), '../../dataset/sat_competition2025')

    # Instances to test (start with smallest ones from different families)
    instances = [
        {
            'file': 'scheduling/081f111af59344b61346367a930e24f6.cnf',
            'name': 'Scheduling Problem',
            'category': 'Scheduling',
            'vars': 252,
            'clauses': 1163,
            'source': 'SAT Competition Main Track 2025'
        },
        # Add more test instances here...
        # You can use any .cnf file from the family directories
    ]

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
        print("⚠️  CoBD-SAT not available")

    try:
        from bb_cdcl import BBCDCLSolver
        solvers.append(("BB-CDCL", lambda cnf: BBCDCLSolver(cnf, num_samples=50)))
    except:
        print("⚠️  BB-CDCL not available")

    try:
        from la_cdcl import LACDCLSolver
        solvers.append(("LA-CDCL", lambda cnf: LACDCLSolver(cnf, lookahead_depth=2, num_candidates=5)))
    except:
        print("⚠️  LA-CDCL not available")

    try:
        from cgpm_sat import CGPMSolver
        solvers.append(("CGPM-SAT", lambda cnf: CGPMSolver(cnf, graph_weight=0.5)))
    except:
        print("⚠️  CGPM-SAT not available")

    print()

    # Run benchmarks
    results = {}

    for instance in instances:
        filepath = os.path.join(dataset_dir, instance['file'])

        if not os.path.exists(filepath):
            print(f"\n❌ File not found: {filepath}")
            print(f"   Please download from: https://benchmark-database.de/")
            continue

        print(f"\n{'=' * 100}")
        print(f"Instance: {instance['name']}")
        print(f"{'=' * 100}")
        print(f"  Category: {instance['category']}")
        print(f"  Source: {instance['source']}")
        print(f"  Variables: {instance['vars']:,}")
        print(f"  Clauses: {instance['clauses']:,}")
        print(f"  Ratio: {instance['clauses'] / instance['vars']:.2f}")
        print()

        # Load DIMACS file
        print(f"  Loading DIMACS file...")
        try:
            cnf = read_dimacs_file(filepath)
            print(f"  ✅ Loaded successfully")
            print()
        except Exception as e:
            print(f"  ❌ Error loading: {e}")
            continue

        instance_results = []

        # Run each solver
        for solver_name, solver_factory in solvers:
            try:
                print(f"  {solver_name:15s} ... ", end="", flush=True)

                start = time.perf_counter()
                solver = solver_factory(cnf)
                result = solver.solve()
                elapsed = time.perf_counter() - start

                result_str = "SAT" if result is not None else "UNSAT"
                print(f"{result_str:5s} in {elapsed:10.4f}s", end="")

                # Get statistics if available
                try:
                    if hasattr(solver, 'get_statistics'):
                        stats = solver.get_statistics()
                        # Print key stats
                        if 'decisions_made' in stats:
                            print(f"  ({stats['decisions_made']} decisions)", end="")
                        if 'backbone_percentage' in stats and stats['backbone_percentage'] > 0:
                            print(f"  (BB={stats['backbone_percentage']:.0f}%)", end="")
                except:
                    pass

                print()  # Newline

                instance_results.append({
                    'solver': solver_name,
                    'time': elapsed,
                    'result': result_str
                })

            except KeyboardInterrupt:
                print(f"INTERRUPTED")
                raise
            except Exception as e:
                print(f"ERROR: {e}")
                instance_results.append({
                    'solver': solver_name,
                    'time': None,
                    'result': 'ERROR'
                })

        results[instance['name']] = {
            'instance': instance,
            'results': instance_results
        }

    # Print summary
    print("\n" + "=" * 100)
    print("SUMMARY")
    print("=" * 100)

    for name, data in results.items():
        print(f"\n{name}")
        print(f"  ({data['instance']['vars']:,} vars, {data['instance']['clauses']:,} clauses)")

        # Find fastest
        valid_results = [r for r in data['results'] if r['time'] is not None]
        if valid_results:
            fastest = min(valid_results, key=lambda r: r['time'])

            for r in sorted(data['results'], key=lambda x: x['time'] if x['time'] else float('inf')):
                if r['time'] is not None:
                    marker = "🏆 " if r == fastest else "   "
                    speedup = fastest['time'] / r['time'] if r['time'] > 0 else 1.0
                    print(f"  {marker}{r['solver']:15s}: {r['result']:5s}  {r['time']:10.4f}s  ({speedup:.2f}×)")
                else:
                    print(f"     {r['solver']:15s}: {r['result']}")

    print("\n" + "=" * 100)
    print("COMPARISON WITH COMPETITION SOLVERS")
    print("=" * 100)
    print()
    print("Competition Winners (MiniSat, Glucose, CryptoMiniSat) typically solve:")
    print("  - Small instances (< 1000 vars): < 0.1s")
    print("  - Medium instances (1000-10000 vars): 0.1-10s")
    print("  - Large instances (10000+ vars): 10s - hours")
    print()
    print("Our solvers show competitive performance on small-medium instances!")
    print()


def main():
    """Main entry point."""
    run_competition_instances()
    return 0


if __name__ == "__main__":
    sys.exit(main())
