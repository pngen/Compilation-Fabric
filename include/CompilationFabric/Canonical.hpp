// Compilation Fabric - Canonical deterministic binary encoding.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#pragma once

#include "CompilationFabric/Core.hpp"

#include <vector>
#include <cstdint>
#include <string>

namespace compilationfabric {

// ---------------------------------------------------------------------------
// CanonicalWriter: grows a deterministic big-endian buffer. Used both to encode
// individual typed field values and to assemble a top-level canonical record.
// ---------------------------------------------------------------------------
class CanonicalWriter {
public:
    void u8(uint8_t v);
    void u16(uint16_t v);
    void u32(uint32_t v);
    void u64(uint64_t v);
    void u128(const Id128& v);
    void f32(float v);
    void f64(double v);
    void string(std::string_view s);
    void bytes(const uint8_t* p, size_t n);
    void bytes(std::string_view s) { bytes(reinterpret_cast<const uint8_t*>(s.data()), s.size()); }

    const std::vector<uint8_t>& data() const { return buf_; }
    std::vector<uint8_t>& data() { return buf_; }
    std::vector<uint8_t> take() { return std::move(buf_); }
    size_t size() const { return buf_.size(); }
    bool empty() const { return buf_.empty(); }
    void clear() { buf_.clear(); }
private:
    std::vector<uint8_t> buf_;
};

// ---------------------------------------------------------------------------
// CanonicalReader: bounded decoder. Any attempt to read past the end is a
// malformed/truncated condition. Reading a length that exceeds available bytes
// is rejected. Trailing bytes after the caller's last read can be detected via
// remaining().
// ---------------------------------------------------------------------------
class CanonicalReader {
public:
    CanonicalReader() = default;
    explicit CanonicalReader(std::string_view data) : data_(data), pos_(0) {}

    bool hasRemaining() const { return pos_ < data_.size(); }
    size_t remaining() const { return data_.size() - pos_; }
    size_t consumed() const { return pos_; }

    bool u8(uint8_t& out);
    bool u16(uint16_t& out);
    bool u32(uint32_t& out);
    bool u64(uint64_t& out);
    bool u128(Id128& out);
    bool f32(float& out);
    bool f64(double& out);
    bool str(std::string& out);
    bool bytesBlob(std::string_view& out);
    bool rawBlob(size_t n, std::string_view& out) { if (pos_ + n > data_.size()) return false; out = data_.substr(pos_, n); pos_ += n; return true; }
    void advance(size_t n) { if (n <= remaining()) pos_ += n; }
    const uint8_t* current() const { return reinterpret_cast<const uint8_t*>(data_.data() + pos_); }

private:
    std::string_view data_;
    size_t pos_ = 0;
};

// ---------------------------------------------------------------------------
// Typed field tags used by the CompilationKey canonical TLV record. The tags
// are stable ordinals; they never change meaning across versions. Field order in
// the canonical record is irrelevant because the record is made canonical by
// sorting on tag before hashing.
// ---------------------------------------------------------------------------
enum class KeyField : uint8_t {
    LogicalOperation = 1,
    SourceIdentity,
    SourceDigest,
    IRIdentity,
    IRDigest,
    IRFormat,
    Frontend,
    FrontendVersion,
    Compiler,
    CompilerVersion,
    Backend,
    BackendVersion,
    CodeGenerator,
    Optimizer,
    OptimizerVersion,
    Linker,
    LinkerVersion,
    Runtime,
    RuntimeVersion,
    Driver,
    DriverVersion,
    AcceleratorVendor,
    AcceleratorFamily,
    TargetArchitecture,
    ComputeCapability,
    ISA,
    ABI,
    CallingConvention,
    KernelABI,
    GraphABI,
    OperatorRevision,
    Datatype,
    Layout,
    Rank,
    StaticShape,
    SymbolicShape,
    Alignment,
    Quantization,
    Precision,
    LaunchSpecialization,
    ScalarConstants,
    FeatureFlags,
    OptimizationFlags,
    CodegenFlags,
    DebugRelease,
    Determinism,
    Reproducibility,
    EnvFingerprint,
    DependencyIdentities,
    DependencyGenerations,
    ModelRevision,
    RuntimeCompatibilityGeneration,
    CompilerPolicyGeneration,
    Namespace,
    Tenant,
    SourceContent,
    IRContent,
};

constexpr uint8_t keyFieldTag(KeyField f) { return static_cast<uint8_t>(f); }
std::string_view keyFieldName(KeyField f);

// A single typed field carried in a canonical record.
struct KeyFieldEntry {
    uint8_t tag = 0;
    std::vector<uint8_t> value;
};

// ---------------------------------------------------------------------------
// Assembling a canonical TLV record and its inverse.
//
// Record layout (big-endian):
//   u32 fieldCount
//   for each field (in canonical, tag-sorted order):
//     u8  tag
//     u64 valueByteLength
//     value[valueByteLength]
//
// makeCanonicalFields() sorts on tag so the byte encoding is independent of the
// order in which fields were added. parseKeyFields() enforces exact grammar:
//   - truncated count / truncated tag / truncated length / truncated value => fail
//   - valueByteLength larger than remaining bytes => fail
//   - unknown tag (0 or > KeyField::Tenant range) => fail
//   - trailing bytes after the declared field count => fail
// Duplicate tags are rejected as malformed.
// ---------------------------------------------------------------------------
std::vector<uint8_t> makeCanonicalFields(const std::vector<KeyFieldEntry>& fields);

struct ParseFieldsResult {
    bool ok = false;
    std::vector<KeyFieldEntry> fields;
    std::string error;
};
ParseFieldsResult parseKeyFields(const uint8_t* data, size_t len);
inline ParseFieldsResult parseKeyFields(const std::vector<uint8_t>& v) { return parseKeyFields(v.data(), v.size()); }

} // namespace compilationfabric