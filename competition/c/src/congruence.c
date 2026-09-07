/* Bounded congruence on complete two-input AND/XOR and ITE definitions.
 * Every learned clause is independently checked by root RUP before mutation.
 * Hashes only select candidates: exact keys and complete CNF patterns matter.
 */
#include "../include/solver.h"
#include <stdlib.h>
#include <string.h>

enum { AND_GATE, XOR_GATE, ITE_GATE, DEAD_GATE };
typedef struct { Var v[3]; unsigned mask; } Cell;
typedef struct { Var out, condition; uint32_t yes, no; } Branch;
typedef struct { Lit input; uint32_t next; } Alternative;
typedef struct { Lit out, a, b, c; unsigned kind; } Gate;
typedef struct {
    Solver *s;
    Cell *cells;size_t cell_cap;
    Branch *branches;size_t branch_cap;
    Alternative *alternatives;uint32_t alternative_count, alternative_capacity;
    Gate *gates;uint32_t count, capacity, gate_limit;
    Lit *parent;
    uint32_t *table;size_t table_cap;
    uint32_t changes;
} Closure;

static bool seed_alias(Closure *, Lit, Lit);
static bool lemma(Closure *, const Lit *, unsigned);

static bool tick(Closure *c) {
    Solver *s = c->s;
    if (solver_budget_exhausted(s)) return false;
    ++s->work;
    return true;
}

static uint64_t mix(uint64_t x) {
    x ^= x >> 30;x *= UINT64_C(0xbf58476d1ce4e5b9);
    x ^= x >> 27;x *= UINT64_C(0x94d049bb133111eb);
    return x ^ (x >> 31);
}

static size_t capacity_for(uint64_t n) {
    size_t cap = 16;
    while (cap < n) { if (cap > SIZE_MAX/2) return 0;cap *= 2; }
    return cap;
}

static Cell *cell(Closure *c, const Var v[3], bool insert) {
    size_t at = mix(v[0] ^ ((uint64_t)v[1]<<21) ^ ((uint64_t)v[2]<<42)) & (c->cell_cap-1);
    for (;;) {
        if (!tick(c)) return NULL;
        Cell *p = c->cells+at;
        if (!p->v[0]) {
            if (!insert) return NULL;
            memcpy(p->v,v,sizeof p->v);return p;
        }
        if (!memcmp(p->v,v,sizeof p->v)) return p;
        at = (at+1) & (c->cell_cap-1);
    }
}

static bool binary(Closure *c, Lit a, Lit b) {
    if (var(a) == var(b)) return false;
    if (var(a) > var(b)) { Lit t=a;a=b;b=t; }
    Var v[] = {var(a),var(b),0};
    Cell *p = cell(c,v,false);
    return p && (p->mask & (1u << (sign(a) | (sign(b)<<1))));
}

static bool gate(Closure *c, Lit out, Lit a, Lit b, Lit condition, unsigned kind) {
    if (!tick(c)) return false;
    if (c->count == c->gate_limit) return true;
    if (c->count == c->capacity) {
        uint64_t cap = c->capacity ? 2*(uint64_t)c->capacity : 128;
        if (cap > c->gate_limit) cap=c->gate_limit;
        if (cap > UINT32_MAX || cap > SIZE_MAX/sizeof *c->gates) { c->s->error=true;return false; }
        Gate *p = realloc(c->gates,(size_t)cap*sizeof *p);
        if (!p) { c->s->error=true;return false; }
        c->gates=p;c->capacity=(uint32_t)cap;
    }
    c->gates[c->count++] = (Gate){out,a,b,condition,kind};
    ++c->s->stats.congruence_gates;
    return true;
}

static bool branch(Closure *c, Var out, Lit condition, Lit input) {
    size_t at = mix(out ^ ((uint64_t)var(condition)<<32)) & (c->branch_cap-1);
    for (;;) {
        if (!tick(c)) return false;
        Branch *p = c->branches+at;
        if (!p->out) { p->out=out;p->condition=var(condition); }
        if (p->out == out && p->condition == var(condition)) {
            /* Keep alternative conditional definitions: choosing only one
               can hide the common gate needed to connect two output cones. */
            uint32_t *slot = sign(condition) ? &p->no : &p->yes;
            for (uint32_t i=*slot;i;i=c->alternatives[i-1].next) {
                if (!tick(c)) return false;
                if (c->alternatives[i-1].input==input) return true;
            }
            if (c->alternative_count==c->alternative_capacity) {
                uint64_t cap=c->alternative_capacity ? 2*(uint64_t)c->alternative_capacity : 128;
                if (cap>UINT32_MAX || cap>SIZE_MAX/sizeof *c->alternatives) { c->s->error=true;return false; }
                Alternative *a=realloc(c->alternatives,(size_t)cap*sizeof *a);
                if (!a) { c->s->error=true;return false; }
                c->alternatives=a;c->alternative_capacity=(uint32_t)cap;
            }
            c->alternatives[c->alternative_count++]=(Alternative){input,*slot};
            *slot=c->alternative_count;
            return true;
        }
        at = (at+1) & (c->branch_cap-1);
    }
}

static bool extract(Closure *c) {
    Solver *s=c->s;
    c->cell_cap=capacity_for(2*(uint64_t)s->num_clauses+1);
    c->branch_cap=capacity_for(4*(uint64_t)s->num_clauses+1);
    if (!c->cell_cap || !c->branch_cap || c->cell_cap > SIZE_MAX/sizeof *c->cells ||
        c->branch_cap > SIZE_MAX/sizeof *c->branches) { s->error=true;return false; }
    /* Avoid allocating a full index when its later scan cannot fit the budget. */
    if (s->work_limit && s->work_limit-s->work < c->cell_cap+c->branch_cap) return false;
    c->cells=calloc(c->cell_cap,sizeof *c->cells);
    c->branches=calloc(c->branch_cap,sizeof *c->branches);
    if (!c->cells || !c->branches) { s->error=true;return false; }
    for (uint32_t i=0;i<s->num_clauses;++i) {
        if (!tick(c)) return false;
        CRef cr=s->clauses[i];
        if (cr == INVALID_CLAUSE || clause_deleted(s->arena,cr)) continue;
        const Lit *lits=CLAUSE_LITS(s->arena,cr);Lit open[3];unsigned n=0;bool satisfied=false;
        for (uint32_t j=0;j<CLAUSE_SIZE(s->arena,cr);++j) {
            if (!tick(c)) return false;
            lbool value=lxor(s->values[var(lits[j])],sign(lits[j]));
            if (value == TRUE) { satisfied=true;break; }
            if (value == UNDEF) { if (n == 3) { ++n;break; }open[n++]=lits[j]; }
        }
        if (satisfied || n<2 || n>3) continue;
        for (unsigned j=1;j<n;++j) for (unsigned k=j;k && var(open[k])<var(open[k-1]);--k) {
            Lit t=open[k];open[k]=open[k-1];open[k-1]=t;
        }
        Var v[3]={0};unsigned mask=0;
        for (unsigned j=0;j<n;++j) { v[j]=var(open[j]);mask |= sign(open[j])<<j; }
        Cell *p=cell(c,v,true);if (!p) return false;p->mask |= 1u<<mask;
    }
    /* (a|b|c) and (~a|b), for example, imply (b|c). Derive at most
       one binary per indexed ternary clause: the original index has at most
       num_clauses keys, so its >2*num_clauses slots cannot fill here. */
    for (size_t i=0;i<c->cell_cap;++i) {
        if (!tick(c) || s->result == FALSE) return false;
        Cell *p=c->cells+i;if (!p->v[2]) continue;
        for (unsigned m=0;m<8;++m) if (p->mask & (1u<<m)) {
            Lit lits[3];for (unsigned j=0;j<3;++j) lits[j]=mkLit(p->v[j],(m>>j)&1);
            for (unsigned j=0;j<3;++j) {
                Lit a=lits[(j+1)%3],b=lits[(j+2)%3];
                if (!binary(c,neg(lits[j]),a) && !binary(c,neg(lits[j]),b)) continue;
                if (!binary(c,a,b)) {
                    Lit pair[]={a,b};if (!lemma(c,pair,2)) return false;
                    ++s->stats.congruence_strengthened;
                    if (var(a)>var(b)) { Lit t=a;a=b;b=t; }
                    Var key[]={var(a),var(b),0};Cell *q=cell(c,key,true);if (!q) return false;
                    q->mask |= 1u << (sign(a) | (sign(b)<<1));
                }
                break;
            }
        }
    }
    for (size_t i=0;i<c->cell_cap;++i) {
        if (!tick(c)) return false;
        if (s->result == FALSE) return false;
        Cell *p=c->cells+i;
        if (!p->v[2]) {
            if (!p->v[0]) continue;
            for (unsigned m=0;m<4;++m) if (p->mask & (1u<<m)) {
                for (unsigned j=0;j<2;++j) {
                    unsigned other=m^(1u<<j);
                    if (m>=other || !(p->mask & (1u<<other))) continue;
                    Lit unit=mkLit(p->v[1-j],(m>>(1-j))&1);
                    if (lxor(s->values[var(unit)],sign(unit))!=TRUE && !lemma(c,&unit,1)) return false;
                    if (s->result == FALSE) return false;
                }
            }
            /* Exact complementary binary pairs already certify these aliases;
               no new proof clauses are needed merely to normalize gate keys. */
            if ((p->mask & 6u)==6u && !seed_alias(c,mkLit(p->v[0],false),mkLit(p->v[1],false))) return false;
            if ((p->mask & 9u)==9u && !seed_alias(c,mkLit(p->v[0],false),mkLit(p->v[1],true))) return false;
            continue;
        }
        for (unsigned m=0;m<8;++m) if (p->mask & (1u<<m)) {
            Lit lits[3];for (unsigned j=0;j<3;++j) lits[j]=mkLit(p->v[j],(m>>j)&1);
            for (unsigned j=0;j<3;++j) {
                Lit a=neg(lits[(j+1)%3]),b=neg(lits[(j+2)%3]);
                if (binary(c,neg(lits[j]),a) && binary(c,neg(lits[j]),b) &&
                    !gate(c,lits[j],a,b,0,AND_GATE)) return false;
                /* Two clauses differing only in the other two signs encode
                   a conditional equivalence. Join its opposite condition. */
                unsigned u=(j+1)%3,v=(j+2)%3,other=m^(1u<<u)^(1u<<v);
                if (m >= other || !(p->mask & (1u<<other))) continue;
                bool inverted=1^((m>>u)&1)^((m>>v)&1);
                if (!branch(c,p->v[u],neg(lits[j]),mkLit(p->v[v],inverted)) ||
                    !branch(c,p->v[v],neg(lits[j]),mkLit(p->v[u],inverted))) return false;
            }
        }
        for (unsigned parity=0;parity<2;++parity) {
            unsigned complete=parity ? 0x69u : 0x96u;
            if ((p->mask & complete) != complete) continue;
            for (unsigned j=0;j<3;++j)
                if (!gate(c,mkLit(p->v[j],parity),mkLit(p->v[(j+1)%3],false),
                          mkLit(p->v[(j+2)%3],false),0,XOR_GATE)) return false;
        }
    }
    for (size_t i=0;i<c->branch_cap;++i) {
        if (!tick(c)) return false;
        Branch *p=c->branches+i;
        for (uint32_t a=p->yes;a;a=c->alternatives[a-1].next)
            for (uint32_t b=p->no;b;b=c->alternatives[b-1].next) {
                if (c->count==c->gate_limit) goto extracted;
                if (!gate(c,mkLit(p->out,false),c->alternatives[a-1].input,c->alternatives[b-1].input,
                          mkLit(p->condition,false),ITE_GATE)) return false;
            }
    }
extracted:
    free(c->cells);c->cells=NULL;free(c->branches);c->branches=NULL;
    free(c->alternatives);c->alternatives=NULL;
    return true;
}

static Lit representative(Closure *c, Lit lit) {
    Lit root=lit;
    while (c->parent[var(root)] != mkLit(var(root),false)) {
        if (!tick(c)) return 0;
        root=c->parent[var(root)] ^ sign(root);
    }
    while (var(lit) != var(root)) {
        if (!tick(c)) return 0;
        Lit next=c->parent[var(lit)] ^ sign(lit);
        c->parent[var(lit)]=root ^ sign(lit);lit=next;
    }
    return root;
}

static bool seed_alias(Closure *c, Lit a, Lit b) {
    a=representative(c,a);b=representative(c,b);
    if (!a || !b) return false;
    if (a==b) return true;
    if (a==neg(b)) {
        /* Binary cycles need a RUP unit before the terminal empty clause. */
        if (!tick(c) || !solver_add_rup_clause(c->s,&a,1)) return false;
        ++c->s->stats.congruence_clauses;return true;
    }
    if (var(a)>var(b)) c->parent[var(a)]=b^sign(a);
    else c->parent[var(b)]=a^sign(b);
    ++c->changes;++c->s->stats.congruence_seeds;return true;
}

static bool lemma(Closure *c, const Lit *lits, unsigned n) {
    if (!tick(c)) return false;
    bool ok=solver_add_rup_clause(c->s,lits,n);
    if (ok) ++c->s->stats.congruence_clauses;
    return ok;
}

/* XOR/ITE equivalences may not themselves be RUP. Split on one input, prove
 * both conditional clauses by RUP, then prove the unconditioned consequence.
 * A budget cutoff can retain valid partial lemmas; it cannot commit an alias. */
static bool consequence(Closure *c, const Lit *lits, unsigned n, Lit split) {
    if (lemma(c,lits,n)) return true;
    if (!split || solver_budget_exhausted(c->s)) return false;
    Lit conditional[3];memcpy(conditional,lits,n*sizeof *lits);
    conditional[n]=split;if (!lemma(c,conditional,n+1)) return false;
    conditional[n]=neg(split);if (!lemma(c,conditional,n+1)) return false;
    return lemma(c,lits,n);
}

static bool merge(Closure *c, Lit a, Lit b, Lit split) {
    a=representative(c,a);b=representative(c,b);
    if (!a || !b) return false;
    if (a == b) return true;
    Lit forward[]={neg(a),b},reverse[]={neg(b),a};
    if (!consequence(c,forward,2,split) || !consequence(c,reverse,2,split)) return false;
    if (var(a) != var(b)) {
        if (var(a)>var(b)) c->parent[var(a)]=b^sign(a);
        else c->parent[var(b)]=a^sign(b);
    }
    ++c->changes;++c->s->stats.congruence_merges;
    return true;
}

static lbool value(Closure *c, Lit l) { return lxor(c->s->values[var(l)],sign(l)); }

static bool constant(Closure *c, Gate *g, bool truth, Lit split) {
    Lit unit=truth ? g->out : neg(g->out);
    if (value(c,unit) == TRUE) { g->kind=DEAD_GATE;return true; }
    if (!consequence(c,&unit,1,split)) return false;
    g->kind=DEAD_GATE;++c->changes;return true;
}

static bool simplify(Closure *c, Gate *g) {
    g->out=representative(c,g->out);g->a=representative(c,g->a);g->b=representative(c,g->b);
    if (!g->out || !g->a || !g->b) return false;
    Lit split=g->a;
    lbool a=value(c,g->a),b=value(c,g->b);
    if (g->kind == ITE_GATE) {
        g->c=representative(c,g->c);if (!g->c) return false;split=g->c;
        if (sign(g->c)) { Lit t=g->a;g->a=g->b;g->b=t;g->c=neg(g->c);a=value(c,g->a);b=value(c,g->b); }
        lbool condition=value(c,g->c);
        if (condition != UNDEF) {
            if (merge(c,g->out,condition == TRUE ? g->a : g->b,split)) g->kind=DEAD_GATE;
            return !solver_budget_exhausted(c->s);
        }
        if (g->a == g->b) {
            if (merge(c,g->out,g->a,split)) g->kind=DEAD_GATE;
            return !solver_budget_exhausted(c->s);
        }
        if (g->a == g->c) a=TRUE;else if (g->a == neg(g->c)) a=FALSE;
        if (g->b == g->c) b=FALSE;else if (g->b == neg(g->c)) b=TRUE;
        if (a != UNDEF) {
            if (a == TRUE) { g->out=neg(g->out);g->a=neg(g->c);g->b=neg(g->b); }
            else g->a=neg(g->c);
            g->kind=AND_GATE;g->c=0;
        } else if (b != UNDEF) {
            if (b == TRUE) { g->out=neg(g->out);g->a=neg(g->a);g->b=g->c; }
            else g->b=g->c;
            g->kind=AND_GATE;g->c=0;
        } else if (g->a == neg(g->b)) {
            g->out=neg(g->out);g->b=g->c;g->c=0;g->kind=XOR_GATE;
        } else {
            bool invert=sign(g->a);g->out ^= invert;g->a ^= invert;g->b ^= invert;
            return true;
        }
        a=value(c,g->a);b=value(c,g->b);
    }
    if (g->kind == AND_GATE) {
        if (g->a == neg(g->out) || g->b == neg(g->out)) return constant(c,g,false,split);
        if (a == FALSE || b == FALSE || g->a == neg(g->b)) return constant(c,g,false,split);
        if (a == TRUE && b == TRUE) return constant(c,g,true,split);
        Lit equal=a == TRUE ? g->b : b == TRUE ? g->a : g->a == g->b ? g->a : 0;
        if (equal) {
            if (merge(c,g->out,equal,split)) g->kind=DEAD_GATE;
            return !solver_budget_exhausted(c->s);
        }
    } else {
        lbool output=value(c,g->out);
        if (output != UNDEF) {
            if (merge(c,g->a,g->b ^ (output == TRUE),split)) g->kind=DEAD_GATE;
            return !solver_budget_exhausted(c->s);
        }
        if (g->a == g->b || g->a == neg(g->b)) return constant(c,g,g->a != g->b,split);
        if (var(g->a)==var(g->out) || var(g->b)==var(g->out)) {
            Lit self=var(g->a)==var(g->out) ? g->a : g->b;
            Lit other=var(g->a)==var(g->out) ? g->b : g->a;
            Lit unit=other ^ (self == g->out);
            if (value(c,unit) != TRUE) {
                if (!consequence(c,&unit,1,g->out)) return false;
                ++c->changes;
            }
            g->kind=DEAD_GATE;return true;
        }
        if (a != UNDEF && b != UNDEF) return constant(c,g,a != b,split);
        if (a != UNDEF || b != UNDEF) {
            Lit equal=a != UNDEF ? (g->b ^ (a == TRUE)) : (g->a ^ (b == TRUE));
            if (merge(c,g->out,equal,split)) g->kind=DEAD_GATE;
            return !solver_budget_exhausted(c->s);
        }
        g->out ^= sign(g->a)^sign(g->b);g->a &= ~1u;g->b &= ~1u;
    }
    if (g->a>g->b) { Lit t=g->a;g->a=g->b;g->b=t; }
    return true;
}

uint32_t solver_congruence(Solver *s) {
    if (!s || s->decision_level || s->has_solved || s->elim || s->num_learnts ||
        s->stats.decisions || s->result == FALSE || !s->num_clauses || solver_budget_exhausted(s)) return 0;
    Closure c={.s=s};
    /* Bound temporary gate storage even if conditional alternatives produce
       a large Cartesian product. Retaining a prefix is sound but incomplete. */
    c.gate_limit=(uint32_t)MIN(4*(uint64_t)s->num_clauses,UINT32_MAX-1u);
    uint64_t start_work=s->work;
    uint32_t start_trail=s->trail_size;
    if ((uint64_t)s->num_vars+1 > SIZE_MAX/sizeof *c.parent) { s->error=true;goto done; }
    c.parent=malloc(((size_t)s->num_vars+1)*sizeof *c.parent);
    if (!c.parent) { s->error=true;goto done; }
    for (Var v=1;v<=s->num_vars;++v) { if (!tick(&c)) goto done;c.parent[v]=mkLit(v,false); }
    if (!extract(&c) || !c.count) goto done;
    c.table_cap=capacity_for(2*(uint64_t)c.count+1);
    if (!c.table_cap || c.table_cap>SIZE_MAX/sizeof *c.table) { s->error=true;goto done; }
    c.table=calloc(c.table_cap,sizeof *c.table);
    if (!c.parent || !c.table) { s->error=true;goto done; }
    for (;;) {
        uint32_t before=c.changes;
        for (size_t i=0;i<c.table_cap;++i) { if (!tick(&c)) goto done;c.table[i]=0; }
        for (uint32_t i=0;i<c.count;++i) {
            if (!tick(&c) || s->result == FALSE) goto done;
            Gate *g=c.gates+i;if (g->kind == DEAD_GATE) continue;
            if (!simplify(&c,g)) {
                if (solver_budget_exhausted(s)) goto done;
                continue;
            }
            if (g->kind == DEAD_GATE || s->result == FALSE) continue;
            size_t at=mix(g->a ^ ((uint64_t)g->b<<20) ^ ((uint64_t)g->c<<40) ^ g->kind) & (c.table_cap-1);
            for (;;) {
                if (!tick(&c)) goto done;
                if (!c.table[at]) { c.table[at]=i+1;break; }
                Gate *h=c.gates+c.table[at]-1;
                if (h->kind == g->kind && h->a == g->a && h->b == g->b && h->c == g->c) {
                    Lit split=g->kind == ITE_GATE ? g->c : g->a;
                    if (merge(&c,g->out,h->out,split)) g->kind=DEAD_GATE;
                    break;
                }
                at=(at+1) & (c.table_cap-1);
            }
        }
        if (c.changes == before) break;
    }
 done:
    s->stats.congruence_work += s->work-start_work;
    s->stats.congruence_units += s->trail_size-start_trail;
    free(c.cells);free(c.branches);free(c.alternatives);free(c.gates);free(c.parent);free(c.table);
    return c.changes;
}
