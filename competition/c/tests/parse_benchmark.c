#ifdef __APPLE__
#define _DARWIN_C_SOURCE 1
#endif
/* Parse-only timing driver; deliberately excluded from the unit-test wildcard. */
#include "../include/dimacs.h"
#include <inttypes.h>
#include <stdio.h>
#include <sys/resource.h>
#include <time.h>

int
main(int argc, char **argv)
{
    if (argc != 2) return 2;
    Solver *s = solver_new();

    if (!s) return 2;
    clock_t start = clock();
    DimacsError status = dimacs_parse_file(s, argv[1]);
    double seconds = (double)(clock() - start) / CLOCKS_PER_SEC;
    /* Hash original literals and clause separators, outside the timed interval. */
    uint64_t fingerprint = UINT64_C(14695981039346656037);

    for (size_t i = 0; i < s->input_size; ++i) {
        fingerprint ^= s->input[i];
        fingerprint *= UINT64_C(1099511628211);
    }
    struct rusage usage;

    if (getrusage(RUSAGE_SELF, &usage)) {
        solver_free(s);
        return 2;
    }
    uint64_t rss = (uint64_t)usage.ru_maxrss;

#ifndef __APPLE__
    rss *= 1024;
#endif
    printf("{\"status\":%d,\"parse_cpu_seconds\":%.6f,\"variables\":%u,"
           "\"clauses\":%" PRIu64 ",\"input_words\":%zu,\"fingerprint\":\"%016" PRIx64 "\","
           "\"peak_rss_bytes\":%" PRIu64 "}\n",
           status, seconds, s->num_vars, (uint64_t)s->input_clauses, s->input_size, fingerprint,
           rss);
    solver_free(s);
    return status == DIMACS_OK ? 0 : 1;
}
