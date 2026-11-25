/*********************************************************************
 * BSAT Competition Solver - Core-Guided Weighted Partial MaxSAT (RC2/OLL)
 *********************************************************************/

#include "../include/maxsat.h"
#include "../include/pb_enc.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

void maxsat_instance_init(MaxSatInstance *inst) {
    memset(inst, 0, sizeof *inst);
}

void maxsat_instance_free(MaxSatInstance *inst) {
    if (!inst) return;
    for (uint32_t i = 0; i < inst->num_clauses; ++i) {
        free(inst->clauses[i].lits);
    }
    free(inst->clauses);
    memset(inst, 0, sizeof *inst);
}

void maxsat_result_free(MaxSatResult *res) {
    if (!res) return;
    free(res->model);
    res->model = NULL;
}

bool maxsat_add_clause(MaxSatInstance *inst, const Lit *lits, uint32_t size, uint64_t weight) {
    if (!inst) return false;
    if (inst->num_clauses == inst->cap_clauses) {
        uint32_t nc = inst->cap_clauses ? inst->cap_clauses * 2u : 32u;
        MaxSatClause *p = realloc(inst->clauses, (size_t)nc * sizeof *p);
        if (!p) return false;
        inst->clauses = p;
        inst->cap_clauses = nc;
    }
    Lit *copy = NULL;
    if (size > 0) {
        copy = malloc((size_t)size * sizeof *copy);
        if (!copy) return false;
        memcpy(copy, lits, (size_t)size * sizeof *copy);
        for (uint32_t i = 0; i < size; ++i) {
            Var v = var(lits[i]);
            if (v > inst->num_vars) inst->num_vars = v;
        }
    }
    inst->clauses[inst->num_clauses++] = (MaxSatClause){copy, size, weight};
    if (weight == 0) {
        inst->num_hard++;
    } else {
        inst->num_soft++;
        if (inst->total_soft_weight > UINT64_MAX - weight) inst->total_soft_weight = UINT64_MAX;
        else inst->total_soft_weight += weight;
    }
    return true;
}

/* =====================================================================
 * WCNF Parser (Classic "p wcnf" + MSE 2022+ "h <lits> 0" / "<w> <lits> 0")
 * ===================================================================== */

static int wcnf_next_line(FILE *fp, char **buf, size_t *cap) {
    if (!*buf) {
        *cap = 1024;
        *buf = malloc(*cap);
        if (!*buf) return -1;
    }
    size_t len = 0;
    for (;;) {
        int c = fgetc(fp);
        if (c == EOF) {
            if (ferror(fp)) return -1;
            if (len == 0) return 0;
            break;
        }
        if (len + 2 >= *cap) {
            size_t ncap = (*cap) * 2u;
            char *p = realloc(*buf, ncap);
            if (!p) return -1;
            *buf = p;
            *cap = ncap;
        }
        (*buf)[len++] = (char)c;
        if (c == '\n') break;
    }
    (*buf)[len] = '\0';
    return 1;
}

static bool parse_u64_tok(const char *s, uint64_t *out) {
    if (!s || !isdigit((unsigned char)*s)) return false;
    char *end = NULL;
    errno = 0;
    unsigned long long v = strtoull(s, &end, 10);
    if (errno || end == s || *end != '\0') return false;
    *out = (uint64_t)v;
    return true;
}

static bool parse_i64_tok(const char *s, int64_t *out) {
    if (!s || (!isdigit((unsigned char)*s) && *s != '-' && *s != '+')) return false;
    char *end = NULL;
    errno = 0;
    long long v = strtoll(s, &end, 10);
    if (errno || end == s || *end != '\0') return false;
    *out = (int64_t)v;
    return true;
}

bool maxsat_parse_stream(MaxSatInstance *inst, FILE *fp) {
    if (!inst || !fp) return false;
    maxsat_instance_init(inst);

    char *line = NULL;
    size_t line_cap = 0;
    Lit *lits = NULL;
    uint32_t lits_cap = 0;

    bool has_header = false;
    bool unweighted_cnf = false;
    uint64_t top_weight = UINT64_MAX;
    bool ok = true;

    for (;;) {
        int r = wcnf_next_line(fp, &line, &line_cap);
        if (r < 0) { ok = false; break; }
        if (r == 0) break;

        char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '\0' || *p == 'c') continue;

        if (*p == 'p' && isspace((unsigned char)p[1])) {
            char fmt[16] = {0};
            unsigned long long nv = 0, nc = 0, top = 0;
            int nread = sscanf(p + 1, "%15s %llu %llu %llu", fmt, &nv, &nc, &top);
            if (strcmp(fmt, "wcnf") == 0 && nread >= 3) {
                has_header = true;
                if (nv > MAX_VARS) { ok = false; break; }
                if ((uint32_t)nv > inst->num_vars) inst->num_vars = (uint32_t)nv;
                top_weight = (nread >= 4) ? (uint64_t)top : UINT64_MAX;
            } else if (strcmp(fmt, "cnf") == 0 && nread >= 3) {
                has_header = true;
                unweighted_cnf = true;
                if (nv > MAX_VARS) { ok = false; break; }
                if ((uint32_t)nv > inst->num_vars) inst->num_vars = (uint32_t)nv;
            } else {
                ok = false; break;
            }
            continue;
        }

        /* Tokenize clause line */
        uint64_t weight = 0;
        bool is_hard = false;

        char *save = NULL;
        char *tok = strtok_r(p, " \t\r\n", &save);
        if (!tok) continue;

        if (unweighted_cnf) {
            weight = 1;
        } else if (strcmp(tok, "h") == 0) {
            is_hard = true;
            tok = strtok_r(NULL, " \t\r\n", &save);
        } else {
            if (!parse_u64_tok(tok, &weight) || weight == 0) { ok = false; break; }
            if (has_header && weight >= top_weight) is_hard = true;
            tok = strtok_r(NULL, " \t\r\n", &save);
        }

        uint32_t sz = 0;
        bool saw_zero = false;
        while (tok) {
            int64_t v = 0;
            if (!parse_i64_tok(tok, &v)) { ok = false; break; }
            if (v == 0) { saw_zero = true; break; }
            if (v < -(int64_t)MAX_VARS || v > (int64_t)MAX_VARS) { ok = false; break; }
            if (sz == lits_cap) {
                uint32_t nc = lits_cap ? lits_cap * 2u : 16u;
                Lit *nl = realloc(lits, (size_t)nc * sizeof *nl);
                if (!nl) { ok = false; break; }
                lits = nl;
                lits_cap = nc;
            }
            lits[sz++] = fromDimacs((int)v);
            tok = strtok_r(NULL, " \t\r\n", &save);
        }
        if (!ok || !saw_zero) { ok = false; break; }

        if (!maxsat_add_clause(inst, lits, sz, is_hard ? 0ULL : weight)) {
            ok = false; break;
        }
    }

    free(lits);
    free(line);
    if (!ok) maxsat_instance_free(inst);
    return ok;
}

bool maxsat_parse_file(MaxSatInstance *inst, const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return false;
    bool ok = maxsat_parse_stream(inst, fp);
    fclose(fp);
    return ok;
}

bool maxsat_parse_string(MaxSatInstance *inst, const char *text) {
    if (!text) return false;
    FILE *fp = fmemopen((void *)text, strlen(text), "r");
    if (!fp) return false;
    bool ok = maxsat_parse_stream(inst, fp);
    fclose(fp);
    return ok;
}

/* =====================================================================
 * Model Verification & Exact Cost Evaluation
 * ===================================================================== */

static bool clause_satisfied_by_model(const MaxSatClause *c, const lbool *model) {
    for (uint32_t j = 0; j < c->size; ++j) {
        Lit l = c->lits[j];
        bool pos = (model[var(l)] == TRUE);
        if (sign(l) ? !pos : pos) return true;
    }
    return false;
}

bool maxsat_verify_model(const MaxSatInstance *inst, const lbool *model, uint64_t *out_cost) {
    if (!inst || !model) return false;
    uint64_t cost = 0;
    for (uint32_t i = 0; i < inst->num_clauses; ++i) {
        const MaxSatClause *c = &inst->clauses[i];
        if (!clause_satisfied_by_model(c, model)) {
            if (c->weight == 0) return false; /* Hard clause violated */
            if (cost > UINT64_MAX - c->weight) cost = UINT64_MAX;
            else cost += c->weight;
        }
    }
    if (out_cost) *out_cost = cost;
    return true;
}

/* =====================================================================
 * RC2 / OLL Core-Guided Weighted Partial MaxSAT Engine
 * ===================================================================== */

typedef struct {
    Lit sel;           /* Assumption literal; TRUE = soft constraint satisfied */
    uint64_t weight;   /* Active remaining weight (>0 when live, 0 when deactivated) */
    int32_t tot_idx;   /* -1 for original soft selector, >=0 for OLL totalizer index */
    uint32_t tot_bound;/* Output index in totalizer (sel == neg(out[tot_bound])) */
} SoftSel;

typedef struct {
    SoftSel *items;
    uint32_t size;
    uint32_t cap;
    /* Direct map lit -> index+1 in items[] (0 = absent) for O(1) core lookup & merging. */
    uint32_t *lit_to_idx;
    uint32_t lit_map_cap;
    PbTotalizer **tots;
    uint32_t num_tots;
    uint32_t cap_tots;
} Rc2State;

static bool rc2_ensure_lit_map(Rc2State *rc, Lit l) {
    uint32_t need = (uint32_t)l + 2u;
    if (need <= rc->lit_map_cap) return true;
    uint32_t nc = rc->lit_map_cap ? rc->lit_map_cap * 2u : 256u;
    while (nc < need) nc *= 2u;
    uint32_t *p = realloc(rc->lit_to_idx, (size_t)nc * sizeof *p);
    if (!p) return false;
    memset(p + rc->lit_map_cap, 0, (size_t)(nc - rc->lit_map_cap) * sizeof *p);
    rc->lit_to_idx = p;
    rc->lit_map_cap = nc;
    return true;
}

static bool rc2_add_or_merge_sel(Rc2State *rc, Lit sel, uint64_t weight,
                                 int32_t tot_idx, uint32_t tot_bound) {
    if (weight == 0) return true;
    if (!rc2_ensure_lit_map(rc, sel)) return false;
    uint32_t existing = rc->lit_to_idx[sel];
    if (existing > 0) {
        SoftSel *e = &rc->items[existing - 1];
        if (e->weight > UINT64_MAX - weight) e->weight = UINT64_MAX;
        else e->weight += weight;
        if (tot_idx >= 0 && e->tot_idx < 0) {
            e->tot_idx = tot_idx;
            e->tot_bound = tot_bound;
        }
        return true;
    }
    if (rc->size == rc->cap) {
        uint32_t nc = rc->cap ? rc->cap * 2u : 64u;
        SoftSel *p = realloc(rc->items, (size_t)nc * sizeof *p);
        if (!p) return false;
        rc->items = p;
        rc->cap = nc;
    }
    uint32_t idx = rc->size++;
    rc->items[idx] = (SoftSel){sel, weight, tot_idx, tot_bound};
    rc->lit_to_idx[sel] = idx + 1;
    return true;
}

static int32_t rc2_push_totalizer(Rc2State *rc, PbTotalizer *t) {
    if (rc->num_tots == rc->cap_tots) {
        uint32_t nc = rc->cap_tots ? rc->cap_tots * 2u : 16u;
        PbTotalizer **p = realloc(rc->tots, (size_t)nc * sizeof *p);
        if (!p) return -1;
        rc->tots = p;
        rc->cap_tots = nc;
    }
    int32_t idx = (int32_t)rc->num_tots++;
    rc->tots[idx] = t;
    return idx;
}

static void rc2_free(Rc2State *rc) {
    for (uint32_t i = 0; i < rc->num_tots; ++i) pb_totalizer_free(rc->tots[i]);
    free(rc->tots);
    free(rc->items);
    free(rc->lit_to_idx);
    memset(rc, 0, sizeof *rc);
}

static uint64_t rc2_max_weight_below(const Rc2State *rc, uint64_t ceiling) {
    uint64_t best = 0;
    for (uint32_t i = 0; i < rc->size; ++i) {
        uint64_t w = rc->items[i].weight;
        if (w > 0 && w < ceiling && w > best) best = w;
    }
    return best;
}

/* Conflict-bounded deletion-based core minimization: shrinks core in-place. */
static void rc2_minimize_core(Solver *s, Lit *core, uint32_t *core_sz,
                              uint32_t *solves, double deadline) {
    if (*core_sz <= 1 || *core_sz > 64) return;
    uint32_t saved_max_conf = s->opts.max_conflicts;
    double saved_max_time = s->opts.max_time;
    Lit *trial = malloc((size_t)(*core_sz) * sizeof *trial);
    if (!trial) return;

    uint32_t i = 0;
    while (i < *core_sz && *core_sz > 1) {
        if (deadline > 0 && solver_cpu_time() >= deadline) break;
        uint32_t n = 0;
        for (uint32_t j = 0; j < *core_sz; ++j) {
            if (j != i) trial[n++] = core[j];
        }
        s->opts.max_conflicts = 1000;
        if (deadline > 0) {
            double rem = deadline - solver_cpu_time();
            s->opts.max_time = rem > 0.01 ? rem : 0.01;
        }
        (*solves)++;
        lbool r = solver_solve_with_assumptions(s, trial, n);
        if (r == FALSE) {
            /* Shrink core to the returned sub-core! */
            uint32_t csz = 0;
            const Lit *sub = solver_conflict(s, &csz);
            if (csz < *core_sz) {
                for (uint32_t k = 0; k < csz; ++k) core[k] = neg(sub[k]);
                *core_sz = csz;
                i = 0; /* Restart scan on strictly smaller core */
                continue;
            }
            /* Otherwise drop core[i] */
            memmove(core + i, core + i + 1, (size_t)(*core_sz - i - 1) * sizeof *core);
            (*core_sz)--;
            continue;
        }
        i++;
    }

    free(trial);
    s->opts.max_conflicts = saved_max_conf;
    s->opts.max_time = saved_max_time;
}

MaxSatResult maxsat_solve(const MaxSatInstance *inst, const SolverOpts *user_opts) {
    MaxSatResult res;
    memset(&res, 0, sizeof res);
    res.status = UNDEF;
    if (!inst) return res;

    SolverOpts opts = user_opts ? *user_opts : default_opts();
    double start_cpu = solver_cpu_time();
    double deadline = (opts.max_time > 0) ? (start_cpu + opts.max_time) : 0.0;

    /* Incremental assumption-friendly solver config (retains learned clauses across cores). */
    opts.reuse_learnts = true;
    opts.inprocess = false;
    opts.sbva = false;
    opts.sym = false;
    opts.elim = false;
    opts.bce = false;

    Solver *s = solver_new_with_opts(&opts);
    if (!s) return res;

    while (s->num_vars < inst->num_vars) {
        if (solver_new_var(s) == INVALID_VAR) { solver_free(s); return res; }
    }

    Rc2State rc;
    memset(&rc, 0, sizeof rc);
    uint64_t lower_bound = 0;
    uint64_t upper_bound = UINT64_MAX;
    bool hard_unsat = false;
    bool ok = true;

    Lit *cls_buf = NULL;
    uint32_t cls_cap = 0;

    for (uint32_t i = 0; i < inst->num_clauses && ok; ++i) {
        const MaxSatClause *c = &inst->clauses[i];
        if (c->weight == 0) {
            if (c->size == 0) {
                hard_unsat = true;
                break;
            }
            solver_add_clause(s, c->lits, c->size);
            if (s->error) ok = false;
        } else {
            if (c->size == 0) {
                lower_bound += c->weight;
            } else if (c->size == 1) {
                ok = rc2_add_or_merge_sel(&rc, c->lits[0], c->weight, -1, 0);
            } else {
                Var rel = solver_new_var(s);
                if (rel == INVALID_VAR) { ok = false; break; }
                if (c->size + 1 > cls_cap) {
                    uint32_t nc = c->size + 16u;
                    Lit *nl = realloc(cls_buf, (size_t)nc * sizeof *nl);
                    if (!nl) { ok = false; break; }
                    cls_buf = nl;
                    cls_cap = nc;
                }
                memcpy(cls_buf, c->lits, (size_t)c->size * sizeof *cls_buf);
                cls_buf[c->size] = mkLit(rel, false);
                solver_add_clause(s, cls_buf, c->size + 1);
                if (s->error) { ok = false; break; }
                ok = rc2_add_or_merge_sel(&rc, mkLit(rel, true), c->weight, -1, 0);
            }
        }
    }
    free(cls_buf);

    if (hard_unsat) {
        res.status = FALSE;
        rc2_free(&rc);
        solver_free(s);
        return res;
    }
    if (!ok) {
        rc2_free(&rc);
        solver_free(s);
        return res;
    }

    Lit *assumps = NULL;
    uint32_t assumps_cap = 0;
    Lit *core_buf = NULL;
    uint32_t core_cap = 0;
    lbool *cur_model = malloc((size_t)(inst->num_vars + 1u) * sizeof(lbool));
    if (!cur_model) { rc2_free(&rc); solver_free(s); return res; }
    cur_model[0] = UNDEF;

    /* Start stratification at the highest active soft weight. */
    uint64_t strat = rc2_max_weight_below(&rc, UINT64_MAX);
    if (strat == 0) strat = 1;

    while (ok) {
        if (deadline > 0) {
            double rem = deadline - solver_cpu_time();
            if (rem <= 0.0) break;
            s->opts.max_time = rem;
        }

        /* Collect assumptions for all live selectors with weight >= strat. */
        uint32_t n_assumps = 0;
        if (rc.size > assumps_cap) {
            uint32_t nc = rc.size + 64u;
            Lit *na = realloc(assumps, (size_t)nc * sizeof *na);
            if (!na) { ok = false; break; }
            assumps = na;
            assumps_cap = nc;
        }
        for (uint32_t i = 0; i < rc.size; ++i) {
            if (rc.items[i].weight >= strat) {
                assumps[n_assumps++] = rc.items[i].sel;
            }
        }

        res.solves++;
        lbool r = solver_solve_with_assumptions(s, assumps, n_assumps);
        if (r == TRUE) {
            /* Extract & verify model on original instance. */
            for (Var v = 1; v <= inst->num_vars; ++v) {
                lbool mv = solver_model_value(s, v);
                cur_model[v] = (mv == TRUE) ? TRUE : FALSE;
            }
            uint64_t mcost = 0;
            if (!maxsat_verify_model(inst, cur_model, &mcost)) {
                ok = false; break;
            }
            if (mcost < upper_bound) {
                upper_bound = mcost;
                res.status = TRUE;
                res.cost = upper_bound;
                if (!res.model) {
                    res.model = malloc((size_t)(inst->num_vars + 1u) * sizeof(lbool));
                    if (!res.model) { ok = false; break; }
                }
                memcpy(res.model, cur_model, (size_t)(inst->num_vars + 1u) * sizeof(lbool));
                if (!opts.quiet) {
                    printf("o %llu\n", (unsigned long long)upper_bound);
                    fflush(stdout);
                }
            }
            if (lower_bound >= upper_bound) {
                res.optimum = true;
                break;
            }

            /* Hardening: any active soft with weight > upper_bound - lower_bound
             * cannot be falsified in any optimal solution (cost would exceed UB). */
            uint64_t gap = upper_bound - lower_bound;
            bool hardened_unsat = false;
            for (uint32_t i = 0; i < rc.size; ++i) {
                if (rc.items[i].weight > 0 && rc.items[i].weight > gap) {
                    Lit unit = rc.items[i].sel;
                    solver_add_clause(s, &unit, 1);
                    if (s->error) { ok = false; break; }
                    if (rc.items[i].weight < strat) hardened_unsat = true;
                    rc.items[i].weight = 0;
                }
            }
            if (!ok) break;

            /* Lower stratification threshold to next distinct active weight. */
            uint64_t next_strat = rc2_max_weight_below(&rc, strat);
            if (next_strat == 0) {
                if (!hardened_unsat) {
                    /* All active selectors were already assumed and satisfied -> optimal! */
                    res.optimum = true;
                    break;
                }
                strat = 1;
            } else {
                strat = next_strat;
            }
        } else if (r == FALSE) {
            uint32_t csz = 0;
            const Lit *raw_core = solver_conflict(s, &csz);
            if (csz == 0) {
                /* Core is empty => hard clauses (or hardened units after UB) are UNSAT! */
                if (res.status == TRUE) {
                    res.optimum = true;
                } else {
                    res.status = FALSE;
                }
                break;
            }
            res.cores++;
            if (csz > core_cap) {
                uint32_t nc = csz + 32u;
                Lit *nb = realloc(core_buf, (size_t)nc * sizeof *nb);
                if (!nb) { ok = false; break; }
                core_buf = nb;
                core_cap = nc;
            }
            for (uint32_t i = 0; i < csz; ++i) core_buf[i] = neg(raw_core[i]);

            /* Shrink core with conflict-bounded deletion. */
            rc2_minimize_core(s, core_buf, &csz, &res.solves, deadline);
            if (csz == 0) {
                if (res.status == TRUE) res.optimum = true;
                else res.status = FALSE;
                break;
            }

            /* Find minimum weight across core literals. */
            uint64_t w_min = UINT64_MAX;
            for (uint32_t i = 0; i < csz; ++i) {
                Lit sel = core_buf[i];
                uint32_t idx1 = (sel < rc.lit_map_cap) ? rc.lit_to_idx[sel] : 0;
                if (idx1 == 0 || rc.items[idx1 - 1].weight == 0) { ok = false; break; }
                if (rc.items[idx1 - 1].weight < w_min) w_min = rc.items[idx1 - 1].weight;
            }
            if (!ok || w_min == UINT64_MAX) { ok = false; break; }

            if (lower_bound > UINT64_MAX - w_min) lower_bound = UINT64_MAX;
            else lower_bound += w_min;

            /* Split w_min off every core literal; advance any OLL totalizer bounds. */
            for (uint32_t i = 0; i < csz && ok; ++i) {
                Lit sel = core_buf[i];
                uint32_t idx = rc.lit_to_idx[sel] - 1;
                rc.items[idx].weight -= w_min;
                if (rc.items[idx].tot_idx >= 0) {
                    int32_t ti = rc.items[idx].tot_idx;
                    uint32_t next_b = rc.items[idx].tot_bound + 1u;
                    PbTotalizer *tot = rc.tots[ti];
                    if (next_b < tot->num_inputs) {
                        if (!pb_totalizer_increase_bound(tot, s, next_b)) { ok = false; break; }
                        Lit next_sel = neg(pb_totalizer_out(tot, next_b));
                        ok = rc2_add_or_merge_sel(&rc, next_sel, w_min, ti, next_b);
                    }
                }
            }
            if (!ok) break;

            /* If core has >= 2 literals, create a new OLL Totalizer counting falsified core lits. */
            if (csz >= 2) {
                Lit *viol = malloc((size_t)csz * sizeof *viol);
                if (!viol) { ok = false; break; }
                for (uint32_t i = 0; i < csz; ++i) viol[i] = neg(core_buf[i]);
                PbTotalizer *tot = pb_totalizer_create(s, viol, csz, 1);
                free(viol);
                if (!tot) { ok = false; break; }
                int32_t ti = rc2_push_totalizer(&rc, tot);
                if (ti < 0) { pb_totalizer_free(tot); ok = false; break; }
                Lit bound1 = neg(pb_totalizer_out(tot, 1));
                ok = rc2_add_or_merge_sel(&rc, bound1, w_min, ti, 1);
            }
            if (!ok) break;

            if (upper_bound < UINT64_MAX && lower_bound >= upper_bound) {
                res.optimum = true;
                break;
            }
            /* Re-harden if LB closed the gap to UB. */
            if (upper_bound < UINT64_MAX) {
                uint64_t gap = upper_bound - lower_bound;
                for (uint32_t i = 0; i < rc.size; ++i) {
                    if (rc.items[i].weight > 0 && rc.items[i].weight > gap) {
                        Lit unit = rc.items[i].sel;
                        solver_add_clause(s, &unit, 1);
                        if (s->error) { ok = false; break; }
                        rc.items[i].weight = 0;
                    }
                }
            }
        } else {
            /* Timeout / cancellation: preserve best incumbent if found. */
            break;
        }
    }

    res.lower_bound = lower_bound;
    free(cur_model);
    free(core_buf);
    free(assumps);
    rc2_free(&rc);
    solver_free(s);
    return res;
}
