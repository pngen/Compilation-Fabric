// Compilation Fabric - Compatibility decision engine.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#pragma once
#include "CompilationFabric/Core.hpp"
#include "CompilationFabric/Key.hpp"
#include "CompilationFabric/Descriptor.hpp"
#include "CompilationFabric/Canonical.hpp"
#include <vector>
#include <string>

namespace compilationfabric {

enum class CompatibilityOutcome {
    ExactReusable,
    ReusableWithRuntimeValidation,
    ReusableAcrossEquivalentTarget,
    RecompileRequiredSourceChange,
    RecompileRequiredIRChange,
    RecompileRequiredCompilerChange,
    RecompileRequiredBackendChange,
    RecompileRequiredOptimizerChange,
    RecompileRequiredArchitectureChange,
    RecompileRequiredABIChange,
    RecompileRequiredDatatypeChange,
    RecompileRequiredLayoutChange,
    RecompileRequiredShapeChange,
    RecompileRequiredQuantizationChange,
    RecompileRequiredPrecisionChange,
    RecompileRequiredSpecializationChange,
    RecompileRequiredDependencyGeneration,
    InvalidArtifact,
    StaleArtifact,
    CorruptArtifact,
    PolicyRejected,
};
std::string_view compatibilityOutcomeName(CompatibilityOutcome o);

enum class CompatibilityReason {
    None, SourceChanged, IRChanged, CompilerChanged, BackendChanged, OptimizerChanged,
    ArchitectureChanged, ABIChanged, DatatypeChanged, LayoutChanged, ShapeChanged,
    QuantizationChanged, PrecisionChanged, SpecializationChanged, DependencyGenerationChanged,
    ToolchainGenerationChanged, RuntimeGenerationChanged, EquivalentTarget, TargetChanged,
    Invalid, Stale, Corrupt, Policy, EnvironmentFingerprint,
};
std::string_view compatibilityReasonName(CompatibilityReason r);

struct FieldDiff;  // defined below; used as vector element

struct CompilationCompatibilityDecision {
    CompatibilityOutcome outcome = CompatibilityOutcome::RecompileRequiredSpecializationChange;
    bool reusable = false;
    std::vector<CompatibilityReason> reasons;   // structured, in priority order
    std::string detail;                          // human-readable explain

    bool exact() const { return outcome == CompatibilityOutcome::ExactReusable; }
    Json toJson() const;
    std::string explainText() const;
};

// Which field tag maps to which compatibility reason.
CompatibilityReason reasonForKeyField(KeyField f);

// Policy for reuse. Conservative by default: reuse is a correctness decision and
// is never downgraded silently. Equivalent-target reuse must be explicitly
// enabled and even then requires runtime validation unless strictly proven.
struct CompilationCompatibilityPolicy {
    bool allowEquivalentTarget = false;          // allow ReusableAcrossEquivalentTarget
    bool requireRuntimeValidation = true;        // validate before reusing
    bool strictEnvironment = true;               // environment fingerprint differences => recompile
    int maxIdenticalFieldsAllowed = 0;           // reserved (not used to override correctness)
    Json toJson() const;
};

class CompilationCompatibility {
public:
    CompilationCompatibility() = default;
    explicit CompilationCompatibility(CompilationCompatibilityPolicy policy) : policy_(std::move(policy)) {}

    // Compare a persisted artifact's authority against a requested key. The result
    // accounts for the artifact lifecycle (valid/stale/corrupt/invalid).
    CompilationCompatibilityDecision decide(const CompilationKey& artifactKey,
                                            const CompilationKey& requestKey,
                                            const ArtifactDescriptor& artifact,
                                            const CompilationCompatibilityPolicy& policy) const;

    // Single field-difference set used for explain.
    std::vector<FieldDiff> fieldDiffs(const CompilationKey& a, const CompilationKey& b) const;
    const CompilationCompatibilityPolicy& policy() const { return policy_; }
private:
    CompilationCompatibilityPolicy policy_;
};

struct FieldDiff {
    KeyField field;
    bool presentInRequest = false;
    std::string requestValue;
    bool presentInArtifact = false;
    std::string artifactValue;
};

} // namespace compilationfabric