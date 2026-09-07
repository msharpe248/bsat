# Restart strategies and clause management

This describes the C implementation in `src/solver.c`, reviewed September 2026.
Performance depends on the input; selecting a restart strategy does not guarantee
that an instance finishes within a resource limit.

## Restart policies

The default is LBD-based EMA restarting. Every learned clause contributes its
LBD to fast and slow exponential moving averages. The update is
`average = alpha * old_average + (1 - alpha) * lbd`, with fast alpha 0.8 and
slow alpha 0.9999. Both averages start at the first observed LBD.

A restart requires at least 100 conflicts **since the previous restart** and
`fast_average * K > slow_average`, where K defaults to 0.8. Equality does not
trigger a restart. Thus the recent average must exceed 1.25 times the slow
average at the default K. The averages survive a restart; the interval counter
resets. `--glucose-min-conflicts` changes the interval minimum in both EMA and
sliding-window modes. `--glucose-k` also applies to both modes.

`--glucose-restart-avg` uses the most recent 50 learned-clause LBDs and the
cumulative average instead of EMAs, with the same inequality and interval gate.
The recent window is cleared at a restart and must fill again before another
restart is eligible. Raising K towards 1 makes the quality test easier to meet.

`--luby-restart` uses intervals proportional to
`1, 1, 2, 1, 1, 2, 4, ...`; `--luby-unit` defaults to 100 conflicts.
Geometric restarting is available through solver options by disabling both
Luby and Glucose, starting at `restart_first` and multiplying by `restart_inc`.
`--no-restarts` disables restart decisions, including alternating-mode switches.

The experimental `--alternating` policy switches between focused and stable
modes at doubling conflict limits. A mode switch requests a root restart;
stable mode uses Luby intervals scaled by ten. An explicitly selected Luby
policy also applies in focused mode. See the current implementation before
combining experimental policies.

Restart requests backtrack to decision level zero by default. Experimental
`--reuse-trail` can retain a priority-selected decision prefix with heap or
VMTF ordering; assumption solves and alternating mode retain root restarts.
See [the reuse experiment](../RESTART_REUSE_EXPERIMENT.md) for its mixed results.
There is currently no trail
length postponement in the search loop: the legacy `restart_postpone` options
field is not consulted. Earlier versions of this guide described postponement
and different threshold formulas that do not match the implementation.

## Clause management

Database reduction runs every 2,000 conflicts by default. Binary clauses,
clauses with LBD at most the configured glue threshold (default 2), and clauses
currently serving as assignment reasons are protected. Optional used-clause
protection also exempts recently used clauses with LBD at most 6 for that pass.

Other clauses are sorted by LBD and activity. `--reduce-fraction` is the fraction
of eligible clauses to **keep** (default 0.5), subject to `--max-lbd`. Remaining
clauses are lazily deleted. Garbage collection compacts the arena once wasted
storage reaches one quarter of arena size, remapping watches and reasons.
Watch lists are contiguous dynamic arrays; they are not linked lists.

## Evidence and tuning

The [restart minimum experiment](../RESTART_MINIMUM_EXPERIMENT.md) compares
100, 10 and 1 conflicts with certified results and explicit resource limits.
Use `tests/benchmark.py` for comparisons with model/proof verification and
process timing. The historical `tests/compare_restart_strategies.sh` does not
verify answers, uses obsolete labels, and is not a production performance gate.

Examples:

```sh
./bin/bsat --glucose-min-conflicts 10 input.cnf
./bin/bsat --glucose-restart-avg --glucose-k 0.9 input.cnf
./bin/bsat --luby-restart --luby-unit 100 input.cnf
./bin/bsat --reduce-fraction 0.7 --reduce-interval 3000 input.cnf
```

These are configuration examples, not recommendations for improved speed.
