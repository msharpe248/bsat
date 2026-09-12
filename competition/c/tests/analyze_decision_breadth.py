#!/usr/bin/env python3
"""Summarize per-decision propagation closures without treating them as removable work."""
import argparse
import json
from pathlib import Path


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--report', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    report = json.loads(a.report.read_text())
    assert report['complete']
    rows = []
    for run in report['runs']:
        observed = run['diagnostic']['decision_observations']
        decisions = sum(v[1] for v in observed)
        propagations = sum(v[2] for v in observed)
        conflicts = sum(v[3] for v in observed)
        assert len({v[0] for v in observed}) == len(observed)
        assert all(v[0] and v[1] > 0 and v[2] >= 0 and 0 <= v[3] <= v[1]
                   for v in observed)
        assert decisions == run['stats']['decisions']
        assert propagations == run['diagnostic']['propagation_source'][2]
        assert conflicts <= run['stats']['conflicts']
        ranked = sorted(observed, key=lambda v: (-v[2], v[0]))
        row = dict(depth=run['depth'], polarity=run['polarity'], decisions=decisions,
                   decision_closure_propagations=propagations,
                   decisions_per_conflict=decisions / run['stats']['conflicts']
                   if run['stats']['conflicts'] else None,
                   propagations_per_decision=propagations / decisions if decisions else None,
                   immediate_conflicts=conflicts,
                   top_10_cost_fraction=sum(v[2] for v in ranked[:10]) / propagations
                   if propagations else None,
                   top_20=ranked[:20])
        row['by_sign'] = {}
        for name, sign in [('positive', 1), ('negative', -1)]:
            group = [v for v in observed if v[0] * sign > 0]
            row['by_sign'][name] = dict(decisions=sum(v[1] for v in group),
                                       propagations=sum(v[2] for v in group),
                                       immediate_conflicts=sum(v[3] for v in group))
        rows.append(row)
    a.output.write_text(json.dumps(dict(
        caveat='A closure is the propagation call started by a decision, not all later '
               'work causally dependent on it. Immediate conflicts exclude conflicts '
               'after subsequent decisions or learned assertions. Sign comparisons are '
               'observational, not controlled experiments on the same assignments.',
        rows=rows), indent=2) + '\n')
    print(f'PASS: {len(rows)} query-local decision profiles reconcile with solver totals')


if __name__ == '__main__':
    main()
