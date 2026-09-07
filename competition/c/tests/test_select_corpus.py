import json
from pathlib import Path
import tempfile
import unittest
from select_corpus import digest, select


class SelectionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.dataset = self.root/'data'; self.dataset.mkdir()
        self.reports = self.root/'reports'; self.reports.mkdir()
        self.output = self.reports/'selection.json'

    def cnf(self, family, name, data):
        path = self.dataset/family/name; path.parent.mkdir(exist_ok=True)
        path.write_text(data)
        return path.resolve()

    def test_history_aliases_families_and_replay(self):
        old = self.cnf('old', 'old.cnf', 'p cnf 1 1\n1 0\n')
        self.cnf('renamed', 'alias.cnf', old.read_text())
        self.cnf('other-name', 'old.cnf', 'different contents')
        a = self.cnf('a', 'first.cnf', 'p cnf 0 0\n')
        self.cnf('a', 'larger.cnf', 'p cnf 0 0\nc padding\n')
        self.cnf('b', 'duplicate.cnf', a.read_text())
        b = self.cnf('b', 'unique.cnf', 'p cnf 1 0\n')
        # History intentionally uses a different schema and an unrelated location.
        (self.reports/'prior.json').write_text(json.dumps({'old_command': '/old/old.cnf',
                                                        'nested': {'hash': digest(old)}}))
        result = select(self.dataset,self.reports,self.output,2)
        self.assertEqual(list(result['inputs']), [str(a),str(b)])
        self.output.write_text(json.dumps(result))
        self.assertEqual(select(self.dataset,self.reports,self.output,2),result)
        self.assertEqual(len(result['history']),1)
        self.assertEqual(result['inputs'][str(a)]['sha256'],digest(a))

    def test_shortfall_and_invalid_history_preserve_output(self):
        self.cnf('a','one.cnf','p cnf 0 0\n')
        self.output.write_text('existing output')
        with self.assertRaisesRegex(ValueError,'only 1 eligible'):
            select(self.dataset,self.reports,self.output,2)
        with self.assertRaisesRegex(ValueError,'only 0 eligible'):
            select(self.dataset,self.reports,self.output,1,1)
        self.assertEqual(self.output.read_text(),'existing output')
        (self.reports/'broken.json').write_text('{bad')
        with self.assertRaises(ValueError):
            select(self.dataset,self.reports,self.output,1)

    def test_parameter_validation(self):
        for count, maximum in ((0,0),(-1,0),(1,-1)):
            with self.assertRaises(ValueError):
                select(self.dataset,self.reports,self.output,count,maximum)


if __name__ == '__main__':
    unittest.main()
