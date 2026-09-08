#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
static size_t calls, cutoff;
static bool hit;
void fuzz_alloc_reset(size_t n) {calls=0;cutoff=n;hit=false;}
bool fuzz_alloc_failed(void) {return hit;}
static bool reject(void) {if(++calls!=cutoff)return false;hit=true;errno=ENOMEM;return true;}
void *fault_malloc(size_t n) {return reject()?NULL:malloc(n);}
void *fault_calloc(size_t n,size_t z) {return reject()?NULL:calloc(n,z);}
void *fault_realloc(void *p,size_t n) {return reject()?NULL:realloc(p,n);}
