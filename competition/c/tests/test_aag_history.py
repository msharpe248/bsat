import itertools
import unittest
from aag_history import Circuit


class UnrollTests(unittest.TestCase):
    def test_exhaustive_transitions(self):
        for reset in (0, 1, 4):
            for output in (0, 1, 4, 5, 6, 7):
                c = Circuit.read(f'aag 3 1 1 1 1\n2\n4 7 {reset}\n{output}\n6 2 5\n')
                clauses = c.frame(0)+c.frame(1)
                for bits in itertools.product((False, True), repeat=7):
                    model = dict(enumerate(bits, 1))
                    cnf = all(any(model[abs(v)] != (v < 0) for v in cl) for cl in clauses)
                    valid = bits[0]
                    state = bits[2] if reset == 4 else bool(reset)
                    for t in range(2):
                        x = bits[1+3*t]; gate = x and not state
                        valid = valid and bits[2+3*t] == state and bits[3+3*t] == gate
                        state = not gate
                    self.assertEqual(cnf, valid)
                    if cnf:
                        outputs = c.simulate(model, 1)
                        for t, value in enumerate(outputs):
                            v = c.literal(output, t)
                            self.assertEqual(value, model[abs(v)] != (v < 0))

    def test_reject_unsupported_or_malformed(self):
        for text in ('aag 0 0 0 1 0 1\n0\n', 'aag 1 1 0 1 0\n3\n0\n',
                     'aag 1 0 0 1 1\n2\n2 2 0\n', 'aag 1 0 1 1 0\n2 0 3\n2\n',
                     'aag 0 0 0 0 0\n', 'aag 0 0 0 1 0\n2\n'):
            with self.assertRaises(ValueError): Circuit.read(text)

    def test_simulation_detects_mutated_gate_and_latch(self):
        c = Circuit.read('aag 3 1 1 1 1\n2\n4 7 0\n6\n6 2 5\n')
        model = {1: True, 2: True, 3: False, 4: True}
        self.assertEqual(c.simulate(model, 0), [True])
        for v in (3, 4):
            bad = dict(model); bad[v] = not bad[v]
            with self.assertRaises(ValueError): c.simulate(bad, 0)


if __name__ == '__main__': unittest.main()
