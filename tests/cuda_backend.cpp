// Compilation Fabric - real CUDA backend integration through the runtime.
#include "CompilationFabric/CompilationFabric.hpp"
#include "CompilationFabric/Cuda.hpp"
#include "CompilationFabric/CpuBackend.hpp"
#include "TestUtil.hpp"
#include <filesystem>
#include <cstdio>
using namespace compilationfabric;

static CompilationRequest mkReq(const std::string& src, uint64_t id) {
    CompilationRequest r; r.requestId = CompilationRequestId::fromU64(id); r.logicalOperation = LogicalOperation::fromU64(1);
    r.source = src; r.sourceDigest = Sha256::hash(src); r.sourceLanguage = "cf-src";
    r.datatype = Datatype::F32; r.rank = 1; r.staticShape = {1024}; r.backend = "cuda-nvrtc";
    r.targetArchitecture = "sm_120"; r.computeCapability = "12.0"; r.isa = "sm_120"; r.kernelABI = "nvrtc";
    return r;
}
int main() {
    auto api = CudaBackend::api();
    if (!api->available()) { std::printf("cuda: not available\n"); return 2; }
    std::filesystem::path root = std::filesystem::temp_directory_path() / ("cf_cuda_" + std::to_string(Clock::monotonicNanos()));
    CompilationFabricConfig cfg; cfg.artifactRoot = root.string(); cfg.persistenceEnabled = true; cfg.allowCuda = true;
    CompilationFabric fabric(cfg);
    // target discovery through the backend
    auto tg = fabric.targets();
    if (tg.ok()) { for (auto& t : *tg) std::fprintf(stderr, "  target: %s %s cc=%s mem=%llu\n", t.deviceName.c_str(), t.architecture.c_str(), t.computeCapability.c_str(), (unsigned long long)t.deviceMemoryBytes); }

    CF_BEGIN("cuda-nvrtc-full-lifecycle");
    auto req = mkReq("name=cuda_vec\nshape=1024\nadd scalar=2.0\n", 77);
    auto res = fabric.compile(req);
    CF_CHECK(res.ok()); if (!res.ok()) { std::printf("cuda compile failed: %s\n", res.message().c_str()); return 3; }
    CF_CHECK_EQ(res->validated, true);
    CF_CHECK_EQ(res->deployable, true);
    CF_CHECK_EQ(res->artifact.format, ArtifactFormat::CUBIN);
    // deploy (module load) + acquire/release lease
    auto mod = fabric.deploy(res->artifactId);
    CF_CHECK(mod.ok());
    auto lease = fabric.acquire(res->artifactId);
    CF_CHECK(lease.ok());
    if (lease.ok()) { auto rel = fabric.release(*lease); CF_CHECK(rel.ok()); }
    std::error_code ec; std::filesystem::remove_all(root, ec);
    std::printf("cuda_backend: validated=%d deployable=%d refMatch=%d\n", (int)res->validated, (int)res->deployable, (int)res->referenceMatched);
    CF_FINISH("cuda_backend");
}