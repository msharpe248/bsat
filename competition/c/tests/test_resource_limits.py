import sys
import tempfile
import unittest
from pathlib import Path
from benchmark import worker, external_wall_command
from resource_limits import child_limits


class Limits(unittest.TestCase):
    def run_case(self, source, timeout=2, **limits):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);cnf=root/'input.cnf';cnf.write_text('p cnf 1 1\n1 0\n')
            script=root/'child.py';script.write_text(source)
            return worker(dict(input=str(cnf),command=[sys.executable,str(script),'{proof}'],
                               timeout=timeout,checker=None,check_timeout=1,**limits))

    def test_invalid_limits(self):
        for value in (-1,1.5,float('nan')):
            with self.assertRaises(ValueError):child_limits(value)
        self.assertIsNone(child_limits())

    def test_uniform_wall_commands(self):
        for flag in ('--time','--time=0','-t','-t60','--conflicts=10','--decisions','--portfolio'):
            with self.assertRaises(ValueError):external_wall_command(['solver',flag])
        external_wall_command(['solver','--chrono','--no-rephase','--proof','{proof}','{input}'])

    def test_wall_rejects_early_status(self):
        row=self.run_case("import time\nprint('s SATISFIABLE',flush=True)\nprint('v 1 0',flush=True)\ntime.sleep(5)\n",timeout=0.2)
        self.assertTrue(row['wall_limit_hit']);self.assertEqual(row['status'],'UNKNOWN');self.assertFalse(row['verified'])

    def test_exit_marker_mismatch(self):
        row=self.run_case("import sys\nprint('s UNSATISFIABLE')\nprint('v 1 0')\nsys.exit(10)\n")
        self.assertEqual(row['status'],'ERROR');self.assertFalse(row['verified'])

    def test_file_ceiling(self):
        row=self.run_case("import sys,os\ntry:\n with open(sys.argv[1],'wb') as f: f.write(b'x'*8192)\nexcept OSError: pass\nelse: raise AssertionError('limit not enforced')\nassert os.stat(sys.argv[1]).st_size<=1024\nprint('s UNKNOWN')\n",file_size_limit=1024)
        self.assertEqual(row['exit_code'],0);self.assertEqual(row['status'],'UNKNOWN');self.assertFalse(row['verified'])

    @unittest.skipUnless(sys.platform=='linux','RLIMIT_AS enforcement is Linux-only')
    def test_address_space(self):
        row=self.run_case("try:\n a=bytearray(256*1024*1024)\nexcept MemoryError:\n print('s UNKNOWN')\nelse:\n raise AssertionError('limit not enforced')\n",address_space_limit=64*1024*1024)
        self.assertEqual(row['exit_code'],0);self.assertEqual(row['status'],'UNKNOWN');self.assertFalse(row['verified'])


if __name__=='__main__':unittest.main()
