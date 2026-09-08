"""Create deterministic starter corpora without replacing evolved entries."""
import argparse
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('directory',type=Path);args=p.parse_args()
seeds=[bytes([0,0,1,1,2,0,2,1,2,1,3,2,3,1,2]),
       bytes([127,0,5]+[1,3,1,3,5,2]*20),
       bytes([255,25,4]+[1,2,0,1,3,0,4,1,2,3]*20),bytes(range(128))]
for target in ['api','dimacs','public']:
    folder=args.directory/target;folder.mkdir(parents=True,exist_ok=True)
    for i,data in enumerate(seeds):
        path=folder/f'seed-{i}'
        if not path.exists():path.write_bytes(data)
