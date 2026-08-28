// example_generation_invalidation
#include "ExampleUtil.hpp"
using namespace compilationfabric;
int main() {
    auto cfg = cfe::baseConfig("example_generation_invalidation");
    CompilationFabric f(cfg);
    int rc = 0;
    try { rc = [&]() -> int {

        auto r1 = f.compile(cfe::cpuReq("name=gen\nshape=256\nadd scalar=1.0\n", 5));
        if (!r1.ok()) { std::printf("FAIL\n"); return 1; }
        auto inv = f.invalidateByCompilerGeneration(1);
        if (!inv.ok()) { std::printf("FAIL invalidate\n"); return 1; }
        auto r2 = f.compile(cfe::cpuReq("name=gen\nshape=256\nadd scalar=1.0\n", 5));
        if (!r2.ok()) { std::printf("FAIL recompile\n"); return 1; }
        std::printf("generation_invalidation: recompiled=%d new_artifact=%d\n", (int)(!r2->reused), (int)(r2->artifactId != r1->artifactId));
        return (!r2->reused && r2->artifactId != r1->artifactId) ? 0 : 1;

    }(); }
    catch (const std::exception& e) { std::fprintf(stderr, "error: %s\n", e.what()); rc = 1; }
    cfe::cleanup(cfg);
    return rc;
}