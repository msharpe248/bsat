#include "../include/solver.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    for(unsigned mask=0;mask<512;++mask) {
        Solver *s=solver_new();assert(s);
        for(unsigned v=0;v<8;++v) assert(solver_new_var(s));
        s->clauses=malloc(9*sizeof(CRef));s->clauses_capacity=9;
        s->learnts=malloc(9*sizeof(CRef));s->learnts_size=9;
        assert(s->clauses && s->learnts);
        Lit pad[128];for(unsigned i=0;i<128;++i) pad[i]=mkLit(1,false);
        CRef waste=arena_alloc(s->arena,pad,128,false);assert(waste!=INVALID_CLAUSE);
        arena_delete(s->arena,waste);
        CRef refs[9],moved[9];unsigned sizes[9];uint32_t copies[9][sizeof(ClauseHeader)/4+7];
        CRef next=1;
        for(unsigned i=0;i<9;++i) {
            unsigned n=sizes[i]=i==8?2:i;bool learned=i%2 || i==8;
            refs[i]=arena_alloc(s->arena,pad,n,learned);assert(refs[i]!=INVALID_CLAUSE);
            // Distinct literals, including empty/unit records and both binary encodings.
            Lit *lits=CLAUSE_LITS(s->arena,refs[i]);
            for(unsigned j=0;j<n;++j) lits[j]=mkLit(j+1,false);
            ClauseHeader *h=CLAUSE_HEADER(s->arena,refs[i]);
            h->search=i%2?UINT32_MAX:i;h->lbd=i+3;h->activity=(float)i/8;
            h->flags|=CLAUSE_FROZEN;
#ifdef BSAT_SEARCH_DIAGNOSTICS
            h->born_hi=1;h->born_lo=i;h->scans=i*17;h->units=i+2;h->analyses=i+9;
#endif
            memcpy(copies[i],h,(sizeof(ClauseHeader)/4+n)*sizeof(uint32_t));
            if(learned) s->learnts[s->num_learnts++]=refs[i];
            else s->clauses[s->num_clauses++]=refs[i];
            if(n>=2) {
                CRef w=n==2?(learned?arena_binary_watch_ref(refs[i]):INVALID_CLAUSE):refs[i];
                watch_add(s->watches,lits[0],w,lits[1]);
                watch_add(s->watches,lits[1],w,lits[0]);
            }
            if(mask&(1u<<i)) {
                // Keep arena watches to verify that collection filters dead references.
                if(n==2 && !learned) watch_remove_clause(s->watches,s->arena,refs[i]);
                arena_delete(s->arena,refs[i]);moved[i]=INVALID_CLAUSE;
            } else {moved[i]=next;next+=sizeof(ClauseHeader)/4+n;}
        }
        s->num_original=s->num_clauses;
        s->vars[1].reason=refs[7];s->values[1]=moved[7]==INVALID_CLAUSE?UNDEF:TRUE;
        solver_collect_garbage(s);assert(s->garbage_collections==1);
        assert(s->arena->size==next && !s->arena->wasted);
        assert(s->vars[1].reason==moved[7]);
        unsigned orig=0,learnt=0,watch_count=0;
        for(unsigned i=0;i<9;++i) if(moved[i]!=INVALID_CLAUSE) {
            assert(!memcmp(CLAUSE_HEADER(s->arena,moved[i]),copies[i],(sizeof(ClauseHeader)/4+sizes[i])*sizeof(uint32_t)));
            if(i%2 || i==8) assert(s->learnts[learnt++]==moved[i]);
            else assert(s->clauses[orig++]==moved[i]);
            if(sizes[i]>=2) ++watch_count;
        }
        assert(s->num_learnts==learnt && s->num_clauses==orig && s->num_original==orig);
        for(unsigned lit=1;lit<=2;++lit) {
            WatchList *wl=watch_list(s->watches,mkLit(lit,false));assert(wl->size==watch_count);
            unsigned j=0;
            for(unsigned i=2;i<9;++i) if(moved[i]!=INVALID_CLAUSE) {
                Watch w=wl->watches[j++];
                CRef expected=i==2?INVALID_CLAUSE:i==8?arena_binary_watch_ref(moved[i]):moved[i];
                assert(w.cref==expected && w.blocker==mkLit(3-lit,false));
            }
        }
        solver_collect_garbage(s);assert(s->garbage_collections==1);
        solver_free(s);
    }
    Solver *s=solver_new();assert(s);
    for(unsigned v=0;v<5;++v) assert(solver_new_var(s));
    Lit pad[128];for(unsigned i=0;i<128;++i) pad[i]=mkLit(1,false);
    CRef waste=arena_alloc(s->arena,pad,128,false);assert(waste!=INVALID_CLAUSE);
    arena_delete(s->arena,waste);
    Lit a[]={mkLit(1,false),mkLit(2,false),mkLit(3,false)};
    Lit b[]={mkLit(1,true),mkLit(4,false),mkLit(5,false)};
    assert(solver_add_clause(s,a,3) && solver_add_clause(s,b,3));
    elim_build_occs(s);assert(!s->error && s->elim->occs_complete);
    s->elim->resolvent_crefs=malloc(sizeof(CRef));assert(s->elim->resolvent_crefs);
    s->elim->resolvent_crefs_size=s->elim->resolvent_crefs_capacity=1;
    s->elim->resolvent_crefs[0]=s->clauses[0];
    CRef old=s->clauses[0];solver_collect_garbage(s);
    assert(s->garbage_collections==1 && s->clauses[0]!=old);
    assert(s->elim->occs_complete && !s->elim->resolvent_crefs_size);
    for(unsigned i=0;i<3;++i) {
        assert(s->elim->occs[a[i]].size==1 && s->elim->occs[a[i]].clauses[0]==s->clauses[0]);
        assert(s->elim->occs[b[i]].size==1 && s->elim->occs[b[i]].clauses[0]==s->clauses[1]);
    }
    assert(solver_solve(s)==TRUE && solver_check_model(s));solver_free(s);
    puts("PASS: 512 GC deletion patterns and BVE occurrence relocation");
}
