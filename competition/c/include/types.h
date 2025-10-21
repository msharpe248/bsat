/*********************************************************************
 * BSAT Competition Solver - Core Type Definitions
 *
 * Author: Claude (Anthropic)
 * Date: 2024
 *
 * Core type definitions for the SAT solver, including:
 * - Literal and variable representations
 * - Truth values
 * - Basic configuration constants
 *********************************************************************/

#ifndef BSAT_TYPES_H
#define BSAT_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include <stdlib.h>

/*********************************************************************
 * Configuration Constants
 *********************************************************************/

// Maximum number of variables (2^29 - 1, like Kissat)
#define MAX_VARS ((1U << 29) - 1)

// Maximum number of clauses
#define MAX_CLAUSES ((1U << 30) - 1)

// Invalid/undefined values
#define INVALID_VAR 0
#define INVALID_LIT 0
#define INVALID_CLAUSE UINT32_MAX
#define BINARY_CONFLICT (UINT32_MAX - 1)  // Special marker for binary conflicts
#define INVALID_LEVEL UINT32_MAX
#define LIT_UNDEF 0

/*********************************************************************
 * Basic Types
 *********************************************************************/

// Variable index (1-based, 0 is invalid)
typedef uint32_t Var;

// Literal representation
// Encoding: lit = 2 * var + sign (0 = positive, 1 = negative)
// This allows efficient operations and array indexing
typedef uint32_t Lit;

// Clause reference (index into clause arena)
typedef uint32_t CRef;

// Decision level
typedef uint32_t Level;

// Truth values
typedef enum {
    UNDEF = 0,
    FALSE = 1,
    TRUE = 2
} lbool;

// Clause flags for metadata
typedef enum {
    CLAUSE_ORIGINAL = 0,     // Original problem clause
    CLAUSE_LEARNED = 1,      // Learned during search
    CLAUSE_DELETED = 2,      // Marked for deletion
    CLAUSE_GLUE = 4,         // Glue clause (LBD <= 2)
    CLAUSE_FROZEN = 8        // Protected from deletion
} ClauseFlags;

/*********************************************************************
 * Literal Operations (Inline for Performance)
 *********************************************************************/

// Create literal from variable and sign
static inline Lit mkLit(Var v, bool sign) {
    return (v << 1) | (sign ? 1 : 0);
}

// Get variable from literal
static inline Var var(Lit l) {
    return l >> 1;
}

// Check if literal is negative
static inline bool sign(Lit l) {
    return l & 1;
}

// Negate a literal
static inline Lit neg(Lit l) {
    return l ^ 1;
}

// Get array index for literal (for watch lists)
static inline uint32_t toInt(Lit l) {
    return l;
}

// Convert literal to external format (DIMACS)
static inline int toDimacs(Lit l) {
    return sign(l) ? -(int)var(l) : (int)var(l);
}

// Create literal from DIMACS format
static inline Lit fromDimacs(int d) {
    return mkLit(abs(d), d < 0);
}

/*********************************************************************
 * Truth Value Operations
 *********************************************************************/

// Negate truth value
static inline lbool lnot(lbool v) {
    return (v == UNDEF) ? UNDEF : (v == TRUE) ? FALSE : TRUE;
}

// Convert bool to lbool
static inline lbool toLbool(bool v) {
    return v ? TRUE : FALSE;
}

// Convert lbool to bool (undefined becomes false)
static inline bool toBool(lbool v) {
    return v == TRUE;
}

// XOR for lbool (for sign handling)
static inline lbool lxor(lbool a, bool b) {
    return (a == UNDEF) ? UNDEF : b ? lnot(a) : a;
}

/*********************************************************************
 * Utility Macros
 *********************************************************************/

// Min/Max macros
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

// Array size macro
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// Alignment macro for cache optimization
#define CACHE_LINE_SIZE 64
#define CACHE_ALIGNED __attribute__((aligned(CACHE_LINE_SIZE)))

// Debug assertion (disabled in release builds)
#ifdef NDEBUG
#define ASSERT(cond) ((void)0)
#else
#include <assert.h>
#define ASSERT(cond) assert(cond)
#endif

#endif // BSAT_TYPES_H