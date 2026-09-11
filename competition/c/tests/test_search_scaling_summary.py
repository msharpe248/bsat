#!/usr/bin/env python3
"""Reject misleading counter deltas and unmatched instrumented histories."""
import copy
import unittest
from analyze_search_scaling import CUMULATIVE, PARITY, parity, summarize


def row(depth, reused, events, restarts):
    diagnostic = dict.fromkeys(PARITY, 10)
    diagnostic.update(dict.fromkeys(CUMULATIVE, events))
    diagnostic.update(reused_solves=reused, restarts=restarts,
                      restart_trail_before=events * 20, restart_trail_kept=events * 2,
                      phase_seconds=[0.0] * 11, phase_calls=[0] * 11)
    return dict(circuit='fixture', repeat=0, flags=3, depth=depth, polarity=1,
                result=20, validated=True, cpu_seconds=1, input_sha256=str(depth),
                bsat_proof_sha256=str(depth), journal_bytes=100,
                incremental_cpu_seconds=1,
                stats=dict(conflicts=10, decisions=10, propagations=10),
                diagnostic=diagnostic)


class SummaryTests(unittest.TestCase):
    def test_retained_and_rebuilt(self):
        report = dict(complete=True, runs=[row(0, 0, 3, 3), row(2, 1, 7, 4),
                                           row(4, 1, 2, 2)])
        rows = summarize(report)
        self.assertEqual([r['counter_delta']['restart_events'] for r in rows], [3, 4, 2])
        self.assertEqual(rows[1]['restart_removed_assignments'], 72)
        self.assertEqual(rows[2]['restart_removed_assignments'], 36)

    def test_counter_mismatch_rejected(self):
        report = dict(complete=True, runs=[row(0, 0, 3, 3), row(2, 1, 7, 7)])
        with self.assertRaisesRegex(AssertionError, 'restart lifecycle'):
            summarize(report)

    def test_parity_requires_exact_work_context_and_proof(self):
        report = dict(complete=True, runs=[row(0, 0, 3, 3)])
        self.assertEqual(parity(report, copy.deepcopy(report)), 1)
        layout = copy.deepcopy(report)
        layout['runs'][0]['diagnostic']['literal_inspections'] += 100
        self.assertEqual(parity(report, layout), 1)  # Arena bytes are not search-path parity.
        for field in ('result', 'input_sha256', 'bsat_proof_sha256', 'journal_bytes'):
            changed = copy.deepcopy(report)
            changed['runs'][0][field] = 'different'
            with self.subTest(field=field), self.assertRaises(AssertionError):
                parity(report, changed)
        changed = copy.deepcopy(report)
        changed['runs'][0]['diagnostic']['conflicts'] += 1
        with self.assertRaises(AssertionError):
            parity(report, changed)

    def test_missing_duplicate_or_unvalidated_query_rejected(self):
        report = dict(complete=True, runs=[row(0, 0, 3, 3)])
        for changed in (dict(complete=True, runs=[]),
                        dict(complete=True, runs=report['runs'] * 2),
                        dict(complete=False, runs=report['runs'])):
            with self.assertRaises(AssertionError):
                parity(report, changed)
        report['runs'][0]['validated'] = False
        with self.assertRaises(AssertionError):
            summarize(report)


if __name__ == '__main__':
    unittest.main()
