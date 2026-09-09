#!/usr/bin/env python3
"""Fetch pinned upstream circuits and convert with an explicit AIGER executable."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import urllib.request
from aag_history import Circuit


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--aigtoaig', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--manifest', type=Path, default=Path(__file__).parent/'fixtures/industrial_aig.json',
                   help='Pinned input manifest; defaults to the original circuit suite')
    a = p.parse_args()
    manifest = json.loads(a.manifest.read_text())
    a.output.mkdir(parents=True, exist_ok=True)
    for row in manifest['inputs']:
        target = a.output/row['file']
        if not target.exists():
            with urllib.request.urlopen(row['url'], timeout=60) as response: data = response.read(row['bytes']+1)
            assert len(data) == row['bytes'] and hashlib.sha256(data).hexdigest() == row['sha256']
            target.write_bytes(data)
        data = target.read_bytes()
        assert len(data) == row['bytes'] and hashlib.sha256(data).hexdigest() == row['sha256']
        out = target.with_suffix('.aag')
        # Upstream refuses to overwrite. Write stdout so reruns remain reproducible.
        run = subprocess.run([str(a.aigtoaig.resolve()), '-a', str(target)], capture_output=True, check=True, timeout=30)
        circuit = Circuit.read(run.stdout.decode())
        out.write_bytes(run.stdout)
        row.update(aag=out.name, aag_sha256=hashlib.sha256(run.stdout).hexdigest(),
                   inputs=len(circuit.inputs), latches=len(circuit.latches), gates=len(circuit.gates))
    manifest['aigtoaig_sha256'] = hashlib.sha256(a.aigtoaig.read_bytes()).hexdigest()
    manifest['aiger_revision'] = '039ec1a2cc37d3093ac35c4b6df65336b346f409'
    manifest['aiger_revision_scope'] = 'Reproduction build pin; supplied executable identified separately by SHA-256.'
    (a.output/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')


if __name__ == '__main__': main()
