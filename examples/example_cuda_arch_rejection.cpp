// example_cuda_arch_rejection
#include "ExampleUtil.hpp"

using namespace compilationfabric;
int main() {
    auto cfg = cfe::baseConfig("example_cuda_arch_rejection");
    CompilationFabric f(cfg);
    int rc = 0;
    try { rc = [&]() -> int {

        auto req = cfe::cpuReq("name=cu\nshape=256\nadd scalar=1.0\n", 16);
        req.backend = "cuda-nvrtc"; req.targetArchitecture = "sm_fake"; req.computeCapability = "99.0"; req.isa = "sm_fake";
        auto r = f.compile(req);
        auto rejected = !r.ok();
        std::printf("cuda_arch_rejection: rejected=%d code=%s\n", (int)rejected, std::string(errorCodeName(r.code())).c_str());
        return rejected ? 0 : 1;

    }(); }
    catch (const std::exception& e) { std::fprintf(stderr, "error: %s\n", e.what()); rc = 1; }
    cfe::cleanup(cfg);
    return rc;
}