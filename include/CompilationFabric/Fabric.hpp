// Compilation Fabric - Orchestration runtime.
//
// CompilationFabric governs compilation requests, planning, specialization,
// code generation, toolchain/target selection, reproducibility, validation,
// caching, single-flight deduplication, invalidation, supersession, generation
// authority, deployment eligibility and persistence/recovery across
// heterogeneous backends. Reuse is a correctness decision: an artifact is
// eligible only when its full semantic and toolchain authority still agrees with
// the request (see CompilationCompatibility). CPU-only orchestration remains
// valid where CUDA is unavailable.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#pragma once
#include "CompilationFabric/Core.hpp"
#include "CompilationFabric/Key.hpp"
#include "CompilationFabric/Compatibility.hpp"
#include "CompilationFabric/Lifecycle.hpp"
#include "CompilationFabric/Request.hpp"
#include "CompilationFabric/Descriptor.hpp"
#include "CompilationFabric/Persistence.hpp"
#include "CompilationFabric/Observability.hpp"
#include "CompilationFabric/Backend.hpp"
#include "CompilationFabric/Json.hpp"
#include <memory>
#include <vector>

namespace compilationfabric {

struct CompilationFabricConfig {
    std::string artifactRoot = "cf-artifacts";
    bool persistenceEnabled = true;
    bool persistOnCompile = true;
    bool allowCuda = true;
    bool autotuneEnabled = true;
    bool strictLifecycle = true;
    CompilationCompatibilityPolicy compatibilityPolicy;
    ReproducibilityMode reproducibility = ReproducibilityMode::BestEffort;
    std::string namespaceName = "default";
    std::string tenant = "default";
    uint64_t maxEventRetention = 8192;
    std::string environmentFingerprint;
};

// Atomic snapshot of runtime state for observability.
struct CompilationFabricSnapshot {
    Json counters;
    Json events;
    Json artifactIndex;
    Json inflight;
    Json generations;
    std::string epochText;
    std::string toJson() const { return Json::object({
        {"counters", counters}, {"events", events}, {"artifact_index", artifactIndex},
        {"inflight", inflight}, {"generations", generations}, {"epoch", Json::str(epochText)} }).dump(); }
};

class CompilationFabric {
public:
    explicit CompilationFabric(CompilationFabricConfig config = {});
    ~CompilationFabric();

    // Deterministic planning; produces explain for toolchain/target/specialization selection.
    Result<CompilationPlan> plan(const CompilationRequest& request) const;

    // Submit + plan + compile (or reuse) + validate + persist. Never returns an
    // artifact that has not been validated.
    Result<CompilationResult> compile(const CompilationRequest& request);

    // Cache lookup + compatibility decision without compiling.
    Result<std::pair<CompilationCompatibilityDecision, std::optional<ArtifactDescriptor>>> lookup(const CompilationKey& key) const;

    // Invalidation by the supported authorities.
    Result<void> invalidateByKey(const CompilationKey& key);
    Result<void> invalidateById(const ArtifactId& id);
    Result<void> invalidateByLogicalOperation(const LogicalOperation& op);
    Result<void> invalidateByCompilerGeneration(uint64_t gen);
    Result<void> invalidateByDependencyGeneration(uint64_t gen);
    Result<void> supersede(const ArtifactId& id, ArtifactGeneration newGen);

    // Deployment/load + leases.
    Result<std::shared_ptr<LoadedModule>> deploy(const ArtifactId& id);
    Result<CompilationLease> acquire(const ArtifactId& id);
    Result<void> release(const CompilationLease& lease);

    // Persistence recovery.
    Result<void> recover();

    // Observability / explain / discovery.
    CompilationFabricSnapshot snapshot() const;
    Json stats() const;
    Json explain(const CompilationKey& key) const;
    Result<ToolchainDescriptor> toolchains() const;
    Result<std::vector<TargetDescriptor>> targets() const;

    // Generation authority.
    CoordinatorEpoch epoch() const;
    CacheGeneration cacheGeneration() const;
    ToolchainGeneration toolchainGeneration() const;

    // Register an external backend (used by tests).
    void registerBackend(std::shared_ptr<ICompilerBackend> backend);

    // The compatibility policy in force.
    const CompilationCompatibilityPolicy& compatibilityPolicy() const { return config_.compatibilityPolicy; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    CompilationFabricConfig config_;
};

} // namespace compilationfabric
