// Real offline nvcc backend through Compilation Fabric.
#include "CompilationFabric/CompilationFabric.hpp"
#include "TestUtil.hpp"
#include <filesystem>
using namespace compilationfabric;
int main(){
    auto api = CudaBackend::api(); if(!api->available()){ std::printf("nvcc: cuda unavailable\n"); return 2; }
    std::filesystem::path root = std::filesystem::temp_directory_path()/("cf_nvcc_"+std::to_string(Clock::monotonicNanos()));
    CompilationFabricConfig cfg; cfg.artifactRoot=root.string(); cfg.allowCuda=true; cfg.persistenceEnabled=false;
    CompilationFabric fabric(cfg);
    CompilationRequest q; q.requestId=CompilationRequestId::fromU64(88); q.logicalOperation=LogicalOperation::fromU64(1);
    q.source="name=nvcc_vec\nshape=1024\nadd scalar=2.0\n"; q.sourceDigest=Sha256::hash(q.source);
    q.datatype=Datatype::F32; q.rank=1; q.staticShape={1024}; q.backend="cuda-nvcc"; q.targetArchitecture="sm_120"; q.computeCapability="12.0"; q.isa="sm_120";
    CF_BEGIN("cuda-nvcc-offline");
    auto res = fabric.compile(q);
    CF_CHECK(res.ok());
    if(!res.ok()){ std::printf("nvcc compile failed: %s\n", res.message().c_str()); return 3; }
    CF_CHECK_EQ(res->artifact.format, ArtifactFormat::CUBIN);
    CF_CHECK_EQ(res->validated, true);
    CF_CHECK_EQ(res->referenceMatched, true);
    CF_CHECK_EQ(res->deployable, true);
    std::printf("cuda_nvcc: validated=%d refMatch=%d format=%s\n", (int)res->validated, (int)res->referenceMatched, std::string(artifactFormatName(res->artifact.format)).c_str());
    auto mod = fabric.deploy(res->artifactId); CF_CHECK(mod.ok());
    std::error_code ec; std::filesystem::remove_all(root, ec);
    CF_FINISH("cuda_nvcc_backend");
}
