#include "../include/dimacs.h"
#include "../include/solver.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void parse_bytes(const char *bytes, size_t size, DimacsError expected) {
    FILE *f = tmpfile();assert(f);
    assert(fwrite(bytes, 1, size, f) == size);rewind(f);
    Solver *s = solver_new();assert(s);
    assert(dimacs_parse_stream(s, f) == expected);
    if (expected == DIMACS_OK) {
        lbool result = solver_solve(s);
        assert(result != UNDEF);
        if (result == TRUE) assert(solver_check_model(s));
    }
    solver_free(s);assert(!fclose(f));
}

static void boundaries(void) {
    const char tail[] = "\np cnf 2 2\r\n+1 -2\n0 2 0\n";
    for (size_t padding = 65500; padding < 65570; ++padding) {
        char *text = malloc(padding + sizeof tail);assert(text);
        memset(text, 'x', padding);text[0] = 'c';
        memcpy(text+padding, tail, sizeof tail);
        parse_bytes(text, padding+sizeof tail-1, DIMACS_OK);free(text);
    }
    const char no_newline[] = "p cnf 1 1\n1 0";
    parse_bytes(no_newline, sizeof no_newline-1, DIMACS_OK);
    char *comment = malloc(150000);assert(comment);
    memset(comment, 'x', 150000);memcpy(comment, "p cnf 0 0\nc", 11);
    parse_bytes(comment, 150000, DIMACS_OK);free(comment);
}

static void malformed(void) {
#define BAD(text) parse_bytes(text, sizeof(text)-1, DIMACS_ERROR_FORMAT)
    BAD("p\0x cnf 1 1\n1 0\n");
    BAD("p cnf\0x 1 1\n1 0\n");
    BAD("p cnf 1\0x 1\n1 0\n");
    BAD("p cnf 1 1\0x\n1 0\n");
    BAD("p cnf 1 1\n1\0x 0\n");
    BAD("p cnf 1 1\n1 0\0x\n");
    BAD("p cnf 1 1\n+ 0\n");
    BAD("p cnf 1 1\n9223372036854775808 0\n");
    BAD("p cnf 1 1\n-9223372036854775809 0\n");
    char text[100] = "p cnf 0 1\n";
    size_t prefix = strlen(text);
    memset(text+prefix, '0', 63);
    parse_bytes(text, prefix+63, DIMACS_OK);
    text[prefix+63] = '0';
    parse_bytes(text, prefix+64, DIMACS_ERROR_FORMAT);
#undef BAD
}

static void input_error(void) {
    Solver *s = solver_new();assert(s);
    FILE *f = tmpfile();assert(f);
    // No descriptor is opened between close and fclose, so it cannot be reused.
    assert(!close(fileno(f)));
    assert(dimacs_parse_stream(s, f) == DIMACS_ERROR_FILE);
    assert(ferror(f));
    fclose(f);solver_free(s);
}

int main(void) {
    boundaries();malformed();input_error();
    puts("PASS: input refill boundaries, embedded NUL rejection, token limits and read errors");
    return 0;
}
