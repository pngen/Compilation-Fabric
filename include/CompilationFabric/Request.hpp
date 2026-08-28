// Compilation Fabric - Compilation request, plan, result, attempt, lease, reservation.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#pragma once
#include "CompilationFabric/Core.hpp"
#include "CompilationFabric/Key.hpp"
#include "CompilationFabric/Descriptor.hpp"
#include "CompilationFabric/Lifecycle.hpp"
#include "CompilationFabric/Json.hpp"
#include <optional>
#include <vector>
#include <string>

namespace compilationfabric {

struct CompilationRequest {
    CompilationRequestId requestId;
    LogicalOperation logicalOperation;       // logical operation/workload identity

    // source / IR
    std::string source;                      // source text or IR representation
    Digest sourceDigest{};                   // digest of source
    std::string sourceLanguage;              // e.g. "cf-src", "cuda", "bytecode"
    Digest irDigest{};                       // digest of lowered IR (optional)
    std::string irFormat;

    // semantic specialization
    Datatype datatype = Datatype::None;
    Layout layout = Layout::None;
    uint64_t rank = 0;
    std::vector<int64_t> staticShape;
    std::string symbolicShape;
    uint64_t alignment = 0;
    QuantizationMode quantization = QuantizationMode::None;
    PrecisionMode precision = PrecisionMode::None;
    std::string scalarConstants;
    std::string launchSpecialization;

    // target requirements
    std::string targetArchitecture;          // e.g. "sm_120", "host-x86_64"
    std::string computeCapability;
    std::string isa;
    std::string abi;
    std::string kernelABI;

    // backend/toolchain preferences + flags
    std::string backend       ;              // "cpu", "cuda-nvrtc", "cuda-nvcc"
    std::string compilerId    ;
    std::string frontend      ;
    std::string featureFlags  ;
    std::string optimizationFlags;
    std::string codegenFlags  ;
    DebugReleaseMode debugRelease = DebugReleaseMode::Release;
    DeterminismMode determinism = DeterminismMode::Unspecified;
    ReproducibilityMode reproducibility = ReproducibilityMode::Unspecified;

    // dependency authority
    std::vector<Id128> dependencyIdentities;
    std::vector<uint64_t> dependencyGenerations;
    std::string modelRevision;

    // policy/namespace
    std::string namespaceName;
    std::string tenant;

    // autotuning / variants
    bool autotune = false;
    int autotuneCandidates = 1;
    uint64_t autotuneSeed = 0;
    int blockSize = 0;                       // 0 = default
    int unrollFactor = 0;
    bool offlinePreferred = false;

    Json toJson() const;
    static std::optional<CompilationRequest> fromJson(const Json& j);
};

struct CompilationPlan {
    CompilationPlanId planId;
    CompilationRequest request;              // normalized request
    std::string reason;                      // explain: why this path was selected

    // selected toolchain path
    std::string frontend;
    std::string compiler;
    std::string backend;
    std::string optimizer;
    std::string linker;
    std::string runtime;

    TargetDescriptor target;
    std::string specializationStrategy;     // e.g. "static-shape", "dynamic-bounded"
    ArtifactFormat expectedFormat = ArtifactFormat::Unknown;

    // dependencies
    std::vector<Id128> dependencyIdentities;
    std::vector<uint64_t> dependencyGenerations;

    // planned stages
    std::vector<StageKind> stages;

    // required capabilities
    std::vector<std::string> requiredToolchainCapabilities;

    // expected validation / deployment / reproducibility
    std::string expectedValidationMethod;
    std::string expectedDeploymentMethod;
    ReproducibilityMode reproducibility = ReproducibilityMode::Unspecified;

    // cache / reuse policy
    std::string cachePolicy;                 // "exact", "equivalent-target", "runtime-validate"

    int64_t createdAt = 0;
    Json toJson() const;
    static std::optional<CompilationPlan> fromJson(const Json& j);
};

struct StageResult {
    StageKind kind = StageKind::Skip;
    bool ran = false;
    bool succeeded = true;
    int64_t durationMs = 0;
    std::string message;
    std::vector<uint8_t> output;             // stage output (e.g. lower/optimize/codegen/link)
};

struct CompilationResult {
    CompilationRequestId requestId;
    CompilationPlanId planId;
    CompilationAttemptId attemptId;
    ArtifactId artifactId;
    ArtifactGeneration generation = 0;
    CompilationKey key;
    Digest keyDigest{};
    ArtifactDescriptor artifact;
    std::vector<uint8_t> artifactBytes;      // executable representation
    std::vector<StageResult> stages;
    bool reused = false;
    bool validated = false;
    bool deployable = false;
    bool referenceMatched = false;
    std::string compatibilityDecision;
    ErrorCode error = ErrorCode::Ok;
    std::string errorMessage;
    int64_t compileMs = 0;
    int64_t totalMs = 0;
    std::string backendUsed;
    Json toJson() const;
};

// A lease over a loaded/deployable artifact. Leases are never negative; the
// orchestrator guards acquire/release.
class CompilationLease {
public:
    CompilationLease() = default;
    explicit CompilationLease(ArtifactId id, ArtifactGeneration gen) : artifactId_(id), generation_(gen) {}
    const std::optional<ArtifactId>& id() const { return artifactId_; }
    ArtifactGeneration generation() const { return generation_; }
    bool valid() const { return artifactId_.has_value(); }
private:
    std::optional<ArtifactId> artifactId_;
    ArtifactGeneration generation_ = 0;
};

struct CompilationReservation {
    CompilationAttemptId attemptId;
    CompilationRequestId requestId;
    CompilationKey key;
    bool active = false;
    bool owner = false;
    int waiters = 0;
};


// Resolved toolchain/semantic authority used to build a CompilationKey.
struct KeyToolchainContext {
    std::string frontend, frontendVersion;
    std::string compiler, compilerVersion;
    std::string backend, backendVersion;
    std::string codeGenerator, codeGeneratorVersion;
    std::string optimizer, optimizerVersion;
    std::string linker, linkerVersion;
    std::string runtime, runtimeVersion;
    std::string driver, driverVersion;
    std::string environmentFingerprint;
    uint64_t runtimeCompatGeneration = 0;
    uint64_t compilerPolicyGeneration = 0;
    ToolchainGeneration toolchainGeneration = 0;
};

// Build the canonical typed CompilationKey for a request under a resolved
// toolchain/specialization authority. Deterministic: identical inputs produce
// identical keys; any relevant semantic or toolchain change alters the key.
CompilationKey buildCompilationKey(const CompilationRequest& r,
                                   const CompilationPlan& p,
                                   const KeyToolchainContext& t);

} // namespace compilationfabric
