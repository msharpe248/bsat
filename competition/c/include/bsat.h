#ifndef BSAT_PUBLIC_H
#define BSAT_PUBLIC_H
#include <stddef.h>
#include <stdint.h>
#if defined(__GNUC__)
#define BSAT_API __attribute__((visibility("default")))
#else
#define BSAT_API
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define BSAT_ABI_VERSION 1u
#define BSAT_REUSE_LEARNTS 1u
#define BSAT_CERTIFICATES 2u
/* Optional bounded RUP failed-literal probing. Requires BSAT_CERTIFICATES;
   preserves original variables and future assumptions. Workload-dependent cost. */
#define BSAT_CERTIFIED_PROBING 4u
#define BSAT_UNKNOWN 0
#define BSAT_SAT 10
#define BSAT_UNSAT 20
typedef struct bsat bsat;
BSAT_API uint32_t bsat_abi_version(void);
/* Unknown ABI/flags or allocation failure returns NULL. */
BSAT_API bsat *bsat_create(uint32_t abi, uint32_t flags);
BSAT_API void bsat_destroy(bsat *s);
/* Set before first input/solve; 0 means unlimited. Returns 1 on success. */
BSAT_API int bsat_set_limits(bsat *s, double cpu_seconds, uint32_t conflicts, uint32_t decisions);
/* Update limits between queries, including after input. Preserves the latest
   model/core. Limits apply separately to each subsequent solve; 0 is unlimited. */
BSAT_API int bsat_set_query_limits(bsat *s, double cpu_seconds, uint32_t conflicts, uint32_t decisions);
/* Cooperative per-query/checkpoint wall deadline and owned-core capacity
   ceiling. Zero disables a limit. Capacity excludes transient allocations,
   allocator/stdio overhead and RSS; polling can overshoot. For hard bounds use
   an isolated process with OS limits. Input exceeding capacity poisons the
   handle; query exhaustion returns UNKNOWN and permits retry after raising limits. */
BSAT_API int bsat_set_service_limits(bsat *s, double wall_seconds, uint64_t owned_bytes);
#define BSAT_SERVICE_NONE 0
#define BSAT_SERVICE_WALL 1
#define BSAT_SERVICE_CAPACITY 2
BSAT_API int bsat_service_limit_hit(const bsat *s);
/* Snapshot of the latest query. CPU excludes input and later idle time. Owned
   capacity is an estimate, not RSS. Getter accepts sizeof(bsat_stats_v1) or more. */
typedef struct bsat_stats_v1 {
    uint32_t version;
    int32_t result;
    uint64_t conflicts, decisions, propagations, reused_preparations;
    uint64_t owned_capacity_bytes;
    double cpu_seconds;
} bsat_stats_v1;
BSAT_API int bsat_get_stats(const bsat *s, bsat_stats_v1 *out, size_t size);
/* DIMACS signed literals, no zero terminator. Variables grow automatically.
   Return 1 means accepted, including an empty/contradictory clause. */
BSAT_API int bsat_add_clause(bsat *s, const int *lits, size_t count);
/* Assumptions must reference variables already introduced by clauses. */
BSAT_API int bsat_solve(bsat *s, const int *assumptions, size_t count);
/* BSAT_CERTIFICATES handles: export the latest conclusive query's exact CNF
   (permanent input plus assumption units) and binary proof prefix. UNSAT adds a
   query-local empty clause. Does not solve again. Both paths must be new files;
   failure returns 0 and can leave incomplete outputs. Check independently. */
BSAT_API int bsat_export_query(bsat *s, const char *cnf_path, const char *proof_path);
/* Certified handles: exact accepted journal bytes, also readable after error.
   Zero limit means unlimited; a limit below existing bytes is rejected without
   changing state. Exhaustion poisons the handle and cannot produce an answer.
   The limit covers the journal, not CNF/export copies or filesystem overhead. */
BSAT_API int bsat_get_journal_bytes(const bsat *s, uint64_t *bytes);
BSAT_API int bsat_set_journal_limit(bsat *s, uint64_t bytes);
/* Rebuild permanent input and discard all learning. Certified handles start a
   new empty journal only after successful rebuild. Invalidates answer/stats.
   Respects CPU limit and cancellation; return 0 means not completed. A failed
   handle cannot be recovered. Checkpoint proactively before exhausting quotas. */
BSAT_API int bsat_checkpoint(bsat *s);
/* Latest SAT: returns lit if true, -lit if false, 0 if unavailable. */
BSAT_API int bsat_value(const bsat *s, int lit);
/* Latest UNSAT: membership in a sufficient, possibly nonminimal assumption core. */
BSAT_API int bsat_failed(const bsat *s, int assumption);
/* Persistent error; discard a failed instance. NULL is an error. */
BSAT_API int bsat_error(const bsat *s);
/* Runs on solving thread. Nonzero requests UNKNOWN. State is borrowed.
   Never reenter this handle; an external thread may update its own atomic flag.
   Calls on one handle must be serialized; distinct handles are independent. */
BSAT_API void bsat_set_terminate(bsat *s, void *state, int (*callback)(void *));

/* --- Extended ABI v1.1 additive capabilities (100% ABI-v1 compatible) --- */

/* Protect/unprotect a variable from destructive simplification (BVE, SCC, BCE, sym).
 * Accepts signed DIMACS literal (nonzero). Returns 1 on success, 0 on error. */
BSAT_API int bsat_freeze(bsat *s, int lit);
BSAT_API int bsat_melt(bsat *s, int lit);
BSAT_API int bsat_is_frozen(const bsat *s, int lit);

/* Configure multi-threaded one-shot portfolio solving (1..64 threads) for
 * non-assumption, non-certified calls. Must be called before first solve. */
BSAT_API int bsat_set_threads(bsat *s, uint32_t threads);

/* Enable one-shot DRAT or native LRAT proof logging to `path` before first input/solve.
 * Incompatible with BSAT_CERTIFICATES journals and threads > 1. Returns 1 on success. */
BSAT_API int bsat_set_proof(bsat *s, const char *path, int binary);
BSAT_API int bsat_set_lrat(bsat *s, const char *path, int binary);

/* Add native XOR clause (lits[0] XOR ... XOR lits[count-1] == (rhs != 0)),
 * unweighted cardinality constraint (sum lits <= k), or weighted PB constraint
 * (sum weights[i] * lits[i] <= rhs). Returns 1 on success. */
BSAT_API int bsat_add_xor(bsat *s, const int *lits, size_t count, int rhs);
BSAT_API int bsat_add_atmost(bsat *s, const int *lits, size_t count, uint32_t k);
BSAT_API int bsat_add_pb_le(bsat *s, const int *lits, const uint64_t *weights,
                            size_t count, uint64_t rhs);

/* Compute exact global backbone of the loaded formula. Returns BSAT_SAT (10)
 * when backbone is computed, BSAT_UNSAT (20) if formula is UNSAT, or BSAT_UNKNOWN (0).
 * Writes up to `cap` signed DIMACS literals into `out_lits` and sets `*out_count`
 * to the total backbone size. */
BSAT_API int bsat_backbone(bsat *s, int *out_lits, size_t cap, size_t *out_count);

/* Extract a Minimal Unsatisfiable Subset (MUS) of 0-based input clause indices.
 * Returns BSAT_UNSAT (20) when MUS is found, BSAT_SAT (10) if formula is SAT,
 * or BSAT_UNKNOWN (0). Writes up to `cap` clause indices into `out_clause_ids`
 * and sets `*out_count` to the total MUS size. */
BSAT_API int bsat_mus(bsat *s, uint32_t *out_clause_ids, size_t cap, size_t *out_count);
#ifdef __cplusplus
}
#endif
#endif
