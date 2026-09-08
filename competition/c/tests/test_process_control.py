from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
from process_control import run_capture

class ProcessTests(unittest.TestCase):
    def test_timeout_kills_descendants(self):
        with tempfile.TemporaryDirectory() as tmp:
            beat=Path(tmp)/'heartbeat'
            worker=f"from pathlib import Path;import time\np=Path({str(beat)!r})\nwhile True:\n p.write_text(str(time.monotonic()))\n time.sleep(.01)\n"
            parent=f"import subprocess,sys,time\nsubprocess.Popen([sys.executable,'-c',{worker!r}])\ntime.sleep(60)\n"
            with self.assertRaises(subprocess.TimeoutExpired):run_capture([sys.executable,'-c',parent],.5)
            self.assertTrue(beat.exists());before=beat.read_text();time.sleep(.1)
            self.assertEqual(beat.read_text(),before)
    def test_nested_wrapper_cleanup(self):
        with tempfile.TemporaryDirectory() as tmp:
            beat=Path(tmp)/'heartbeat'
            worker=f"from pathlib import Path;import time\np=Path({str(beat)!r})\nwhile True:\n p.write_text(str(time.monotonic()))\n time.sleep(.01)\n"
            wrapper=f"import sys,signal\nsys.path.insert(0,{str(Path(__file__).resolve().parent)!r})\nfrom process_control import run_capture,interrupt_on_term\nsignal.signal(signal.SIGTERM,interrupt_on_term)\nrun_capture([sys.executable,'-c',{worker!r}],60)\n"
            with self.assertRaises(subprocess.TimeoutExpired):run_capture([sys.executable,'-c',wrapper],.5)
            self.assertTrue(beat.exists());before=beat.read_text();time.sleep(.1)
            self.assertEqual(beat.read_text(),before)
    def test_result(self):
        r=run_capture([sys.executable,'-c','print("ok")'],5)
        self.assertEqual((r.returncode,r.stdout),(0,'ok\n'))

if __name__=='__main__':unittest.main()
