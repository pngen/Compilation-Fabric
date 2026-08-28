// example_source_change_recompile
#include "ExampleUtil.hpp"
using namespace compilationfabric;
int main() {
    auto cfg = cfe::baseConfig("example_source_change_recompile");
    CompilationFabric f(cfg);
    int rc = 0;
    try { rc = [&]() -> int {

        auto r1 = f.compile(cfe::cpuReq("name=src\nshape=256\nadd scalar=2.0\n", 3));
        auto r2 = f.compile(cfe::cpuReq("name=src\nshape=256\nadd scalar=5.0\n", 4));
        if (!r1.ok() || !r2.ok()) { std::printf("FAIL: %s %s\n", r1.message().c_str(), r2.message().c_str()); return 1; }
        std::printf("source_change_recompile: different_artifact=%d\n", (int)(r1->artifactId != r2->artifactId));
        return r1->artifactId != r2->artifactId ? 0 : 1;

    }(); }
    catch (const std::exception& e) { std::fprintf(stderr, "error: %s\n", e.what()); rc = 1; }
    cfe::cleanup(cfg);
    return rc;
}