# Fresh competitiveness baseline — 2026-09-09

[Linux run 34388664761](https://github.com/msharpe248/bsat/actions/runs/34388664761)
passes the frozen campaign at revision 3060825 (runtime c6b4ac8). Four newly
selected circuit files, two repetitions, retained certified flags 3 and depths
0,2,4,8 yield 64 queries. Both retained solvers receive ten query CPU seconds;
reference callbacks are cooperative and their measured overshoot is retained.
Fresh Kissat and exact model/proof checking are outside solve timing.
Within each query BSAT runs before CaDiCaL in both repetitions; solver order was
not reversed. Use this frozen protocol limitation when interpreting small timings.

BSAT checks 64/64 within budget, CaDiCaL 62/64. Mean query CPU PAR2 is 0.110940
versus 0.753183 seconds, respectively. This is a baseline comparison between
solvers, not a before/after BSAT optimization result or a competition ranking.

| Depth-8 positive query | BSAT CPU seconds | CaDiCaL CPU seconds |
|---|---:|---:|
| Protocol | 0.0245 / 0.0242 | 0.0193 / 0.0187 |
| cal162 | 0.2365 / 0.2349 | 3.4197 / 3.2941 |
| Arithmetic | 0.0046 / 0.0045 | 0.0002 / 0.0002 |
| PicoRV32 | 2.5789 / 2.6595 | UNKNOWN at 10.3578 / 10.3617 |

All BSAT conclusive answers independently check. Inputs grow to 464,293 variables
and 1,297,035 clauses. Maximum reported BSAT owned capacity is 173,435,155 bytes;
maximum journal size is 4,527,796 bytes. Neither is peak RSS. Both solvers share
the Python process, so this harness does not provide isolated per-solver peak RSS.
Worker/checker cgroup memory is measured separately in complete acceptance.

The full query-and-validation total is 278.796 wall seconds, including both
retained solvers and fresh reference/proof checks, excluding encoding/additions.
It must not be described as BSAT-only latency. Most queries are easy; the important
new distinctions are cal162 latency and the PicoRV32 checked completion. These
files were selected before running, but are drawn from the same upstream collection
and related families as earlier data. They are now development evidence, not future
untouched holdouts. Public BMC histories are not customer application traces.

Evidence: benchmark_results/fresh-linux-20260909/ includes all queries, strict
summary, reference-extension parity, hashes, compiler and CPU metadata. The pinned
manifest records selection seed, size filter, pool exception and exact bytes.


## Isolated resource follow-up

[Run 34390131562](https://github.com/msharpe248/bsat/actions/runs/34390131562)
passes one fresh process per solver/family, using the same exact independently
checked query contexts. SAT models are checked again. BSAT answers all 32 queries;
CaDiCaL again leaves the PicoRV32 positive depth-8 query unfinished.

| Family | BSAT embedding peak RSS, MiB | CaDiCaL embedding peak RSS, MiB |
|---|---:|---:|
| Protocol | 214.75 | 244.32 |
| cal162 | 339.96 | 457.50 |
| Arithmetic | 346.37 | 344.66 |
| PicoRV32 | 535.55 | 676.24 |

These are actual isolated process high-water marks, including the Python driver,
CNF encoding/hash buffers, model validation and one native solver. They exclude
the other solver, external proof checking and parent. They are not native-only
solver memory and are one observation per family, not deployment tail bounds.
BSAT uses certified retained flags 3 with its journal; CaDiCaL uses the pinned
normal IPASIR policy. The separate hard transaction acceptance includes worker,
checker and tmpfs charges. Evidence: `benchmark_results/retained-resources-linux-20260909/`.
