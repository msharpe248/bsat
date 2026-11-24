/*********************************************************************
 * BSAT Competition Solver - Dynamic GF(2) Gaussian Elimination
 *
 * Extracts exact 2..6-variable XOR parity constraints from CNF clauses,
 * runs root-level RREF for units/equivalences/UNSAT, and maintains
 * Bird / Han-Jiang two-watched-column GF(2) matrices during CDCL search
 * with zero-undo backtracking and 1-UIP-compatible reason clauses.
 *********************************************************************/

#ifndef BSAT_GAUSS_H
#define BSAT_GAUSS_H

#include "types.h"
#include "arena.h"

struct Solver;

typedef struct GaussState GaussState;

/* Free all Gaussian elimination state owned by the solver. */
void gauss_free(struct Solver *s);

/* True if variable v belongs to an active in-search GF(2) matrix
 * and should be protected from destructive BVE. */
bool gauss_var_protected(const struct Solver *s, Var v);

/* Extract 2..6-var XOR constraints from live irredundant clauses, run
 * root-level RREF to derive UNSAT/units/equivalences, and (when
 * keep_matrices is true) build live Bird/Han-Jiang matrices for search.
 * Returns false on UNSAT or OOM error. */
bool gauss_extract_and_init(struct Solver *s, bool keep_matrices);

/* Refresh/validate live matrices after inprocessing (e.g. if SCC
 * substituted a matrix variable or root units simplified rows).
 * Returns false on UNSAT or OOM error. */
bool gauss_sync_after_inprocess(struct Solver *s);

/* True if s->gauss has unprocessed trail assignments pending. */
bool gauss_has_work(const struct Solver *s);

/* In-search Bird / Han-Jiang two-watched-column GF(2) propagation.
 * Returns INVALID_CLAUSE if no conflict (check s->error / s->result),
 * or an arena CRef for a conflict clause with its two highest-level
 * literals placed at [0] and [1]. */
CRef gauss_propagate(struct Solver *s);

/* Notify Gaussian state of backtracking to trail position `trail_pos`.
 * Zero matrix undo required; simply clamps qhead and frees unlocked
 * temporary reason clauses. */
void gauss_backtrack(struct Solver *s, uint32_t trail_pos);

/* Remap temporary Gauss reason CRefs across arena garbage collection. */
void gauss_relocate_clauses(struct Solver *s, const Arena *old_arena);

/* True if `cr` is an active temporary Gauss reason/conflict clause. */
bool gauss_is_temp_clause(const struct Solver *s, CRef cr);

/* In --proof mode, lazily emit the linear Tseitin XOR-chain DRAT proof for a
 * temporary Gauss reason/conflict clause `cr` the first time conflict analysis
 * or minimization resolves on it. No-op in non-proof mode or for normal clauses. */
void gauss_ensure_clause_proved(struct Solver *s, CRef cr);

#endif /* BSAT_GAUSS_H */
