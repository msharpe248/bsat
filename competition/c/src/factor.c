/* Bounded clause rectangle factoring. For every (a OR C), a in A, C in B,
   replace the rectangle by (x OR a) and (!x OR C), with |C| <= 2. The !x clauses are RAT
   on the fresh variable; each subsequent x clause has original resolvents.
   Originals are deleted only after all replacement clauses exist. */
#include "solver.h"

typedef struct { Lit a,b; CRef ref; } Pair;
typedef struct { Lit first,second; } Residue;
typedef struct { Lit other; CRef ref; } Edge;
typedef struct {
    Solver *s; Pair *pairs; Edge *edges,*reverse; Residue *residues;
    size_t *offset,*column_offset; uint32_t *counts; Lit *touched,*columns,*rows;
    unsigned char *marked; uint32_t literals,residue_count;
} Factor;
static bool tick(Factor *f) {
    ++f->s->work;++f->s->stats.factor_work;
    return !solver_budget_exhausted(f->s);
}
static bool live(Factor *f,Edge e) {
    return !clause_deleted(f->s->arena,e.ref); /* Graph literals stay unassigned during factoring. */
}
static size_t hash_pair(uint64_t x) {
    x^=x>>30;x*=UINT64_C(0xbf58476d1ce4e5b9);x^=x>>27;
    return (size_t)(x*UINT64_C(0x94d049bb133111eb));
}
static bool same_clause(Solver *s,CRef r,const Lit *v,uint32_t n) {
    if(CLAUSE_SIZE(s->arena,r)!=n)return false;
    const Lit *p=CLAUSE_LITS(s->arena,r);
    for(uint32_t i=0;i<n;++i){bool found=false;for(uint32_t j=0;j<n;++j)found|=v[i]==p[j];if(!found)return false;}
    return true;
}
static bool graph(Factor *f) {
    Solver *s=f->s;size_t binary=0,ternary=0;
    for(uint32_t i=0;i<s->num_clauses;++i) {
        if(!tick(f))return false;
        CRef r=s->clauses[i];if(clause_deleted(s->arena,r))continue;
        uint32_t n=CLAUSE_SIZE(s->arena,r);binary+=n==2;ternary+=n==3;
    }
    bool triples=2*binary+3*ternary<=3000000;
    size_t arcs=2*binary+(triples?3*ternary:0),clauses=binary+(triples?ternary:0);
    if(!arcs || arcs>4000000)return false;
    size_t cap=1,clause_cap=1;
    while(cap<2*arcs)cap*=2;
    while(clause_cap<2*clauses)clause_cap*=2;
    uint32_t *keys=calloc(cap,sizeof *keys);
    CRef *original=calloc(clause_cap,sizeof *original);
    f->pairs=malloc(arcs*sizeof *f->pairs);
    f->residues=malloc(arcs*sizeof *f->residues);
    f->offset=calloc((size_t)f->literals+1,sizeof *f->offset);
    if(!keys || !original || !f->pairs || !f->residues || !f->offset){s->error=true;goto fail;}
    size_t used=0;
    for(uint32_t i=0;i<s->num_clauses;++i) {
        if(!tick(f))goto fail;
        CRef r=s->clauses[i];uint32_t n=CLAUSE_SIZE(s->arena,r);
        if(clause_deleted(s->arena,r) || (n!=2 && !(triples && n==3)))continue;
        Lit v[3]={0};bool assigned=false;
        for(uint32_t j=0;j<n;++j){v[j]=CLAUSE_LITS(s->arena,r)[j];assigned|=s->values[var(v[j])]!=UNDEF;}
        if(assigned)continue;
        for(uint32_t j=0;j<n;++j)for(uint32_t k=j+1;k<n;++k)if(v[j]>v[k]){Lit t=v[j];v[j]=v[k];v[k]=t;}
        size_t at=hash_pair(((uint64_t)v[0]<<32)^v[1]^hash_pair(v[2]))&(clause_cap-1);
        while(original[at] && !same_clause(s,original[at],v,n)){if(!tick(f))goto fail;at=(at+1)&(clause_cap-1);}
        if(original[at])continue;
        original[at]=r;
        for(uint32_t row=0;row<n;++row) {
            if(!tick(f))goto fail;
            Residue res={0};
            for(uint32_t j=0;j<n;++j)if(j!=row){if(!res.first)res.first=v[j];else res.second=v[j];}
            at=hash_pair(((uint64_t)res.first<<32)|res.second)&(cap-1);
            while(keys[at]) {
                Residue old=f->residues[keys[at]-1];
                if(old.first==res.first && old.second==res.second)break;
                if(!tick(f))goto fail;at=(at+1)&(cap-1);
            }
            if(!keys[at]){f->residues[f->residue_count]=res;keys[at]=++f->residue_count;}
            f->pairs[used++]=(Pair){v[row],keys[at]-1,r};++f->offset[v[row]+1];
        }
    }
    free(keys);keys=NULL;free(original);original=NULL;
    f->column_offset=calloc((size_t)f->residue_count+1,sizeof *f->column_offset);
    if(!f->column_offset){s->error=true;goto fail;}
    for(size_t i=0;i<used;++i){if(!tick(f))goto fail;++f->column_offset[f->pairs[i].b+1];}
    for(uint32_t i=1;i<=f->literals;++i){if(!tick(f))goto fail;f->offset[i]+=f->offset[i-1];}
    for(uint32_t i=1;i<=f->residue_count;++i){if(!tick(f))goto fail;f->column_offset[i]+=f->column_offset[i-1];}
    f->edges=malloc(MAX((size_t)1,used)*sizeof *f->edges);
    f->reverse=malloc(MAX((size_t)1,used)*sizeof *f->reverse);
    if(!f->edges || !f->reverse){s->error=true;goto fail;}
    /* Fill backwards, then restore the prefix offsets; no extra cursor arrays. */
    for(size_t i=0;i<used;++i) {
        if(!tick(f))goto fail;
        Pair p=f->pairs[i];f->edges[--f->offset[p.a+1]]=(Edge){p.b,p.ref};
        f->reverse[--f->column_offset[p.b+1]]=(Edge){p.a,p.ref};
    }
    for(uint32_t i=0;i<f->literals;++i){if(!tick(f))goto fail;f->offset[i]=f->offset[i+1];}f->offset[f->literals]=used;
    for(uint32_t i=0;i<f->residue_count;++i){if(!tick(f))goto fail;f->column_offset[i]=f->column_offset[i+1];}f->column_offset[f->residue_count]=used;
    free(f->pairs);f->pairs=NULL;
    f->counts=calloc(f->literals,sizeof *f->counts);
    f->marked=calloc(MAX(1u,f->residue_count),sizeof *f->marked);
    f->touched=malloc((size_t)f->literals*sizeof *f->touched);
    f->columns=malloc((size_t)MAX(1u,f->residue_count)*sizeof *f->columns);
    f->rows=malloc((size_t)f->literals*sizeof *f->rows);
    if(!f->counts || !f->marked || !f->touched || !f->columns || !f->rows){s->error=true;goto fail;}
    return true;
fail:
    free(keys);free(original);return false;
}
static bool add(Factor *f,Lit first,Lit second,Lit third) {
    Lit clause[]={first,second,third};uint32_t n=third?3:2;Solver *s=f->s;
    proof_add_clause(s,clause,n);if(s->error)return false;
    bool saved=s->internal_add;s->internal_add=true;
    solver_add_clause(s,clause,n);s->internal_add=saved;
    ++s->stats.factor_added;
    return !s->error && !s->watches->failed;
}
static bool rectangle(Factor *f,Lit a) {
    Solver *s=f->s;uint32_t touched=0;Lit best=0;uint32_t score=2;
    for(size_t i=f->offset[a];i<f->offset[a+1];++i) {
        if(!tick(f))return false;
        Edge e=f->edges[i];if(!live(f,e))continue;
        for(size_t j=f->column_offset[e.other];j<f->column_offset[e.other+1];++j) {
            if(!tick(f))return false;
            Edge other=f->reverse[j];Lit b=other.other;
            if(b==a || !live(f,other))continue;
            if(!f->counts[b]++)f->touched[touched++]=b;
            if(f->counts[b]>score){best=b;score=f->counts[b];}
        }
    }
    for(uint32_t i=0;i<touched;++i){if(!tick(f))return false;f->counts[f->touched[i]]=0;}
    if(!best)return true;
    for(size_t i=f->offset[a];i<f->offset[a+1];++i) {
        if(!tick(f))return false;
        Edge e=f->edges[i];if(live(f,e))f->marked[e.other]=1;
    }
    uint32_t columns=0;
    for(size_t i=f->offset[best];i<f->offset[best+1];++i) {
        if(!tick(f))return false;
        Edge e=f->edges[i];if(live(f,e)&&f->marked[e.other])f->columns[columns++]=e.other;
    }
    for(size_t i=f->offset[a];i<f->offset[a+1];++i){if(!tick(f))return false;f->marked[f->edges[i].other]=0;}
    touched=0;
    for(uint32_t i=0;i<columns;++i) {
        Lit b=f->columns[i];f->marked[b]=1;
        for(size_t j=f->column_offset[b];j<f->column_offset[b+1];++j) {
            if(!tick(f))return false;
            Edge e=f->reverse[j];if(!live(f,e))continue;
            if(!f->counts[e.other]++)f->touched[touched++]=e.other;
        }
    }
    uint32_t rows=0;
    for(uint32_t i=0;i<touched;++i) {
        if(!tick(f))return false;
        Lit b=f->touched[i];if(f->counts[b]==columns)f->rows[rows++]=b;f->counts[b]=0;
    }
    if((uint64_t)rows*columns <= (uint64_t)rows+columns)goto done;
    /* Reserve an auxiliary namespace before any mutation. All partial prefixes
       remain equisatisfiable; cancellation never requires undoing a rectangle. */
    if(!s->factor_original_vars)s->factor_original_vars=s->num_vars;
    Var fresh=solver_new_var(s);if(!fresh){s->error=true;return false;}
    Lit x=mkLit(fresh,false);++s->stats.factor_variables;
    for(uint32_t i=0;i<columns;++i){
        Residue r=f->residues[f->columns[i]];
        if(!tick(f)||!add(f,neg(x),r.first,r.second))return false;
    }
    for(uint32_t i=0;i<rows;++i){if(!tick(f)||!add(f,x,f->rows[i],0))return false;}
    for(uint32_t i=0;i<rows;++i) {
        Lit row=f->rows[i];
        for(size_t j=f->offset[row];j<f->offset[row+1];++j) {
            if(!tick(f))return false;
            Edge e=f->edges[j];
            if(f->marked[e.other] && live(f,e)) {
                solver_delete_clause(s,e.ref);++s->stats.factor_deleted;
                if(s->error)return false;
            }
        }
    }
done:
    for(uint32_t i=0;i<columns;++i){if(!tick(f))return false;f->marked[f->columns[i]]=0;}
    return true;
}
uint32_t solver_factor(Solver *s) {
    if(!s || s->decision_level || s->has_solved || s->elim || s->num_learnts ||
       s->factor_original_vars || s->opts.inprocess || s->opts.local_search ||
       s->result==FALSE || !s->num_vars)return 0;
    Factor f={.s=s,.literals=2*(s->num_vars+1)};
    if(graph(&f))for(Lit a=2;a<f.literals;++a) {
        if(!tick(&f) || s->stats.factor_variables>=s->opts.factor_max_variables)break;
        if(s->values[var(a)]==UNDEF && f.offset[a+1]-f.offset[a]>=3 && !rectangle(&f,a))break;
    }
    free(f.pairs);free(f.edges);free(f.reverse);free(f.residues);free(f.column_offset);free(f.offset);free(f.counts);free(f.marked);
    free(f.touched);free(f.columns);free(f.rows);
    return s->stats.factor_variables;
}
