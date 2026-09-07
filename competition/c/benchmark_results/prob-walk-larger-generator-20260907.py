import hashlib,json,random
from pathlib import Path
root=Path('/tmp/bsat-prob-corpus');root.mkdir(exist_ok=True)
inputs={}
for n in (500,1000,2000):
 for index in range(2):
  seed=20261007+10*n+index;rng=random.Random(seed)
  model=[False]+[bool(rng.getrandbits(1)) for _ in range(n)]
  clauses=[]
  for _ in range(21*n//5):
   while True:
    c=[v if rng.getrandbits(1) else -v for v in rng.sample(range(1,n+1),3)]
    if any(model[abs(l)]==(l>0) for l in c):clauses.append(c);break
  p=root/f'vars{n}-seed{index}.cnf'
  p.write_text(f'c planted development seed {seed}\np cnf {n} {len(clauses)}\n'+''.join(' '.join(map(str,c))+' 0\n' for c in clauses))
  inputs[p.name]={'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'generator_seed':seed}
(root/'manifest.json').write_text(json.dumps({'schema':1,'inputs':inputs},indent=2)+'\n')
