/*********************************************************************
 * BSAT Competition Solver - AllSAT, Exact Backbone & MUS Engines
 *********************************************************************/

#include "../include/queries.h"
#include <stdlib.h>
#include <string.h>

/* =====================================================================
 * Internal Helpers: Input Clause Extraction & Verification
 * ===================================================================== */

typedef struct {
    uint32_t num_vars;
    uint32_t num_clauses;
    uint32_t *offsets; /* length num_clauses + 1 */
    Lit *lits;         /* concatenated clause literals */
} InputCnf;

static void input_cnf_free(InputCnf *cnf) {
    free(cnf->offsets);
    free(cnf->lits);
    memset(cnf, 0, sizeof *cnf);
}

static bool input_cnf_extract(const Solver *src, uint32_t orig_vars, InputCnf *out) {
    memset(out, 0, sizeof *out);
    if (!src) return false;
    out->num_vars = orig_vars ? orig_vars : src->num_vars;
    out->num_clauses = src->input_clauses;

    out->offsets = malloc((size_t)(out->num_clauses + 1u) * sizeof *out->offsets);
    if (!out->offsets) return false;

    size_t total_lits = 0;
    for (size_t i = 0; i < src->input_size; ++i) {
        if (src->input[i] != 0) total_lits++;
    }
    out->lits = malloc((total_lits ? total_lits : 1u) * sizeof *out->lits);
    if (!out->lits) { free(out->offsets); out->offsets = NULL; return false; }

    uint32_t ci = 0;
    uint32_t pos = 0;
    out->offsets[0] = 0;
    for (size_t i = 0; i < src->input_size; ++i) {
        Lit l = src->input[i];
        if (l == 0) {
            ci++;
            if (ci <= out->num_clauses) out->offsets[ci] = pos;
        } else {
            out->lits[pos++] = l;
            if (var(l) > out->num_vars) out->num_vars = var(l);
        }
    }
    out->num_clauses = ci;
    out->offsets[ci] = pos;
    return true;
}

static bool input_cnf_check_model(const InputCnf *cnf, const lbool *model) {
    for (uint32_t c = 0; c < cnf->num_clauses; ++c) {
        bool sat = false;
        for (uint32_t i = cnf->offsets[c]; i < cnf->offsets[c + 1]; ++i) {
            Lit l = cnf->lits[i];
            bool pos = (model[var(l)] == TRUE);
            if (sign(l) ? !pos : pos) { sat = true; break; }
        }
        if (!sat) return false;
    }
    return true;
}

static Solver *make_incremental_solver(const InputCnf *cnf, const SolverOpts *user_opts,
                                       bool add_clauses) {
    SolverOpts opts = user_opts ? *user_opts : default_opts();
    opts.reuse_learnts = true;
    opts.inprocess = false;
    opts.sbva = false;
    opts.sym = false;
    opts.elim = false;
    opts.bce = false;
    opts.proof_path = NULL;

    Solver *s = solver_new_with_opts(&opts);
    if (!s) return NULL;
    while (s->num_vars < cnf->num_vars) {
        if (solver_new_var(s) == INVALID_VAR) { solver_free(s); return NULL; }
    }
    if (add_clauses) {
        for (uint32_t c = 0; c < cnf->num_clauses; ++c) {
            uint32_t sz = cnf->offsets[c + 1] - cnf->offsets[c];
            solver_add_clause(s, cnf->lits + cnf->offsets[c], sz);
            if (s->error) { solver_free(s); return NULL; }
        }
    }
    return s;
}

/* =====================================================================
 * 1. AllSAT / Projected Model Enumeration
 * ===================================================================== */

AllSatResult bsat_enumerate_models(const Solver *template_solver,
                                   uint32_t orig_vars,
                                   const Var *project_vars,
                                   uint32_t num_project,
                                   uint64_t max_models,
                                   const SolverOpts *user_opts,
                                   BsatModelCallback callback,
                                   void *callback_ctx) {
    AllSatResult res;
    memset(&res, 0, sizeof res);
    res.status = UNDEF;

    InputCnf cnf;
    if (!input_cnf_extract(template_solver, orig_vars, &cnf)) return res;
    uint32_t report_vars = (orig_vars && orig_vars <= cnf.num_vars) ? orig_vars : cnf.num_vars;

    double start_cpu = solver_cpu_time();
    double deadline = (user_opts && user_opts->max_time > 0) ? (start_cpu + user_opts->max_time) : 0.0;

    Solver *s = make_incremental_solver(&cnf, user_opts, true);
    if (!s) { input_cnf_free(&cnf); return res; }

    /* Build deduplicated blocking variable list. */
    uint32_t n_block = (project_vars && num_project > 0) ? num_project : report_vars;
    Var *block_vars = malloc((size_t)(n_block ? n_block : 1u) * sizeof *block_vars);
    uint8_t *seen = calloc((size_t)cnf.num_vars + 1u, 1);
    lbool *model = malloc((size_t)(cnf.num_vars + 1u) * sizeof *model);
    Lit *block_cl = malloc((size_t)(n_block ? n_block : 1u) * sizeof *block_cl);
    if (!block_vars || !seen || !model || !block_cl) {
        free(block_vars); free(seen); free(model); free(block_cl);
        solver_free(s); input_cnf_free(&cnf);
        return res;
    }

    uint32_t bcnt = 0;
    if (project_vars && num_project > 0) {
        for (uint32_t i = 0; i < num_project; ++i) {
            Var v = project_vars[i];
            if (v >= 1 && v <= cnf.num_vars && !seen[v]) {
                seen[v] = 1;
                block_vars[bcnt++] = v;
            }
        }
    } else {
        for (Var v = 1; v <= report_vars; ++v) block_vars[bcnt++] = v;
    }
    free(seen);

    model[0] = UNDEF;
    for (;;) {
        if (deadline > 0) {
            double rem = deadline - solver_cpu_time();
            if (rem <= 0.0) break;
            s->opts.max_time = rem;
        }
        res.solves++;
        lbool r = solver_solve(s);
        if (r == TRUE) {
            for (Var v = 1; v <= cnf.num_vars; ++v) {
                lbool mv = solver_model_value(s, v);
                model[v] = (mv == TRUE) ? TRUE : FALSE;
            }
            if (!input_cnf_check_model(&cnf, model)) {
                res.status = UNDEF;
                break;
            }
            res.status = TRUE;
            res.count++;
            if (callback && !callback(callback_ctx, model, report_vars, res.count)) {
                res.complete = false;
                break;
            }
            if (max_models > 0 && res.count >= max_models) {
                res.complete = true;
                break;
            }
            /* Add blocking clause forbidding this (projected) assignment. */
            if (bcnt == 0) {
                /* 0 variables => empty formula has exactly 1 empty assignment. */
                res.complete = true;
                break;
            }
            for (uint32_t i = 0; i < bcnt; ++i) {
                Var v = block_vars[i];
                /* If model[v] == TRUE, include ~v (sign=true); else +v (sign=false). */
                block_cl[i] = mkLit(v, model[v] == TRUE);
            }
            solver_add_clause(s, block_cl, bcnt);
            if (s->error) { res.status = UNDEF; break; }
        } else if (r == FALSE) {
            if (res.count == 0) res.status = FALSE;
            res.complete = true;
            break;
        } else {
            /* Budget/timeout hit */
            break;
        }
    }

    free(block_vars);
    free(model);
    free(block_cl);
    solver_free(s);
    input_cnf_free(&cnf);
    return res;
}

/* =====================================================================
 * 2. Exact Global Backbone Computation (Flip-Filter + Incremental Probing)
 * ===================================================================== */

void bsat_backbone_result_free(BackboneResult *res) {
    if (!res) return;
    free(res->lits);
    res->lits = NULL;
    res->size = 0;
}

typedef struct {
    uint32_t *head;    /* size 2*(num_vars+1) + 1 */
    uint32_t *clauses; /* clause indices per literal */
    uint32_t *sat_cnt; /* satisfied literal count per clause under current model */
} FlipIndex;

static bool lit_seen_earlier(const InputCnf *cnf, uint32_t c, uint32_t idx) {
    Lit l = cnf->lits[idx];
    for (uint32_t j = cnf->offsets[c]; j < idx; ++j) {
        if (cnf->lits[j] == l) return true;
    }
    return false;
}

static bool flip_index_build(const InputCnf *cnf, FlipIndex *fi) {
    memset(fi, 0, sizeof *fi);
    uint32_t nl = 2u * (cnf->num_vars + 1u);
    fi->head = calloc((size_t)nl + 1u, sizeof *fi->head);
    fi->sat_cnt = calloc((size_t)(cnf->num_clauses ? cnf->num_clauses : 1u), sizeof *fi->sat_cnt);
    if (!fi->head || !fi->sat_cnt) { free(fi->head); free(fi->sat_cnt); return false; }

    uint32_t total = 0;
    for (uint32_t c = 0; c < cnf->num_clauses; ++c) {
        for (uint32_t i = cnf->offsets[c]; i < cnf->offsets[c + 1]; ++i) {
            if (!lit_seen_earlier(cnf, c, i)) {
                fi->head[cnf->lits[i] + 1u]++;
                total++;
            }
        }
    }
    for (uint32_t l = 1; l <= nl; ++l) fi->head[l] += fi->head[l - 1];

    fi->clauses = malloc((size_t)(total ? total : 1u) * sizeof *fi->clauses);
    uint32_t *cur = malloc((size_t)(nl + 1u) * sizeof *cur);
    if (!fi->clauses || !cur) {
        free(cur); free(fi->clauses); free(fi->head); free(fi->sat_cnt);
        return false;
    }
    memcpy(cur, fi->head, (size_t)(nl + 1u) * sizeof *cur);
    for (uint32_t c = 0; c < cnf->num_clauses; ++c) {
        for (uint32_t i = cnf->offsets[c]; i < cnf->offsets[c + 1]; ++i) {
            if (!lit_seen_earlier(cnf, c, i)) {
                fi->clauses[cur[cnf->lits[i]]++] = c;
            }
        }
    }
    free(cur);
    return true;
}

static void flip_index_free(FlipIndex *fi) {
    free(fi->head);
    free(fi->clauses);
    free(fi->sat_cnt);
    memset(fi, 0, sizeof *fi);
}

/* Eliminate non-backbone candidates with zero SAT calls by greedily checking
 * whether flipping a candidate variable preserves satisfaction of all clauses. */
static uint32_t flip_filter_candidates(const InputCnf *cnf, FlipIndex *fi,
                                       lbool *model, uint8_t *cand, uint32_t report_vars) {
    /* Recompute exact distinct-literal sat_cnt for each clause under model. */
    for (uint32_t c = 0; c < cnf->num_clauses; ++c) {
        uint32_t cnt = 0;
        for (uint32_t i = cnf->offsets[c]; i < cnf->offsets[c + 1]; ++i) {
            if (lit_seen_earlier(cnf, c, i)) continue;
            Lit l = cnf->lits[i];
            bool pos = (model[var(l)] == TRUE);
            if (sign(l) ? !pos : pos) cnt++;
        }
        fi->sat_cnt[c] = cnt;
    }

    uint32_t dropped = 0;
    for (Var v = 1; v <= report_vars; ++v) {
        if (!cand[v]) continue;
        /* Currently true literal for v is cur_lit; flipping v makes cur_lit false and neg(cur_lit) true. */
        Lit cur_lit = mkLit(v, model[v] == FALSE);
        Lit new_lit = neg(cur_lit);
        bool can_flip = true;
        for (uint32_t p = fi->head[cur_lit]; p < fi->head[cur_lit + 1u]; ++p) {
            uint32_t c = fi->clauses[p];
            if (fi->sat_cnt[c] <= 1u) {
                /* Wait: if the clause contains BOTH v and ~v (tautology), sat_cnt would be 1
                 * for cur_lit and flipping would still keep new_lit true; check safely. */
                bool has_new = false;
                for (uint32_t i = cnf->offsets[c]; i < cnf->offsets[c + 1]; ++i) {
                    if (cnf->lits[i] == new_lit) { has_new = true; break; }
                }
                if (!has_new) { can_flip = false; break; }
            }
        }
        if (can_flip) {
            /* Both polarities of v are satisfiable! Drop v and apply the flip to unlock more flips. */
            cand[v] = 0;
            dropped++;
            model[v] = (model[v] == TRUE) ? FALSE : TRUE;
            for (uint32_t p = fi->head[cur_lit]; p < fi->head[cur_lit + 1u]; ++p)
                fi->sat_cnt[fi->clauses[p]]--;
            for (uint32_t p = fi->head[new_lit]; p < fi->head[new_lit + 1u]; ++p)
                fi->sat_cnt[fi->clauses[p]]++;
        }
    }
    return dropped;
}

BackboneResult bsat_compute_backbone(const Solver *template_solver,
                                     uint32_t orig_vars,
                                     const SolverOpts *user_opts) {
    BackboneResult res;
    memset(&res, 0, sizeof res);
    res.status = UNDEF;

    InputCnf cnf;
    if (!input_cnf_extract(template_solver, orig_vars, &cnf)) return res;
    uint32_t report_vars = (orig_vars && orig_vars <= cnf.num_vars) ? orig_vars : cnf.num_vars;

    double start_cpu = solver_cpu_time();
    double deadline = (user_opts && user_opts->max_time > 0) ? (start_cpu + user_opts->max_time) : 0.0;

    Solver *s = make_incremental_solver(&cnf, user_opts, true);
    if (!s) { input_cnf_free(&cnf); return res; }

    res.solves = 1;
    lbool r0 = solver_solve(s);
    if (r0 != TRUE) {
        res.status = r0;
        res.complete = (r0 == FALSE);
        solver_free(s);
        input_cnf_free(&cnf);
        return res;
    }

    lbool *ref_model = malloc((size_t)(cnf.num_vars + 1u) * sizeof *ref_model);
    lbool *work_model = malloc((size_t)(cnf.num_vars + 1u) * sizeof *work_model);
    uint8_t *cand = calloc((size_t)report_vars + 1u, 1); /* 1=candidate, 2=proven backbone, 0=non-backbone */
    FlipIndex fi;
    if (!ref_model || !work_model || !cand || !flip_index_build(&cnf, &fi)) {
        free(ref_model); free(work_model); free(cand);
        solver_free(s); input_cnf_free(&cnf);
        return res;
    }

    ref_model[0] = UNDEF;
    for (Var v = 1; v <= cnf.num_vars; ++v) {
        lbool mv = solver_model_value(s, v);
        ref_model[v] = (mv == TRUE) ? TRUE : FALSE;
    }
    if (!input_cnf_check_model(&cnf, ref_model)) {
        flip_index_free(&fi); free(ref_model); free(work_model); free(cand);
        solver_free(s); input_cnf_free(&cnf);
        return res;
    }

    for (Var v = 1; v <= report_vars; ++v) cand[v] = 1;

    /* Initial flip-guided bulk filtering (zero SAT calls!). */
    memcpy(work_model, ref_model, (size_t)(cnf.num_vars + 1u) * sizeof *work_model);
    res.flip_dropped += flip_filter_candidates(&cnf, &fi, work_model, cand, report_vars);

    bool timed_out = false;
    for (Var v = 1; v <= report_vars; ++v) {
        if (cand[v] != 1) continue;
        if (deadline > 0) {
            double rem = deadline - solver_cpu_time();
            if (rem <= 0.0) { timed_out = true; break; }
            s->opts.max_time = rem;
        }
        /* Probe opposite polarity of ref_model[v]. */
        Lit expected = mkLit(v, ref_model[v] == FALSE);
        Lit probe = neg(expected);
        res.solves++;
        lbool r = solver_solve_with_assumptions(s, &probe, 1);
        if (r == FALSE) {
            /* Proven backbone! Permanently assert unit to accelerate all future probes. */
            cand[v] = 2;
            solver_add_clause(s, &expected, 1);
            if (s->error) { timed_out = true; break; }
        } else if (r == TRUE) {
            /* Eliminate v AND every other candidate that disagrees with ref_model! */
            cand[v] = 0;
            work_model[0] = UNDEF;
            for (Var u = 1; u <= cnf.num_vars; ++u) {
                lbool mv = solver_model_value(s, u);
                work_model[u] = (mv == TRUE) ? TRUE : FALSE;
            }
            for (Var u = 1; u <= report_vars; ++u) {
                if (cand[u] == 1 && work_model[u] != ref_model[u]) {
                    cand[u] = 0;
                }
            }
            res.flip_dropped += flip_filter_candidates(&cnf, &fi, work_model, cand, report_vars);
        } else {
            timed_out = true;
            break;
        }
    }

    uint32_t bb_cnt = 0;
    for (Var v = 1; v <= report_vars; ++v) {
        if (cand[v] == 2) bb_cnt++;
    }
    res.lits = malloc((size_t)(bb_cnt ? bb_cnt : 1u) * sizeof *res.lits);
    if (res.lits) {
        uint32_t k = 0;
        for (Var v = 1; v <= report_vars; ++v) {
            if (cand[v] == 2) res.lits[k++] = mkLit(v, ref_model[v] == FALSE);
        }
        res.size = k;
        res.status = TRUE;
        res.complete = !timed_out;
    }

    flip_index_free(&fi);
    free(ref_model);
    free(work_model);
    free(cand);
    solver_free(s);
    input_cnf_free(&cnf);
    return res;
}

/* =====================================================================
 * 3. Minimal Unsatisfiable Subset (MUS) Extraction (Deletion + Core Trim + RMR)
 * ===================================================================== */

void bsat_mus_result_free(MusResult *res) {
    if (!res) return;
    free(res->clause_ids);
    res->clause_ids = NULL;
    res->size = 0;
}

/* Recursive Model Rotation (Marques-Silva & Lynce 2011):
 * Given a SAT model that satisfies all active clauses EXCEPT transition clause `cid`,
 * flip variables occurring in `cid`; if flipping a variable satisfies `cid` and falsifies
 * exactly ONE other active candidate clause `other`, then `other` is also a transition
 * clause (necessary in the MUS) and we recurse from `other`! */
static uint32_t mus_model_rotate(const InputCnf *cnf, const FlipIndex *fi,
                                 Solver *s, const Var *sel_var,
                                 lbool *model, uint8_t *state, uint32_t start_cid) {
    uint32_t rotated = 0;
    uint32_t stack_cap = 64, stack_sz = 0;
    uint32_t *stack = malloc(stack_cap * sizeof *stack);
    if (!stack) return 0;
    stack[stack_sz++] = start_cid;

    /* Compute distinct-literal sat_cnt for all currently active clauses under model. */
    uint32_t *sat_cnt = calloc((size_t)(cnf->num_clauses ? cnf->num_clauses : 1u), sizeof *sat_cnt);
    if (!sat_cnt) { free(stack); return 0; }
    for (uint32_t c = 0; c < cnf->num_clauses; ++c) {
        if (!state[c]) continue;
        uint32_t cnt = 0;
        for (uint32_t i = cnf->offsets[c]; i < cnf->offsets[c + 1]; ++i) {
            if (lit_seen_earlier(cnf, c, i)) continue;
            Lit l = cnf->lits[i];
            bool pos = (model[var(l)] == TRUE);
            if (sign(l) ? !pos : pos) cnt++;
        }
        sat_cnt[c] = cnt;
    }

    uint32_t steps = 0;
    while (stack_sz > 0 && steps < 256) {
        uint32_t cid = stack[--stack_sz];
        steps++;
        /* Only rotate if cid is currently the sole falsified active clause. */
        if (sat_cnt[cid] != 0) continue;

        for (uint32_t i = cnf->offsets[cid]; i < cnf->offsets[cid + 1]; ++i) {
            Var v = var(cnf->lits[i]);
            Lit old_true = mkLit(v, model[v] == FALSE);
            Lit new_true = neg(old_true);

            /* Check which active clauses become falsified if we flip v. */
            uint32_t newly_unsat_cnt = 0;
            uint32_t sole_other = UINT32_MAX;
            for (uint32_t p = fi->head[old_true]; p < fi->head[old_true + 1u]; ++p) {
                uint32_t oc = fi->clauses[p];
                if (!state[oc] || oc == cid) continue;
                if (sat_cnt[oc] == 1u) {
                    /* Verify oc doesn't also contain new_true (tautology). */
                    bool has_new = false;
                    for (uint32_t k = cnf->offsets[oc]; k < cnf->offsets[oc + 1]; ++k) {
                        if (cnf->lits[k] == new_true) { has_new = true; break; }
                    }
                    if (!has_new) {
                        newly_unsat_cnt++;
                        sole_other = oc;
                        if (newly_unsat_cnt > 1) break;
                    }
                }
            }
            if (newly_unsat_cnt == 1 && sole_other != UINT32_MAX && state[sole_other] == 1) {
                /* sole_other is now proven necessary (transition clause)! */
                state[sole_other] = 2;
                rotated++;
                Lit unit = mkLit(sel_var[sole_other], false);
                solver_add_clause(s, &unit, 1);
                if (s->error) { stack_sz = 0; break; }

                /* Apply the flip to model & sat_cnt so we can rotate further from sole_other! */
                model[v] = (model[v] == TRUE) ? FALSE : TRUE;
                for (uint32_t p = fi->head[old_true]; p < fi->head[old_true + 1u]; ++p) {
                    uint32_t oc = fi->clauses[p];
                    if (state[oc]) sat_cnt[oc]--;
                }
                for (uint32_t p = fi->head[new_true]; p < fi->head[new_true + 1u]; ++p) {
                    uint32_t oc = fi->clauses[p];
                    if (state[oc]) sat_cnt[oc]++;
                }
                if (stack_sz == stack_cap) {
                    uint32_t nc = stack_cap * 2u;
                    uint32_t *ns = realloc(stack, nc * sizeof *ns);
                    if (!ns) break;
                    stack = ns; stack_cap = nc;
                }
                stack[stack_sz++] = sole_other;
                break; /* Model changed; continue rotating from sole_other */
            }
        }
    }

    free(sat_cnt);
    free(stack);
    return rotated;
}

MusResult bsat_compute_mus(const Solver *template_solver,
                           uint32_t orig_vars,
                           const SolverOpts *user_opts) {
    MusResult res;
    memset(&res, 0, sizeof res);
    res.status = UNDEF;

    InputCnf cnf;
    if (!input_cnf_extract(template_solver, orig_vars, &cnf)) return res;
    res.orig_clauses = cnf.num_clauses;

    /* Fast path: an empty clause by itself is a size-1 MUS! */
    for (uint32_t c = 0; c < cnf.num_clauses; ++c) {
        if (cnf.offsets[c + 1] == cnf.offsets[c]) {
            res.clause_ids = malloc(sizeof(uint32_t));
            if (res.clause_ids) {
                res.clause_ids[0] = c;
                res.size = 1;
                res.status = FALSE;
                res.minimal = true;
            }
            input_cnf_free(&cnf);
            return res;
        }
    }
    if (cnf.num_clauses == 0) {
        res.status = TRUE;
        res.minimal = true;
        input_cnf_free(&cnf);
        return res;
    }

    double start_cpu = solver_cpu_time();
    double deadline = (user_opts && user_opts->max_time > 0) ? (start_cpu + user_opts->max_time) : 0.0;

    Solver *s = make_incremental_solver(&cnf, user_opts, false);
    if (!s) { input_cnf_free(&cnf); return res; }

    Var *sel_var = malloc((size_t)cnf.num_clauses * sizeof *sel_var);
    uint8_t *state = calloc((size_t)cnf.num_clauses, 1); /* 0=dropped, 1=candidate, 2=necessary */
    uint8_t *in_core = calloc((size_t)cnf.num_clauses, 1);
    Lit *assumps = malloc((size_t)cnf.num_clauses * sizeof *assumps);
    if (!sel_var || !state || !in_core || !assumps) {
        free(sel_var); free(state); free(in_core); free(assumps);
        solver_free(s); input_cnf_free(&cnf);
        return res;
    }

    uint32_t sel_base = s->num_vars + 1u;
    Lit *cbuf = NULL;
    uint32_t cbuf_cap = 0;
    bool ok = true;
    for (uint32_t c = 0; c < cnf.num_clauses && ok; ++c) {
        Var sv = solver_new_var(s);
        if (sv == INVALID_VAR) { ok = false; break; }
        sel_var[c] = sv;
        state[c] = 1;
        uint32_t sz = cnf.offsets[c + 1] - cnf.offsets[c];
        if (sz + 1 > cbuf_cap) {
            uint32_t nc = sz + 16u;
            Lit *nl = realloc(cbuf, (size_t)nc * sizeof *nl);
            if (!nl) { ok = false; break; }
            cbuf = nl; cbuf_cap = nc;
        }
        memcpy(cbuf, cnf.lits + cnf.offsets[c], (size_t)sz * sizeof *cbuf);
        cbuf[sz] = mkLit(sv, true); /* (~sv | C_c) => assuming +sv activates C_c */
        solver_add_clause(s, cbuf, sz + 1);
        if (s->error) ok = false;
    }
    free(cbuf);

    if (!ok) {
        free(sel_var); free(state); free(in_core); free(assumps);
        solver_free(s); input_cnf_free(&cnf);
        return res;
    }

    /* Step 1: Initial solve with all selectors active to verify UNSAT & trim to first core. */
    for (uint32_t c = 0; c < cnf.num_clauses; ++c) assumps[c] = mkLit(sel_var[c], false);
    res.solves = 1;
    lbool r0 = solver_solve_with_assumptions(s, assumps, cnf.num_clauses);
    if (r0 != FALSE) {
        res.status = r0;
        res.minimal = (r0 == TRUE);
        free(sel_var); free(state); free(in_core); free(assumps);
        solver_free(s); input_cnf_free(&cnf);
        return res;
    }

    /* Keep only clauses present in the initial UNSAT core! */
    uint32_t core_sz = 0;
    const Lit *core = solver_conflict(s, &core_sz);
    memset(state, 0, cnf.num_clauses);
    for (uint32_t i = 0; i < core_sz; ++i) {
        Var sv = var(core[i]);
        if (sv >= sel_base && sv < sel_base + cnf.num_clauses) {
            state[sv - sel_base] = 1;
        }
    }

    FlipIndex fi;
    bool have_fi = flip_index_build(&cnf, &fi);
    lbool *model = malloc((size_t)(cnf.num_vars + 1u) * sizeof *model);
    if (!model) ok = false;

    /* Step 2: Deletion-based MUS minimization with core-based bulk drop + RMR. */
    bool timed_out = false;
    for (uint32_t c = 0; c < cnf.num_clauses && ok; ++c) {
        if (state[c] != 1) continue;
        if (deadline > 0) {
            double rem = deadline - solver_cpu_time();
            if (rem <= 0.0) { timed_out = true; break; }
            s->opts.max_time = rem;
        }
        uint32_t na = 0;
        for (uint32_t j = 0; j < cnf.num_clauses; ++j) {
            if (j != c && state[j] == 1) assumps[na++] = mkLit(sel_var[j], false);
        }
        res.solves++;
        lbool r = solver_solve_with_assumptions(s, assumps, na);
        if (r == TRUE) {
            /* Clause c is necessary (transition clause)! Permanently harden +sel_var[c]. */
            state[c] = 2;
            Lit unit = mkLit(sel_var[c], false);
            solver_add_clause(s, &unit, 1);
            if (s->error) { ok = false; break; }

            if (have_fi) {
                model[0] = UNDEF;
                for (Var v = 1; v <= cnf.num_vars; ++v) {
                    lbool mv = solver_model_value(s, v);
                    model[v] = (mv == TRUE) ? TRUE : FALSE;
                }
                res.rotated += mus_model_rotate(&cnf, &fi, s, sel_var, model, state, c);
                if (s->error) { ok = false; break; }
            }
        } else if (r == FALSE) {
            /* Clause c is redundant; drop c AND any other candidate absent from the new core! */
            state[c] = 0;
            uint32_t ncsz = 0;
            const Lit *ncore = solver_conflict(s, &ncsz);
            memset(in_core, 0, cnf.num_clauses);
            for (uint32_t i = 0; i < ncsz; ++i) {
                Var sv = var(ncore[i]);
                if (sv >= sel_base && sv < sel_base + cnf.num_clauses) {
                    in_core[sv - sel_base] = 1;
                }
            }
            for (uint32_t j = 0; j < cnf.num_clauses; ++j) {
                if (state[j] == 1 && !in_core[j]) state[j] = 0;
            }
        } else {
            timed_out = true;
            break;
        }
    }

    if (ok) {
        uint32_t msz = 0;
        for (uint32_t c = 0; c < cnf.num_clauses; ++c) {
            if (state[c] != 0) msz++;
        }
        res.clause_ids = malloc((size_t)(msz ? msz : 1u) * sizeof *res.clause_ids);
        if (res.clause_ids) {
            uint32_t k = 0;
            for (uint32_t c = 0; c < cnf.num_clauses; ++c) {
                if (state[c] != 0) res.clause_ids[k++] = c;
            }
            res.size = k;
            res.status = FALSE;
            res.minimal = !timed_out;
        }
    }

    if (have_fi) flip_index_free(&fi);
    free(model);
    free(sel_var);
    free(state);
    free(in_core);
    free(assumps);
    solver_free(s);
    input_cnf_free(&cnf);
    return res;
}
