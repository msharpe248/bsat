#!/usr/bin/env python3
"""Summarize retained-query search work without mixing cumulative and query counters."""
import argparse
import json
from pathlib import Path


PHASES = ('parse', 'propagate', 'analyze', 'reduce', 'gc', 'preprocess',
          'simplify', 'reconstruct', 'model', 'proof', 'search')
CUMULATIVE = ('binary_visits', 'long_visits', 'blocker_hits', 'first_hits',
              'replacement_scans', 'replacement_moves', 'long_units', 'long_conflicts',
              'original_scans', 'learned_scans', 'scan_size_9_plus',
              'learned_reason_uses', 'learned_reason_lbd_sum', 'restart_events',
              'restart_trail_before', 'restart_trail_kept',
              'restart_levels_before', 'restart_levels_kept', 'literal_inspections')
PARITY = ('conflicts', 'decisions', 'propagations', 'restarts', 'reduces',
          'learned_clauses', 'learned_literals', 'deleted_clauses',
          'minimized_literals', 'minimize_inspections')


def ratio(a, b):
    return a / b if b else None


def key(row):
    return tuple(row[k] for k in ('circuit', 'repeat', 'flags', 'depth', 'polarity'))


def summarize(report):
    assert report['complete'], 'incomplete benchmark'
    previous = {}
    rows = []
    seen = set()
    for row in report['runs']:
        identity = key(row)
        assert identity not in seen, ('duplicate query', identity)
        seen.add(identity)
        assert row['validated'], ('unvalidated query', identity)
        d = row.get('diagnostic', {})
        history = identity[:3]
        old = previous.get(history)
        # Fast reuse preserves these counters. A rebuild starts them at zero,
        # while carrying reused_solves forward without incrementing it.
        retained = old is not None and d.get('reused_solves') == old.get('reused_solves', -2) + 1
        delta = {k: d[k] - (old[k] if retained else 0)
                 for k in CUMULATIVE if k in d}
        assert all(v >= 0 for v in delta.values()), ('counter went backwards', identity)
        if d:
            assert 'reused_solves' in d, 'requires explicit counter lifecycle metadata'
            assert len(d['phase_seconds']) == len(PHASES)
            assert len(d['phase_calls']) == len(PHASES)
            previous[history] = d
        stats = row['stats']
        conflicts, propagations = stats['conflicts'], stats['propagations']
        output = {k: row[k] for k in ('circuit', 'repeat', 'flags', 'depth', 'polarity',
                                     'result', 'cpu_seconds', 'input_sha256')}
        output.update(conflicts=conflicts, decisions=stats['decisions'], propagations=propagations,
                      cpu_per_conflict=ratio(row['cpu_seconds'], conflicts),
                      propagations_per_conflict=ratio(propagations, conflicts),
                      decisions_per_conflict=ratio(stats['decisions'], conflicts),
                      journal_bytes=row.get('journal_bytes'), counter_delta=delta,
                      incremental_cpu_seconds=row.get('incremental_cpu_seconds'),
                      reference_statistics=row.get('reference_statistics'))
        if d:
            output['query_work'] = {k: d[k] for k in PARITY}
            output['phase_seconds_inclusive'] = dict(zip(PHASES, d['phase_seconds']))
            output['phase_calls'] = dict(zip(PHASES, d['phase_calls']))
            output['minimize_lbd_seconds'] = d.get('minimize_lbd_seconds')
            output['learned_literals_per_conflict'] = ratio(d['learned_literals'], conflicts)
            output['scans_per_propagation'] = ratio(delta.get('replacement_scans', 0), propagations)
            output['blocker_hit_fraction'] = ratio(delta.get('blocker_hits', 0), delta.get('long_visits', 0))
            if 'restart_events' in delta:
                assert delta['restart_events'] == d['restarts'], ('restart lifecycle mismatch', identity)
                removed = delta['restart_trail_before'] - delta['restart_trail_kept']
                assert removed >= 0
                output['restart_removed_assignments'] = removed
                output['removed_per_restart'] = ratio(removed, delta['restart_events'])
                output['restart_removed_to_propagations'] = ratio(removed, propagations)
        rows.append(output)
    return rows


def parity(control, diagnostic):
    a = {key(r): r for r in control['runs']}
    b = {key(r): r for r in diagnostic['runs']}
    assert len(a) == len(control['runs']) and len(b) == len(diagnostic['runs'])
    assert a.keys() == b.keys(), 'query sets differ'
    assert control['complete'] and diagnostic['complete']
    for identity, row in a.items():
        other = b[identity]
        assert row['validated'] and other['validated']
        for field in ('result', 'input_sha256', 'bsat_proof_sha256', 'journal_bytes'):
            assert row.get(field) == other.get(field), (identity, field)
        for field in PARITY:
            assert row['diagnostic'][field] == other['diagnostic'][field], (identity, field)
    return len(a)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--control', type=Path)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('report', type=Path)
    a = p.parse_args()
    report = json.loads(a.report.read_text())
    result = dict(scope='Query work; intrusive inclusive phase times are not performance scores. '
                        'Restart removals do not count identical assignments replayed. '
                        'literal_inspections is cumulative resource work including arena copies; '
                        'expanded diagnostic headers make that counter layout-dependent, '
                        'so it is excluded from cross-layout search parity.',
                  rows=summarize(report))
    if a.control:
        result['exact_work_parity_queries'] = parity(json.loads(a.control.read_text()), report)
    a.output.write_text(json.dumps(result, indent=2) + '\n')
    print('PASS:', len(result['rows']), 'query summaries')


if __name__ == '__main__':
    main()
