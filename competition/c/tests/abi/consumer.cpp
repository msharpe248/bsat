#include <bsat.h>
#include <cassert>
#include <memory>
int main() {
    std::unique_ptr<bsat,decltype(&bsat_destroy)> s(bsat_create(BSAT_ABI_VERSION,0),bsat_destroy);
    assert(s);int unit=-3;assert(bsat_add_clause(s.get(),&unit,1));
    assert(bsat_solve(s.get(),nullptr,0)==BSAT_SAT);
    assert(bsat_value(s.get(),3)==-3);
}
