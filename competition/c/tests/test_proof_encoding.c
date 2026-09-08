/* Byte-level proof compatibility and immediate/deferred output failures. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#include "../include/solver.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

static void reference(FILE *out,const Lit *lits,unsigned n,bool binary,bool deletion) {
    if(binary) {
        assert(fputc(deletion?'d':'a',out)!=EOF);
        for(unsigned i=0;i<n;++i) {
            unsigned value=lits[i];
            do {
                unsigned byte=value%128;value/=128;
                assert(fputc(byte+(value?128:0),out)!=EOF);
            } while(value);
        }
        assert(fputc(0,out)!=EOF);
    } else {
        if(deletion)assert(fputs("d ",out)>=0);
        for(unsigned i=0;i<n;++i)assert(fprintf(out,"%d ",toDimacs(lits[i]))>0);
        assert(fputs("0\n",out)>=0);
    }
}
static void equal(FILE *a,FILE *b) {
    assert(!fflush(a) && !fflush(b));rewind(a);rewind(b);
    for(;;) {
        unsigned char x[4096],y[4096];size_t nx=fread(x,1,sizeof x,a),ny=fread(y,1,sizeof y,b);
        assert(nx==ny && !memcmp(x,y,nx));if(!nx)break;
    }
    assert(!ferror(a) && !ferror(b));
}
static void bytes(void) {
    const unsigned vars[]={1,9,10,63,64,99,100,127,128,999,1000,8191,8192,
        9999,10000,99999,100000,999999,1000000,9999999,10000000,99999999,
        100000000,MAX_VARS,INT32_MAX};
    const unsigned sizes[]={0,1,2,31,340,341,342,343,355,356,357,371,372,373,818,819,820,4095,4096,4097,10000};
    Lit *lits=malloc(10000*sizeof *lits);assert(lits);unsigned cases=0;
    for(unsigned binary=0;binary<2;++binary)for(unsigned unbuffered=0;unbuffered<2;++unbuffered)
    for(unsigned pattern=0;pattern<4;++pattern)for(unsigned k=0;k<sizeof sizes/sizeof *sizes;++k) {
        unsigned n=sizes[k];
        for(unsigned i=0;i<n;++i) {
            unsigned v=pattern==0?vars[(i/2)%(sizeof vars/sizeof *vars)]:pattern==1?1:INT32_MAX;
            bool negative=(i+(pattern==3?2:pattern))&1u;
            if(pattern==3 && i+1==n)negative=false;
            lits[i]=mkLit(v,negative);
        }
        Solver *s=solver_new();assert(s);s->opts.binary_proof=binary;
        s->proof_file=tmpfile();FILE *expected=tmpfile();assert(s->proof_file && expected);
        if(unbuffered)assert(!setvbuf(s->proof_file,NULL,_IONBF,0));
        // Consecutive addition/deletion/empty records must not share stale bytes.
        proof_add_clause(s,lits,n);reference(expected,lits,n,binary,false);
        proof_delete_clause(s,lits,n);reference(expected,lits,n,binary,true);
        proof_add_clause(s,NULL,0);reference(expected,NULL,0,binary,false);
        assert(!s->error);equal(s->proof_file,expected);fclose(expected);solver_free(s);++cases;
    }
    assert(cases==336);free(lits);
    Solver *s=solver_new();assert(s);proof_add_clause(s,NULL,UINT32_MAX);
    proof_delete_clause(s,NULL,UINT32_MAX);assert(!s->error);solver_free(s);
    puts("PASS: 336 text/binary proof byte cases, numeric/chunk boundaries and disabled output");
}

#if defined(__APPLE__) || defined(__GLIBC__)
typedef struct {size_t limit,written,calls;} Sink;
static ssize_t sink_write(void *cookie,const char *data,size_t size) {
    Sink *s=cookie;(void)data;++s->calls;
    if(s->written==s->limit) {errno=ENOSPC;return -1;}
    size_t accepted=MIN(size,s->limit-s->written);s->written+=accepted;
    return (ssize_t)accepted;
}
#ifdef __APPLE__
static int mac_write(void *cookie,const char *data,int size) {
    return (int)sink_write(cookie,data,(size_t)size);
}
#endif
static FILE *sink_file(Sink *s) {
#ifdef __APPLE__
    return funopen(s,NULL,mac_write,NULL,NULL);
#else
    cookie_io_functions_t functions={.write=sink_write};
    return fopencookie(s,"w",functions);
#endif
}
static void failures(void) {
    const unsigned limits[]={0,1,2,3,11,12,4095,4096,4097,8191,10000};
    Lit *lits=malloc(10000*sizeof *lits);assert(lits);
    for(unsigned i=0;i<10000;++i)lits[i]=mkLit(MAX_VARS,i&1u);
    for(unsigned binary=0;binary<2;++binary) {
        for(unsigned i=0;i<sizeof limits/sizeof *limits;++i) {
            Sink sink={.limit=limits[i]};Solver *s=solver_new();assert(s);
            s->opts.binary_proof=binary;s->proof_file=sink_file(&sink);assert(s->proof_file);
            assert(!setvbuf(s->proof_file,NULL,_IONBF,0));
            proof_delete_clause(s,lits,10000);
            assert(s->error && ferror(s->proof_file) && sink.calls && sink.written<=sink.limit);
            assert(solver_solve(s)==UNDEF);
            solver_free(s);
        }
        Sink sink={0};char buffer[4096];Solver *s=solver_new();assert(s);
        s->opts.binary_proof=binary;s->proof_file=sink_file(&sink);assert(s->proof_file);
        assert(!setvbuf(s->proof_file,buffer,_IOFBF,sizeof buffer));
        proof_add_clause(s,NULL,0);assert(!s->error && !sink.calls);
        assert(!solver_add_clause(s,NULL,0));
        assert(solver_solve(s)==UNDEF && s->error && sink.calls);
        solver_free(s);
    }
    free(lits);
    puts("PASS: 22 proof output cutoffs and 2 deferred-flush failures return UNKNOWN");
}
#endif
int main(void) {
    bytes();
#if defined(__APPLE__) || defined(__GLIBC__)
    failures();
#endif
}
