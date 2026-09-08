/* Force-include only in isolated fault-test core objects, never production. */
#include <stdlib.h>
void *fault_malloc(size_t);
void *fault_calloc(size_t, size_t);
void *fault_realloc(void *, size_t);
#define malloc fault_malloc
#define calloc fault_calloc
#define realloc fault_realloc
