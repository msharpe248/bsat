/* Grammar-based stream fuzzing with known SAT truth and deliberate syntax/NUL
   corruption. Headers stay bounded so valid huge allocations are not fuzz noise. */
#include "solver.h"
#include "dimacs.h"
#include <assert.h>
#include <stdint.h>
void fuzz_alloc_reset(size_t);
bool fuzz_alloc_failed(void);
int LLVMFuzzerTestOneInput(const uint8_t *data,size_t size) {
    if(size<4||size>512)return 0;
    unsigned mode=data[0]%4,nc=1+data[1]%16;
    fuzz_alloc_reset(data[0]&128?(size_t)data[2]+1:0);
    int cs[16][3];FILE *file=tmpfile();assert(file);
    fprintf(file,"%c cnf 6 %u\n",mode==1?'q':'p',nc);
    for(unsigned i=0;i<nc;++i) {
        for(unsigned j=0;j<3;++j) {
            unsigned b=data[(3+i*3+j)%size];cs[i][j]=(1+b%6)*(b&8?-1:1);
            fprintf(file,"%d%c",cs[i][j],b&16?'\n':' ');
        }
        if(mode==3 && i==0)fputc(0,file);
        if(mode!=2||i+1!=nc)fputs("0\n",file);
    }
    rewind(file);Solver *s=solver_new();if(!s){assert(fuzz_alloc_failed());fclose(file);return 0;}
    DimacsError error=dimacs_parse_stream(s,file);fclose(file);
    if(error==DIMACS_OK) {
        assert(mode==0);
        bool sat=false;
        for(unsigned bits=0;bits<64;++bits) {
            bool ok=true;
            for(unsigned i=0;i<nc;++i) {
                bool clause=false;
                for(unsigned j=0;j<3;++j){int l=cs[i][j];unsigned v=abs(l);clause|=((bits>>(v-1))&1u)==(l>0);}
                ok &= clause;
            }
            sat |= ok;
        }
        lbool result=solver_solve(s);
        if(!s->error)assert(result==(sat?TRUE:FALSE));else assert(result==UNDEF);
    } else if(mode==0)assert(fuzz_alloc_failed());
    solver_free(s);return 0;
}
