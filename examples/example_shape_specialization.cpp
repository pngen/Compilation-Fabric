// example_shape_specialization
#include "ExampleUtil.hpp"
using namespace compilationfabric;
int main() {
    auto cfg = cfe::baseConfig("example_shape_specialization");
    CompilationFabric f(cfg);
    int rc = 0;
    try { rc = [&]() -> int {

        auto r1 = f.compile(cfe::cpuReq("name=shape\nshape=256\nadd scalar=1.0\n", 6, Datatype::F32, {256}));
        auto r2 = f.compile(cfe::cpuReq("name=shape\nshape=512\nadd scalar=1.0\n", 6, Datatype::F32, {512}));
        if (!r1.ok() || !r2.ok()) { std::printf("FAIL\n"); return 1; }
        std::printf("shape_specialization: distinct=%d\n", (int)(r1->artifactId != r2->artifactId));
        return r1->artifactId != r2->artifactId ? 0 : 1;

    }(); }
    catch (const std::exception& e) { std::fprintf(stderr, "error: %s\n", e.what()); rc = 1; }
    cfe::cleanup(cfg);
    return rc;
}