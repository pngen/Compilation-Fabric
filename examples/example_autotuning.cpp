// example_autotuning
#include "ExampleUtil.hpp"

using namespace compilationfabric;
int main() {
    auto cfg = cfe::baseConfig("example_autotuning");
    CompilationFabric f(cfg);
    int rc = 0;
    try { rc = [&]() -> int {

        auto req = cfe::cpuReq("name=at\nshape=2048\nscale scalar=1.5\n", 15);
        req.autotune = true; req.autotuneCandidates = 4; req.autotuneSeed = 42; req.optimizeLevel = 1;
        auto r = f.compile(req);
        if (!r.ok()) { std::printf("FAIL: %s\n", r.message().c_str()); return 1; }
        std::printf("autotuning: autotuned=%d winner=%s candidates=%zu\n", (int)r->autotuned, r->autotuneWinner.c_str(), r->autotuneCandidates.size());
        for (auto& a : r->autotuneCandidates) std::printf("  %s validated=%d perf=%.2fms reason=%s\n", a.variantId.c_str(), (int)a.validated, a.perfMs, a.reason.c_str());
        return (r->autotuned && !r->autotuneCandidates.empty()) ? 0 : 1;

    }(); }
    catch (const std::exception& e) { std::fprintf(stderr, "error: %s\n", e.what()); rc = 1; }
    cfe::cleanup(cfg);
    return rc;
}