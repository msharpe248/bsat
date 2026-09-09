#!/usr/bin/env python3
"""Frozen serial Linux comparison of certified restart policy across eight circuits."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

p=argparse.ArgumentParser(description=__doc__)
for name in ('baseline','candidate','reference','incremental-reference','original','expanded','output'):
    p.add_argument('--'+name,required=True,type=Path)
p.add_argument('--baseline-revision',default='d6601b90183d186185eb7d091235145f5f189264')
a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True)
policy=dict(baseline_revision=a.baseline_revision,
            order=['baseline','candidate','candidate','baseline'],flags=3,
            original=dict(depths='0,1,2,4,8,16',cpu=60),
            expanded=dict(depths='0,2,4,8',cpu=10),
            reference_cpu='same as BSAT query',reference_wall=90,
            checker_wall=600,checker_heap_mb=2048,checker_stack_mb=512,
            scope='Frozen existing pinned circuit sets; serial per host, no pristine holdout claim')
(a.output/'policy.json').write_text(json.dumps(policy,indent=2)+'\n')
for suite in ('original','expanded'):
    for index,version in enumerate(policy['order']):
        command=[sys.executable,str(Path(__file__).with_name('industrial_histories.py')),
                 '--library',str(getattr(a,version)), '--reference',str(a.reference),
                 '--incremental-reference',str(a.incremental_reference),
                 '--circuits',str(getattr(a,suite)),'--flags','3',
                 '--depths',policy[suite]['depths'],'--cpu',str(policy[suite]['cpu']),
                 '--reference-cpu',str(policy[suite]['cpu']),'--reference-wall','90',
                 '--fresh-wall','90','--checker-wall','600',
                 '--checker-heap-mb','2048','--checker-stack-mb','512',
                 '--retain-unverified',str(a.output/'unverified'),
                 '--output',str(a.output/f'{suite}-{index}-{version}.json')]
        subprocess.run(command,check=True)
