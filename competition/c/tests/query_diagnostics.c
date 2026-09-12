/* Test-only facade: compile instead of bsat.o, never into an installed library.
   Uses the production facade directly; exposes pre-input ablations and snapshots
   without freezing internal Solver structures into the public ABI. */
#include "../src/bsat.c"
#include <string.h>

BSAT_API int
bsat_diagnostic_configure(bsat *s, const char *profile, int accounting)
{
    if (bsat_error(s) || s->started || !profile) return 0;
#ifndef BSAT_ASSUMPTION_CONGRUENCE
    /* These profiles require the archived assumption-preprocessing prototype. */
    if (!strcmp(profile, "fresh-elimination") || !strcmp(profile, "fresh-substitution") ||
        !strcmp(profile, "fresh-congruence"))
        return 0;
#endif
    SolverOpts *o = &s->core->opts;

    if (!strcmp(profile, "control")) {
    } else if (!strcmp(profile, "reuse-prefix")) {
        o->reuse_trail = true;
    } else if (!strcmp(profile, "no-reuse-prefix")) {
        o->reuse_trail = false;
    } else if (!strcmp(profile, "no-retained-elim")) {
        o->retained_elim = false;
    } else if (!strcmp(profile, "seeded-ema")) {
        o->unbiased_ema = false;
    } else if (!strcmp(profile, "recursive-minimize")) {
        o->iterative_minimize = false;
    } else if (!strcmp(profile, "legacy-restarts")) {
        SolverOpts defaults = default_opts();

        o->glucose_fast_alpha = defaults.glucose_fast_alpha;
        o->glucose_slow_alpha = defaults.glucose_slow_alpha;
        o->glucose_min_conflicts = defaults.glucose_min_conflicts;
        o->glucose_k = defaults.glucose_k;
    } else if (!strcmp(profile, "focused-restarts")) {
        o->glucose_fast_alpha = 32.0 / 33.0;
        o->glucose_slow_alpha = 99999.0 / 100000.0;
        o->glucose_min_conflicts = 2;
        o->glucose_k = 1.0 / 1.1;
    } else if (!strcmp(profile, "elim-conservative")) {
        o->elim_max_occ = 100;
        o->elim_grow = 0;
    } else if (!strcmp(profile, "elim-wide")) {
        o->elim_max_occ = 100;
        o->elim_grow = 4;
    } else if (!strcmp(profile, "vivify")) {
        o->inprocess = true;
        o->restart_assumptions = false;
    } else if (!strcmp(profile, "protect-learnts")) {
        o->dynamic_lbd = true;
        o->protect_used = true;
    } else if (!strcmp(profile, "dynamic")) {
        o->dynamic_lbd = true;
        o->protect_used = true;
        o->reduce_increment = 1000;
    } else if (!strcmp(profile, "large-db"))
        o->reduce_interval = 100000;
    else if (!strcmp(profile, "no-reduce"))
        o->reduce_interval = UINT32_MAX;
    else if (!strcmp(profile, "lrb")) {
        o->vmtf = false;
        o->lrb = true;
    } else if (!strcmp(profile, "fresh-elimination")) {
        o->reuse_learnts = false;
        o->congruence = true;
        o->equiv = true;
        o->equiv_budget = 100000000;
        o->elim = true;
        o->preprocess_budget = 100000000;
    } else if (!strcmp(profile, "fresh-substitution")) {
        o->reuse_learnts = false;
        o->congruence = true;
        o->equiv = true;
        o->equiv_budget = 100000000;
    } else if (!strcmp(profile, "fresh-congruence")) {
        o->reuse_learnts = false;
        o->congruence = true;
    } else if (!strcmp(profile, "fresh"))
        o->reuse_learnts = false;
    else if (!strcmp(profile, "iterative"))
        o->iterative_minimize = true;
    else if (!strcmp(profile, "glue-three"))
        o->glue_lbd = 3;
    else if (!strcmp(profile, "sustained")) {
        o->restart_first = UINT32_MAX;
        o->rephase = false;
    } else if (!strcmp(profile, "reduce-ternary"))
        o->retain_ternary = false;
    else if (!strcmp(profile, "queue"))
        o->vmtf = true;
    else if (!strcmp(profile, "vsids")) {
        o->vmtf = false;
        o->reuse_trail = false;
    } else if (!strcmp(profile, "positive")) {
        o->phase_saving = false;
        o->rephase = false;
    } else if (!strcmp(profile, "no-rephase"))
        o->rephase = false;
    else if (!strcmp(profile, "alternating"))
        o->alternating = true;
    else if (!strcmp(profile, "chrono-short")) {
        o->chrono = true;
        o->chrono_levels = 16;
    } else if (!strcmp(profile, "chrono"))
        o->chrono = true;
    else if (!strcmp(profile, "growing-reduce"))
        o->reduce_increment = 1000;
    else if (!strcmp(profile, "probe-default-budget"))
        o->probing = true;
    else if (!strcmp(profile, "journal-off")) {
        if (!s->journal) return 0;
        if (fclose(s->journal)) {
            s->core->error = true;
            s->journal = NULL;
            s->core->proof_journal = NULL;
            return 0;
        }
        s->journal = NULL;
        s->core->proof_journal = NULL;
    } else
        return 0;
    o->accounting = accounting != 0;
    return 1;
}

BSAT_API int
bsat_diagnostic_write(bsat *s, const char *path)
{
    if (bsat_error(s) || !path) return 0;
    FILE *f = fopen(path, "wx");

    if (!f) return 0;
    Solver *core = s->core;

    fprintf(f, "{\"ordered_queue\":%s,", core->opts.vmtf ? "true" : "false");
    fprintf(f, "\"dynamic_lbd\":%s,", core->opts.dynamic_lbd ? "true" : "false");
    fprintf(f, "\"protect_used\":%s,", core->opts.protect_used ? "true" : "false");
#ifdef BSAT_SEARCH_DIAGNOSTICS
    fputs("\"accounting_available\":true,", f);
#else
    fputs("\"accounting_available\":false,", f);
#endif
    fprintf(f, "\"retain_ternary\":%s,", core->opts.retain_ternary ? "true" : "false");
#ifdef BSAT_CERTIFIED_SSR
    fprintf(f, "\"ssr_candidates\":%llu,\"ssr_inspections\":%llu,\"ssr_strengthened\":%llu,",
            (unsigned long long)core->ssr_candidates, (unsigned long long)core->ssr_inspections,
            (unsigned long long)core->ssr_strengthened);
#endif
#define STAT(field) fprintf(f, "\"" #field "\":%llu,", (unsigned long long)core->stats.field)
    fprintf(f, "\"eliminated_variables\":%llu,",
            (unsigned long long)(core->elim ? core->elim->vars_eliminated : 0));
    STAT(equiv_variables);
    STAT(equiv_work);
    STAT(congruence_work);
    STAT(congruence_gates);
    STAT(congruence_merges);
    STAT(congruence_units);
    STAT(conflicts);
    STAT(decisions);
    STAT(propagations);
    STAT(restarts);
    STAT(reduces);
    STAT(learned_clauses);
    STAT(learned_literals);
    STAT(deleted_clauses);
    STAT(minimized_literals);
    STAT(minimize_inspections);
    STAT(minimize_budget_hits);
    STAT(glue_clauses);
    STAT(max_lbd);
    STAT(lbd_updates);
    STAT(reused_levels);
#undef STAT
    fprintf(f, "\"literal_inspections\":%llu,\"garbage_collections\":%llu,",
            (unsigned long long)core->work, (unsigned long long)core->garbage_collections);
#define COUNT(field) fprintf(f, "\"" #field "\":%llu,", (unsigned long long)core->accounting.field)
    COUNT(binary_visits);
    COUNT(long_visits);
    COUNT(blocker_hits);
    COUNT(first_hits);
    COUNT(replacement_scans);
    COUNT(replacement_moves);
    COUNT(long_units);
    COUNT(long_conflicts);
    COUNT(original_scans);
    COUNT(learned_scans);
    COUNT(scan_size_9_plus);
    COUNT(learned_reason_uses);
    COUNT(learned_reason_lbd_sum);
#ifdef BSAT_SEARCH_DIAGNOSTICS
#define ARRAY(field)                                                                               \
    do {                                                                                           \
        fprintf(f, "\"" #field "\":[");                                                            \
        for (size_t i = 0; i < sizeof core->accounting.field / sizeof core->accounting.field[0];   \
             ++i)                                                                                  \
            fprintf(f, "%s%llu", i ? "," : "", (unsigned long long)core->accounting.field[i]);     \
        fputs("],", f);                                                                            \
    } while (0)
    COUNT(elimination_events);
    ARRAY(elimination_conflicts);
    ARRAY(elimination_work);
    ARRAY(elimination_variables);
    ARRAY(elimination_journal);
    ARRAY(elimination_microseconds);
    ARRAY(elimination_budget_hits);
    fputs("\"decision_observations\":[", f);
    bool first_decision = true;

    for (size_t i = 0; i < core->accounting.decision_observations_count; ++i) {
        DecisionObservation *d = &core->accounting.decision_observations[i];

        if (!d->decisions) continue;
        fprintf(f, "%s[%d,%llu,%llu,%llu]", first_decision ? "" : ",", toDimacs((Lit)i),
                (unsigned long long)d->decisions, (unsigned long long)d->propagations,
                (unsigned long long)d->conflicts);
        first_decision = false;
    }
    fputs("],", f);
    ARRAY(cone_reasons);
    ARRAY(cone_repeated);
    ARRAY(cone_dominated);
    COUNT(boundary_literals);
    COUNT(boundary_binary);
    COUNT(boundary_binary_covered);
    ARRAY(backjump_distance);
    ARRAY(backjump_removed);
    ARRAY(backjump_preservable);
    ARRAY(propagation_source);
    ARRAY(propagation_frame);
    ARRAY(decision_frame);
    ARRAY(replay_same);
    ARRAY(replay_opposite);
    ARRAY(removed_processed);
#undef ARRAY
    COUNT(hyperbinary_candidates);
    COUNT(hyperbinary_original);
    COUNT(restart_events);
    COUNT(restart_trail_before);
    COUNT(restart_trail_kept);
    COUNT(restart_levels_before);
    COUNT(restart_levels_kept);
    COUNT(use_lbd_checks);
    COUNT(use_lbd_lower);
    COUNT(use_lbd_to_glue);
    COUNT(use_binary_units);
    COUNT(use_learned_conflicts);
    COUNT(use_reduction_visits);
    COUNT(use_kept);
    COUNT(use_deleted);
    COUNT(use_deleted_never);
    COUNT(use_deleted_recent);
    COUNT(use_deleted_recent_analysis);
    COUNT(use_deleted_recent_high_lbd);
    COUNT(use_kept_recent_analysis);
    COUNT(use_deleted_scans);
    COUNT(use_deleted_never_scans);
    COUNT(use_deleted_age_sum);
    COUNT(use_deleted_recent_high_lbd_scans);
    COUNT(use_kept_unused);
    COUNT(use_kept_unused_scans);
#endif
#undef COUNT
    fprintf(f, "\"reused_solves\":%llu,", (unsigned long long)core->reused_solves);
#ifdef BSAT_SEARCH_DIAGNOSTICS
    fprintf(f, "\"minimize_lbd_seconds\":%.9f,", core->accounting.minimize_lbd_seconds);
#endif
    /* Post-query database inspection only; it is outside measured solving CPU.
       Guarded clauses remain globally entailed; the count is not a proof claim. */
    uint64_t live = 0, guarded = 0, guarded_lbd3 = 0, fixed = 0, fixed_lbd3 = 0;

    for (uint32_t i = 0; i < core->num_learnts; ++i) {
        CRef cr = core->learnts[i];

        if (clause_deleted(core->arena, cr)) continue;
        ++live;
        bool guard = false, has_fixed = false;
        Lit *lits = CLAUSE_LITS(core->arena, cr);

        for (uint32_t j = 0; j < CLAUSE_SIZE(core->arena, cr) && !guard; ++j)
            for (size_t k = 0; k < s->query_count; ++k)
                if (lits[j] == neg(s->query[k])) {
                    guard = true;
                    break;
                }
        if (guard) {
            ++guarded;
            if (clause_lbd(core->arena, cr) == 3) ++guarded_lbd3;
        }
        for (uint32_t j = 0; j < CLAUSE_SIZE(core->arena, cr) && !has_fixed; ++j) {
            Var v = var(lits[j]);
            Level level = core->vars[v].level;

            has_fixed = core->values[v] != UNDEF && level && level <= s->query_count;
        }
        if (has_fixed) {
            ++fixed;
            if (clause_lbd(core->arena, cr) == 3) ++fixed_lbd3;
        }
    }
    fprintf(f, "\"live_learned\":%llu,\"query_guarded_learned\":%llu,\"query_guarded_lbd3\":%llu,",
            (unsigned long long)live, (unsigned long long)guarded,
            (unsigned long long)guarded_lbd3);
    fprintf(f, "\"query_fixed_learned\":%llu,\"query_fixed_lbd3\":%llu,", (unsigned long long)fixed,
            (unsigned long long)fixed_lbd3);
    fputs("\"phase_seconds\":[", f);
    for (unsigned i = 0; i < ACCOUNT_PHASES; ++i)
        fprintf(f, "%s%.9f", i ? "," : "", core->accounting.seconds[i]);
    fputs("],\"phase_calls\":[", f);
    for (unsigned i = 0; i < ACCOUNT_PHASES; ++i)
        fprintf(f, "%s%llu", i ? "," : "", (unsigned long long)core->accounting.calls[i]);
    fputs("]}\n", f);
    int okay = !ferror(f);

    if (fclose(f)) okay = 0;
    return okay;
}

BSAT_API void
bsat_diagnostic_begin_query(bsat *s)
{
    if (bsat_error(s)) return;
    memset(s->core->accounting.seconds, 0, sizeof s->core->accounting.seconds);
    memset(s->core->accounting.calls, 0, sizeof s->core->accounting.calls);
#ifdef BSAT_SEARCH_DIAGNOSTICS
    s->core->accounting.minimize_lbd_seconds = 0;
    SolverAccounting *a = &s->core->accounting;

    a->elimination_events = 0;
    memset(a->elimination_conflicts, 0, sizeof a->elimination_conflicts);
    memset(a->elimination_work, 0, sizeof a->elimination_work);
    memset(a->elimination_variables, 0, sizeof a->elimination_variables);
    memset(a->elimination_journal, 0, sizeof a->elimination_journal);
    memset(a->elimination_microseconds, 0, sizeof a->elimination_microseconds);
    memset(a->elimination_budget_hits, 0, sizeof a->elimination_budget_hits);
    memset(a->cone_reasons, 0, sizeof a->cone_reasons);
    memset(a->cone_repeated, 0, sizeof a->cone_repeated);
    memset(a->cone_dominated, 0, sizeof a->cone_dominated);
    a->boundary_literals = a->boundary_binary = a->boundary_binary_covered = 0;
    memset(a->backjump_distance, 0, sizeof a->backjump_distance);
    memset(a->backjump_removed, 0, sizeof a->backjump_removed);
    memset(a->backjump_preservable, 0, sizeof a->backjump_preservable);
    memset(a->propagation_source, 0, sizeof a->propagation_source);
    memset(a->propagation_frame, 0, sizeof a->propagation_frame);
    memset(a->decision_frame, 0, sizeof a->decision_frame);
    memset(a->replay_same, 0, sizeof a->replay_same);
    memset(a->replay_opposite, 0, sizeof a->replay_opposite);
    memset(a->removed_processed, 0, sizeof a->removed_processed);
    a->hyperbinary_candidates = a->hyperbinary_original = 0;
    if (a->decision_observations_count)
        memset(a->decision_observations, 0,
               a->decision_observations_count * sizeof *a->decision_observations);
    a->observed_decision = 0;
    a->propagation_cause = PROP_OTHER;
    a->removal_cause = REMOVAL_OTHER;
    for (Var v = 1; v <= s->core->num_vars; ++v) {
        s->core->vars[v].processed_value = 0;
        s->core->vars[v].removed_value = 0;
        s->core->vars[v].removed_cause = 0;
    }
#endif
}

/* AAG frame layout is supplied by the harness, never inferred by the solver. */
BSAT_API void
bsat_diagnostic_frames(bsat *s, uint32_t width)
{
#ifdef BSAT_SEARCH_DIAGNOSTICS
    if (bsat_error(s)) return;
    SolverAccounting *a = &s->core->accounting;

    a->frame_width = width;
    free(a->decision_observations);
    a->decision_observations_count = 2 * ((size_t)s->core->num_vars + 1);
    a->decision_observations =
        calloc(a->decision_observations_count, sizeof *a->decision_observations);
    a->observed_decision = 0;
    if (!a->decision_observations) {
        a->decision_observations_count = 0;
        s->core->error = true;
    }
#else
    (void)s;
    (void)width;
#endif
}
