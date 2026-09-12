"""Serial comparison of two pinned libraries on checked incremental histories."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--protocol', type=Path, required=True)
p.add_argument('--output-dir', type=Path, required=True)
p.add_argument('--controls-from', type=Path,
               help='Reuse pinned control reports when rechecking a hardened candidate')
p.add_argument('--repeat-target', action='store_true',
               help='Append candidate/control cal100 runs for an ABBA target comparison')
a = p.parse_args()
protocol = json.loads(a.protocol.read_text())
for role in ('control', 'candidate'):
    assert hashlib.sha256(Path(protocol[role]).read_bytes()).hexdigest() == protocol[role + '_sha256']
a.output_dir.mkdir(parents=True, exist_ok=True)

def run(name, case, role, cpu=60):
    output = a.output_dir / (name + '.json')
    assert not output.exists(), output
    if role == 'control' and a.controls_from:
        source = a.controls_from / output.name
        result = json.loads(source.read_text())
        assert result['complete'] and result['library_sha256'] == protocol['control_sha256']
        assert all(r['validated'] for r in result['runs'])
        shutil.copyfile(source, output)
        print(name, 'reused control from', source, flush=True)
        return
    print(name, role, flush=True)
    subprocess.run([sys.executable, str(Path(__file__).with_name('measure_search_scaling.py')),
                    '--library', protocol[role], '--circuits', case['circuits'],
                    '--checked-report', case['checked_report'], '--circuit', case['circuit'],
                    '--profile', 'control', '--macos-qos', '--conflicts', '0',
                    '--cpu', str(cpu), '--output', str(output)], check=True)
    result = json.loads(output.read_text())
    assert result['complete'] and result['library_sha256'] == protocol[role + '_sha256']
    assert all(r['validated'] for r in result['runs'])

for i, role in enumerate(('control', 'candidate', 'candidate', 'control')):
    run(f'cal3-{i}-{role}', protocol['cases'][0], role)
for i, case in enumerate(protocol['cases'][1:], 1):
    for role in ('control', 'candidate'):
        run(f'case-{i}-{role}', case, role)
cal100 = dict(circuit='cal100.aig', circuits='/private/tmp/bsat-expanded-circuits',
              checked_report='competition/c/benchmark_results/cal100-search-20260910/baseline.json')
for role in ('control', 'candidate'):
    run('cal100-' + role, cal100, role, 300)
if a.repeat_target:
    for role in ('candidate', 'control'):
        run('cal100-repeat-' + role, cal100, role, 300)
print('PASS: serial history comparison completed', flush=True)
