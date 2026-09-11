/* POSIX example: parent owns accepted input; replace a killed child and replay.
   Deliberately kills the first child during replay. Never publish an answer
   before a clean child exit. This is a lifecycle demonstration, not an RPC
   server: fork only from a single-threaded owner and add OS limits for service use. */
#include "recoverable_service.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    unsigned calls, stop;
    int kill;
} interruption;

static int
interrupt(void *state)
{
    interruption *p = state;

    if (++p->calls != p->stop) return 0;
    if (p->kill) raise(SIGKILL);
    return 1;
}

static int
worker(recoverable_service *s, const char *directory, unsigned attempt)
{
    interruption stop = {0, 5, attempt == 0};

    recovery_set_cancel(s, &stop, interrupt);
    if (recovery_rebuild(s) || s->worker || recovery_value(s, 1)) return 2;
    /* A cancelled replay leaves no partial worker; clear and retry. */
    stop.stop = 0;
    s->journal_quota = 1;
    if (!recovery_rebuild(s)) return 2;
    int assumption = -1;

    if (recovery_solve(s, &assumption, 1) != BSAT_UNKNOWN || s->worker) return 2;
    s->journal_quota = s->maximum_journal_quota;
    if (!recovery_rebuild(s)) return 2;
    for (unsigned q = 0; q < 2; ++q) {
        int result = recovery_solve(s, q ? NULL : &assumption, q ? 0 : 1);

        if (result != (q ? BSAT_SAT : BSAT_UNSAT)) return 2;
        char cnf[1024], proof[1024], model[1024];

        if (snprintf(cnf, sizeof cnf, "%s/%u-%u.cnf", directory, attempt, q) >= (int)sizeof cnf ||
            snprintf(proof, sizeof proof, "%s/%u-%u.drat", directory, attempt, q) >=
                (int)sizeof proof ||
            snprintf(model, sizeof model, "%s/%u-%u.model", directory, attempt, q) >=
                (int)sizeof model)
            return 2;
        if (!bsat_export_query(s->worker, cnf, proof)) return 2;
        if (q) {
            FILE *f = fopen(model, "wx");

            if (!f) return 2;
            int okay = fputs("v ", f) >= 0;

            for (int v = 1; v <= 7; ++v) {
                if (recovery_value(s, v) != v) okay = 0;
                if (fprintf(f, "%d ", recovery_value(s, v)) < 0) okay = 0;
            }
            if (fputs("0\n", f) < 0) okay = 0;
            if (fclose(f)) okay = 0;
            if (!okay) return 2;
        }
    }
    recovery_destroy(s);
    return 0;
}

int
main(int argc, char **argv)
{
    if (argc != 2) return 2;
    recoverable_service owner;

    if (!recovery_init(&owner, 8192, 65536)) return 2;
    /* Exactly the all-true assignment on six variables is permitted. */
    for (unsigned bits = 0; bits < 63; ++bits) {
        int c[6];

        for (unsigned v = 0; v < 6; ++v)
            c[v] = (bits & (1u << v)) ? -(int)(v + 1) : (int)(v + 1);
        if (!recovery_add(&owner, c, 6)) {
            recovery_destroy(&owner);
            return 2;
        }
    }
    for (unsigned attempt = 0; attempt < 2; ++attempt) {
        if (attempt) {
            int unit = 7;

            if (!recovery_add(&owner, &unit, 1)) return 2;
        }
        pid_t child = fork();

        if (child < 0) {
            recovery_destroy(&owner);
            return 2;
        }
        if (!child) _exit(worker(&owner, argv[1], attempt));
        int status;
        pid_t waited;

        do
            waited = waitpid(child, &status, 0);
        while (waited < 0 && errno == EINTR);
        if (waited != child) {
            recovery_destroy(&owner);
            return 2;
        }
        if (!attempt) {
            if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGKILL) {
                recovery_destroy(&owner);
                return 2;
            }
        } else if (!WIFEXITED(status) || WEXITSTATUS(status)) {
            recovery_destroy(&owner);
            return 2;
        }
    }
    recovery_destroy(&owner);
    puts("{\"complete\":true,\"killed_workers\":1,\"accepted_attempt\":1,\"cancelled_replays\":1,"
         "\"quota_recoveries\":1,\"queries\":2}");
    return 0;
}
