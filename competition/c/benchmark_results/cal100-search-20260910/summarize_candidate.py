"""Reproduce the frozen local candidate gate from complete checked reports."""
import json
from pathlib import Path
import statistics
import sys

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parents[1] / 'tests'))
from analyze_search_scaling import parity


def read(name):
    result = json.loads((ROOT / name).read_text())
    assert result['complete'], name
    assert all(r['validated'] for r in result['runs']), name
    return result


def identity(row):
    return tuple(row[k] for k in ('circuit', 'repeat', 'flags', 'depth', 'polarity'))


def cost(row):
    if not row['within_cpu_budget']:
        return 120.0
    check = row.get('certificate_checks', {}).get('bsat-check', {})
    if row['result'] == 20:
        assert check['verified'] and row['bsat_proof_sha256']
    else:
        assert row['result'] == 10 and row['bsat_model_and_simulation']
    return row['cpu_seconds'] + row.get('export_cpu_seconds', 0) + check.get('total_cpu_seconds', 0)


fixed = [read(f'fixed-cal100-{i}-{v}.json')
         for i, v in enumerate(('control', 'candidate', 'candidate', 'control'))]
for other in fixed[1:]:
    assert parity(fixed[0], other) == 8
    for x, y in zip(fixed[0]['runs'], other['runs']):
        assert x['diagnostic']['literal_inspections'] == y['diagnostic']['literal_inspections']
fixed_cpu = [next(r['cpu_seconds'] for r in d['runs'] if r['depth'] == 8 and r['polarity'] == 1)
             for d in fixed]
baseline = statistics.median([fixed_cpu[0], fixed_cpu[3]])
improvement = 100 * (1 - statistics.median(fixed_cpu[1:3]) / baseline)
summary = dict(fixed_work=dict(order=['control', 'candidate', 'candidate', 'control'],
                              target_cpu_seconds=fixed_cpu, improvement_percent=improvement,
                              repeatable_over_five_percent=all(c < baseline * .95 for c in fixed_cpu[1:3]),
                              exact_search_resource_work_and_proof_parity=True), targets={})
for family in ('cal100', 'cal3'):
    control = read('baseline.json' if family == 'cal100' else 'cal3-baseline.json')
    candidates = [read(f'timed-{family}-{i}-candidate.json') for i in range(2)]
    rows = []
    for i, d in enumerate(candidates):
        for r in d['runs']:
            rows.append(dict(r, repeat=i))
    a = {identity(r): r for r in control['runs']}
    b = {identity(r): r for r in rows}
    assert len(a) == len(control['runs']) and len(b) == len(rows) and a.keys() == b.keys()
    losses, gains = [], []
    for k, x in a.items():
        y = b[k]
        assert x['input_sha256'] == y['input_sha256']
        assert not x['result'] or not y['result'] or x['result'] == y['result']
        if x['within_cpu_budget'] and not y['within_cpu_budget']:
            losses.append(k)
        if y['within_cpu_budget'] and not x['within_cpu_budget']:
            gains.append(k)
    old, new = sum(map(cost, a.values())), sum(map(cost, b.values()))
    target_depth = max(r['depth'] for r in rows)
    summary['targets'][family] = dict(queries=len(a), checked_baseline=sum(r['within_cpu_budget'] for r in a.values()),
        checked_candidate=sum(r['within_cpu_budget'] for r in b.values()), checked_losses=losses, checked_gains=gains,
        cost_scope='Solve/export/independent BSAT check CPU; UNKNOWN receives 120-second PAR2. Not full transaction CPU.',
        baseline_cpu_par2=old, candidate_cpu_par2=new, cost_change_percent=100*(new/old-1),
        target_baseline=[dict(cpu=r['cpu_seconds'],result=r['result'],conflicts=r['stats']['conflicts']) for r in a.values() if r['depth']==target_depth and r['polarity']==1],
        target_candidate=[dict(cpu=r['cpu_seconds'],result=r['result'],conflicts=r['stats']['conflicts']) for r in b.values() if r['depth']==target_depth and r['polarity']==1])
target = summary['targets']['cal100']
cal3 = summary['targets']['cal3']
summary['local_gate_passed'] = bool((len(target['checked_gains']) == 2 or summary['fixed_work']['repeatable_over_five_percent']) and
    not target['checked_losses'] and not cal3['checked_losses'] and cal3['cost_change_percent'] <= 5)
summary['production_promotion'] = False
summary['promotion_scope'] = 'A local pass would still require independent holdout and cross-host confirmation.'
(ROOT / 'candidate-summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print(json.dumps(summary, indent=2))
