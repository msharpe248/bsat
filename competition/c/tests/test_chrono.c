#include "../include/solver.h"
#include "../include/dimacs.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static void assign(Solver *s, Lit lit) {
    Var v=var(lit);assert(s->values[v]==UNDEF);
    s->trail_lims[++s->decision_level]=s->trail_size;
    s->values[v]=sign(lit)?FALSE:TRUE;s->vars[v].level=s->decision_level;
    s->vars[v].reason=INVALID_CLAUSE;s->binary_reasons[v]=LIT_UNDEF;
    s->vars[v].trail_pos=s->trail_size;s->trail[s->trail_size++]=(Trail){lit};
}

static void replay(unsigned kind, bool queue) {
    SolverOpts o=default_opts();o.chrono=true;o.chrono_levels=0;o.vmtf=queue;o.probing=false;
    Solver *s=solver_new_with_opts(&o);assert(s);
    for(unsigned i=0;i<6;++i)assert(solver_new_var(s));
    Lit clause[]={fromDimacs(1),fromDimacs(2),fromDimacs(3)};
    for(unsigned i=0;i<20;++i){CRef dead=arena_alloc(s->arena,clause,3,true);assert(dead!=INVALID_CLAUSE);arena_delete(s->arena,dead);}
    if(kind==1) {
        /* An arena-backed learned binary, unlike original implicit binaries. */
        CRef cr=arena_alloc(s->arena,clause,2,true);assert(cr!=INVALID_CLAUSE);
        s->learnts=malloc(sizeof *s->learnts);assert(s->learnts);
        s->learnts[0]=cr;s->num_learnts=s->learnts_size=1;
        watch_add(s->watches,clause[0],arena_binary_watch_ref(cr),clause[1]);
        watch_add(s->watches,clause[1],arena_binary_watch_ref(cr),clause[0]);
    } else assert(solver_add_clause(s,clause,kind==2?3:2));
    Lit chain[]={fromDimacs(kind==2?-3:-2),fromDimacs(6)};
    assert(solver_add_clause(s,chain,2));
    assign(s,fromDimacs(-1));if(kind==2)assign(s,fromDimacs(-2));
    Level retained=s->decision_level;
    /* Delay propagation past unrelated decisions to construct late reasons. */
    assign(s,fromDimacs(-4));assign(s,fromDimacs(-5));
    assert(solver_propagate(s)==INVALID_CLAUSE);
    Var implied=kind==2?3:2;
    assert(s->values[implied]==TRUE && s->values[6]==TRUE);
    assert(s->vars[implied].level==retained && s->vars[6].level==retained && s->qhead==s->trail_size);
    CRef old_reason=s->vars[implied].reason;
    solver_collect_garbage(s);assert(s->garbage_collections==1);
    if(kind)assert(s->vars[implied].reason!=old_reason);
    solver_backtrack(s,retained);
    assert(s->values[implied]==TRUE && s->values[6]==TRUE);
    assert(s->qhead<s->trail_size && s->stats.chrono_retained==2);
    assert(s->vars[implied].trail_pos<s->trail_size && s->vars[6].trail_pos<s->trail_size);
    assert(s->trail[s->vars[implied].trail_pos].lit==mkLit(implied,0));
    assert(s->trail[s->vars[6].trail_pos].lit==mkLit(6,0));
    assert(solver_propagate(s)==INVALID_CLAUSE);
    assert(s->values[implied]==TRUE && s->values[6]==TRUE);
    assert(s->vars[implied].level==retained && s->vars[6].level==retained);
    /* A subsequent root restart may discard both antecedents and consequences. */
    solver_backtrack(s,0);assert(solver_propagate(s)==INVALID_CLAUSE);
    assert(s->values[implied]==UNDEF && s->values[6]==UNDEF);
    solver_free(s);
}

static uint32_t random_state=0x93a618d2u;
static uint32_t random_word(void){random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state;}
static bool satisfies(unsigned bits,const Lit *lits,unsigned n){for(unsigned i=0;i<n;++i)if((((bits>>(var(lits[i])-1))&1)!=0)!=sign(lits[i]))return true;return false;}
static void exhaustive(void) {
    uint64_t chronological=0,replayed=0,lower=0;unsigned solves=0;
    for(unsigned sample=0;sample<2048;++sample) {
        Lit clauses[30][4];unsigned sizes[30],count=10+random_word()%21;
        for(unsigned i=0;i<count;++i){sizes[i]=2+random_word()%3;for(unsigned j=0;j<sizes[i];++j)clauses[i][j]=mkLit(1+random_word()%7,random_word()&1);}
        for(unsigned mode=0;mode<2;++mode) {
            SolverOpts o=default_opts();o.chrono=true;o.chrono_levels=0;o.probing=false;o.vmtf=mode;
            o.luby_restart=true;o.luby_unit=3;o.reduce_interval=4;o.reuse_trail=true;
            Solver *s=solver_new_with_opts(&o);assert(s);for(unsigned i=0;i<7;++i)assert(solver_new_var(s));
            for(unsigned i=0;i<count;++i){solver_add_clause(s,clauses[i],sizes[i]);assert(!s->error);}
            for(unsigned query=0;query<3;++query) {
                Lit assumptions[2]={mkLit(1+random_word()%7,random_word()&1),mkLit(1+random_word()%7,random_word()&1)};
                unsigned na=query;bool sat=false;
                for(unsigned bits=0;bits<128;++bits){bool ok=true;for(unsigned i=0;i<count && ok;++i)ok=satisfies(bits,clauses[i],sizes[i]);for(unsigned i=0;i<na && ok;++i)ok=satisfies(bits,assumptions+i,1);if(ok){sat=true;break;}}
                lbool answer=na?solver_solve_with_assumptions(s,assumptions,na):solver_solve(s);
                assert(!s->error && answer==(sat?TRUE:FALSE));if(sat)assert(solver_check_model(s));
                chronological+=s->stats.chronological;replayed+=s->stats.chrono_retained;lower+=s->stats.chrono_lower_conflicts;++solves;
            }
            solver_free(s);
        }
    }
    assert(chronological && replayed);
    printf("PASS: %u exact seven-variable formula/assumption solves; %llu chronological backtracks, %llu retained assignments, %llu lower conflicts\n",solves,(unsigned long long)chronological,(unsigned long long)replayed,(unsigned long long)lower);
}

static void root_unit(void) {
    SolverOpts o=default_opts();o.chrono=true;o.chrono_levels=0;o.probing=false;o.vmtf=true;o.max_conflicts=1;
    Solver *s=solver_new_with_opts(&o);assert(s);
    assert(dimacs_parse_string(s,"p cnf 8 2\n7 8 0\n7 -8 0\n")==DIMACS_OK);
    for(unsigned v=8;v;--v)solver_vmtf_bump(s,v);
    assert(solver_solve(s)==UNDEF && !s->error);
    assert(s->stats.decisions>=7 && s->values[7]==TRUE && s->vars[7].level==0);
    assert(s->decision_level==6 && s->stats.chronological==1);
    solver_backtrack(s,0);
    assert(s->values[7]==TRUE && s->vars[7].level==0 && s->vars[7].trail_pos==0);
    assert(s->trail_size==1 && s->qhead==0);
    assert(solver_propagate(s)==INVALID_CLAUSE);
    solver_free(s);
}
static void lower_conflicts(void) {
    for(unsigned size=2;size<=3;++size) {
        SolverOpts o=default_opts();o.chrono=true;o.probing=false;
        Solver *s=solver_new_with_opts(&o);assert(s);for(unsigned i=0;i<5;++i)assert(solver_new_var(s));
        Lit lits[]={mkLit(1,0),mkLit(2,0),mkLit(3,0)};
        assert(solver_add_clause(s,lits,size));
        for(unsigned i=1;i<=size;++i)assign(s,mkLit(i,1));
        assign(s,mkLit(5,0));
        CRef cr=s->clauses[0];
        if(size==2){s->binary_conflict_lits[0]=lits[0];s->binary_conflict_lits[1]=lits[1];cr=BINARY_CONFLICT;}
        s->work_limit=1;s->work=1;
        assert(!solver_normalize_conflict(s,cr) && s->decision_level==size+1);
        s->work_limit=0;
        assert(solver_normalize_conflict(s,cr));
        assert(s->decision_level==size && s->values[5]==UNDEF && s->stats.chrono_lower_conflicts==1);
        if(size==3) {
            assert(s->stats.chrono_rewatched==1);
            const Lit *watched=CLAUSE_LITS(s->arena,cr);
            assert(s->vars[var(watched[0])].level==3 && s->vars[var(watched[1])].level==2);
        }
        Lit learnt[6];uint32_t n;Level bt;solver_analyze(s,cr,learnt,&n,&bt);
        assert(n==size && bt==size-1);
        solver_backtrack(s,size-1);
        assert(solver_propagate(s)==INVALID_CLAUSE && s->values[size]==TRUE);
        assert(s->vars[size].level==size-1);
        solver_free(s);
    }
    SolverOpts o=default_opts();o.chrono=true;Solver *s=solver_new_with_opts(&o);assert(s);
    assert(dimacs_parse_string(s,"p cnf 3 2\n-1 0\n-2 0\n")==DIMACS_OK);
    assign(s,mkLit(3,0));s->binary_conflict_lits[0]=mkLit(1,0);s->binary_conflict_lits[1]=mkLit(2,0);
    assert(!solver_normalize_conflict(s,BINARY_CONFLICT) && !s->decision_level && s->values[3]==UNDEF);
    solver_free(s);
}
static void late_seen_literal(void) {
    SolverOpts o=default_opts();o.chrono=true;o.probing=false;
    Solver *s=solver_new_with_opts(&o);assert(s);
    assert(dimacs_parse_string(s,"p cnf 3 2\n1 -3 0\n3 2 0\n")==DIMACS_OK);
    assign(s,mkLit(1,1));assign(s,mkLit(2,1));
    CRef conflict=solver_propagate(s);assert(conflict!=INVALID_CLAUSE);
    assert(s->trail[s->trail_size-1].lit==mkLit(3,1) && s->vars[3].level==1);
    assert(solver_normalize_conflict(s,conflict));
    Lit learnt[4];uint32_t n;Level bt;solver_analyze(s,conflict,learnt,&n,&bt);
    assert(n==2 && learnt[0]==mkLit(2,0) && bt==1);
    solver_backtrack(s,1);assert(s->values[2]==UNDEF && s->values[3]==FALSE);
    solver_free(s);
}
static void reason_maximum(void) {
    SolverOpts o=default_opts();o.chrono=true;o.probing=false;
    Solver *s=solver_new_with_opts(&o);assert(s);
    /* Variable 3 is a higher-level non-watched antecedent. Variable 1 is
       enqueued later at level 1 and triggers the long clause after variable 3. */
    assert(dimacs_parse_string(s,"p cnf 4 2\n4 -1 0\n1 2 3 0\n")==DIMACS_OK);
    assign(s,mkLit(4,1));assign(s,mkLit(3,1));
    assert(solver_propagate(s)==INVALID_CLAUSE);
    assert(s->values[1]==FALSE && s->vars[1].level==1);
    assert(s->values[2]==TRUE && s->vars[2].level==2);
    solver_backtrack(s,1);
    assert(s->values[1]==FALSE && s->values[2]==UNDEF && s->values[3]==UNDEF);
    assert(solver_propagate(s)==INVALID_CLAUSE && s->values[2]==UNDEF);
    solver_free(s);
}
int main(void) {
    assert(!default_opts().chrono && default_opts().chrono_levels==100);
    for(unsigned k=0;k<3;++k)for(unsigned q=0;q<2;++q)replay(k,q);
    root_unit();lower_conflicts();late_seen_literal();reason_maximum();exhaustive();puts("PASS: late implications, compacted reasons, root units, conflict watches and current-level UIP selection");return 0;
}
