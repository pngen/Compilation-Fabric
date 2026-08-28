// example_quantization_precision_specialization
#include "ExampleUtil.hpp"
using namespace compilationfabric;
int main() {
    auto cfg = cfe::baseConfig("example_quantization_precision_specialization");
    CompilationFabric f(cfg);
    int rc = 0;
    try { rc = [&]() -> int {

        auto r1 = cfe::cpuReq("name=qp\nshape=256\nadd scalar=1.0\n", 8);
        auto r2 = r1; r2.quantization = QuantizationMode::Int8; r2.precision = PrecisionMode::Reduced;
        auto c1 = f.compile(r1); auto c2 = f.compile(r2);
        if (!c1.ok() || !c2.ok()) { std::printf("FAIL\n"); return 1; }
        std::printf("quantization_precision_specialization: distinct=%d\n", (int)(c1->artifactId != c2->artifactId));
        return c1->artifactId != c2->artifactId ? 0 : 1;

    }(); }
    catch (const std::exception& e) { std::fprintf(stderr, "error: %s\n", e.what()); rc = 1; }
    cfe::cleanup(cfg);
    return rc;
}