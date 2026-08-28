// example_invalidation_recompile
#include "ExampleUtil.hpp"

using namespace compilationfabric;
int main() {
    auto cfg = cfe::baseConfig("example_invalidation_recompile");
    CompilationFabric f(cfg);
    int rc = 0;
    try { rc = [&]() -> int {

        auto r1 = f.compile(cfe::cpuReq("name=inv\nshape=256\nadd scalar=1.0\n", 10));
        if (!r1.ok()) { std::printf("FAIL\n"); return 1; }
        auto inv = f.invalidateByKey(r1->key); if (!inv.ok()) { std::printf("FAIL inv\n"); return 1; }
        auto after = f.compile(cfe::cpuReq("name=inv\nshape=256\nadd scalar=1.0\n", 10));
        if (!after.ok()) { std::printf("FAIL recompile\n"); return 1; }
        std::printf("invalidation_recompile: recompiled=%d new=%d\n", (int)(!after->reused), (int)(after->artifactId != r1->artifactId));
        return (!after->reused && after->artifactId != r1->artifactId) ? 0 : 1;

    }(); }
    catch (const std::exception& e) { std::fprintf(stderr, "error: %s\n", e.what()); rc = 1; }
    cfe::cleanup(cfg);
    return rc;
}