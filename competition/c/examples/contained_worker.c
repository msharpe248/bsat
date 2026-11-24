/* POSIX deployment starter kit: durable append-only WAL + OS-contained worker.
 *
 * Demonstrates how an embedding service closes the operational readiness gap
 * around BSAT's public ABI (bsat.h):
 *   1. Parent persists every accepted permanent clause to an on-disk framed,
 *      CRC-checked, fsync'd write-ahead log (clauses.wal) before dispatching
 *      to a forked worker.
 *   2. Each forked worker enforces kernel-level POSIX resource ceilings via
 *      setrlimit (RLIMIT_CPU, RLIMIT_FSIZE, RLIMIT_CORE=0) before instantiating
 *      bsat(BSAT_REUSE_LEARNTS | BSAT_CERTIFICATES) and replaying the WAL.
 *   3. Attempt 0 simulates an abrupt worker crash (SIGKILL) plus a torn partial
 *      record at the tail of clauses.wal.
 *   4. Attempt 1 spawns a fresh OS-contained worker that replays only complete,
 *      CRC-verified WAL records (truncating/ignoring the torn tail), solves
 *      both an UNSAT assumption query and a SAT query, and exports exact
 *      query CNF + binary DRAT certificate + verified model to disk.
 *
 * Usage: contained_worker <output_directory>
 */

#include "bsat.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#define WAL_MAGIC 0x42534154u /* "BSAT" */

typedef struct {
    uint32_t magic;
    uint32_t count;
    uint32_t checksum;
} WalHeader;

static uint32_t wal_checksum(uint32_t count, const int32_t *lits) {
    uint32_t h = 2166136261u ^ count;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t v = (uint32_t)lits[i];
        h ^= v;
        h *= 16777619u;
    }
    return h;
}

static int write_all(int fd, const void *buf, size_t n) {
    const uint8_t *p = (const uint8_t *)buf;
    while (n > 0) {
        ssize_t w = write(fd, p, n);
        if (w < 0) {
            if (errno == EINTR) continue;
            return 0;
        }
        p += (size_t)w;
        n -= (size_t)w;
    }
    return 1;
}

static int read_exact(int fd, void *buf, size_t n, int *eof_clean) {
    uint8_t *p = (uint8_t *)buf;
    size_t got = 0;
    while (got < n) {
        ssize_t r = read(fd, p + got, n - got);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) {
            if (got == 0 && eof_clean) *eof_clean = 1;
            return 0; /* EOF or torn tail */
        }
        got += (size_t)r;
    }
    return 1;
}

static int wal_append_clause(int fd, const int32_t *lits, uint32_t count) {
    WalHeader hdr;
    hdr.magic = WAL_MAGIC;
    hdr.count = count;
    hdr.checksum = wal_checksum(count, lits);
    if (!write_all(fd, &hdr, sizeof hdr)) return 0;
    if (count > 0 && !write_all(fd, lits, (size_t)count * sizeof(int32_t))) return 0;
    if (fsync(fd) != 0) return 0;
    return 1;
}

static int apply_worker_rlimits(rlim_t cpu_sec, rlim_t fsize_bytes) {
    struct rlimit rl;
    rl.rlim_cur = cpu_sec;
    rl.rlim_max = cpu_sec;
    if (setrlimit(RLIMIT_CPU, &rl) != 0) return 0;

    rl.rlim_cur = fsize_bytes;
    rl.rlim_max = fsize_bytes;
    if (setrlimit(RLIMIT_FSIZE, &rl) != 0) return 0;

    rl.rlim_cur = 0;
    rl.rlim_max = 0;
    (void)setrlimit(RLIMIT_CORE, &rl);
    return 1;
}

static int run_contained_worker(const char *wal_path, const char *out_dir, int simulate_kill) {
    if (!apply_worker_rlimits(10, 64u * 1024u * 1024u)) return 10;

    if (simulate_kill) {
        /* Simulate an abrupt worker OOM/SIGKILL mid-flight. */
        raise(SIGKILL);
        return 11;
    }

    int fd = open(wal_path, O_RDONLY);
    if (fd < 0) return 12;

    bsat *solver = bsat_create(BSAT_ABI_VERSION, BSAT_REUSE_LEARNTS | BSAT_CERTIFICATES);
    if (!solver) { close(fd); return 13; }
    if (!bsat_set_journal_limit(solver, 1024u * 1024u) ||
        !bsat_set_query_limits(solver, 5, 0, 0)) {
        bsat_destroy(solver);
        close(fd);
        return 14;
    }

    unsigned replayed = 0;
    unsigned torn_tail = 0;
    for (;;) {
        WalHeader hdr;
        int eof_clean = 0;
        int rr = read_exact(fd, &hdr, sizeof hdr, &eof_clean);
        if (rr < 0) { bsat_destroy(solver); close(fd); return 15; }
        if (rr == 0) {
            if (!eof_clean) torn_tail = 1;
            break;
        }
        if (hdr.magic != WAL_MAGIC || hdr.count > 100000u) {
            torn_tail = 1;
            break;
        }
        int32_t *buf = hdr.count ? (int32_t *)malloc((size_t)hdr.count * sizeof(int32_t)) : NULL;
        if (hdr.count && !buf) { bsat_destroy(solver); close(fd); return 16; }
        int dr = hdr.count ? read_exact(fd, buf, (size_t)hdr.count * sizeof(int32_t), NULL) : 1;
        if (dr != 1 || wal_checksum(hdr.count, buf) != hdr.checksum) {
            free(buf);
            torn_tail = 1;
            break;
        }
        int ok = bsat_add_clause(solver, buf, hdr.count);
        free(buf);
        if (!ok) { bsat_destroy(solver); close(fd); return 17; }
        replayed++;
    }
    close(fd);

    /* Query 0: UNSAT under assumption -1 (all 63 clauses forbid any assignment with v1=false). */
    int assum = -1;
    if (bsat_solve(solver, &assum, 1) != BSAT_UNSAT) {
        bsat_destroy(solver);
        return 18;
    }
    char cnf0[1024], drat0[1024];
    if (snprintf(cnf0, sizeof cnf0, "%s/query-0.cnf", out_dir) >= (int)sizeof cnf0 ||
        snprintf(drat0, sizeof drat0, "%s/query-0.drat", out_dir) >= (int)sizeof drat0 ||
        !bsat_export_query(solver, cnf0, drat0)) {
        bsat_destroy(solver);
        return 19;
    }

    /* Query 1: SAT with no assumptions (unique model: v1..v7 all true). */
    if (bsat_solve(solver, NULL, 0) != BSAT_SAT) {
        bsat_destroy(solver);
        return 20;
    }
    char cnf1[1024], drat1[1024], model1[1024];
    if (snprintf(cnf1, sizeof cnf1, "%s/query-1.cnf", out_dir) >= (int)sizeof cnf1 ||
        snprintf(drat1, sizeof drat1, "%s/query-1.drat", out_dir) >= (int)sizeof drat1 ||
        snprintf(model1, sizeof model1, "%s/query-1.model", out_dir) >= (int)sizeof model1 ||
        !bsat_export_query(solver, cnf1, drat1)) {
        bsat_destroy(solver);
        return 21;
    }
    FILE *mf = fopen(model1, "wb");
    if (!mf) { bsat_destroy(solver); return 22; }
    int mok = (fputs("v ", mf) >= 0);
    for (int v = 1; v <= 7; ++v) {
        int val = bsat_value(solver, v);
        if (val != v) mok = 0;
        if (fprintf(mf, "%d ", val) < 0) mok = 0;
    }
    if (fputs("0\n", mf) < 0) mok = 0;
    if (fclose(mf) != 0) mok = 0;
    bsat_destroy(solver);
    if (!mok) return 23;

    printf("{\"complete\":true,\"wal_replayed_clauses\":%u,\"torn_tail_ignored\":%u,"
           "\"rlimit_enforced\":true,\"killed_workers\":1,\"queries\":2}\n",
           replayed, torn_tail);
    fflush(stdout);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <output_directory>\n", argv[0]);
        return 2;
    }
    const char *out_dir = argv[1];
    char wal_path[1024];
    if (snprintf(wal_path, sizeof wal_path, "%s/clauses.wal", out_dir) >= (int)sizeof wal_path) {
        return 2;
    }

    int fd = open(wal_path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) return 3;

    /* Persist 63 6-literal clauses forcing v1..v6 all true, plus unit clause (7). */
    for (unsigned bits = 0; bits < 63u; ++bits) {
        int32_t cl[6];
        for (unsigned v = 0; v < 6u; ++v) {
            cl[v] = (bits & (1u << v)) ? -(int32_t)(v + 1u) : (int32_t)(v + 1u);
        }
        if (!wal_append_clause(fd, cl, 6u)) { close(fd); return 4; }
    }
    int32_t unit7 = 7;
    if (!wal_append_clause(fd, &unit7, 1u)) { close(fd); return 4; }

    /* Append a simulated torn partial WAL header (5 bytes) as if host crashed mid-write. */
    const uint8_t partial[5] = {0x54, 0x41, 0x53, 0x42, 0x03};
    if (!write_all(fd, partial, sizeof partial) || fsync(fd) != 0) { close(fd); return 5; }
    close(fd);

    /* Attempt 0: OS-contained worker killed by SIGKILL. */
    pid_t c0 = fork();
    if (c0 < 0) return 6;
    if (c0 == 0) _exit(run_contained_worker(wal_path, out_dir, 1));
    int st0 = 0;
    while (waitpid(c0, &st0, 0) < 0 && errno == EINTR) {}
    if (!WIFSIGNALED(st0) || WTERMSIG(st0) != SIGKILL) return 7;

    /* Attempt 1: Fresh OS-contained worker replays durable WAL from disk and certifies queries! */
    pid_t c1 = fork();
    if (c1 < 0) return 8;
    if (c1 == 0) _exit(run_contained_worker(wal_path, out_dir, 0));
    int st1 = 0;
    while (waitpid(c1, &st1, 0) < 0 && errno == EINTR) {}
    if (!WIFEXITED(st1) || WEXITSTATUS(st1) != 0) return 9;
    return 0;
}
