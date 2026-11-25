/*********************************************************************
 * BSAT Competition Solver - Native LRAT Proof Logger & Verifier
 *
 * Emits linearly checkable text (--lrat) and binary (--binary-lrat)
 * LRAT proofs directly during CDCL search with zero overhead when
 * disabled (allocated only when opts.lrat_path != NULL).
 *
 * Binary LRAT encoding follows the standard CaDiCaL / drat-trim /
 * cake_lpr specification:
 *   - Addition: 'a', LEB128(2*id), LEB128(lit)..., 0, LEB128(2*hint)..., 0
 *   - Deletion: 'd', LEB128(2*id)..., 0
 *********************************************************************/

#ifndef BSAT_LRAT_H
#define BSAT_LRAT_H

#include "types.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct Solver Solver;
typedef struct Arena Arena;
typedef struct LratState LratState;

LratState *lrat_create(const char *path, bool binary);
void lrat_free(LratState *lrat);
bool lrat_flush(LratState *lrat);
bool lrat_has_error(const LratState *lrat);
bool lrat_emitted_empty(const LratState *lrat);

/* Original input clause registration (1-to-1 with DIMACS input lines). */
uint64_t lrat_register_original(Solver *s, CRef cr);
void lrat_set_cref_id(Solver *s, CRef cr, uint64_t id);
uint64_t lrat_get_cref_id(const Solver *s, CRef cr);
void lrat_set_unit_id(Solver *s, Var v, uint64_t id);
uint64_t lrat_get_unit_id(const Solver *s, Var v);

/* Emit derived unit clauses for any level-0 literals propagated onto trail[from_trail..]. */
void lrat_sync_level0_units(Solver *s, uint32_t from_trail);

/* Binary-resolution minimization hint helpers (recorded before lrat_emit_learned). */
void lrat_clear_bin_hints(Solver *s);
bool lrat_record_bin_hint(Solver *s, Var removed_var, CRef bin_cr);

/* Emit a conflict-derived learned clause (BEFORE backtracking, while reasons are live).
 * Returns the newly assigned LRAT clause ID (0 on error). */
uint64_t lrat_emit_learned(Solver *s, CRef conflict, const Lit *learnt, uint32_t n);

/* Emit a 2-clause RUP resolvent (e.g. SSR or BVE: cr1 resolves with cr2 -> lits),
 * automatically prepending any level-0 unit hints for stripped false literals. */
uint64_t lrat_emit_pair_rup(Solver *s, const Lit *lits, uint32_t n, CRef cr1, CRef cr2);

/* Emit a root-strengthened clause (old_cr with level-0 falsified literals stripped). */
uint64_t lrat_emit_root_strengthen(Solver *s, CRef old_cr, const Lit *new_lits, uint32_t new_n);

/* Emit the final empty clause from a level-0 falsified clause or conflicting unit pair. */
void lrat_emit_empty_from_conflict(Solver *s, CRef conflict);
void lrat_emit_empty_from_pair(Solver *s, uint64_t id_a, uint64_t id_b);

/* Buffer a clause deletion (by CRef or directly by ID) and remap IDs across arena GC. */
void lrat_buffer_delete(Solver *s, CRef cr);
void lrat_buffer_delete_id(Solver *s, uint64_t id);
void lrat_relocate_clauses(Solver *s, const Arena *old_arena);

/* Independent strict linear-time LRAT verifier (supports text and binary LRAT).
 * Verifies 1-to-1 original clause IDs, live deletion tracking, exact forward
 * hint unit propagation, and empty-clause derivation. */
bool lrat_verify_file(const char *cnf_path, const char *lrat_path, bool binary);

#endif /* BSAT_LRAT_H */
