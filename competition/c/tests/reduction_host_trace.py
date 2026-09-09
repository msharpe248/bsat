#!/usr/bin/env python3
"""Fixed-work reduction boundary traces; UNKNOWN prefixes are not certificates."""
import argparse,gzip,hashlib,json,platform,subprocess,tempfile
from pathlib import Path

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--solver',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 a.output.mkdir(parents=True,exist_ok=True)
 fixture=Path(__file__).parent/'fixtures/acceptance/cal3-query.cnf.gz'
 with tempfile.TemporaryDirectory() as tmp:
  cnf=Path(tmp)/'input.cnf';cnf.write_bytes(gzip.decompress(fixture.read_bytes()))
  report={'scope':__doc__,'platform':platform.platform(),'revision':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'binary_sha256':sha(a.solver),'input_sha256':sha(cnf),'runs':[],'complete':False}
  for mode,flags in [('default',[]),('no-reduction',['--reduce-interval','1000000000'])]:
   for limit in (1999,2000,2001,4001,10000):
    for repeat in range(2):
     name=f'{mode}-{limit}-{repeat}';proof=Path(tmp)/'proof'
     cmd=[str(a.solver.resolve()),*flags,'--conflicts',str(limit),'--binary-proof','--proof',str(proof),str(cnf)]
     r=subprocess.run(cmd,capture_output=True,timeout=60);assert r.returncode==0,(cmd,r.stdout,r.stderr)
     (a.output/(name+'.proof.gz')).write_bytes(gzip.compress(proof.read_bytes(),mtime=0))
     (a.output/(name+'.log.gz')).write_bytes(gzip.compress(r.stdout+r.stderr,mtime=0))
     stats={}
     for line in r.stdout.decode().splitlines():
      if line.startswith('c ') and ':' in line:
       k,v=line[2:].split(':',1);stats[k.strip()]=v.strip()
     assert int(stats['Conflicts'])==limit
     row={'name':name,'mode':mode,'conflicts':limit,'repeat':repeat,'proof_sha256':sha(proof),'stats':stats}
     if repeat:assert row['proof_sha256']==report['runs'][-1]['proof_sha256']
     report['runs'].append(row)
  report['complete']=True;(a.output/'results.json').write_text(json.dumps(report,indent=2)+'\n')
 print('PASS: 20 deterministic UNKNOWN boundary traces')
if __name__=='__main__':main()
