/* Public-ABI recovery pattern. Single owner, bounded retained permanent input.
   This log survives replacement of a solver handle, not death of this process.
   Persist accepted input outside the worker process for crash/OOM-kill recovery.
   A failed solve returns UNKNOWN; recovery is explicit and never silently
   retries a query beyond its budget. Assumptions are never appended to the log. */
#ifndef BSAT_RECOVERABLE_SERVICE_H
#define BSAT_RECOVERABLE_SERVICE_H
#include "bsat.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct recoverable_service {
    bsat *worker;
    int *log;
    size_t used, capacity;
    uint64_t journal_quota, maximum_journal_quota, generation;
    int result;
    void *cancel_state;
    int (*cancel)(void *);
} recoverable_service;

/* Application-level bound, intentionally smaller than the solver's namespace. */
#define RECOVERY_MAX_VAR 1000000

static void
recovery_discard(recoverable_service *s)
{
    bsat_destroy(s->worker);
    s->worker = NULL;
    s->result = BSAT_UNKNOWN;
}

static int
recovery_init(recoverable_service *s, size_t log_bytes, uint64_t maximum_quota)
{
    memset(s, 0, sizeof *s);
    if (log_bytes < sizeof(int) || !maximum_quota) return 0;
    s->capacity = log_bytes / sizeof(int);
    s->log = malloc(s->capacity * sizeof(int));
    if (!s->log) return 0;
    s->maximum_journal_quota = s->journal_quota = maximum_quota;
    return 1;
}

static void
recovery_destroy(recoverable_service *s)
{
    recovery_discard(s);
    free(s->log);
    memset(s, 0, sizeof *s);
}

static int
recovery_stopped(recoverable_service *s)
{
    return s->cancel && s->cancel(s->cancel_state);
}

static void
recovery_set_cancel(recoverable_service *s, void *state, int (*callback)(void *))
{
    s->cancel_state = state;
    s->cancel = callback;
    if (s->worker) bsat_set_terminate(s->worker, state, callback);
}

static int
recovery_rebuild(recoverable_service *s)
{
    /* Reclaim the old worker first: recovery does not double its live memory. */
    recovery_discard(s);
    if (!s->log || !s->journal_quota || s->journal_quota > s->maximum_journal_quota ||
        recovery_stopped(s) || s->generation == UINT64_MAX)
        return 0;
    bsat *fresh = bsat_create(BSAT_ABI_VERSION, BSAT_REUSE_LEARNTS | BSAT_CERTIFICATES);

    if (!fresh) return 0;
    if (!bsat_set_journal_limit(fresh, s->journal_quota) || !bsat_set_query_limits(fresh, 5, 0, 0))
        goto failed;
    bsat_set_terminate(fresh, s->cancel_state, s->cancel);
    for (size_t at = 0; at < s->used;) {
        size_t count = (size_t)s->log[at++];

        if (recovery_stopped(s) || !bsat_add_clause(fresh, s->log + at, count)) goto failed;
        at += count;
    }
    if (recovery_stopped(s)) goto failed;
    s->worker = fresh;
    ++s->generation;
    return 1;
failed:
    bsat_destroy(fresh);
    return 0;
}

static int
recovery_add(recoverable_service *s, const int *lits, size_t count)
{
    if (!s->log || count > RECOVERY_MAX_VAR || (count && !lits) || s->used >= s->capacity ||
        count > s->capacity - s->used - 1)
        return 0;
    for (size_t i = 0; i < count; ++i)
        if (!lits[i] || lits[i] < -RECOVERY_MAX_VAR || lits[i] > RECOVERY_MAX_VAR) return 0;
    /* Commit to the owner log before forwarding. Return 1 means recorded even
       if forwarding fails; the complete accepted prefix is replayable. */
    s->log[s->used++] = (int)count;
    if (count) memcpy(s->log + s->used, lits, count * sizeof(int));
    s->used += count;
    s->result = BSAT_UNKNOWN;
    if (s->worker && !bsat_add_clause(s->worker, lits, count)) recovery_discard(s);
    return 1;
}

static int
recovery_solve(recoverable_service *s, const int *assumptions, size_t count)
{
    s->result = BSAT_UNKNOWN;
    if (!s->worker) return BSAT_UNKNOWN;
    int result = bsat_solve(s->worker, assumptions, count);

    if (bsat_error(s->worker)) {
        recovery_discard(s);
        return BSAT_UNKNOWN;
    }
    s->result = result;
    return result;
}

static int
recovery_value(const recoverable_service *s, int lit)
{
    return s->result == BSAT_SAT ? bsat_value(s->worker, lit) : 0;
}
#endif
