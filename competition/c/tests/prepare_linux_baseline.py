"""Verify and unpack the two pinned industrial development inputs for Linux CI."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path

p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=Path);a=p.parse_args()
source=Path(__file__).parent/'fixtures/linux_baseline';manifest=json.loads((source/'manifest.json').read_text())
a.output.mkdir(parents=True,exist_ok=False);inputs={}
for row in manifest['inputs']:
    packed=(source/row['file']).read_bytes();assert hashlib.sha256(packed).hexdigest()==row['compressed_sha256']
    data=gzip.decompress(packed);assert hashlib.sha256(data).hexdigest()==row['sha256'] and len(data)==row['bytes']
    target=a.output/row['family']/row['file'][:-3];target.parent.mkdir(exist_ok=True);target.write_bytes(data)
    inputs[str(target.resolve())]={'sha256':row['sha256']}
(a.output/'manifest.json').write_text(json.dumps({'inputs':inputs},indent=2)+'\n')
