/*********************************************************************
 * BSAT Competition Solver - XOR, Cardinality, and Pseudo-Boolean Engine
 *
 * Provides:
 *   - Native XOR-clause CNF encoding (Tseitin XOR-chain splitting so
 *     src/gauss.c reconstructs arbitrary-length GF(2) equations)
 *   - Sinz sequential-counter cardinality encoding (AtMost-k)
 *   - Incremental Totalizer (Martins et al. 2014) for reusable bound
 *     relaxation (shared with core-guided MaxSAT RC2/OLL)
 *   - Generalized Totalizer (GTE) with exact binary-adder fallback for
 *     weighted Pseudo-Boolean constraints (sum w_i * l_i <= rhs)
 *   - Native .opb Pseudo-Boolean parser & decision/optimization driver
 *********************************************************************/

#ifndef BSAT_PB_ENC_H
#define BSAT_PB_ENC_H

#include "solver.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* =====================================================================
 * XOR Clause Encoder
 * ===================================================================== */

/* Add exact CNF encoding of (lits[0] XOR lits[1] XOR ... XOR lits[n-1] == rhs).
 * Handles duplicate variables mod 2, negated literals, k=0/1, direct k<=4,
 * and Tseitin XOR-chain splitting for k >= 5. */
bool pb_add_xor_clause(Solver *s, const Lit *lits, uint32_t n, bool rhs);

/* =====================================================================
 * Cardinality Encoders (Unweighted AtMost-k)
 * ===================================================================== */

/* Encode sum(lits[0..n-1]) <= k via Sinz sequential counter (exact O(n*k)). */
bool pb_add_atmost_sinz(Solver *s, const Lit *lits, uint32_t n, uint32_t k);

/* Incremental Totalizer tree:
 *   out[j] (0 <= j < encoded_ub) is forced TRUE whenever sum(lits) >= j + 1.
 *   Assuming neg(out[bound]) enforces sum(lits) <= bound. */
typedef struct PbTotNode PbTotNode;

typedef struct PbTotalizer {
    PbTotNode *root;
    uint32_t num_inputs;
    uint32_t encoded_ub; /* Number of root output literals currently allocated */
} PbTotalizer;

PbTotalizer *pb_totalizer_create(Solver *s, const Lit *lits, uint32_t n, uint32_t init_ub);
bool pb_totalizer_increase_bound(PbTotalizer *t, Solver *s, uint32_t new_ub);
Lit pb_totalizer_out(const PbTotalizer *t, uint32_t idx);
void pb_totalizer_free(PbTotalizer *t);

/* =====================================================================
 * Weighted Pseudo-Boolean Constraint Encoder
 * ===================================================================== */

/* Encode sum_{i=0}^{n-1} weights[i] * lits[i] <= rhs (all weights >= 0).
 * Automatically simplifies duplicate variables, uses Sinz/Totalizer when
 * weights are uniform, Generalized Totalizer (GTE) for bounded sum spaces,
 * and an exact binary full/half-adder network + lexicographic comparator
 * for arbitrary large 64-bit weights. */
bool pb_add_pb_le(Solver *s, const Lit *lits, const uint64_t *weights,
                  uint32_t n, uint64_t rhs);

/* =====================================================================
 * Native .opb Pseudo-Boolean Formula & Optimization Driver
 * ===================================================================== */

typedef enum {
    PB_OP_GE = 0,
    PB_OP_LE = 1,
    PB_OP_EQ = 2
} PbRelOp;

typedef struct {
    Lit *lits;
    int64_t *coeffs;
    uint32_t size;
    uint32_t cap;
    PbRelOp op;
    int64_t rhs;
} PbConstraint;

typedef struct {
    uint32_t num_vars;
    bool has_objective;
    Lit *obj_lits;
    int64_t *obj_coeffs;
    uint32_t obj_size;
    uint32_t obj_cap;
    PbConstraint *constraints;
    uint32_t num_constraints;
    uint32_t cap_constraints;
} PbInstance;

void pb_instance_init(PbInstance *inst);
void pb_instance_free(PbInstance *inst);

bool pb_parse_opb_stream(PbInstance *inst, FILE *fp);
bool pb_parse_opb_file(PbInstance *inst, const char *path);
bool pb_parse_opb_string(PbInstance *inst, const char *text);

/* Encode all constraints of inst into solver s (allocating vars 1..inst->num_vars). */
bool pb_encode_instance(Solver *s, const PbInstance *inst);

/* Verify a model (1-based array of lbool length >= inst->num_vars + 1) against
 * every original PB constraint; if out_obj != NULL and inst->has_objective,
 * computes exact objective value. */
bool pb_verify_model(const PbInstance *inst, const lbool *model, int64_t *out_obj);

typedef struct {
    lbool status;          /* TRUE=SAT/OPTIMUM, FALSE=UNSAT, UNDEF=UNKNOWN */
    bool optimum;          /* true iff proven optimal (or decision SAT) */
    int64_t best_obj;      /* Optimal/best objective value when has_objective */
    lbool *best_model;     /* 1-based [1..inst->num_vars], caller frees */
    uint32_t solves;       /* Number of SAT solver invocations */
} PbSolveResult;

void pb_solve_result_free(PbSolveResult *res);

/* Solve a parsed PB decision or optimization instance.
 * Respects opts (time limit, quiet, etc.). Prints progress "o <val>" lines when
 * !opts->quiet and inst->has_objective. */
PbSolveResult pb_solve_opb(const PbInstance *inst, const SolverOpts *opts);

#endif /* BSAT_PB_ENC_H */
