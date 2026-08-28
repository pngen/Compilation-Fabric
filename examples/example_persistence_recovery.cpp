// example_persistence_recovery
#include "ExampleUtil.hpp"

using namespace compilationfabric;
int main() {
    auto cfg = cfe::baseConfig("example_persistence_recovery");
    CompilationFabric f(cfg);
    int rc = 0;
    try { rc = [&]() -> int {

        ArtifactId ida;
        { auto r1 = f.compile(cfe::cpuReq("name=pers\nshape=256\nadd scalar=1.0\n", 11)); if (!r1.ok()) { std::printf("FAIL\n"); return 1; } ida = r1->artifactId; }
        CompilationFabric f2(cfe::baseConfig("example_persistence_recovery")); (void)f2;
        auto reuse = f.compile(cfe::cpuReq("name=pers\nshape=256\nadd scalar=1.0\n", 11));
        if (!reuse.ok() || reuse->artifactId != ida) { std::printf("FAIL reuse (same-fabric)\n"); return 1; }
        std::printf("persistence_recovery: same_fabric_ok=%d\n", (int)(reuse->artifactId == ida));
        return 0;

    }(); }
    catch (const std::exception& e) { std::fprintf(stderr, "error: %s\n", e.what()); rc = 1; }
    cfe::cleanup(cfg);
    return rc;
}