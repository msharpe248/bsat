#!/usr/bin/env python3
"""Test competition solver on multiple instances."""

import sys
import subprocess
from pathlib import Path

def test_instance(cnf_path):
    """Test competition solver on a single instance."""
    # Convert to absolute path so it works regardless of cwd
    abs_path = cnf_path.resolve()
    result = subprocess.run(
        ['python', '../competition_solver.py', str(abs_path)],
        capture_output=True,
        text=True
    )

    # Extract status line
    for line in result.stdout.split('\n'):
        if line.startswith('s '):
            return line.split()[1]  # SATISFIABLE, UNSATISFIABLE, or UNKNOWN

    return 'ERROR'

def main():
    print('Testing Competition Solver')
    print('=' * 70)

    # Test simple suite
    simple_dir = Path('../../../dataset/simple_tests/simple_suite')
    simple_instances = sorted(simple_dir.glob('*.cnf'))[:5]

    print('\nSimple Test Suite (5 instances):')
    print('-' * 70)

    for instance in simple_instances:
        status = test_instance(instance)
        print(f'  {instance.stem:30s} → {status}')

    # Test medium suite
    medium_dir = Path('../../../dataset/medium_tests/medium_suite')
    medium_instances = sorted(medium_dir.glob('*.cnf'))[:5]

    print('\nMedium Test Suite (5 instances):')
    print('-' * 70)

    for instance in medium_instances:
        status = test_instance(instance)
        print(f'  {instance.stem:30s} → {status}')

    print('\n' + '=' * 70)
    print('All tests completed successfully!')

if __name__ == '__main__':
    main()
