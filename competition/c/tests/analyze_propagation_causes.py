#!/usr/bin/env python3
"""Reconcile query-local propagation causes against an uninstrumented control."""
import argparse
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--observed', type=Path, required=True)
    parser.add_argument('--control', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    observed = json.loads(args.observed.read_text())
    control = json.loads(args.control.read_text())
    assert observed['complete'] and control['complete']
    assert len(observed['runs']) == len(control['runs'])
    rows = []
    for row, baseline in zip(observed['runs'], control['runs']):
        for key in ('depth', 'polarity', 'input_sha256', 'result'):
            assert row[key] == baseline[key], key
        for key in ('conflicts', 'decisions', 'propagations'):
            assert row['stats'][key] == baseline['stats'][key], key
        if row['result'] == 20:
            assert row['bsat_proof_sha256'] == baseline['bsat_proof_sha256']
        counters = row['diagnostic']
        total = row['stats']['propagations']
        assert sum(counters['propagation_source']) == total
        assert sum(counters['propagation_frame']) <= total  # Constant variable excluded.
        assert sum(counters['decision_frame']) <= row['stats']['decisions']
        for same, opposite, removed in zip(counters['replay_same'],
                                            counters['replay_opposite'],
                                            counters['removed_processed']):
            assert same + opposite <= removed
        assert sum(counters['backjump_preservable']) <= sum(counters['backjump_removed'])
        rows.append(dict(
            depth=row['depth'], polarity=row['polarity'], propagations=total,
            propagation_sources=dict(zip(('other', 'assumption', 'decision', 'assertion'),
                                         counters['propagation_source'])),
            same_value_replay_after_restart=counters['replay_same'][1],
            same_value_replay_after_backjump=counters['replay_same'][2],
            restart_replay_fraction=counters['replay_same'][1] / max(1, total),
            backjump_replay_fraction=counters['replay_same'][2] / max(1, total),
            potentially_preservable_prefix=sum(counters['backjump_preservable']),
            potentially_preservable_fraction=sum(counters['backjump_preservable']) / max(1, total),
            propagation_frames=counters['propagation_frame'][:row['depth'] + 1],
            decision_frames=counters['decision_frame'][:row['depth'] + 1],
            jump_bins=dict(zip(('1', '2-3', '4-7', '8-15', '16-31', '32-63', '64-127', '128+'),
                               counters['backjump_distance']))))
        if 'cone_reasons' in counters:
            reasons = counters['cone_reasons']
            assert len(reasons) == 4
            assert all(0 <= n <= total for n, total in
                       zip(counters['cone_repeated'], reasons))
            assert all(0 <= n <= total for n, total in
                       zip(counters['cone_dominated'], reasons))
            assert (0 <= counters['boundary_binary_covered'] <=
                    counters['boundary_binary'] <= counters['boundary_literals'])
            conflicts = row['stats']['conflicts']
            rows[-1]['conflict_cone'] = dict(
                reason_expansions=dict(zip(('implicit_binary', 'original_ternary',
                                             'other_original', 'learned'), reasons)),
                reasons_per_conflict=sum(reasons) / conflicts if conflicts else None,
                propagations_per_conflict=total / conflicts if conflicts else None,
                boundary_literals=counters['boundary_literals'],
                directly_covered_binary=counters['boundary_binary_covered'],
                covered_fraction=counters['boundary_binary_covered'] /
                                 max(1, counters['boundary_literals']))
    args.output.write_text(json.dumps(dict(
        parity='Exact results, search counts and UNSAT proof hashes match control.',
        caveat='Replay is a repeated value after removal, not proof that the work is avoidable. '
               'Prefix preservation is an upper bound before accounting for changed search. '
               'Frame 63 includes all later frames; frame mapping is supplied by the AAG harness.',
        rows=rows), indent=2) + '\n')
    print(f'PASS: {len(rows)} query outcomes, exact search/proof parity and reconciled causal counters')


if __name__ == '__main__':
    main()
