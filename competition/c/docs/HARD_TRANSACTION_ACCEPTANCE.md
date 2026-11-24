# Hard certified transaction acceptance — 2026-09-09

[Run 34389473487](https://github.com/msharpe248/bsat/actions/runs/34389473487)
passes on hosted Ubuntu with runtime c6b4ac8 and harness revision 5fe7630.
All 66 release C executables pass before acceptance, as does the focused
interrupted-query/checkpoint regression. The full report is complete: two normal,
two hard, one deliberately interrupted, five injected failures and four external
replay transactions (storage recovery happens inside its worker).

The envelope was frozen before running: worker/checker cgroup 4 GiB memory, zero
swap, 64 tasks; private 1 GiB tmpfs; 180 wall / 120 aggregate CPU seconds per
transaction; query CPU 60 seconds; journal 256 MiB; checker stage 60 seconds with
2048/512 MiB heap/stack. The lightweight supervisor and persistent source/results
remain outside that envelope. Limits are provisional, not a customer's SLO.

| Complete transaction | Wall seconds | Aggregate CPU seconds | Peak cgroup memory, MiB | Sampled peak tmpfs, MiB |
|---|---:|---:|---:|---:|
| gen23, first / second | 2.399 / 2.346 | 2.348 / 2.330 | 673.53 / 674.98 | 10.01 / 10.01 |
| cal3, first / second | 93.888 / 94.351 | 93.849 / 94.311 | 2731.38 / 2731.86 | 648.05 / 648.05 |

Each normal/hard transaction freshly loads the permanent formula, checks positive
UNSAT then negative SAT, and checkpoints after each answer. This differs from a
warm retained BMC history. First cal3 positive solve takes 52.341 wall seconds;
independent validation takes 41.055 seconds. Total cost includes loading, parsing,
export, checking and checkpointing. Journal/storage/checker cost is substantial.
Cgroup memory includes charged tmpfs pages; storage and memory are not additive
independent allowances. The 20 ms storage samples are lower bounds on peak use.

The tiny first-query budget intentionally produces unaccepted UNKNOWN; restoring
the normal budget permits checkpoint and independently checked later SAT. The
first campaign run 34388896901 exposed a harness error: it attempted checkpoint
with that same one-microsecond CPU budget. The API correctly bounds rebuilding.
The corrected sequence restores the normal budget before checkpoint. The failed
run is archived, and no outer resource gate was relaxed for the successful retry.

Worker SIGKILL, 64 MiB OOM, one-second CPU, one-second wall and full tmpfs all
recover to checked answers. Failure plus replay takes 2.708–3.406 wall seconds;
CPU termination is observed at 1.020 CPU seconds and wall termination at 1.017
wall seconds. Polling can overshoot and failed attempts are never accepted.

Evidence: `benchmark_results/hard-acceptance-final-20260909/`, with initial failed
run in `hard-acceptance-first-20260909/`. This is public-workload hosted acceptance.
Actual application traces, durable parent/power-loss recovery, deployment host
and workload-specific tail-latency/containment requirements remain unvalidated.
