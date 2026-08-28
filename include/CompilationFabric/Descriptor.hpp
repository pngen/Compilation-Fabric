// Compilation Fabric - Strong typed descriptor model.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#pragma once
#include "CompilationFabric/Core.hpp"
#include "CompilationFabric/Key.hpp"
#include "CompilationFabric/Semantic.hpp"
#include "CompilationFabric/Json.hpp"
#include <optional>
#include <vector>

namespace compilationfabric {

enum class ArtifactFormat : uint8_t {
    Unknown, Executable, Object, StaticLibrary, SharedLibrary, PTX, CUBIN, Fatbinary, Bytecode,
    IR, Source, Graph, KernelImage, Operational
};
std::string_view artifactFormatName(ArtifactFormat f);
std::optional<ArtifactFormat> artifactFormatFromName(std::string_view s);

// Strong typed identity for a target.
struct TargetDescriptor {
    AcceleratorVendor vendor = AcceleratorVendor::Unknown;
    AcceleratorFamily family = AcceleratorFamily::Unknown;
    std::string architecture;       // e.g. "sm_120"
    std::string computeCapability;  // e.g. "12.0"
    std::string isa;                // e.g. "sm_120"
    std::string abi;                // e.g. "cuda", "sysv64"
    std::string callingConvention;  // e.g. "device", "platform"
    std::string kernelABI;          // e.g. "nvrtc"
    std::string graphABI;           // e.g. "cuda-graph" or ""
    std::string deviceName;         // e.g. "NVIDIA GeForce RTX 5090"
    uint32_t deviceCount = 0;
    uint64_t deviceMemoryBytes = 0;
    std::string driverVersion;
    std::string runtimeVersion;
    Json toJson() const;
    static std::optional<TargetDescriptor> fromJson(const Json& j);
    bool operator==(const TargetDescriptor&) const = default;
};

struct CompilerDescriptor {
    std::string id;         // e.g. "msvc", "nvcc", "nvrtc"
    std::string version;    // e.g. "19.44.35222", "13.1.80"
    std::string vendor;     // e.g. "Microsoft", "NVIDIA"
    std::string backendType;// e.g. "cpu", "nvrtc"
    Json toJson() const;
    static std::optional<CompilerDescriptor> fromJson(const Json& j);
    bool operator==(const CompilerDescriptor&) const = default;
};

struct ToolchainDescriptor {
    bool msvcPresent = false;
    CompilerDescriptor msvc;
    bool nvccPresent = false;
    CompilerDescriptor nvcc;
    bool nvrtcPresent = false;
    std::string nvrtcVersion;
    bool cudaDriverPresent = false;
    std::string cudaDriverVersion;
    bool cmakePresent = false;
    std::string cmakeVersion;
    bool ninjaPresent = false;
    std::string ninjaVersion;
    bool windowsSdkPresent = false;
    std::string windowsSdkVersion;
    bool cudaToolkitPresent = false;
    std::string cudaToolkitPath;
    Json toJson() const;
    static std::optional<ToolchainDescriptor> fromJson(const Json& j);
};

struct BackendDescriptor {
    std::string id;                 // "cpu", "cuda-nvrtc", "cuda-nvcc"
    std::string name;
    std::string compilerId;
    std::string compilerVersion;
    std::string codeGenerator;
    std::string optimizer;
    std::string linker;
    std::string runtime;
    std::string targetArchitecture;
    bool supportsNVRTC = false;
    bool supportsOffline = false;
    bool isSyntheticCPU = false;
    std::vector<std::string> supportedDatatypes;
    std::vector<std::string> supportedLayouts;
    std::vector<std::string> featureFlags;
    std::string capabilityDescriptor; // machine-readable
    Json toJson() const;
    static std::optional<BackendDescriptor> fromJson(const Json& j);
    bool operator==(const BackendDescriptor&) const = default;
};

struct SourceDescriptor {
    SourceIdentity identity;
    Digest digest{};
    std::string language;   // "cuda", "cfir", "text"
    std::string format;     // source format identifier
    uint64_t byteSize = 0;
    std::string frontend;
    Json toJson() const;
    static std::optional<SourceDescriptor> fromJson(const Json& j);
};

struct IRDescriptor {
    IRIdentity identity;
    Digest digest{};
    std::string format;     // e.g. "cf-bytecode", "ptx", "llvm-ir"
    uint64_t byteSize = 0;
    uint64_t rank = 0;
    Datatype datatype = Datatype::None;
    Layout layout = Layout::None;
    std::vector<int64_t> staticShape;
    std::string symbolicShape;
    Json toJson() const;
    static std::optional<IRDescriptor> fromJson(const Json& j);
};

struct SpecializationDescriptor {
    std::vector<int64_t> shape;
    Datatype datatype = Datatype::None;
    Layout layout = Layout::None;
    QuantizationMode quantization = QuantizationMode::None;
    PrecisionMode precision = PrecisionMode::None;
    std::string scalarConstants;
    std::string launchSpecialization;   // block/grid/params
    std::string featureFlags;
    std::string architecture;
    std::string modelRev;
    Json toJson() const;
    static std::optional<SpecializationDescriptor> fromJson(const Json& j);
};

// Per-stage outcome in the multi-stage pipeline.
enum class StageKind : uint8_t { Normalize, Lower, Optimize, Codegen, Assemble, Link, Validate, Persist, Deploy, Skip, NotApplicable };
std::string_view stageKindName(StageKind k);
std::optional<StageKind> stageKindFromName(std::string_view s);

struct StageOutcome {
    StageKind kind = StageKind::Skip;
    bool ran = false;
    bool succeeded = true;
    int64_t durationMs = 0;
    std::string message;
    Json toJson() const;
};

struct ValidationDescriptor {
    bool executed = false;
    bool passed = false;
    std::string method;          // e.g. "reference", "load-launch", "digest"
    int64_t durationMs = 0;
    std::string message;
    bool contentDigestOk = false;
    bool formatOk = false;
    bool architectureOk = false;
    bool abiOk = false;
    bool dependencyOk = false;
    bool metadataConsistent = false;
    bool loadable = false;
    bool executionSmoke = false;
    bool referenceComparison = false;
    Json toJson() const;
    static std::optional<ValidationDescriptor> fromJson(const Json& j);
};

struct DeploymentDescriptor {
    bool deployable = false;
    std::string method;             // "load", "cpu-run"
    bool runtimeCompatible = false;
    bool architectureValidated = false;
    bool generationValidated = false;
    int64_t lastLoadDurationMs = 0;
    int64_t lastLoadAt = 0;
    bool moduleResident = false;    // cleared on restart
    uint64_t activeLeases = 0;
    Json toJson() const;
    static std::optional<DeploymentDescriptor> fromJson(const Json& j);
};

struct ProvenanceDescriptor {
    CompilationRequestId requestId;
    CompilationPlanId planId;
    CompilationAttemptId attemptId;
    Digest sourceDigest{};
    Digest irDigest{};
    std::string toolchainFingerprint;
    std::string environmentFingerprint;
    ReproducibilityMode reproducibility = ReproducibilityMode::Unspecified;
    bool deterministic = false;
    int64_t compileDurationMs = 0;
    int64_t optimizeDurationMs = 0;
    int64_t analyzeDurationMs = 0;
    int64_t validateDurationMs = 0;
    int64_t linkDurationMs = 0;
    int64_t deployDurationMs = 0;
    int64_t createdAt = 0;
    Json toJson() const;
    static std::optional<ProvenanceDescriptor> fromJson(const Json& j);
};

struct ArtifactDescriptor {
    ArtifactId id;
    ArtifactGeneration generation = 0;
    ArtifactFormat format = ArtifactFormat::Unknown;
    Digest contentDigest{};
    uint64_t byteSize = 0;
    Digest keyDigest{};        // digest of the compilation key that produced it
    std::string state;         // lifecycle state string (see Lifecycle.hpp)
    TargetDescriptor target;
    CompilerDescriptor compiler;
    BackendDescriptor backend;
    SpecializationDescriptor specialization;
    ValidationDescriptor validation;
    DeploymentDescriptor deployment;
    ProvenanceDescriptor provenance;
    std::string namespaceName;
    std::string tenant;
    int64_t createdAt = 0;
    int64_t lastAccess = 0;
    uint64_t reuseCount = 0;
    Json toJson() const;
    static std::optional<ArtifactDescriptor> fromJson(const Json& j);
};

} // namespace compilationfabric