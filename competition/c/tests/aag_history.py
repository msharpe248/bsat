"""Strict safety-only AAG reader, incremental Tseitin unrolling and simulation.

Binary decoding is delegated to the pinned upstream AIGER converter. Reject
extended constraints/justice/fairness rather than silently changing a property.
One legacy output is interpreted as the bad-state predicate at each frame.
"""
from dataclasses import dataclass


@dataclass
class Circuit:
    maximum: int
    inputs: list
    latches: list
    output: int
    gates: list

    @classmethod
    def read(cls, text):
        lines = iter(text.splitlines())
        header = next(lines, '').split()
        if len(header) != 6 or header[0] != 'aag':
            raise ValueError('requires legacy safety AAG with one output')
        m, ni, nl, no, na = map(int, header[1:])
        if min(m, ni, nl, na) < 0 or no != 1 or m != ni + nl + na:
            raise ValueError('invalid or unsupported AAG dimensions')
        def row(counts):
            values = list(map(int, next(lines, '').split()))
            if len(values) not in counts or any(v < 0 or v > 2*m+1 for v in values):
                raise ValueError('invalid AAG literal row')
            return values
        inputs = [row((1,))[0] for _ in range(ni)]
        latches = []
        for _ in range(nl):
            r = row((2, 3)); current, nxt = r[:2]; reset = r[2] if len(r) == 3 else 0
            if reset not in (0, 1, current): raise ValueError('unsupported latch reset')
            latches.append((current, nxt, reset))
        output = row((1,))[0]
        gates = [tuple(row((3,))) for _ in range(na)]
        definitions = inputs + [l[0] for l in latches] + [g[0] for g in gates]
        if set(definitions) != set(range(2, 2*m+1, 2)) or len(set(definitions)) != len(definitions):
            raise ValueError('duplicate, odd or missing signal definition')
        available = {0, *inputs, *(l[0] for l in latches)}
        for lhs, a, b in gates:
            if (a & ~1) not in available or (b & ~1) not in available:
                raise ValueError('non-topological or cyclic gate')
            available.add(lhs)
        for line in lines:
            if line == 'c': break
            if not line: continue
            symbol, sep, _ = line.partition(' ')
            if not sep or len(symbol) < 2 or symbol[0] not in 'ilo' or not symbol[1:].isdigit():
                raise ValueError('invalid trailing symbol')
            if int(symbol[1:]) >= {'i': ni, 'l': nl, 'o': no}[symbol[0]]:
                raise ValueError('symbol index out of range')
        return cls(m, inputs, latches, output, gates)

    def literal(self, aig, frame):
        if aig < 2: return 1 if aig else -1
        v = 1 + frame*self.maximum + aig//2
        return -v if aig & 1 else v

    def frame(self, t):
        if t < 0: raise ValueError('negative frame')
        lit = lambda x: self.literal(x, t)
        clauses = [[1]] if t == 0 else []
        # Declare even unused inputs through the public API.
        last = 1+(t+1)*self.maximum
        clauses.append([last, -last])
        for current, nxt, reset in self.latches:
            if t == 0:
                if reset != current: clauses.append([lit(current) if reset else -lit(current)])
            else:
                a, b = lit(current), self.literal(nxt, t-1)
                clauses.extend([[-a, b], [a, -b]])
        for z, a, b in self.gates:
            z, a, b = lit(z), lit(a), lit(b)
            clauses.extend([[-z, a], [-z, b], [z, -a, -b]])
        return clauses

    def simulate(self, assignment, depth):
        """Evaluate transitions from primary inputs; never use CNF gate values."""
        read = lambda signal, t: assignment[abs(self.literal(signal, t))] != bool(self.literal(signal, t) < 0)
        state = {c: (read(c, 0) if reset == c else bool(reset)) for c, _, reset in self.latches}
        outputs = []
        for t in range(depth+1):
            values = {0: False, **state, **{i: read(i, t) for i in self.inputs}}
            def value(signal): return values[signal & ~1] != bool(signal & 1)
            for z, a, b in self.gates: values[z] = value(a) and value(b)
            for signal in values:
                if signal and values[signal] != read(signal, t):
                    raise ValueError('model disagrees with independent circuit simulation')
            outputs.append(value(self.output))
            state = {c: value(nxt) for c, nxt, _ in self.latches}
        return outputs
