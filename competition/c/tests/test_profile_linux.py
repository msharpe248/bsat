import unittest
from profile_linux import parse_perf


class PerfParsing(unittest.TestCase):
    def test_scaled_counter(self):
        self.assertEqual(parse_perf('123;;cycles:u;1000;98.50;;\n',('cycles:u',)),
                         {'cycles:u':{'count':123.0,'running_percent':98.5}})

    def test_fail_closed(self):
        for text in ['', '<not supported>;;cycles:u;0;0;;',
                     '<not counted>;;cycles:u;0;0;;','nan;;cycles:u;10;100;;',
                     '12;;cycles:u;10;0;;','12;;cycles:u;10;100;;\n12;;cycles:u;10;100;;']:
            with self.subTest(text=text), self.assertRaises(ValueError):
                parse_perf(text,('cycles:u',))


if __name__=='__main__':unittest.main()
