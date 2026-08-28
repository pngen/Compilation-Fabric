// Compilation Fabric - Compatibility.cpp implementation.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#include "CompilationFabric/Compatibility.hpp"
#include <algorithm>
#include <set>
#include <sstream>

namespace compilationfabric {

namespace {
std::string renderFieldValue(const std::vector<uint8_t>& v) {
    auto s = std::string_view(reinterpret_cast<const char*>(v.data()), v.size());
    CanonicalReader r(s);
    std::string str;
    if (r.str(str) && !r.hasRemaining()) return str;
    CanonicalReader r2(s);
    uint64_t u;
    if (r2.u64(u) && !r2.hasRemaining()) return std::to_string(u);
    CanonicalReader r3(s);
    Id128 id;
    if (r3.u128(id) && !r3.hasRemaining()) return id.toHex();
    if (v.size() == 32) return "0x" + bytesToHex(v.data(), v.size());
    return "0x" + bytesToHex(v.data(), v.size());
}
} // namespace

std::string_view compatibilityOutcomeName(CompatibilityOutcome o) {
    switch (o) {
        case CompatibilityOutcome::ExactReusable: return "ExactReusable";
        case CompatibilityOutcome::ReusableWithRuntimeValidation: return "ReusableWithRuntimeValidation";
        case CompatibilityOutcome::ReusableAcrossEquivalentTarget: return "ReusableAcrossEquivalentTarget";
        case CompatibilityOutcome::RecompileRequiredSourceChange: return "RecompileRequiredSourceChange";
        case CompatibilityOutcome::RecompileRequiredIRChange: return "RecompileRequiredIRChange";
        case CompatibilityOutcome::RecompileRequiredCompilerChange: return "RecompileRequiredCompilerChange";
        case CompatibilityOutcome::RecompileRequiredBackendChange: return "RecompileRequiredBackendChange";
        case CompatibilityOutcome::RecompileRequiredOptimizerChange: return "RecompileRequiredOptimizerChange";
        case CompatibilityOutcome::RecompileRequiredArchitectureChange: return "RecompileRequiredArchitectureChange";
        case CompatibilityOutcome::RecompileRequiredABIChange: return "RecompileRequiredABIChange";
        case CompatibilityOutcome::RecompileRequiredDatatypeChange: return "RecompileRequiredDatatypeChange";
        case CompatibilityOutcome::RecompileRequiredLayoutChange: return "RecompileRequiredLayoutChange";
        case CompatibilityOutcome::RecompileRequiredShapeChange: return "RecompileRequiredShapeChange";
        case CompatibilityOutcome::RecompileRequiredQuantizationChange: return "RecompileRequiredQuantizationChange";
        case CompatibilityOutcome::RecompileRequiredPrecisionChange: return "RecompileRequiredPrecisionChange";
        case CompatibilityOutcome::RecompileRequiredSpecializationChange: return "RecompileRequiredSpecializationChange";
        case CompatibilityOutcome::RecompileRequiredDependencyGeneration: return "RecompileRequiredDependencyGeneration";
        case CompatibilityOutcome::InvalidArtifact: return "InvalidArtifact";
        case CompatibilityOutcome::StaleArtifact: return "StaleArtifact";
        case CompatibilityOutcome::CorruptArtifact: return "CorruptArtifact";
        case CompatibilityOutcome::PolicyRejected: return "PolicyRejected";
    }
    return "Unknown";
}

std::string_view compatibilityReasonName(CompatibilityReason r) {
    switch (r) {
        case CompatibilityReason::None: return "None";
        case CompatibilityReason::SourceChanged: return "SourceChanged";
        case CompatibilityReason::IRChanged: return "IRChanged";
        case CompatibilityReason::CompilerChanged: return "CompilerChanged";
        case CompatibilityReason::BackendChanged: return "BackendChanged";
        case CompatibilityReason::OptimizerChanged: return "OptimizerChanged";
        case CompatibilityReason::ArchitectureChanged: return "ArchitectureChanged";
        case CompatibilityReason::ABIChanged: return "ABIChanged";
        case CompatibilityReason::DatatypeChanged: return "DatatypeChanged";
        case CompatibilityReason::LayoutChanged: return "LayoutChanged";
        case CompatibilityReason::ShapeChanged: return "ShapeChanged";
        case CompatibilityReason::QuantizationChanged: return "QuantizationChanged";
        case CompatibilityReason::PrecisionChanged: return "PrecisionChanged";
        case CompatibilityReason::SpecializationChanged: return "SpecializationChanged";
        case CompatibilityReason::DependencyGenerationChanged: return "DependencyGenerationChanged";
        case CompatibilityReason::ToolchainGenerationChanged: return "ToolchainGenerationChanged";
        case CompatibilityReason::RuntimeGenerationChanged: return "RuntimeGenerationChanged";
        case CompatibilityReason::EquivalentTarget: return "EquivalentTarget";
        case CompatibilityReason::TargetChanged: return "TargetChanged";
        case CompatibilityReason::Invalid: return "Invalid";
        case CompatibilityReason::Stale: return "Stale";
        case CompatibilityReason::Corrupt: return "Corrupt";
        case CompatibilityReason::Policy: return "Policy";
        case CompatibilityReason::EnvironmentFingerprint: return "EnvironmentFingerprint";
    }
    return "None";
}

Json CompilationCompatibilityDecision::toJson() const {
    Json j = Json::object({});
    j.set("outcome", Json::str(std::string(compatibilityOutcomeName(outcome))));
    j.set("reusable", Json::boolean(reusable));
    std::vector<Json> rs; for (auto r : reasons) rs.push_back(Json::str(std::string(compatibilityReasonName(r))));
    j.set("reasons", Json::array(std::move(rs)));
    j.set("detail", Json::str(detail));
    return j;
}
std::string CompilationCompatibilityDecision::explainText() const {
    std::string s = std::string(compatibilityOutcomeName(outcome)) + " reusable=" + (reusable ? "yes" : "no");
    if (!reasons.empty()) {
        s += " reasons:";
        for (auto r : reasons) s += std::string(" ") + std::string(compatibilityReasonName(r));
    }
    if (!detail.empty()) s += " (" + detail + ")";
    return s;
}

Json CompilationCompatibilityPolicy::toJson() const {
    Json j = Json::object({});
    j.set("allow_equivalent_target", Json::boolean(allowEquivalentTarget));
    j.set("require_runtime_validation", Json::boolean(requireRuntimeValidation));
    j.set("strict_environment", Json::boolean(strictEnvironment));
    return j;
}

CompatibilityReason reasonForKeyField(KeyField f) {
    switch (f) {
        case KeyField::LogicalOperation:
        case KeyField::SourceIdentity:
        case KeyField::SourceDigest:
        case KeyField::SourceContent:
            return CompatibilityReason::SourceChanged;
        case KeyField::IRIdentity:
        case KeyField::IRDigest:
        case KeyField::IRFormat:
        case KeyField::IRContent:
            return CompatibilityReason::IRChanged;
        case KeyField::Frontend:
        case KeyField::FrontendVersion:
        case KeyField::Compiler:
        case KeyField::CompilerVersion:
            return CompatibilityReason::CompilerChanged;
        case KeyField::Backend:
        case KeyField::BackendVersion:
            return CompatibilityReason::BackendChanged;
        case KeyField::CodeGenerator:
        case KeyField::Optimizer:
        case KeyField::OptimizerVersion:
        case KeyField::Linker:
        case KeyField::LinkerVersion:
            return CompatibilityReason::OptimizerChanged;
        case KeyField::Runtime:
        case KeyField::RuntimeVersion:
        case KeyField::RuntimeCompatibilityGeneration:
            return CompatibilityReason::RuntimeGenerationChanged;
        case KeyField::Driver:
        case KeyField::DriverVersion:
        case KeyField::TargetArchitecture:
        case KeyField::ComputeCapability:
        case KeyField::ISA:
        case KeyField::AcceleratorVendor:
        case KeyField::AcceleratorFamily:
            return CompatibilityReason::ArchitectureChanged;
        case KeyField::ABI:
        case KeyField::CallingConvention:
        case KeyField::KernelABI:
        case KeyField::GraphABI:
            return CompatibilityReason::ABIChanged;
        case KeyField::Datatype:
            return CompatibilityReason::DatatypeChanged;
        case KeyField::Layout:
            return CompatibilityReason::LayoutChanged;
        case KeyField::Rank:
        case KeyField::StaticShape:
        case KeyField::SymbolicShape:
            return CompatibilityReason::ShapeChanged;
        case KeyField::Alignment:
        case KeyField::Quantization:
            return CompatibilityReason::QuantizationChanged;
        case KeyField::Precision:
            return CompatibilityReason::PrecisionChanged;
        case KeyField::LaunchSpecialization:
        case KeyField::ScalarConstants:
        case KeyField::FeatureFlags:
        case KeyField::OptimizationFlags:
        case KeyField::CodegenFlags:
        case KeyField::DebugRelease:
        case KeyField::Determinism:
        case KeyField::Reproducibility:
        case KeyField::ModelRevision:
        case KeyField::OperatorRevision:
            return CompatibilityReason::SpecializationChanged;
        case KeyField::DependencyIdentities:
        case KeyField::DependencyGenerations:
            return CompatibilityReason::DependencyGenerationChanged;
        case KeyField::CompilerPolicyGeneration:
            return CompatibilityReason::ToolchainGenerationChanged;
        case KeyField::EnvFingerprint:
            return CompatibilityReason::EnvironmentFingerprint;
        case KeyField::Namespace:
        case KeyField::Tenant:
            return CompatibilityReason::Policy;
    }
    return CompatibilityReason::SpecializationChanged;
}

std::vector<FieldDiff> CompilationCompatibility::fieldDiffs(const CompilationKey& a, const CompilationKey& b) const {
    std::vector<FieldDiff> out;
    std::set<uint8_t> tags;
    for (auto& e : a.fields()) tags.insert(e.tag);
    for (auto& e : b.fields()) tags.insert(e.tag);
    for (uint8_t t : tags) {
        KeyField f = static_cast<KeyField>(t);
        const KeyFieldEntry* ae = nullptr; const KeyFieldEntry* be = nullptr;
        for (auto& e : a.fields()) if (e.tag == t) ae = &e;
        for (auto& e : b.fields()) if (e.tag == t) be = &e;
        bool same = ((ae == nullptr && be == nullptr) || (ae && be && ae->value == be->value));
        if (same) continue;
        FieldDiff d;
        d.field = f;
        if (be) { d.presentInRequest = true; d.requestValue = renderFieldValue(be->value); }
        if (ae) { d.presentInArtifact = true; d.artifactValue = renderFieldValue(ae->value); }
        out.push_back(std::move(d));
    }
    return out;
}

CompilationCompatibilityDecision CompilationCompatibility::decide(const CompilationKey& artifactKey,
                                                                  const CompilationKey& requestKey,
                                                                  const ArtifactDescriptor& artifact,
                                                                  const CompilationCompatibilityPolicy& policy) const {
    CompilationCompatibilityDecision d;
    // Lifecycle authority first: invalidated/corrupt/stale artifacts are never reusable.
    std::string state = artifact.state;
    if (state == "Corrupt") { d.outcome = CompatibilityOutcome::CorruptArtifact; d.reusable = false; d.reasons.push_back(CompatibilityReason::Corrupt); d.detail = "artifact state is Corrupt"; return d; }
    if (state == "Invalidated" || state == "Retired") { d.outcome = CompatibilityOutcome::StaleArtifact; d.reusable = false; d.reasons.push_back(CompatibilityReason::Stale); d.detail = "artifact state is " + state; return d; }
    if (state.empty() || state == "Invalid" || state == "Failed" || state == "Cancelled") {
        d.outcome = CompatibilityOutcome::InvalidArtifact; d.reusable = false; d.reasons.push_back(CompatibilityReason::Invalid);
        d.detail = "artifact is not in a valid state (" + (state.empty() ? std::string("unspecified") : state) + ")"; return d;
    }
    // Full equality => exact reusable, provided the artifact claims a current key digest match.
    if (artifactKey == requestKey) {
        d.outcome = CompatibilityOutcome::ExactReusable; d.reusable = true; d.reasons.push_back(CompatibilityReason::None);
        d.detail = "artifact compilation key exactly matches request"; return d;
    }
    // Collect field diffs and reasons.
    auto diffs = fieldDiffs(artifactKey, requestKey);
    std::vector<CompatibilityReason> reasons;
    bool envChanged = false; bool targetChanged = false; bool hardRecompile = false;
    for (auto& fd : diffs) {
        auto r = reasonForKeyField(fd.field);
        if (r == CompatibilityReason::EnvironmentFingerprint) { envChanged = true; reasons.push_back(r); }
        else if (r == CompatibilityReason::ArchitectureChanged) { targetChanged = true; reasons.push_back(r); }
        else if (r == CompatibilityReason::Policy) { reasons.push_back(r); }
        else { reasons.push_back(r); hardRecompile = true; }
    }
    d.reasons = reasons;

    // Equivalent-target reuse only if requested, and only when the only difference
    // is architecture/target with proven equivalence.
    if (!hardRecompile && !envChanged) {
        if (policy.allowEquivalentTarget && targetChanged && !diffs.empty()) {
            bool onlyTarget = true;
            for (auto& fd : diffs) if (reasonForKeyField(fd.field) != CompatibilityReason::ArchitectureChanged) onlyTarget = false;
            if (onlyTarget) {
                d.outcome = CompatibilityOutcome::ReusableAcrossEquivalentTarget;
                d.reusable = true; d.reasons.clear(); d.reasons.push_back(CompatibilityReason::EquivalentTarget);
                d.detail = "request differs only by target; equivalent target reuse permitted by policy"; return d;
            }
        }
    }
    // Environment-only difference => runtime validation reuse (if policy allows).
    if (!hardRecompile && envChanged && !policy.strictEnvironment) {
        d.outcome = CompatibilityOutcome::ReusableWithRuntimeValidation;
        d.reusable = true; d.reasons.clear(); d.reasons.push_back(CompatibilityReason::EnvironmentFingerprint);
        d.detail = "only environment fingerprint differs; reuse with runtime validation by policy"; return d;
    }
    // Pick the most specific recomprequired outcome from the first hard reason.
    CompatibilityOutcome out = CompatibilityOutcome::RecompileRequiredSpecializationChange;
    for (auto r : reasons) {
        switch (r) {
            case CompatibilityReason::SourceChanged: out = CompatibilityOutcome::RecompileRequiredSourceChange; break;
            case CompatibilityReason::IRChanged: out = CompatibilityOutcome::RecompileRequiredIRChange; break;
            case CompatibilityReason::CompilerChanged: out = CompatibilityOutcome::RecompileRequiredCompilerChange; break;
            case CompatibilityReason::BackendChanged: out = CompatibilityOutcome::RecompileRequiredBackendChange; break;
            case CompatibilityReason::OptimizerChanged: out = CompatibilityOutcome::RecompileRequiredOptimizerChange; break;
            case CompatibilityReason::ArchitectureChanged: out = CompatibilityOutcome::RecompileRequiredArchitectureChange; break;
            case CompatibilityReason::ABIChanged: out = CompatibilityOutcome::RecompileRequiredABIChange; break;
            case CompatibilityReason::DatatypeChanged: out = CompatibilityOutcome::RecompileRequiredDatatypeChange; break;
            case CompatibilityReason::LayoutChanged: out = CompatibilityOutcome::RecompileRequiredLayoutChange; break;
            case CompatibilityReason::ShapeChanged: out = CompatibilityOutcome::RecompileRequiredShapeChange; break;
            case CompatibilityReason::QuantizationChanged: out = CompatibilityOutcome::RecompileRequiredQuantizationChange; break;
            case CompatibilityReason::PrecisionChanged: out = CompatibilityOutcome::RecompileRequiredPrecisionChange; break;
            case CompatibilityReason::SpecializationChanged: out = CompatibilityOutcome::RecompileRequiredSpecializationChange; break;
            case CompatibilityReason::DependencyGenerationChanged:
            case CompatibilityReason::ToolchainGenerationChanged:
            case CompatibilityReason::RuntimeGenerationChanged: out = CompatibilityOutcome::RecompileRequiredDependencyGeneration; break;
            default: break;
        }
        if (out != CompatibilityOutcome::RecompileRequiredSpecializationChange) break;
    }
    d.outcome = out; d.reusable = false;
    if (d.detail.empty()) d.detail = "compilation key differs; recompile required";
    return d;
}

} // namespace compilationfabric