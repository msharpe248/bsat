/* Stateful API bytecode, independent exhaustive oracle, optional allocation
   failure. Every artifact is replayable/minimizable by the same libFuzzer binary. */
#include "solver.h"
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
void fuzz_alloc_reset(size_t);
bool fuzz_alloc_failed(void);
typedef struct {Lit lits[3];unsigned size;} Original;

static bool satisfies(unsigned bits,const Lit *lits,unsigned n) {
    for(unsigned j=0;j<n;++j)if(((bits>>(var(lits[j])-1))&1u)!=sign(lits[j]))return true;
    return false;
}
static bool oracle(unsigned vars,Original *cs,unsigned nc,Lit *as,unsigned na) {
    for(unsigned bits=0;bits<(1u<<vars);++bits) {
        bool ok=true;
        for(unsigned i=0;i<nc;++i)ok &= satisfies(bits,cs[i].lits,cs[i].size);
        for(unsigned i=0;i<na;++i)ok &= satisfies(bits,as+i,1);
        if(ok)return true;
    }
    return false;
}
int LLVMFuzzerTestOneInput(const uint8_t *data,size_t size) {
    if(size<3 || size>512)return 0;
    fuzz_alloc_reset(data[0]&128 ? (size_t)data[1]+1 : 0);
    SolverOpts o=default_opts();o.probing=data[0]&1;o.equiv=data[0]&2;
    o.congruence=data[0]&4;o.elim=data[0]&8;o.bce=data[0]&16;
    o.chrono=data[0]&32;o.chrono_levels=0;o.vmtf=data[0]&64;
    o.reduce_interval=2;o.inprocess=true;o.inprocess_interval=2;
    o.preprocess_budget=10000;o.equiv_budget=10000;o.congruence_budget=10000;
    Solver *s=solver_new_with_opts(&o);if(!s){assert(fuzz_alloc_failed());return 0;}
    unsigned vars=1+data[2]%6,nc=0;Original clauses[32];
    for(unsigned i=0;i<vars;++i)if(!solver_new_var(s))goto done;
    for(size_t at=3;at<size && nc<32;) {
        unsigned op=data[at++]%5;
        if(op==0 && vars<6) {if(!solver_new_var(s))break;++vars;}
        else if(op==1) {
            if(at==size)break;
            Original c={.size=data[at++]%4};if(size-at<c.size)break;
            for(unsigned j=0;j<c.size;++j){unsigned v=data[at++];c.lits[j]=mkLit(1+(v>>1)%vars,v&1);}
            solver_add_clause(s,c.lits,c.size);
            if(s->error||s->watches->failed)break;
            clauses[nc++]=c;
        } else {
            Lit assumptions[3];unsigned na=op==2?0:op==3?1:3;
            if(size-at<na)break;
            for(unsigned j=0;j<na;++j){unsigned v=data[at++];assumptions[j]=mkLit(1+(v>>1)%vars,v&1);}
            bool expected=oracle(vars,clauses,nc,assumptions,na);
            lbool result=solver_solve_with_assumptions(s,assumptions,na);
            if(s->error||s->watches->failed){assert(result==UNDEF);break;}
            assert(result==(expected?TRUE:FALSE));
            if(result==TRUE) {
                unsigned bits=0;
                for(unsigned v=1;v<=vars;++v)if(solver_model_value(s,v)==TRUE)bits|=1u<<(v-1);
                for(unsigned j=0;j<nc;++j)assert(satisfies(bits,clauses[j].lits,clauses[j].size));
                for(unsigned j=0;j<na;++j)assert(satisfies(bits,assumptions+j,1));
            }
        }
    }
done:
    if(s->error||s->watches->failed)assert(solver_solve(s)==UNDEF);
    solver_free(s);return 0;
}
