#include "solver.h"
#include "dimacs.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
#ifdef BSAT_CERTIFIED_REFRESH
    for(unsigned certified=0;certified<2;++certified) {
        SolverOpts o=default_opts();o.probing=false;o.assumption_lbd=certified;
        Solver *s=solver_new_with_opts(&o);assert(s);
        assert(dimacs_parse_string(s,"p cnf 4 3\n-1 -2 3 4 0\n-1 -2 3 -4 0\n-1 -3 0\n")==DIMACS_OK);
        FILE *journal=tmpfile();assert(journal);s->proof_journal=journal;
        s->levels_capacity=5;s->level_seen=calloc(5,1);assert(s->level_seen);
        CRef a=s->clauses[0],b=s->clauses[1];
        CLAUSE_HEADER(s->arena,a)->flags|=CLAUSE_LEARNED;CLAUSE_HEADER(s->arena,b)->flags|=CLAUSE_LEARNED;
        set_clause_lbd(s->arena,a,8);set_clause_lbd(s->arena,b,8);
        CRef conflict=INVALID_CLAUSE;
        for(Var v=1;v<=2;++v) {
            s->trail_lims[++s->decision_level]=s->trail_size;s->values[v]=TRUE;
            s->vars[v].level=s->decision_level;s->vars[v].reason=INVALID_CLAUSE;
            s->vars[v].trail_pos=s->trail_size;s->trail[s->trail_size++]=(Trail){mkLit(v,false)};
            conflict=solver_propagate(s);
        }
        assert(conflict==b);Lit out[4];uint32_t n;Level backjump;
        solver_analyze(s,conflict,out,&n,&backjump);assert(n && backjump==1);
        assert(clause_lbd(s->arena,a)==(certified?3u:8u));assert(clause_lbd(s->arena,b)==(certified?3u:8u));
        assert(s->stats.lbd_updates==(certified?2u:0u));
        assert(ftell(journal)==0); /* Ranking update emits no inference. */
        for(unsigned i=0;i<5;++i)assert(!s->level_seen[i]);
        solver_free(s);fclose(journal);
    }
    puts("PASS: certified refresh lowers stale scores, never promotes glue, leaves proofs and scratch intact");
#else
    puts("SKIP: compile BSAT_CERTIFIED_REFRESH for archived experiment");
#endif
}
