// Compilation Fabric - Typed CompilationKey.
//
// A CompilationKey is the canonical, deterministic identity of a compilation
// request: enough typed semantic + toolchain authority to decide whether an
// existing artifact is eligible for reuse. Two requests produce identical keys
// (and therefore identical digests) if and only if every encoded semantic and
// toolchain field agrees.
//
// Encoding: each field is a KeyFieldEntry whose value is encoded canonically
// (big-endian). The digest is SHA-256 over makeCanonicalFields(fields()), which
// sorts on tag first, so field order can never change the identity. The typed
// metadata is retained in full for explainability; recomputing the digest is an
// O(#fields) hash, so no stale cached digest can survive a mutation.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#pragma once

#include "CompilationFabric/Core.hpp"
#include "CompilationFabric/Canonical.hpp"
#include "CompilationFabric/Semantic.hpp"

#include <vector>
#include <string>
#include <optional>

namespace compilationfabric {

class CompilationKey {
public:
    CompilationKey() = default;

    // --- typed field setters -------------------------------------------------
    void logicalOperation(LogicalOperation id) { setId(KeyField::LogicalOperation, id.raw()); }
    void sourceIdentity(SourceIdentity id) { setId(KeyField::SourceIdentity, id.raw()); }
    void sourceDigest(Digest d) { setDigest(KeyField::SourceDigest, d); }
    void sourceContent(std::string_view s) { setString(KeyField::SourceContent, s); }
    void irIdentity(IRIdentity id) { setId(KeyField::IRIdentity, id.raw()); }
    void irDigest(Digest d) { setDigest(KeyField::IRDigest, d); }
    void irContent(std::string_view s) { setString(KeyField::IRContent, s); }
    void irFormat(std::string_view s) { setString(KeyField::IRFormat, s); }
    void frontend(std::string_view s) { setString(KeyField::Frontend, s); }
    void frontendVersion(std::string_view s) { setString(KeyField::FrontendVersion, s); }
    void compiler(std::string_view s) { setString(KeyField::Compiler, s); }
    void compilerVersion(std::string_view s) { setString(KeyField::CompilerVersion, s); }
    void backend(std::string_view s) { setString(KeyField::Backend, s); }
    void backendVersion(std::string_view s) { setString(KeyField::BackendVersion, s); }
    void codeGenerator(std::string_view s) { setString(KeyField::CodeGenerator, s); }
    void optimizer(std::string_view s) { setString(KeyField::Optimizer, s); }
    void optimizerVersion(std::string_view s) { setString(KeyField::OptimizerVersion, s); }
    void linker(std::string_view s) { setString(KeyField::Linker, s); }
    void linkerVersion(std::string_view s) { setString(KeyField::LinkerVersion, s); }
    void runtime(std::string_view s) { setString(KeyField::Runtime, s); }
    void runtimeVersion(std::string_view s) { setString(KeyField::RuntimeVersion, s); }
    void driver(std::string_view s) { setString(KeyField::Driver, s); }
    void driverVersion(std::string_view s) { setString(KeyField::DriverVersion, s); }
    void acceleratorVendor(AcceleratorVendor v) { setU8(KeyField::AcceleratorVendor, static_cast<uint8_t>(v)); }
    void acceleratorFamily(AcceleratorFamily f) { setU8(KeyField::AcceleratorFamily, static_cast<uint8_t>(f)); }
    void targetArchitecture(std::string_view s) { setString(KeyField::TargetArchitecture, s); }
    void computeCapability(std::string_view s) { setString(KeyField::ComputeCapability, s); }
    void isa(std::string_view s) { setString(KeyField::ISA, s); }
    void abi(std::string_view s) { setString(KeyField::ABI, s); }
    void callingConvention(std::string_view s) { setString(KeyField::CallingConvention, s); }
    void kernelABI(std::string_view s) { setString(KeyField::KernelABI, s); }
    void graphABI(std::string_view s) { setString(KeyField::GraphABI, s); }
    void operatorRevision(std::string_view s) { setString(KeyField::OperatorRevision, s); }
    void datatype(Datatype d) { setU8(KeyField::Datatype, static_cast<uint8_t>(d)); }
    void layout(Layout l) { setU8(KeyField::Layout, static_cast<uint8_t>(l)); }
    void rank(uint64_t r) { setU64(KeyField::Rank, r); }
    void staticShape(std::vector<int64_t> shape) { setShape(KeyField::StaticShape, shape); }
    void symbolicShape(std::string_view s) { setString(KeyField::SymbolicShape, s); }
    void alignment(uint64_t a) { setU64(KeyField::Alignment, a); }
    void quantization(QuantizationMode q) { setU8(KeyField::Quantization, static_cast<uint8_t>(q)); }
    void precision(PrecisionMode p) { setU8(KeyField::Precision, static_cast<uint8_t>(p)); }
    void launchSpecialization(std::string_view s) { setString(KeyField::LaunchSpecialization, s); }
    void scalarConstants(std::string_view s) { setString(KeyField::ScalarConstants, s); }
    void featureFlags(std::string_view s) { setString(KeyField::FeatureFlags, s); }
    void optimizationFlags(std::string_view s) { setString(KeyField::OptimizationFlags, s); }
    void codegenFlags(std::string_view s) { setString(KeyField::CodegenFlags, s); }
    void debugRelease(DebugReleaseMode m) { setU8(KeyField::DebugRelease, static_cast<uint8_t>(m)); }
    void determinism(DeterminismMode m) { setU8(KeyField::Determinism, static_cast<uint8_t>(m)); }
    void reproducibility(ReproducibilityMode m) { setU8(KeyField::Reproducibility, static_cast<uint8_t>(m)); }
    void environmentFingerprint(std::string_view s) { setString(KeyField::EnvFingerprint, s); }
    void dependencyIdentities(std::vector<Id128> deps) { setIds(KeyField::DependencyIdentities, deps); }
    void dependencyGenerations(std::vector<uint64_t> gens) { setU64s(KeyField::DependencyGenerations, gens); }
    void modelRevision(std::string_view s) { setString(KeyField::ModelRevision, s); }
    void runtimeCompatibilityGeneration(uint64_t g) { setU64(KeyField::RuntimeCompatibilityGeneration, g); }
    void compilerPolicyGeneration(uint64_t g) { setU64(KeyField::CompilerPolicyGeneration, g); }
    void namespaceName(std::string_view s) { setString(KeyField::Namespace, s); }
    void tenant(std::string_view s) { setString(KeyField::Tenant, s); }

    // --- typed field getters ------------------------------------------------
    bool has(KeyField f) const { return findField(f) != nullptr; }
    std::optional<LogicalOperation> logicalOperation() const { return getId<LogicalOperationTag>(KeyField::LogicalOperation); }
    std::optional<SourceIdentity> sourceIdentity() const { return getId<SourceIdentityTag>(KeyField::SourceIdentity); }
    std::optional<Digest> sourceDigest() const { return getDigest(KeyField::SourceDigest); }
    std::optional<IRIdentity> irIdentity() const { return getId<IRIdentityTag>(KeyField::IRIdentity); }
    std::optional<Digest> irDigest() const { return getDigest(KeyField::IRDigest); }
    std::optional<uint64_t> rank() const { return getU64(KeyField::Rank); }
    std::optional<std::vector<int64_t>> staticShape() const { return getShape(KeyField::StaticShape); }
    std::optional<std::string> stringField(KeyField f) const { return getString(f); }
    std::optional<uint64_t> u64Field(KeyField f) const { return getU64(f); }
    std::optional<uint8_t> u8Field(KeyField f) const { return getU8(f); }
    std::optional<AcceleratorVendor> acceleratorVendor() const { auto v = getU8(KeyField::AcceleratorVendor); return v ? std::optional<AcceleratorVendor>(static_cast<AcceleratorVendor>(*v)) : std::nullopt; }
    std::optional<AcceleratorFamily> acceleratorFamily() const { auto v = getU8(KeyField::AcceleratorFamily); return v ? std::optional<AcceleratorFamily>(static_cast<AcceleratorFamily>(*v)) : std::nullopt; }
    std::optional<Datatype> datatype() const { auto v = getU8(KeyField::Datatype); return v ? std::optional<Datatype>(static_cast<Datatype>(*v)) : std::nullopt; }
    std::optional<Layout> layout() const { auto v = getU8(KeyField::Layout); return v ? std::optional<Layout>(static_cast<Layout>(*v)) : std::nullopt; }
    std::optional<QuantizationMode> quantization() const { auto v = getU8(KeyField::Quantization); return v ? std::optional<QuantizationMode>(static_cast<QuantizationMode>(*v)) : std::nullopt; }
    std::optional<PrecisionMode> precision() const { auto v = getU8(KeyField::Precision); return v ? std::optional<PrecisionMode>(static_cast<PrecisionMode>(*v)) : std::nullopt; }
    std::optional<DeterminismMode> determinism() const { auto v = getU8(KeyField::Determinism); return v ? std::optional<DeterminismMode>(static_cast<DeterminismMode>(*v)) : std::nullopt; }
    std::optional<ReproducibilityMode> reproducibility() const { auto v = getU8(KeyField::Reproducibility); return v ? std::optional<ReproducibilityMode>(static_cast<ReproducibilityMode>(*v)) : std::nullopt; }

    // --- canonical identity --------------------------------------------------
    void clear() { fields_.clear(); }
    size_t fieldCount() const { return fields_.size(); }
    const std::vector<KeyFieldEntry>& fields() const { return fields_; }
    // Recompute the digest deterministically from current fields. Never cached by
    // the key object, so mutations cannot leave a stale digest behind.
    Digest digest() const;
    std::string toHex() const { return digestToHex(digest()); }
    // Stable human/JSON explain of every present field and its decoded value.
    std::string explainText() const;
    struct FieldExplain { std::string name; bool present; std::string value; };
    std::vector<FieldExplain> explain() const;

    // --- equality / ordering --------------------------------------------------
    bool operator==(const CompilationKey& o) const;
    bool operator!=(const CompilationKey& o) const { return !(*this == o); }
    bool operator<(const CompilationKey& o) const;

    // --- decode from canonical bytes ------------------------------------------
    static std::optional<CompilationKey> fromCanonicalBytes(const uint8_t* p, size_t n);
    static std::optional<CompilationKey> fromCanonicalBytes(const std::vector<uint8_t>& v) { return fromCanonicalBytes(v.data(), v.size()); }

private:
    std::vector<KeyFieldEntry> fields_; // unique tags

    const KeyFieldEntry* findField(KeyField f) const;
    KeyFieldEntry* findField(KeyField f);
    void put(std::vector<uint8_t> value, KeyField f);

    void setId(KeyField f, Id128 v);
    void setDigest(KeyField f, Digest v);
    void setString(KeyField f, std::string_view v);
    void setU8(KeyField f, uint8_t v);
    void setU64(KeyField f, uint64_t v);
    void setShape(KeyField f, const std::vector<int64_t>& shape);
    void setIds(KeyField f, const std::vector<Id128>& ids);
    void setU64s(KeyField f, const std::vector<uint64_t>& vs);

    std::optional<uint8_t> getU8(KeyField f) const;
    std::optional<uint64_t> getU64(KeyField f) const;
    std::optional<std::string> getString(KeyField f) const;
    std::optional<Digest> getDigest(KeyField f) const;
    std::optional<std::vector<int64_t>> getShape(KeyField f) const;
    template <typename Tag> std::optional<TypedId<Tag>> getId(KeyField f) const {
        auto raw = getU128(f);
        if (!raw) return std::nullopt;
        return TypedId<Tag>(*raw);
    }
    std::optional<Id128> getU128(KeyField f) const;
};

std::ostream& operator<<(std::ostream& os, const CompilationKey& k);

} // namespace compilationfabric

namespace std {
template <> struct hash<compilationfabric::CompilationKey> {
    size_t operator()(const compilationfabric::CompilationKey& k) const noexcept {
        auto d = k.digest();
        size_t h = 0;
        for (uint8_t b : d) h = (h * 131) + b;
        return h;
    }
};
} // namespace std