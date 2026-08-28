// example_exact_reuse
#include "ExampleUtil.hpp"
using namespace compilationfabric;
int main() {
    auto cfg = cfe::baseConfig("example_exact_reuse");
    CompilationFabric f(cfg);
    int rc = 0;
    try { rc = [&]() -> int {

        auto r1 = f.compile(cfe::cpuReq("name=reuse\nshape=256\nscale scalar=3.0\n", 2));
        if (!r1.ok()) { std::printf("FAIL: %s\n", r1.message().c_str()); return 1; }
        auto r2 = f.compile(cfe::cpuReq("name=reuse\nshape=256\nscale scalar=3.0\n", 2));
        if (!r2.ok()) { std::printf("FAIL: %s\n", r2.message().c_str()); return 1; }
        std::printf("exact_reuse: reused=%d same_artifact=%d\n", (int)r2->reused, (int)(r2->artifactId == r1->artifactId));
        return r2->reused ? 0 : 1;

    }(); }
    catch (const std::exception& e) { std::fprintf(stderr, "error: %s\n", e.what()); rc = 1; }
    cfe::cleanup(cfg);
    return rc;
}