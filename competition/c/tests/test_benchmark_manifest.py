import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from benchmark import digest, verified_manifest


class ManifestTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.cnf = self.root/'case.cnf'; self.cnf.write_text('p cnf 1 1\n1 0\n')
        self.manifest = self.root/'manifest.json'
        self.write({'case.cnf': {'sha256': digest(self.cnf)}})

    def write(self, entries):
        self.manifest.write_text(json.dumps({'inputs':entries}))

    def test_relative_path_and_manifest_hash(self):
        inputs, metadata = verified_manifest(self.manifest)
        self.assertEqual(list(inputs), [str(self.cnf.resolve())])
        self.assertEqual(inputs[str(self.cnf.resolve())]['sha256'],digest(self.cnf))
        self.assertEqual(metadata['sha256'],digest(self.manifest))

    def test_changed_bytes_fail_before_benchmark_output(self):
        self.cnf.write_text('p cnf 1 1\n-1 0\n')
        with self.assertRaisesRegex(ValueError,'hash mismatch'):
            verified_manifest(self.manifest)
        output = self.root/'result.json'
        result = subprocess.run([sys.executable,str(Path(__file__).with_name('benchmark.py')),
                                 '--manifest',str(self.manifest),'--solver',f'python={sys.executable}',
                                 '--output',str(output)],capture_output=True,text=True)
        self.assertEqual(result.returncode,2)
        self.assertIn('hash mismatch',result.stderr)
        self.assertFalse(output.exists())
        result = subprocess.run([sys.executable,str(Path(__file__).with_name('benchmark.py')),
                                 '--manifest',str(self.manifest),'--solver',f'python={sys.executable}',
                                 str(self.cnf)],capture_output=True,text=True)
        self.assertEqual(result.returncode,2)
        self.assertIn('either --manifest or positional inputs',result.stderr)

    def test_bad_entries_and_aliases(self):
        for entries in ({}, {'case.cnf':{}}, {'case.cnf':{'sha256':'x'*64}},
                        {'case.cnf':{'sha256':digest(self.cnf)},
                         './case.cnf':{'sha256':digest(self.cnf)}}):
            self.write(entries)
            with self.assertRaises(ValueError):
                verified_manifest(self.manifest)
        self.write({'missing.cnf':{'sha256':digest(self.cnf)}})
        with self.assertRaises(OSError):
            verified_manifest(self.manifest)


if __name__ == '__main__':
    unittest.main()
