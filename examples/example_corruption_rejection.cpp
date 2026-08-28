// example_corruption_rejection
#include "ExampleUtil.hpp"
#include <fstream>
#include <filesystem>
using namespace compilationfabric;
int main() {
    auto cfg = cfe::baseConfig("example_corruption_rejection");
    CompilationFabric f(cfg);
    int rc = 0;
    try { rc = [&]() -> int {

        auto r1 = f.compile(cfe::cpuReq("name=corrupt\nshape=256\nadd scalar=1.0\n", 12));
        if (!r1.ok()) { std::printf("FAIL\n"); return 1; }
        // corrupt the persisted content file
        std::filesystem::path bin = std::filesystem::path(cfg.artifactRoot) / (r1->artifactId.toHex() + std::string("_") + std::to_string(r1->generation) + ".bin");
        { std::ofstream o(bin, std::ios::binary | std::ios::trunc); o.write("BADC0DEBADC0DEBADC0DEBADC0DE", 28); }
        PersistenceStore st(cfg.artifactRoot);
        auto l = st.load(r1->artifactId, r1->generation);
        std::printf("corruption_rejection: rejected=%d code=%s\n", (int)(!l.ok()), std::string(errorCodeName(l.code())).c_str());
        return l.ok() ? 1 : 0;

    }(); }
    catch (const std::exception& e) { std::fprintf(stderr, "error: %s\n", e.what()); rc = 1; }
    cfe::cleanup(cfg);
    return rc;
}