/*********************************************************************
 * BSAT Competition Solver - Core-Guided Weighted Partial MaxSAT
 *
 * Implements RC2 / OLL (Morgado, Dodaro, Marques-Silva 2014; Ignatiev
 * et al. 2019) over BSAT's incremental CDCL engine:
 *   - Classic WCNF ("p wcnf <vars> <clauses> <top>") & modern MaxSAT
 *     Evaluation 2022+ headerless format ("h <lits> 0" / "<w> <lits> 0")
 *   - Unit soft weight merging & weight stratification
 *   - Deletion-based UNSAT core minimization (conflict-bounded)
 *   - Incremental Totalizer OLL relaxation (reusing src/pb_enc.c)
 *   - Exact incumbent cost verification & upper-bound hardening
 *********************************************************************/

#ifndef BSAT_MAXSAT_H
#define BSAT_MAXSAT_H

#include "solver.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    Lit *lits;
    uint32_t size;
    uint64_t weight; /* 0 = hard clause, >0 = soft clause weight */
} MaxSatClause;

typedef struct {
    uint32_t num_vars;
    MaxSatClause *clauses;
    uint32_t num_clauses;
    uint32_t cap_clauses;
    uint32_t num_hard;
    uint32_t num_soft;
    uint64_t total_soft_weight;
} MaxSatInstance;

typedef struct {
    lbool status;          /* TRUE=SAT/OPTIMUM, FALSE=UNSAT (hard unsat), UNDEF=UNKNOWN */
    bool optimum;          /* true iff proven globally optimal */
    uint64_t cost;         /* Optimal (or best incumbent) sum of falsified soft weights */
    uint64_t lower_bound;  /* Proven lower bound at termination */
    lbool *model;          /* 1-based [1..inst->num_vars], caller frees */
    uint32_t cores;        /* Number of UNSAT cores extracted */
    uint32_t solves;       /* Number of incremental SAT calls */
} MaxSatResult;

void maxsat_instance_init(MaxSatInstance *inst);
void maxsat_instance_free(MaxSatInstance *inst);
void maxsat_result_free(MaxSatResult *res);

/* Programmatic clause construction (weight == 0 means hard clause). */
bool maxsat_add_clause(MaxSatInstance *inst, const Lit *lits, uint32_t size, uint64_t weight);

/* WCNF parsers (auto-detect classic "p wcnf", "p cnf", and MSE 2022+ headerless). */
bool maxsat_parse_stream(MaxSatInstance *inst, FILE *fp);
bool maxsat_parse_file(MaxSatInstance *inst, const char *path);
bool maxsat_parse_string(MaxSatInstance *inst, const char *text);

/* Verify model (1-based array of length >= inst->num_vars + 1) against all hard
 * clauses and compute exact sum of falsified soft weights in *out_cost. */
bool maxsat_verify_model(const MaxSatInstance *inst, const lbool *model, uint64_t *out_cost);

/* Solve Weighted Partial MaxSAT via RC2/OLL core-guided search. */
MaxSatResult maxsat_solve(const MaxSatInstance *inst, const SolverOpts *user_opts);

#endif /* BSAT_MAXSAT_H */
