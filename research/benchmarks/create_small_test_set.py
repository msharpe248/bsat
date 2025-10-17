#!/usr/bin/env python3
"""
Create a small test set with only manageable instances.

Filters the SAT Competition 2025 benchmarks to include only instances
with reasonable size that can be solved by simple DPLL/CDCL implementations.
"""

import os
import shutil
from pathlib import Path


def parse_dimacs_header(filepath):
    """Parse DIMACS header to get variable and clause counts."""
    try:
        with open(filepath, 'r') as f:
            for line in f:
                line = line.strip()
                if line.startswith('p cnf'):
                    parts = line.split()
                    if len(parts) >= 4:
                        num_vars = int(parts[2])
                        num_clauses = int(parts[3])
                        return num_vars, num_clauses
    except:
        pass
    return None, None


def create_small_test_set():
    """Create small test set directory with manageable instances."""

    # Thresholds for "small" instances
    MAX_VARS = 500      # Maximum 500 variables
    MAX_CLAUSES = 2000  # Maximum 2000 clauses
    MAX_SIZE_MB = 0.5   # Maximum 0.5 MB file size

    source_dir = Path("../../dataset/sat_competition2025")
    dest_dir = Path("../../dataset/sat_competition2025_small")

    if dest_dir.exists():
        print(f"Removing existing {dest_dir}...")
        shutil.rmtree(dest_dir)

    dest_dir.mkdir(parents=True)

    print("Scanning for small instances...")
    print(f"Criteria: vars <= {MAX_VARS}, clauses <= {MAX_CLAUSES}, size <= {MAX_SIZE_MB} MB")
    print()

    small_files = []
    total_files = 0

    # Scan all families
    for family_dir in sorted(source_dir.iterdir()):
        if not family_dir.is_dir() or family_dir.name.startswith('.'):
            continue

        family_name = family_dir.name
        cnf_files = list(family_dir.glob("*.cnf"))
        total_files += len(cnf_files)

        for cnf_file in cnf_files:
            # Check file size
            size_mb = cnf_file.stat().st_size / (1024 * 1024)
            if size_mb > MAX_SIZE_MB:
                continue

            # Check variable/clause counts
            num_vars, num_clauses = parse_dimacs_header(str(cnf_file))
            if num_vars is None or num_clauses is None:
                continue

            if num_vars <= MAX_VARS and num_clauses <= MAX_CLAUSES:
                small_files.append({
                    'family': family_name,
                    'file': cnf_file,
                    'vars': num_vars,
                    'clauses': num_clauses,
                    'size_kb': cnf_file.stat().st_size / 1024
                })

    print(f"Found {len(small_files)} small instances out of {total_files} total")
    print()

    # Group by family
    families = {}
    for item in small_files:
        family = item['family']
        if family not in families:
            families[family] = []
        families[family].append(item)

    # Copy files to destination
    total_copied = 0
    for family_name in sorted(families.keys()):
        items = families[family_name]

        # Create family directory
        family_dest = dest_dir / family_name
        family_dest.mkdir()

        print(f"{family_name}: {len(items)} files")

        for item in items:
            # Copy file
            dest_file = family_dest / item['file'].name
            shutil.copy2(item['file'], dest_file)
            total_copied += 1

    print()
    print(f"Created small test set: {dest_dir}")
    print(f"  {len(families)} families")
    print(f"  {total_copied} total instances")
    print()
    print("Run with:")
    print(f"  ./run_all_benchmarks.py ../../dataset/sat_competition2025_small -t 30")


if __name__ == "__main__":
    create_small_test_set()
