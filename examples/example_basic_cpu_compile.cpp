// example_basic_cpu_compile
#include "ExampleUtil.hpp"
using namespace compilationfabric;
int main() {
    auto cfg = cfe::baseConfig("example_basic_cpu_compile");
    CompilationFabric f(cfg);
    int rc = 0;
    try { rc = [&]() -> int {

        auto r = f.compile(cfe::cpuReq("name=vec_add\nshape=1024\nadd scalar=2.0\n", 1));
        if (!r.ok()) { std::printf("FAIL: %s\n", r.message().c_str()); return 1; }
        std::printf("basic_cpu_compile: validated=%d deployable=%d format=%s key=%s\n",
            (int)r->validated, (int)r->deployable, std::string(artifactFormatName(r->artifact.format)).c_str(), r->key.toHex().c_str());
        return 0;

    }(); }
    catch (const std::exception& e) { std::fprintf(stderr, "error: %s\n", e.what()); rc = 1; }
    cfe::cleanup(cfg);
    return rc;
}