import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import unittest
from benchmark import digest, worker
from validate import checker_verified


class ArtifactTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.input = self.root/'case.cnf'; self.input.write_text('p cnf 1 2\n1 0\n-1 0\n')
        self.solver = self.root/'solver.py'
        self.solver.write_text("import sys\nfrom pathlib import Path\nPath(sys.argv[1]).write_bytes(b'a\\x02\\x00\\x00')\nprint('s UNSATISFIABLE')\nsys.exit(20)\n")
        self.saved = self.root/'saved'
        self.config = dict(input=str(self.input), command=[sys.executable,str(self.solver),'{proof}'],
                           timeout=5, checker=None, check_timeout=0.5, retain_unverified=str(self.saved))

    def metadata(self, result):
        saved = Path(result['artifacts']['directory'])
        self.assertEqual(digest(saved/'run.json'),result['artifacts']['metadata_sha256'])
        metadata = json.loads((saved/'run.json').read_text())
        for name, info in metadata['files'].items():
            self.assertEqual(digest(saved/name),info['sha256'])
            self.assertEqual((saved/name).stat().st_size,info['bytes'])
        self.assertEqual((saved/'input.cnf').read_bytes(),self.input.read_bytes())
        return saved, metadata

    def test_timeout_keeps_exact_evidence_and_penalty(self):
        checker = self.root/'checker'
        checker.write_text(f'#!{sys.executable}\nimport time\nprint("partial checker output",flush=True)\ntime.sleep(10)\n')
        checker.chmod(0o755); self.config['checker']=str(checker)
        result = worker(self.config)
        self.assertEqual(result['status'],'UNSAT'); self.assertFalse(result['verified'])
        self.assertEqual(result['par2'],10); self.assertEqual(result['diagnostic'],'proof check timed out')
        saved, metadata = self.metadata(result)
        self.assertEqual((saved/'proof.drat').read_bytes(),b'a\x02\x00\x00')
        self.assertIn('partial checker output',(saved/'checker-out').read_text())
        self.assertIn('UNSATISFIABLE',(saved/'out').read_text())
        self.assertFalse(metadata['result']['verified'])
        self.assertFalse(Path(metadata['executed_command'][2]).exists())

    def test_repeats_do_not_overwrite(self):
        first = worker(self.config); second = worker(self.config)
        a, _ = self.metadata(first); b, _ = self.metadata(second)
        self.assertNotEqual(a,b)
        self.assertEqual((a/'proof.drat').read_bytes(),(b/'proof.drat').read_bytes())

    def test_checker_success_requires_recognized_exit_and_exact_marker(self):
        for code, output, expected in [(0,'s VERIFIED\n',True),(1,'s VERIFIED\n',False),
                                       (-11,'s VERIFIED\n',False),(0,'c expected s VERIFIED\n',False),
                                       (0,'s NOT VERIFIED\n',False),(0,'',False),
                                       (1,'c trivial UNSAT\ns VERIFIED\n',True),
                                       (2,'c trivial UNSAT\ns VERIFIED\n',False),
                                       (-11,'c trivial UNSAT\ns VERIFIED\n',False),
                                       (0,'s VERIFIED\ns NOT VERIFIED\n',False)]:
            result=subprocess.CompletedProcess([],code,stdout=output,stderr='')
            self.assertEqual(checker_verified(result),expected)
        checker=self.root/'checker'
        checker.write_text(f'#!{sys.executable}\nimport sys\nprint("s VERIFIED")\nsys.exit(1)\n')
        checker.chmod(0o755); self.config['checker']=str(checker)
        result=worker(self.config)
        self.assertEqual(result['status'],'ERROR'); self.assertFalse(result['verified'])
        self.assertEqual(result['par2'],10)
        saved, _=self.metadata(result)
        self.assertEqual((saved/'checker-out').read_text(),'s VERIFIED\n')

    def test_real_checker_trivial_unsat(self):
        checker=os.environ.get('DRAT_TRIM') or shutil.which('drat-trim')
        if not checker:self.skipTest('set DRAT_TRIM to run external checker compatibility cases')
        inputs=['p cnf 1 1\n0\n','p cnf 1 2\n1 0\n-1 0\n',
                'p cnf 2 3\n1 0\n-1 2 0\n-2 0\n']
        proof=self.root/'proof'
        for text in inputs:
            self.input.write_text(text)
            for data in [b'0\n',b'a\x00']:
                proof.write_bytes(data)
                checked=subprocess.run([checker,str(self.input),str(proof)],capture_output=True,text=True,timeout=10)
                self.assertTrue(checker_verified(checked),(checked.returncode,checked.stdout,checked.stderr))

    def test_verified_and_unknown_do_not_retain(self):
        self.input.write_text('p cnf 1 1\n1 0\n')
        for code, output in [(10,'s SATISFIABLE\nv 1 0'),(0,'s UNKNOWN')]:
            self.solver.write_text(f'import sys\nprint({output!r})\nsys.exit({code})\n')
            result=worker(self.config)
            self.assertNotIn('artifacts',result)
            self.assertFalse(self.saved.exists())
            self.assertEqual(result['verified'],code==10)

    def test_invalid_model_retains_error(self):
        self.input.write_text('p cnf 1 1\n1 0\n')
        self.solver.write_text("import sys\nprint('s SATISFIABLE\\nv -1 0')\nsys.exit(10)\n")
        result=worker(self.config)
        self.assertEqual(result['status'],'ERROR'); self.assertFalse(result['verified'])
        self.assertEqual(result['par2'],10)
        saved, _ = self.metadata(result)
        self.assertIn('v -1 0',(saved/'out').read_text())

    def test_disabled_and_cli(self):
        result=worker({**self.config,'retain_unverified':None})
        self.assertNotIn('artifacts',result); self.assertFalse(self.saved.exists())
        output=self.root/'report.json'
        command=[sys.executable,str(Path(__file__).with_name('benchmark.py')),
                 '--solver','fake='+shlex.join(self.config['command']),
                 '--retain-unverified',str(self.saved),'--repeats','1',
                 '--output',str(output),str(self.input)]
        run=subprocess.run(command,capture_output=True,text=True,timeout=10)
        self.assertEqual(run.returncode,0,run.stderr)
        report=json.loads(output.read_text())
        self.assertEqual(report['summary']['fake']['verified_runs'],0)
        self.metadata(report['runs'][0])


if __name__ == '__main__':
    unittest.main()
