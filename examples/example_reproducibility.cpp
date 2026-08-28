// example_reproducibility
#include "ExampleUtil.hpp"

using namespace compilationfabric;
int main() {
    auto cfg = cfe::baseConfig("example_reproducibility");
    CompilationFabric f(cfg);
    int rc = 0;
    try { rc = [&]() -> int {

        auto r1 = f.compile(cfe::cpuReq("name=repro\nshape=512\nscale scalar=3.0\n", 13));
        auto r2 = f.compile(cfe::cpuReq("name=repro\nshape=512\nscale scalar=3.0\n", 14));
        if (!r1.ok() || !r2.ok()) { std::printf("FAIL\n"); return 1; }
        bool sameBytes = (r1->artifact.contentDigest == r2->artifact.contentDigest);
        std::printf("reproducibility: deterministic_digest=%d reproducibility=%s\n", (int)sameBytes, std::string(reproducibilityName(r1->artifact.provenance.reproducibility)).c_str());
        return sameBytes ? 0 : 1;

    }(); }
    catch (const std::exception& e) { std::fprintf(stderr, "error: %s\n", e.what()); rc = 1; }
    cfe::cleanup(cfg);
    return rc;
}