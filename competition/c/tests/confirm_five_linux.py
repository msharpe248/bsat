"""Serial matched-host replay of the frozen eight-circuit Linux evaluation."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('baseline', 'candidate', 'checked', 'circuits', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument('--candidate-flags', type=int, default=3)
    a = p.parse_args()
    assert sys.platform == 'linux'
    os.sched_setaffinity(0, {min(os.sched_getaffinity(0))})
    checked = json.loads(a.checked.read_text())
    assert checked['complete'] and all(r['validated'] for r in checked['runs'])
    a.output.mkdir(parents=True, exist_ok=True)
    report = dict(complete=False, cpu=10, schedule='AB on even circuits, BA on odd circuits',
                  candidate_flags=a.candidate_flags,
                  checked_sha256=hashlib.sha256(a.checked.read_bytes()).hexdigest(),
                  baseline_sha256=hashlib.sha256(a.baseline.read_bytes()).hexdigest(),
                  candidate_sha256=hashlib.sha256(a.candidate.read_bytes()).hexdigest(),
                  cases=[], lost_queries=[], gained_queries=[])
    destination = a.output / 'summary.json'
    destination.write_text(json.dumps(report, indent=2) + '\n')
    for index, item in enumerate(checked['circuits']['inputs']):
        runs = {}
        for role in (('baseline', 'candidate') if index % 2 == 0 else ('candidate', 'baseline')):
            path = a.output / f'{index}-{role}.json'
            subprocess.run([sys.executable, str(Path(__file__).with_name('measure_search_scaling.py')),
                            '--library', str(getattr(a, role)), '--circuits', str(a.circuits),
                            '--checked-report', str(a.checked), '--circuit', item['file'],
                            '--profile', 'control', '--conflicts', '0', '--cpu', '10',
                            '--flags', str(a.candidate_flags if role == 'candidate' else 3),
                            '--output', str(path)], check=True)
            d = json.loads(path.read_text())
            assert d['complete'] and all(r['validated'] for r in d['runs'])
            assert d['library_sha256'] == report[role + '_sha256']
            runs[role] = d['runs']
        assert len(runs['baseline']) == len(runs['candidate'])
        for old, new in zip(runs['baseline'], runs['candidate']):
            assert (old['input_sha256'], old['depth'], old['polarity']) == (
                new['input_sha256'], new['depth'], new['polarity'])
            if old['result'] and new['result']:
                assert old['result'] == new['result']
            key = [item['file'], old['depth'], old['polarity']]
            if old['result'] and not new['result']:
                report['lost_queries'].append(key)
            if new['result'] and not old['result']:
                report['gained_queries'].append(key)
        row = dict(circuit=item['file'])
        for role in ('baseline', 'candidate'):
            row[role + '_checked'] = sum(bool(r['result']) for r in runs[role])
            row[role + '_cpu'] = sum(r['cpu_seconds'] for r in runs[role])
            row[role + '_par2'] = sum(r['cpu_seconds'] if r['result'] and r['cpu_seconds'] <= 10
                                     else 20 for r in runs[role])
        report['cases'].append(row)
        destination.write_text(json.dumps(report, indent=2) + '\n')
    report['complete'] = True
    destination.write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
