/*********************************************************************
 * BSAT Competition Solver - Multi-Core Clause-Sharing Portfolio
 *
 * Runs N diversified, completely isolated Solver instances across POSIX
 * threads with a TSan-clean mutex-guarded shared clause ring buffer
 * (exchanging short logically-entailed clauses of size <= 4) and instant
 * atomic cancellation as soon as any worker proves SAT or UNSAT.
 *********************************************************************/

#ifndef BSAT_PORTFOLIO_H
#define BSAT_PORTFOLIO_H

#include "types.h"
#include <stdint.h>

struct Solver;

/* Solve `master`'s permanent formula using `num_threads` diversified
 * parallel workers with short learned-clause sharing (size <= 4).
 * When num_threads <= 1, falls back directly to solver_solve(master).
 * On SAT, copies the verified model into master->values[1..master->num_vars].
 * Not supported with DRAT proof files (returns UNDEF if proof_file is set). */
lbool solver_solve_parallel(struct Solver *master, uint32_t num_threads);

#endif /* BSAT_PORTFOLIO_H */
