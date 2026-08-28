// Compilation Fabric - Backend interfaces.
//
// Vendor-neutral boundaries: a Frontend lowers a source/IR representation, a
// CompilerBackend turns a normalized request+plan into an executable artifact,
// an ArtifactValidator proves an artifact is loadable/correct, and an
// ArtifactLoader governs deployment/load. Backends advertise typed capability
// descriptors; the orchestrator never assumes a backend can do work it did not
// advertise.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#pragma once
#include "CompilationFabric/Core.hpp"
#include "CompilationFabric/Descriptor.hpp"
#include "CompilationFabric/Request.hpp"
#include <memory>

namespace compilationfabric {

// Typed capabilities a backend advertises.
struct BackendCapabilities {
    std::string id;
    std::string name;
    std::string targetArchitecture;
    std::string compiler;
    std::string compilerVersion;
    AcceleratorVendor vendor = AcceleratorVendor::Unknown;
    AcceleratorFamily family = AcceleratorFamily::Unknown;
    std::vector<std::string> datatypes;
    std::vector<std::string> layouts;
    std::vector<std::string> featureFlags;
    bool synthetic = false;      // deterministic synthetic CPU
    bool supportsNVRTC = false;
    bool supportsOffline = false;
    bool supportsExecution = true;
    Json toJson() const;
};

// Result of a backend compile: the executable representation plus typed
// descriptors and per-stage evidence.
struct BackendOutput {
    std::vector<uint8_t> executable;
    ArtifactFormat format = ArtifactFormat::Unknown;
    BackendDescriptor backend;
    CompilerDescriptor compiler;
    SpecializationDescriptor specialization;
    std::vector<StageResult> stages;
    ValidationDescriptor validation;
    bool deterministic = false;
    std::string referenceDigest;
};

// A resolved, deployable module handle. Opaque to the orchestrator; the loader
// knows how to free it. Residency is not implicitly resumed after restart.
class LoadedModule {
public:
    virtual ~LoadedModule() = default;
    virtual std::string kind() const = 0;
    // Execute the module and return a canonical result digest (for smoke checks).
    virtual Result<Digest> executeSmoke() = 0;
};

class ICompilerBackend {
public:
    virtual ~ICompilerBackend() = default;
    virtual const BackendCapabilities& capabilities() const = 0;
    virtual std::string id() const = 0;
    // True if the backend can satisfy a plan (target/arch/toolchain capability).
    virtual Result<void> checkCompatible(const CompilationPlan& plan) const = 0;
    // Frontend: lower source into IR. Backends may produce IR inline.
    virtual Result<IRDescriptor> lower(const CompilationRequest& request) const { (void)request; return Err<IRDescriptor>(ErrorCode::UnsupportedIR, "backend does not expose a separate lower stage"); }
    // Compile a normalized request+plan into an executable artifact.
    virtual Result<BackendOutput> compile(const CompilationRequest& request,
                                          const CompilationPlan& plan,
                                          const KeyToolchainContext& tc) = 0;
    // Validate an artifact produced by this backend.
    virtual Result<ValidationDescriptor> validate(const ArtifactDescriptor& descriptor,
                                                  const std::vector<uint8_t>& executable) = 0;
    // Load/deploy an artifact into an executable module handle.
    virtual Result<std::shared_ptr<LoadedModule>> load(const ArtifactDescriptor& descriptor,
                                                       const std::vector<uint8_t>& executable) = 0;
};

// Toolchain probe: real, exact discovery with failure reasons, never inferred.
class IToolchainProbe {
public:
    virtual ~IToolchainProbe() = default;
    virtual ToolchainDescriptor probe() const = 0;
    // Optional per-tool failure reasons for explainability.
    virtual std::string failureReason(std::string_view tool) const { (void)tool; return ""; }
};

// Target probe: discover real accelerator targets and supported compile targets.
class ITargetProbe {
public:
    virtual ~ITargetProbe() = default;
    virtual Result<std::vector<TargetDescriptor>> listTargets() const = 0;
    virtual Result<TargetDescriptor> defaultTarget() const = 0;
};

} // namespace compilationfabric
