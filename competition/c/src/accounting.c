#include "../include/solver.h"
#include "../include/elim.h"
#include "../include/local_search.h"

/* Inclusive CPU intervals: nested propagation/proof work also belongs to its
   caller. Timings are intrusive diagnostics, never additive benchmark scores. */
double solver_account_begin(const Solver *s) {
    if (!s || !s->opts.accounting) return -1;
    return solver_cpu_time();
}
void solver_account_end(Solver *s, AccountPhase phase, double start) {
    if (start < 0) return;
    double end=solver_cpu_time();
    s->accounting.seconds[phase] += end-start;
    s->accounting.calls[phase]++;
}

SolverMemory solver_memory(const Solver *s) {
    SolverMemory m={0};
    if (!s) return m;
    /* Only report complete capacity estimates for successfully allocated state.
       A failed realloc sequence can leave differently sized variable arrays. */
    m.complete=!s->error && s->watches && !s->watches->failed;
    m.other=sizeof *s;
    if (s->arena) m.arena=sizeof *s->arena+(uint64_t)s->arena->capacity*sizeof(uint32_t);
    if (s->watches) {
        uint64_t n=2*((uint64_t)s->watches->num_vars+1);
        m.watches=sizeof *s->watches+n*sizeof(WatchList);
        for (uint64_t i=0;i<n;++i) m.watches+=(uint64_t)s->watches->lists[i].capacity*sizeof(Watch);
    }
    m.input=(uint64_t)s->input_capacity*sizeof(Lit);
    m.references=(uint64_t)(s->clauses_capacity)*sizeof(CRef)+(uint64_t)s->learnts_size*sizeof(CRef);
    uint64_t n=(uint64_t)s->var_capacity+1;
#define VAR_BYTES(field) do { if (s->field) m.variables+=n*sizeof *s->field; } while (0)
    VAR_BYTES(vars);VAR_BYTES(values);VAR_BYTES(trail);VAR_BYTES(order.heap);
    VAR_BYTES(seen);VAR_BYTES(analyze_stack);VAR_BYTES(minimize_touched);
    VAR_BYTES(binary_reasons);VAR_BYTES(lrb_last_conflict);VAR_BYTES(rephase.best_phase);
#undef VAR_BYTES
    m.variables+=(uint64_t)s->trail_limits_capacity*sizeof(Level);
    if (s->vmtf.nodes) m.variables+=((uint64_t)s->vmtf.capacity+1)*sizeof(VmtfNode);
    m.other+=s->levels_capacity+(uint64_t)s->conflict_size*sizeof(Lit);
    if (s->restart.recent_lbds) m.other+=(uint64_t)s->opts.glucose_window_size*sizeof(uint32_t);
    const ElimState *e=s->elim;
    if (e) {
        m.elimination=sizeof *e+(uint64_t)e->occs_capacity*sizeof(OccList)+
            (uint64_t)e->elim_capacity*sizeof(bool)+(uint64_t)e->stack_capacity*sizeof(ElimEntry)+
            (uint64_t)e->resolvent_crefs_capacity*sizeof(CRef);
        if(e->occs) for(uint32_t i=0;i<e->occs_capacity;++i)
            m.elimination+=(uint64_t)e->occs[i].capacity*sizeof(CRef);
        for(uint32_t i=0;i<e->stack_size;++i)
            m.elimination+=(uint64_t)(e->stack[i].clause_size?e->stack[i].clause_size:1)*sizeof(Lit);
    }
    const LocalSearchState *ls=s->local_search.state;
    if(ls) {
        uint64_t v=(uint64_t)ls->num_vars+1,c=ls->num_clauses;
        m.local_search=sizeof *ls+v*(sizeof(bool)+sizeof(int32_t)+2*sizeof(uint32_t*)+2*sizeof(uint32_t))+
            c*(3*sizeof(uint32_t)+sizeof(Lit*))+sizeof(uint32_t);
        for(uint32_t i=0;i<ls->num_clauses;++i) m.local_search+=(uint64_t)ls->clause_sizes[i]*sizeof(Lit);
        for(uint32_t i=0;i<=ls->num_vars;++i)
            m.local_search+=((uint64_t)ls->pos_occ_count[i]+ls->neg_occ_count[i])*sizeof(uint32_t);
    }
    m.total=m.arena+m.watches+m.input+m.variables+m.references+m.elimination+m.local_search+m.other;
    return m;
}

void solver_print_accounting(const Solver *s) {
    static const char *names[]={"parse","propagate","analyze","reduce","gc","preprocess",
        "simplify","reconstruct","model","proof","search"};
    for(unsigned i=0;i<ACCOUNT_PHASES;++i)
        printf("c Inclusive phase %s: %.9f seconds, %llu calls\n",names[i],
            s->accounting.seconds[i],(unsigned long long)s->accounting.calls[i]);
    printf("c Temporary congruence capacity peak: %llu\n",(unsigned long long)s->accounting.congruence_temporary_peak);
    printf("c Rebuild owned capacity overlap peak: %llu\n",(unsigned long long)s->accounting.rebuild_overlap_peak);
#ifdef BSAT_SEARCH_DIAGNOSTICS
    puts("c Search diagnostics: compiled in; counters require --accounting");
#else
    puts("c Search diagnostics: disabled at build time (use MODE=diagnostic)");
#endif
#define PRINT_SEARCH(field) printf("c Search " #field ": %llu\n", (unsigned long long)s->accounting.field)
    PRINT_SEARCH(binary_visits);PRINT_SEARCH(long_visits);PRINT_SEARCH(blocker_hits);PRINT_SEARCH(first_hits);
    PRINT_SEARCH(replacement_scans);PRINT_SEARCH(replacement_moves);PRINT_SEARCH(long_units);PRINT_SEARCH(long_conflicts);
    PRINT_SEARCH(scan_size_3);PRINT_SEARCH(scan_size_4_8);PRINT_SEARCH(scan_size_9_plus);
    PRINT_SEARCH(original_scans);PRINT_SEARCH(learned_scans);
    PRINT_SEARCH(scan_age_0_99);PRINT_SEARCH(scan_age_100_999);PRINT_SEARCH(scan_age_1000_plus);
    PRINT_SEARCH(scans_before_unit_or_analysis);PRINT_SEARCH(scans_after_unit_or_analysis);
    PRINT_SEARCH(learned_reason_uses);PRINT_SEARCH(learned_reason_lbd_sum);
    PRINT_SEARCH(reduced_candidates);PRINT_SEARCH(deleted_without_analysis_use);
#ifdef BSAT_SEARCH_DIAGNOSTICS
    PRINT_SEARCH(learned_scans_eligible_size);PRINT_SEARCH(learned_scans_oversize);
    PRINT_SEARCH(learned_scans_unlocked);PRINT_SEARCH(vivify_attempts);
    PRINT_SEARCH(vivify_replaced);PRINT_SEARCH(vivify_removed_literals);
    PRINT_SEARCH(vivify_work);PRINT_SEARCH(strengthened_scans);
    PRINT_SEARCH(use_lbd_checks);
    PRINT_SEARCH(use_lbd_lower);
    PRINT_SEARCH(use_lbd_to_glue);
    PRINT_SEARCH(use_binary_units);
    PRINT_SEARCH(use_learned_conflicts);
    PRINT_SEARCH(use_reduction_visits);
    PRINT_SEARCH(use_kept);
    PRINT_SEARCH(use_deleted);
    PRINT_SEARCH(use_deleted_never);
    PRINT_SEARCH(use_deleted_recent);
    PRINT_SEARCH(use_deleted_recent_analysis);
    PRINT_SEARCH(use_deleted_recent_high_lbd);
    PRINT_SEARCH(use_kept_recent_analysis);
    PRINT_SEARCH(use_deleted_scans);
    PRINT_SEARCH(use_deleted_never_scans);
    PRINT_SEARCH(use_deleted_age_sum);
    PRINT_SEARCH(use_deleted_recent_high_lbd_scans);
    PRINT_SEARCH(use_kept_unused);
    PRINT_SEARCH(use_kept_unused_scans);
#endif
#undef PRINT_SEARCH
#ifdef BSAT_SEARCH_DIAGNOSTICS
    /* Bounded top ten live records. CRefs identify this snapshot only; GC copies
       all metadata. Deleted records are excluded, aggregate scan counts are not. */
    CRef top[10]={0};
    for(size_t at=1;at<s->arena->size;) {
        const ClauseHeader *h=CLAUSE_HEADER(s->arena,at);
        if(!clause_deleted(s->arena,at) && h->scans) {
            for(unsigned k=0;k<10;++k) if(!top[k] || h->scans>CLAUSE_HEADER(s->arena,top[k])->scans) {
                for(unsigned j=9;j>k;--j) top[j]=top[j-1];
                top[k]=(CRef)at;break;
            }
        }
        at+=sizeof(ClauseHeader)/sizeof(uint32_t)+h->size;
    }
    for(unsigned k=0;k<10 && top[k];++k) {
        const ClauseHeader *h=CLAUSE_HEADER(s->arena,top[k]);
        uint64_t born=((uint64_t)h->born_hi<<32)|h->born_lo;
        printf("c Hot live clause: cref=%u learned=%u size=%u scans=%u units=%u analyses=%u born=%llu\n",
               top[k],clause_learned(s->arena,top[k]),h->size,h->scans,h->units,h->analyses,(unsigned long long)born);
    }
#endif
    SolverMemory m=solver_memory(s);
#define PRINT_MEMORY(field) printf("c Owned capacity " #field ": %llu\n",(unsigned long long)m.field)
    PRINT_MEMORY(arena);PRINT_MEMORY(watches);PRINT_MEMORY(input);PRINT_MEMORY(variables);
    PRINT_MEMORY(references);PRINT_MEMORY(elimination);PRINT_MEMORY(local_search);PRINT_MEMORY(other);PRINT_MEMORY(total);
#undef PRINT_MEMORY
    printf("c Owned capacity complete: %s\n",m.complete?"yes":"no");
    puts("c Accounting scope: inclusive intrusive CPU; retained requested capacities, not RSS or transient peaks");
}
