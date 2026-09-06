/*********************************************************************
 * BSAT Competition Solver - DIMACS CNF Parser Implementation
 *********************************************************************/

#include "../include/dimacs.h"
#include "../include/arena.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

/* DIMACS clauses are terminated by zero, never by a physical line. */
static int token(FILE *f, char *buf, size_t cap) {
    int c;
    do {
        c = fgetc(f);
        if (c == 'c') {
            while (c != '\n' && c != EOF) c = fgetc(f);
        }
    } while (c != EOF && isspace((unsigned char)c));
    if (c == EOF) return ferror(f) ? -1 : 0;
    size_t n = 0;
    do {
        if (n + 1 >= cap) return -1;
        buf[n++] = (char)c;
        c = fgetc(f);
    } while (c != EOF && !isspace((unsigned char)c));
    buf[n] = 0;
    return 1;
}

static bool integer(const char *buf, long long *v) {
    char *end;
    errno = 0;
    *v = strtoll(buf, &end, 10);
    return !errno && end != buf && !*end;
}

DimacsError dimacs_parse_stream(Solver *s, FILE *file) {
    if (!s || !file) return DIMACS_ERROR_FILE;
    char buf[64];
    Lit *clause = NULL;
    uint32_t size = 0, capacity = 0, count = 0;
    long long nvars = 0, nclauses = 0;
    DimacsError result = DIMACS_ERROR_FORMAT;
    if (token(file, buf, sizeof buf) != 1 || strcmp(buf, "p")) goto done;
    /* Read cnf without comment skipping: it is the format token. */
    if (fscanf(file, " %63s", buf) != 1 || strcmp(buf, "cnf")) goto done;
    if (token(file, buf, sizeof buf) != 1 || !integer(buf, &nvars) ||
        nvars < 0 || nvars > MAX_VARS) goto done;
    if (token(file, buf, sizeof buf) != 1 || !integer(buf, &nclauses) ||
        nclauses < 0 || nclauses > MAX_CLAUSES) goto done;
    while (s->num_vars < (uint32_t)nvars) {
        if (!solver_new_var(s)) { result = DIMACS_ERROR_MEMORY; goto done; }
    }
    for (;;) {
        int status = token(file, buf, sizeof buf);
        if (!status) break;
        long long value;
        if (status < 0 || !integer(buf, &value)) goto done;
        if (!value) {
            if (++count > (uint32_t)nclauses) goto done;
            solver_add_clause(s, clause, size);
            if (s->error) { result = DIMACS_ERROR_MEMORY; goto done; }
            size = 0;
        } else {
            if (value < -nvars || value > nvars) goto done;
            if (size == capacity) {
                if (capacity >= (1u << 27)) { result = DIMACS_ERROR_SIZE; goto done; }
                uint32_t next = capacity ? capacity * 2 : 16;
                Lit *grown = realloc(clause, (size_t)next * sizeof *clause);
                if (!grown) { result = DIMACS_ERROR_MEMORY; goto done; }
                clause = grown; capacity = next;
            }
            clause[size++] = fromDimacs((int)value);
        }
    }
    if (ferror(file)) result = DIMACS_ERROR_FILE;
    else if (!size && count == (uint32_t)nclauses) result = DIMACS_OK;
done:
    free(clause);
    return result;
}

DimacsError dimacs_parse_file(Solver* s, const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        return DIMACS_ERROR_FILE;
    }

    DimacsError result = dimacs_parse_stream(s, file);
    fclose(file);
    return result;
}

DimacsError dimacs_parse_string(Solver* s, const char* str) {
    if (!s || !str) {
        return DIMACS_ERROR_FILE;
    }

    // Create a memory stream
    FILE* stream = fmemopen((void*)str, strlen(str), "r");
    if (!stream) {
        return DIMACS_ERROR_MEMORY;
    }

    DimacsError result = dimacs_parse_stream(s, stream);
    fclose(stream);
    return result;
}

/*********************************************************************
 * Error Messages
 *********************************************************************/

const char* dimacs_error_string(DimacsError err) {
    switch (err) {
        case DIMACS_OK:
            return "Success";
        case DIMACS_ERROR_FILE:
            return "Cannot open or read file";
        case DIMACS_ERROR_FORMAT:
            return "Invalid DIMACS format";
        case DIMACS_ERROR_MEMORY:
            return "Out of memory";
        case DIMACS_ERROR_SIZE:
            return "Problem too large";
        default:
            return "Unknown error";
    }
}

/*********************************************************************
 * Output Functions
 *********************************************************************/

void dimacs_write_solution(const Solver* s, FILE* out) {
    if (s->result == TRUE) {
        fprintf(out, "s SATISFIABLE\n");
        fprintf(out, "v ");

        for (Var v = 1; v <= s->num_vars; v++) {
            lbool val = solver_model_value(s, v);
            if (val == TRUE) {
                fprintf(out, "%u ", v);
            } else if (val == FALSE) {
                fprintf(out, "-%u ", v);
            }
            // Skip UNDEF variables

            // Line wrapping
            if (v % 20 == 0) {
                fprintf(out, "\nv ");
            }
        }
        fprintf(out, "0\n");
    } else if (s->result == FALSE) {
        fprintf(out, "s UNSATISFIABLE\n");
    } else {
        fprintf(out, "s UNKNOWN\n");
    }
}

bool dimacs_write_proof(const Solver* s, FILE* out) {
    if (!s || !out || !s->proof_file || !s->opts.proof_path || s->result != FALSE) return false;
    if (fflush(s->proof_file)) return false;
    FILE *input=fopen(s->opts.proof_path,"rb");
    if (!input) return false;
    struct stat source, target;
    if (!fstat(fileno(input), &source) && !fstat(fileno(out), &target) &&
        source.st_dev == target.st_dev && source.st_ino == target.st_ino) {
        fclose(input); return false;
    }
    char buffer[8192];size_t n;
    bool ok=true;
    while ((n=fread(buffer,1,sizeof buffer,input)))
        if (fwrite(buffer,1,n,out)!=n) { ok=false;break; }
    if (ferror(input)) ok=false;
    fclose(input);
    return ok && !fflush(out);
}

void dimacs_write_cnf(const Solver* s, FILE* out) {
    fprintf(out, "p cnf %u %u\n", s->num_vars, s->input_clauses);
    for (size_t i = 0; i < s->input_size; ++i) {
        Lit lit = s->input[i];
        if (lit) fprintf(out, "%d ", toDimacs(lit));
        else fprintf(out, "0\n");
    }
}
