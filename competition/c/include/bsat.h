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
/* DIMACS signed literals, no zero terminator. Variables grow automatically.
   Return 1 means accepted, including an empty/contradictory clause. */
BSAT_API int bsat_add_clause(bsat *s, const int *lits, size_t count);
/* Assumptions must reference variables already introduced by clauses. */
BSAT_API int bsat_solve(bsat *s, const int *assumptions, size_t count);
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
