#!/usr/bin/env python3
"""Check the frozen retention gate and summarize independently checked timings."""
import argparse
import json
from pathlib import Path
from statistics import mean


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--reports', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    protocol = json.loads((a.reports / 'ternary-confirmation-protocol.json').read_text())

    def read(name):
        d = json.loads((a.reports / (name + '.json')).read_text())
        assert d['complete'] and d['library_sha256'] == protocol['library_sha256']
        assert all(r['validated'] for r in d['runs'])
        return d['runs']

    def cpu(rows):
        return sum(r['cpu_seconds'] for r in rows)

    def compare(control, candidate):
        assert len(control) == len(candidate)
        for x, y in zip(control, candidate):
            assert (x['depth'], x['polarity'], x['input_sha256']) == (
                y['depth'], y['polarity'], y['input_sha256'])
            assert x['result'] == y['result']
        c, n = cpu(control), cpu(candidate)
        return dict(control_cpu=c, candidate_cpu=n, improvement_percent=100 * (1 - n / c),
                    control_peak_owned=max(r['stats']['owned_capacity_bytes'] for r in control),
                    candidate_peak_owned=max(r['stats']['owned_capacity_bytes'] for r in candidate))

    cal100 = [read(f'ternary-confirm-cal100-{i}') for i in range(4)]
    cal3 = [read(f'ternary-confirm-cal3-{i}') for i in range(4)]
    targets = []
    for indices, runs in [([(0, 1), (3, 2)], cal100), ([(1, 0), (2, 3)], cal3)]:
        pairs = []
        for c, n in indices:
            comparison = compare(runs[c], runs[n])
            deepest = max(r['depth'] for r in runs[c] if r['result'] == 20)
            old = next(r for r in runs[c] if r['depth'] == deepest and r['polarity'] == 1)
            new = next(r for r in runs[n] if r['depth'] == deepest and r['polarity'] == 1)
            comparison['target'] = dict(depth=deepest, control_cpu=old['cpu_seconds'],
                                        candidate_cpu=new['cpu_seconds'], control_stats=old['stats'],
                                        candidate_stats=new['stats'])
            assert all(new['stats'][k] <= old['stats'][k]
                       for k in ('conflicts', 'decisions', 'propagations'))
            pairs.append(comparison)
        targets.append(pairs)
    cases = [dict(circuit='cal3.aig', control_cpu=mean(cpu(cal3[i]) for i in (1, 2)),
                  candidate_cpu=mean(cpu(cal3[i]) for i in (0, 3)))]
    for i, case in enumerate(protocol['cases'][1:], 1):
        c = read(f'ternary-confirm-case-{i}-control')
        n = read(f'ternary-confirm-case-{i}-candidate')
        cases.append(dict(circuit=case['circuit'], **compare(c, n)))
    c = sum(r['control_cpu'] for r in cases)
    n = sum(r['candidate_cpu'] for r in cases)
    target_gain = all(r['improvement_percent'] > 5 for r in targets[1]) or all(
        r['target']['candidate_cpu'] < .95 * r['target']['control_cpu'] for r in targets[0])
    assert target_gain and n <= c * 1.05, 'frozen performance gate failed'
    a.output.write_text(json.dumps(dict(gate_passed=True, cal100_pairs=targets[0],
        cal3_pairs=targets[1], seven_histories=cases, control_cpu=c, candidate_cpu=n,
        improvement_percent=100 * (1 - n / c),
        limitation='Cal100 depth 8 remains UNKNOWN; seven-history aggregate excludes cal100.'),
        indent=2) + '\n')
    print(f'PASS: frozen retention gate; seven-history CPU {c:.3f} -> {n:.3f}s')


if __name__ == '__main__':
    main()
