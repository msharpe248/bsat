#include <bsat.h>
#include <ipasir.h>
#include <cassert>
#include <memory>
int main() {
    std::unique_ptr<bsat,decltype(&bsat_destroy)> s(bsat_create(BSAT_ABI_VERSION,0),bsat_destroy);
    assert(s);int unit=-3;assert(bsat_add_clause(s.get(),&unit,1));
    assert(bsat_solve(s.get(),nullptr,0)==BSAT_SAT);
    assert(bsat_value(s.get(),3)==-3);
    assert(bsat_set_query_limits(s.get(),0,0,0));
    bsat_stats_v1 stats{};assert(bsat_get_stats(s.get(),&stats,sizeof stats));
    assert(stats.version==1 && stats.result==10);
    void *i=ipasir_init();assert(i);ipasir_add(i,1);ipasir_add(i,0);
    ipasir_assume(i,-1);assert(ipasir_solve(i)==20);assert(ipasir_failed(i,-1));
    assert(ipasir_solve(i)==10);assert(ipasir_val(i,1)==1);ipasir_release(i);
}
