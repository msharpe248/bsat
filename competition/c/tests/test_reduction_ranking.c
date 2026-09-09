#include "../include/solver.h"
#include <assert.h>
#include <stdio.h>

/* IDs 0/1 are protected binary/glue clauses. The remaining six have a
   strict expected quality order, independent of their insertion order. */
static const uint32_t lbds[] = {2, 2, 3, 3, 4, 31, 32, 40};
static const float activities[] = {16, 8, 1, 9, 2, 5, 3, 7};
static const unsigned ranked[] = {3, 2, 4, 5, 6, 7};
static const uint32_t cutoffs[] = {0, 2, 3, 4, 30, 31, 32, UINT32_MAX};
static unsigned cases;

static void check(const unsigned *order, unsigned fraction, uint32_t cutoff) {
    SolverOpts o = default_opts();o.reduce_fraction = fraction / 8.0;
    o.max_lbd = cutoff;o.clause_decay = 0.5;
    Solver *s = solver_new_with_opts(&o);assert(s);
    for (Var v = 1; v <= 47; ++v) {
        assert(solver_new_var(s) == v);
        Lit unit = mkLit(v, false);assert(solver_add_clause(s, &unit, 1));
    }
    /* The original units entail every synthetic learned clause. Dead prefix
       padding forces relocation, even when reduction retains every clause. */
    Lit padding[1000];for (unsigned i = 0; i < 1000; ++i) padding[i] = mkLit(9, false);
    CRef dead = arena_alloc(s->arena, padding, 1000, false);assert(dead != INVALID_CLAUSE);
    arena_delete(s->arena, dead);
    s->learnts = malloc(8 * sizeof *s->learnts);assert(s->learnts);s->learnts_size = 8;
    for (unsigned at = 0; at < 8; ++at) {
        unsigned id = at < 2 ? at : order[at - 2];
        Lit lits[40];lits[0] = mkLit(id + 1, false);
        for (unsigned j = 1; j < 40; ++j) lits[j] = mkLit(j + 8, false);
        unsigned size = id ? 40 : 2;
        CRef cr = arena_alloc(s->arena, lits, size, true);assert(cr != INVALID_CLAUSE);
        set_clause_lbd(s->arena, cr, lbds[id]);
        CLAUSE_HEADER(s->arena, cr)->activity = activities[id];
        s->learnts[s->num_learnts++] = cr;
        CRef watched = size == 2 ? arena_binary_watch_ref(cr) : cr;
        watch_add(s->watches, lits[0], watched, lits[1]);
        watch_add(s->watches, lits[1], watched, lits[0]);
    }
    unsigned expected = 3, keep = 6 * fraction / 8, count = 2;
    for (unsigned rank = 0; rank < keep; ++rank)
        if (lbds[ranked[rank]] <= cutoff) { expected |= 1u << ranked[rank];++count; }
    solver_reduce_db(s);
    assert(s->garbage_collections == 1 && s->num_learnts == count);
    assert(s->stats.deleted_clauses == 8 - count && s->stats.reduces == 1);
    unsigned actual = 0;
    for (unsigned i = 0; i < s->num_learnts; ++i) {
        CRef cr = s->learnts[i];unsigned id = var(CLAUSE_LITS(s->arena, cr)[0]) - 1;
        assert(id < 8 && !(actual & (1u << id)));actual |= 1u << id;
        assert(clause_lbd(s->arena, cr) == lbds[id]);
        assert(clause_activity(s->arena, cr) == activities[id] * (id < 2 ? 1.0f : 0.5f));
        WatchList *own = watch_list(s->watches, mkLit(id + 1, false));
        assert(own->size == 1);
        assert(own->watches[0].cref == (id ? cr : arena_binary_watch_ref(cr)));
    }
    assert(actual == expected);
    for (unsigned id = 0; id < 8; ++id)
        assert(watch_list(s->watches, mkLit(id + 1, false))->size == !!(expected & (1u << id)));
    assert(watch_list(s->watches, mkLit(9, false))->size == count);
    assert(!s->watches->failed && solver_check_model(s));
    solver_free(s);++cases;
}

static void permute(unsigned *order, unsigned at) {
    if (at == 6) {
        for (unsigned f = 0; f <= 8; ++f)
            for (unsigned c = 0; c < sizeof cutoffs / sizeof *cutoffs; ++c)
                check(order, f, cutoffs[c]);
        return;
    }
    for (unsigned i = at; i < 6; ++i) {
        unsigned tmp = order[at];order[at] = order[i];order[i] = tmp;
        permute(order, at + 1);
        tmp = order[at];order[at] = order[i];order[i] = tmp;
    }
}

/* The fixed eight-element equal-key policy places ID 7 first, then 1..6,0.
   The retention boundary must survive watch deletion and arena relocation. */
static void tied_boundary(void) {
    Solver *s=solver_new();assert(s);s->opts.reduce_fraction=0.5;
    for(Var v=1;v<=11;++v) {
        assert(solver_new_var(s)==v);Lit unit=mkLit(v,false);
        assert(solver_add_clause(s,&unit,1));
    }
    Lit padding[1000];for(unsigned i=0;i<1000;++i)padding[i]=mkLit(11,false);
    CRef dead=arena_alloc(s->arena,padding,1000,false);assert(dead!=INVALID_CLAUSE);
    arena_delete(s->arena,dead);
    s->learnts=malloc(8*sizeof *s->learnts);assert(s->learnts);s->learnts_size=8;
    for(unsigned id=0;id<8;++id) {
        Lit c[]={mkLit(id+1,false),mkLit(9,false),mkLit(10,false),mkLit(11,false)};
        CRef cr=arena_alloc(s->arena,c,4,true);assert(cr!=INVALID_CLAUSE);
        set_clause_lbd(s->arena,cr,3);CLAUSE_HEADER(s->arena,cr)->activity=1;
        s->learnts[s->num_learnts++]=cr;
        watch_add(s->watches,c[0],cr,c[1]);watch_add(s->watches,c[1],cr,c[0]);
    }
    solver_reduce_db(s);assert(s->num_learnts==4 && s->garbage_collections==1);
    unsigned ids=0;
    for(unsigned i=0;i<s->num_learnts;++i) {
        CRef cr=s->learnts[i];Var v=var(CLAUSE_LITS(s->arena,cr)[0]);ids|=1u<<(v-1);
        WatchList *w=watch_list(s->watches,mkLit(v,false));assert(w->size==1 && w->watches[0].cref==cr);
    }
    assert(ids==((1u<<7)|(1u<<1)|(1u<<2)|(1u<<3)));
    for(unsigned id=0;id<8;++id)assert(watch_list(s->watches,mkLit(id+1,false))->size==!!(ids&(1u<<id)));
    assert(watch_list(s->watches,mkLit(9,false))->size==4);
    assert(s->stats.deleted_clauses==4 && solver_check_model(s));solver_free(s);
}

int main(void) {
    unsigned order[] = {2, 3, 4, 5, 6, 7};permute(order, 0);
    assert(cases == 51840);tied_boundary();
    printf("PASS: %u reduction rankings, fractions, cutoffs, protected clauses and relocated watches\n", cases);
}
