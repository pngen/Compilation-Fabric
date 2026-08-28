// Compilation Fabric - Canonical.cpp implementation.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#include "CompilationFabric/Canonical.hpp"

#include <algorithm>
#include <cstring>

namespace compilationfabric {

namespace {
void putU16BE(std::vector<uint8_t>& b, uint16_t v) {
    b.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    b.push_back(static_cast<uint8_t>(v & 0xFF));
}
void putU32BE(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    b.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    b.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    b.push_back(static_cast<uint8_t>(v & 0xFF));
}
void putU64BE(std::vector<uint8_t>& b, uint64_t v) {
    for (int i = 7; i >= 0; --i) b.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
}
} // namespace

void CanonicalWriter::u8(uint8_t v) { buf_.push_back(v); }
void CanonicalWriter::u16(uint16_t v) { putU16BE(buf_, v); }
void CanonicalWriter::u32(uint32_t v) { putU32BE(buf_, v); }
void CanonicalWriter::u64(uint64_t v) { putU64BE(buf_, v); }
void CanonicalWriter::u128(const Id128& v) { putU64BE(buf_, v.hi); putU64BE(buf_, v.lo); }
void CanonicalWriter::f32(float v) {
    uint32_t bits; std::memcpy(&bits, &v, 4); putU32BE(buf_, bits);
}
void CanonicalWriter::f64(double v) {
    uint64_t bits; std::memcpy(&bits, &v, 8); putU64BE(buf_, bits);
}
void CanonicalWriter::string(std::string_view s) { putU64BE(buf_, s.size()); bytes(s); }
void CanonicalWriter::bytes(const uint8_t* p, size_t n) { buf_.insert(buf_.end(), p, p + n); }

bool CanonicalReader::u8(uint8_t& out) {
    if (pos_ + 1 > data_.size()) return false;
    out = static_cast<uint8_t>(data_[pos_]); pos_ += 1; return true;
}
bool CanonicalReader::u16(uint16_t& out) {
    if (pos_ + 2 > data_.size()) return false;
    out = (static_cast<uint16_t>(static_cast<uint8_t>(data_[pos_])) << 8) |
          static_cast<uint16_t>(static_cast<uint8_t>(data_[pos_ + 1]));
    pos_ += 2; return true;
}
bool CanonicalReader::u32(uint32_t& out) {
    if (pos_ + 4 > data_.size()) return false;
    out = (static_cast<uint32_t>(static_cast<uint8_t>(data_[pos_])) << 24) |
          (static_cast<uint32_t>(static_cast<uint8_t>(data_[pos_ + 1])) << 16) |
          (static_cast<uint32_t>(static_cast<uint8_t>(data_[pos_ + 2])) << 8) |
          static_cast<uint32_t>(static_cast<uint8_t>(data_[pos_ + 3]));
    pos_ += 4; return true;
}
bool CanonicalReader::u64(uint64_t& out) {
    if (pos_ + 8 > data_.size()) return false;
    out = 0;
    for (int i = 0; i < 8; ++i) out = (out << 8) | static_cast<uint8_t>(data_[pos_ + i]);
    pos_ += 8; return true;
}
bool CanonicalReader::u128(Id128& out) {
    uint64_t h, l;
    if (!u64(h)) return false;
    if (!u64(l)) return false;
    out = Id128(h, l); return true;
}
bool CanonicalReader::f32(float& out) {
    uint32_t bits;
    if (!u32(bits)) return false;
    std::memcpy(&out, &bits, 4); return true;
}
bool CanonicalReader::f64(double& out) {
    uint64_t bits;
    if (!u64(bits)) return false;
    std::memcpy(&out, &bits, 8); return true;
}
bool CanonicalReader::str(std::string& out) {
    uint64_t len;
    if (!u64(len) || pos_ + len > data_.size()) return false;
    out.assign(data_.data() + pos_, len); pos_ += len; return true;
}
bool CanonicalReader::bytesBlob(std::string_view& out) {
    uint64_t len;
    if (!u64(len) || pos_ + len > data_.size()) return false;
    out = data_.substr(pos_, len); pos_ += len; return true;
}

std::string_view keyFieldName(KeyField f) {
    switch (f) {
        case KeyField::LogicalOperation: return "logical_operation";
        case KeyField::SourceIdentity: return "source_identity";
        case KeyField::SourceDigest: return "source_digest";
        case KeyField::IRIdentity: return "ir_identity";
        case KeyField::IRDigest: return "ir_digest";
        case KeyField::IRFormat: return "ir_format";
        case KeyField::Frontend: return "frontend";
        case KeyField::FrontendVersion: return "frontend_version";
        case KeyField::Compiler: return "compiler";
        case KeyField::CompilerVersion: return "compiler_version";
        case KeyField::Backend: return "backend";
        case KeyField::BackendVersion: return "backend_version";
        case KeyField::CodeGenerator: return "code_generator";
        case KeyField::Optimizer: return "optimizer";
        case KeyField::OptimizerVersion: return "optimizer_version";
        case KeyField::Linker: return "linker";
        case KeyField::LinkerVersion: return "linker_version";
        case KeyField::Runtime: return "runtime";
        case KeyField::RuntimeVersion: return "runtime_version";
        case KeyField::Driver: return "driver";
        case KeyField::DriverVersion: return "driver_version";
        case KeyField::AcceleratorVendor: return "accelerator_vendor";
        case KeyField::AcceleratorFamily: return "accelerator_family";
        case KeyField::TargetArchitecture: return "target_architecture";
        case KeyField::ComputeCapability: return "compute_capability";
        case KeyField::ISA: return "isa";
        case KeyField::ABI: return "abi";
        case KeyField::CallingConvention: return "calling_convention";
        case KeyField::KernelABI: return "kernel_abi";
        case KeyField::GraphABI: return "graph_abi";
        case KeyField::OperatorRevision: return "operator_revision";
        case KeyField::Datatype: return "datatype";
        case KeyField::Layout: return "layout";
        case KeyField::Rank: return "rank";
        case KeyField::StaticShape: return "static_shape";
        case KeyField::SymbolicShape: return "symbolic_shape";
        case KeyField::Alignment: return "alignment";
        case KeyField::Quantization: return "quantization";
        case KeyField::Precision: return "precision";
        case KeyField::LaunchSpecialization: return "launch_specialization";
        case KeyField::ScalarConstants: return "scalar_constants";
        case KeyField::FeatureFlags: return "feature_flags";
        case KeyField::OptimizationFlags: return "optimization_flags";
        case KeyField::CodegenFlags: return "codegen_flags";
        case KeyField::DebugRelease: return "debug_release";
        case KeyField::Determinism: return "determinism";
        case KeyField::Reproducibility: return "reproducibility";
        case KeyField::EnvFingerprint: return "environment_fingerprint";
        case KeyField::DependencyIdentities: return "dependency_identities";
        case KeyField::DependencyGenerations: return "dependency_generations";
        case KeyField::ModelRevision: return "model_revision";
        case KeyField::RuntimeCompatibilityGeneration: return "runtime_compatibility_generation";
        case KeyField::CompilerPolicyGeneration: return "compiler_policy_generation";
        case KeyField::Namespace: return "namespace";
        case KeyField::Tenant: return "tenant";
        case KeyField::SourceContent: return "source_content";
        case KeyField::IRContent: return "ir_content";
    }
    return "unknown";
}

namespace {
// Uppermost valid tag value; unknown/malformed tags are rejected.
constexpr uint8_t kMaxKeyField = static_cast<uint8_t>(KeyField::IRContent);
} // namespace

std::vector<uint8_t> makeCanonicalFields(const std::vector<KeyFieldEntry>& fields) {
    // Copy, then sort on tag so the byte encoding is order-independent.
    std::vector<KeyFieldEntry> sorted = fields;
    std::sort(sorted.begin(), sorted.end(),
              [](const KeyFieldEntry& a, const KeyFieldEntry& b) { return a.tag < b.tag; });

    CanonicalWriter w;
    w.u32(static_cast<uint32_t>(sorted.size()));
    for (const auto& f : sorted) {
        if (f.tag == 0 || f.tag > kMaxKeyField) continue; // never encode invalid tags
        w.u8(f.tag);
        w.u64(static_cast<uint64_t>(f.value.size()));
        w.bytes(f.value.data(), f.value.size());
    }
    return w.take();
}

ParseFieldsResult parseKeyFields(const uint8_t* data, size_t len) {
    ParseFieldsResult r;
    CanonicalReader rd(std::string_view(reinterpret_cast<const char*>(data), len));
    uint32_t count = 0;
    if (!rd.u32(count)) { r.error = "truncated field count"; return r; }
    if (count > 4096) { r.error = "field count out of range"; return r; }

    std::vector<KeyFieldEntry> out;
    for (uint32_t i = 0; i < count; ++i) {
        uint8_t tag;
        if (!rd.u8(tag)) { r.error = "truncated field tag"; return r; }
        if (tag == 0 || tag > kMaxKeyField) { r.error = "unknown field tag"; return r; }
        // reject duplicate tags
        for (const auto& e : out) if (e.tag == tag) { r.error = "duplicate field tag"; return r; }
        uint64_t vlen;
        if (!rd.u64(vlen)) { r.error = "truncated field length"; return r; }
        if (vlen > rd.remaining()) { r.error = "field value length exceeds available bytes"; return r; }
        std::string_view v = std::string_view(reinterpret_cast<const char*>(data) + rd.consumed(), vlen);
        KeyFieldEntry e;
        e.tag = tag;
        e.value.assign(v.begin(), v.end());
        rd.advance(vlen); // consume validated value bytes
        out.push_back(std::move(e));
    }
    if (rd.hasRemaining()) { r.error = "trailing garbage after canonical record"; return r; }
    r.ok = true;
    r.fields = std::move(out);
    return r;
}

} // namespace compilationfabric