#ifndef BSAT_IPASIR_H
#define BSAT_IPASIR_H
#include "bsat.h"
#ifdef __cplusplus
extern "C" {
#endif
/* IPASIR 1.0. Calls on each instance must be serialized; callbacks must not
   reenter that instance. Allocation or invalid-input errors yield UNKNOWN. */
BSAT_API const char *ipasir_signature(void);
BSAT_API void *ipasir_init(void);
BSAT_API void ipasir_release(void *solver);
BSAT_API void ipasir_add(void *solver, int lit_or_zero);
BSAT_API void ipasir_assume(void *solver, int lit);
BSAT_API int ipasir_solve(void *solver);
BSAT_API int ipasir_val(void *solver, int lit);
BSAT_API int ipasir_failed(void *solver, int lit);
BSAT_API void ipasir_set_terminate(void *solver, void *state, int (*terminate)(void *));
BSAT_API void ipasir_set_learn(void *solver, void *state, int max_length,
                               void (*learn)(void *, int *));
#ifdef __cplusplus
}
#endif
#endif
