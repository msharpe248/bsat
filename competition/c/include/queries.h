/*********************************************************************
 * BSAT Competition Solver - Higher-Level Query Engines
 *
 * Provides three exact SAT-based reasoning engines built over BSAT's
 * incremental assumption/core CDCL solver:
 *   1. AllSAT / Projected Model Enumeration (--models, --project)
 *   2. Exact Global Backbone Computation (--backbone) with flip-guided
 *      bulk filtering and incremental assumption probing
 *   3. Minimal Unsatisfiable Subset (MUS) Extraction (--mus) with
 *      core-based bulk trimming and recursive model rotation (RMR)
 *********************************************************************/

#ifndef BSAT_QUERIES_H
#define BSAT_QUERIES_H

#include "solver.h"
#include <stdint.h>
#include <stdbool.h>

/* =====================================================================
 * 1. AllSAT / Projected Model Enumeration
 * ===================================================================== */

/* Callback invoked for each distinct model (1-based array [1..num_vars]).
 * Return true to continue enumerating, false to stop early. */
typedef bool (*BsatModelCallback)(void *ctx, const lbool *model,
                                  uint32_t num_vars, uint64_t model_index);

typedef struct {
    lbool status;          /* TRUE=at least one model, FALSE=UNSAT, UNDEF=stopped/error */
    bool complete;         /* true iff entire model space (or max_models limit) exhausted cleanly */
    uint64_t count;        /* Number of distinct (projected) models enumerated */
    uint32_t solves;       /* Number of SAT solver invocations */
} AllSatResult;

/* Enumerate up to max_models (0 = unlimited) distinct models of the formula
 * currently stored in template_solver->input[]. If project_vars != NULL and
 * num_project > 0, projects onto those variables (distinct assignments to
 * project_vars). Respects user_opts (e.g., max_time). */
AllSatResult bsat_enumerate_models(const Solver *template_solver,
                                   uint32_t orig_vars,
                                   const Var *project_vars,
                                   uint32_t num_project,
                                   uint64_t max_models,
                                   const SolverOpts *user_opts,
                                   BsatModelCallback callback,
                                   void *callback_ctx);

/* =====================================================================
 * 2. Exact Global Backbone Computation
 * ===================================================================== */

typedef struct {
    lbool status;          /* TRUE=SAT (backbone computed), FALSE=UNSAT, UNDEF=timeout/error */
    bool complete;         /* true iff every variable in 1..orig_vars was decided */
    Lit *lits;             /* Exact backbone literals (sorted by variable), caller frees */
    uint32_t size;         /* Number of backbone literals */
    uint32_t solves;       /* Number of incremental SAT calls */
    uint32_t flip_dropped; /* Candidates eliminated with zero SAT calls via flip filtering */
} BackboneResult;

void bsat_backbone_result_free(BackboneResult *res);

BackboneResult bsat_compute_backbone(const Solver *template_solver,
                                     uint32_t orig_vars,
                                     const SolverOpts *user_opts);

/* =====================================================================
 * 3. Minimal Unsatisfiable Subset (MUS) Extraction
 * ===================================================================== */

typedef struct {
    lbool status;          /* FALSE=UNSAT (MUS found), TRUE=SAT (no MUS), UNDEF=timeout/error */
    bool minimal;          /* true iff proven irreducibly minimal (complete) */
    uint32_t *clause_ids;  /* 0-based original clause indices forming the MUS, caller frees */
    uint32_t size;         /* Number of clauses in MUS */
    uint32_t orig_clauses; /* Total original clauses in input formula */
    uint32_t solves;       /* Number of incremental SAT calls */
    uint32_t rotated;      /* Necessary clauses proved with zero SAT calls via model rotation */
} MusResult;

void bsat_mus_result_free(MusResult *res);

MusResult bsat_compute_mus(const Solver *template_solver,
                           uint32_t orig_vars,
                           const SolverOpts *user_opts);

#endif /* BSAT_QUERIES_H */
