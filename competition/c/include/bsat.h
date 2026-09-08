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
#ifdef __cplusplus
}
#endif
#endif
