"""Check and summarize the frozen cal100 library comparison."""
import argparse
import json
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--input-dir', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()

def read(name):
    d = json.loads((a.input_dir / (name + '.json')).read_text())
    assert d['complete']
    for row in d['runs']:
        assert row['validated']
        if row['result'] == 20:
            assert row['certificate_checks']['bsat-check']['verified']
        if row['result'] == 10:
            assert row['bsat_model_and_simulation']
    return d

def compare(control, candidate):
    assert len(control['runs']) == len(candidate['runs'])
    for old, new in zip(control['runs'], candidate['runs']):
        assert old['input_sha256'] == new['input_sha256']
        if old['result']:
            assert new['result'] == old['result']

def cpu(d):
    return sum(r['cpu_seconds'] for r in d['runs'])

rows = []
old = [read('cal3-0-control'), read('cal3-3-control')]
new = [read('cal3-1-candidate'), read('cal3-2-candidate')]
for x, y in zip(old, new):
    compare(x, y)
rows.append(dict(circuit='cal3.aig', control_cpu=sum(map(cpu, old)) / 2,
                 candidate_cpu=sum(map(cpu, new)) / 2))
for i in range(1, 7):
    old, new = read(f'case-{i}-control'), read(f'case-{i}-candidate')
    compare(old, new)
    rows.append(dict(circuit=old['runs'][0]['circuit'], control_cpu=cpu(old),
                     candidate_cpu=cpu(new)))
for row in rows:
    row['candidate_over_control'] = row['candidate_cpu'] / row['control_cpu']
old, new = read('cal100-control'), read('cal100-candidate')
compare(old, new)
target = lambda d: next(r for r in d['runs'] if r['depth'] == 8 and r['polarity'] == 1)
assert target(new)['result'] == 20
old_sum = sum(r['control_cpu'] for r in rows)
new_sum = sum(r['candidate_cpu'] for r in rows)
result = dict(complete=True, all_conclusive_answers_preserved=True, other_histories=rows,
              aggregate_control_cpu=old_sum, aggregate_candidate_cpu=new_sum,
              aggregate_improvement_percent=100 * (1 - new_sum / old_sum),
              cal100_depth8_control=target(old), cal100_depth8_candidate=target(new))
if (a.input_dir / 'cal100-repeat-candidate.json').exists():
    repeated_old, repeated_new = read('cal100-repeat-control'), read('cal100-repeat-candidate')
    compare(repeated_old, repeated_new)
    assert target(repeated_new)['result'] == 20
    result['cal100_depth8_repeat_control'] = target(repeated_old)
    result['cal100_depth8_repeat_candidate'] = target(repeated_new)
    result['cal100_control_mean_cpu'] = (target(old)['cpu_seconds'] + target(repeated_old)['cpu_seconds']) / 2
    result['cal100_candidate_mean_cpu'] = (target(new)['cpu_seconds'] + target(repeated_new)['cpu_seconds']) / 2
a.output.write_text(json.dumps(result, indent=2) + '\n')
print('PASS: all conclusive answers preserved; cal100 depth 8 independently certified')
print(f'Other seven histories: {old_sum:.3f} -> {new_sum:.3f} CPU seconds')
