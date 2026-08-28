#include "CompilationFabric/CompilationFabric.hpp"
#include "CompilationFabric/Cuda.hpp"
#include <cstdio>
using namespace compilationfabric;
int main() {
    auto a = CudaBackend::api();
    std::fprintf(stderr, "avail=%d err=%s nvrtc=%s\n", (int)a->available(), a->error().c_str(), a->nvrtcVersion().c_str());
    if (!a->available()) return 2;
    CudaBackend be;
    CompilationRequest r; r.requestId = CompilationRequestId::fromU64(7); r.logicalOperation = LogicalOperation::fromU64(1);
    r.source = "name=cuda_vec\nshape=1024\nadd scalar=2.0\n"; r.sourceDigest = Sha256::hash(r.source);
    r.datatype = Datatype::F32; r.rank = 1; r.staticShape = {1024}; r.backend = "cuda-nvrtc"; r.targetArchitecture = "sm_120";
    CompilationPlan p; p.backend = "cuda-nvrtc"; p.target.vendor = AcceleratorVendor::Nvidia; p.target.architecture = "sm_120"; p.target.computeCapability = "12.0"; p.target.kernelABI = "nvrtc"; p.target.family = AcceleratorFamily::NvidiaBlackwell;
    KeyToolchainContext tc; tc.compiler = "nvrtc"; tc.backend = "cuda-nvrtc"; tc.frontend="cf-frontend"; tc.optimizer="nvrtc-opt"; tc.linker="nvrtc"; tc.runtime="cuda-driver";
    auto bo = be.compile(r, p, tc);
    std::fprintf(stderr, "compile ok=%d %s\n", (int)bo.ok(), bo.message().c_str());
    if (!bo.ok()) return 3;
    std::fprintf(stderr, "artifact format=%s size=%zu\n", std::string(artifactFormatName(bo->format)).c_str(), bo->executable.size());
    // validate (load-launch-reference)
    ArtifactDescriptor ad; ad.id = ArtifactId::fromU64(7); ad.generation = 1; ad.format = bo->format;
    ad.specialization = bo->specialization; ad.provenance.attemptId = CompilationAttemptId::fromU64(1);
    ad.keyDigest = Sha256::hash("k"); ad.backend = bo->backend; ad.contentDigest = Sha256::hash(bo->executable.data(), bo->executable.size());
    auto vd = be.validate(ad, bo->executable);
    std::fprintf(stderr, "validate ok=%d passed=%d %s\n", (int)vd.ok(), vd.ok() ? (int)vd->passed : 0, vd.ok() ? vd->message.c_str() : vd.message().c_str());
    return vd.ok() ? (vd->passed ? 0 : 4) : 5;
}