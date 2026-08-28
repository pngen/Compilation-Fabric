// example_concurrent_single_flight
#include "ExampleUtil.hpp"
#include <thread>
#include <vector>
using namespace compilationfabric;
int main() {
    auto cfg = cfe::baseConfig("example_concurrent_single_flight");
    CompilationFabric f(cfg);
    int rc = 0;
    try { rc = [&]() -> int {

        auto req = cfe::cpuReq("name=csf\nshape=512\nscale scalar=2.0\n", 9);
        const int N = 8; std::vector<CompilationResult> rs(N); std::vector<std::thread> ts;
        for (int i=0;i<N;++i) ts.emplace_back([&,i]{ auto r=f.compile(req); if(r.ok()) rs[i]=*r; });
        for (auto& t: ts) t.join();
        bool same=true; for (int i=1;i<N;++i) if (rs[i].artifactId != rs[0].artifactId) same=false;
        std::printf("concurrent_single_flight: identical_artifact=%d\n", (int)same);
        return same ? 0 : 1;

    }(); }
    catch (const std::exception& e) { std::fprintf(stderr, "error: %s\n", e.what()); rc = 1; }
    cfe::cleanup(cfg);
    return rc;
}