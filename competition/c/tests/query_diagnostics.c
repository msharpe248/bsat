/* Test-only facade: compile instead of bsat.o, never into an installed library.
   Uses the production facade directly; exposes pre-input ablations and snapshots
   without freezing internal Solver structures into the public ABI. */
#include "../src/bsat.c"
#include <string.h>

BSAT_API int
bsat_diagnostic_configure(bsat *s, const char *profile, int accounting)
{
    if (bsat_error(s) || s->started || !profile) return 0;
    SolverOpts *o = &s->core->opts;

    if (!strcmp(profile, "control")) {
    } else if (!strcmp(profile, "no-rephase"))
        o->rephase = false;
    else if (!strcmp(profile, "alternating"))
        o->alternating = true;
    else if (!strcmp(profile, "chrono"))
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

    fputs("{", f);
#ifdef BSAT_CERTIFIED_SSR
    fprintf(f, "\"ssr_candidates\":%llu,\"ssr_inspections\":%llu,\"ssr_strengthened\":%llu,",
            (unsigned long long)core->ssr_candidates, (unsigned long long)core->ssr_inspections,
            (unsigned long long)core->ssr_strengthened);
#endif
#define STAT(field) fprintf(f, "\"" #field "\":%llu,", (unsigned long long)core->stats.field)
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
#endif
}
