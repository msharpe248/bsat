#!/usr/bin/env python3
"""Run the frozen serial retention comparison using independently checked histories."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--protocol', type=Path, required=True)
    p.add_argument('--output-dir', type=Path, required=True)
    a = p.parse_args()
    protocol = json.loads(a.protocol.read_text())
    library = Path(protocol['library'])
    digest = hashlib.sha256(library.read_bytes()).hexdigest()
    assert digest == protocol['library_sha256']
    a.output_dir.mkdir(parents=True, exist_ok=True)

    def run(name, case, candidate, conflicts):
        profile = protocol['candidate_profile' if candidate else 'control_profile']
        output = a.output_dir / (name + '.json')
        assert not output.exists(), output
        print(name, profile, flush=True)
        subprocess.run([
            sys.executable, str(Path(__file__).with_name('measure_search_scaling.py')),
            '--library', str(library), '--circuits', case['circuits'],
            '--checked-report', case['checked_report'], '--circuit', case['circuit'],
            '--profile', profile, '--macos-qos', '--conflicts', str(conflicts),
            '--cpu', '300' if conflicts else '60', '--output', str(output)], check=True)
        result = json.loads(output.read_text())
        assert result['complete'] and result['library_sha256'] == digest
        assert all(r['validated'] for r in result['runs'])

    cal100 = dict(circuit='cal100.aig', circuits='/private/tmp/bsat-expanded-circuits',
                  checked_report='competition/c/benchmark_results/cal100-search-20260910/baseline.json')
    for i, candidate in enumerate((False, True, True, False)):
        run(f'ternary-confirm-cal100-{i}', cal100, candidate, 200000)
    for i, candidate in enumerate((True, False, False, True)):
        run(f'ternary-confirm-cal3-{i}', protocol['cases'][0], candidate, 0)
    for i, case in enumerate(protocol['cases'][1:], 1):
        for candidate in (False, True):
            run(f'ternary-confirm-case-{i}-' + ('candidate' if candidate else 'control'),
                case, candidate, 0)
    print('PASS: all frozen serial comparisons and independent checks completed', flush=True)


if __name__ == '__main__':
    main()
