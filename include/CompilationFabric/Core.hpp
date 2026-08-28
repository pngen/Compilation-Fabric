// Compilation Fabric - Core type foundation (error codes, Result, identities, digest, clock).
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <functional>
#include <chrono>
#include <stdexcept>
#include <memory>
#include <atomic>
#include <mutex>
#include <ostream>

namespace compilationfabric {

// ---------------------------------------------------------------------------
// Structured error codes. Every failure carries a typed, stable code so callers
// and the CLI can branch on the exact cause rather than string-matching.
// ---------------------------------------------------------------------------
enum class ErrorCode : int {
    Ok = 0,
    InvalidArgument,
    InvalidSource,
    UnsupportedIR,
    CompilerMissing,
    BackendMissing,
    TargetUnsupported,
    ArchitectureMismatch,
    ABIMismatch,
    DatatypeMismatch,
    LayoutMismatch,
    ShapeMismatch,
    QuantizationMismatch,
    PrecisionMismatch,
    InvalidSpecialization,
    InvalidKey,
    MalformedKey,
    MalformedFrame,
    ProtocolVersionMismatch,
    UnknownMessageType,
    FrameTooLarge,
    TruncatedFrame,
    StaleEpoch,
    StaleWorkerBoot,
    StaleGeneration,
    StaleAttempt,
    StaleArtifact,
    DuplicateCompletion,
    DuplicateRequest,
    DuplicateAttempt,
    DuplicateArtifact,
    ObsoleteAttempt,
    InvalidStateTransition,
    LifecycleConflict,
    ArtifactCorrupt,
    ArtifactTruncated,
    ArtifactInvalid,
    MetadataCorrupt,
    PersistenceFailure,
    PersistenceVersionUnknown,
    TrailingGarbage,
    PolicyRejected,
    NotDeployable,
    NotValid,
    ConcurrentModification,
    ReentrancyLocked,
    NoBackend,
    NoToolchain,
    NotFound,
    Internal,
    NotImplemented,
    ValidationFailed,
    LoadFailed,
    ExecutionFailed,
    BuildFailed,
    Cancelled,
    IOError,
    Timeout,
    GenerationRollback,
    NoActiveUsers,
    LeaseUnderflow,
    RaceDetected,
    OverSizedFrame,
};

// Human-readable, stable messages for error codes.
std::string_view errorCodeName(ErrorCode c);
std::string_view errorCodeMessage(ErrorCode c);
std::ostream& operator<<(std::ostream& os, ErrorCode c);

// ---------------------------------------------------------------------------
// Result<T> / Result<void>. Throws std::logic_error on value()/operator-> when
// not ok. Prefer explicit checks for control flow.
// ---------------------------------------------------------------------------
template <typename T>
class Result {
public:
    Result() = default; // disengaged
    Result(const T& v) : value_(v), code_(ErrorCode::Ok) {}
    Result(T&& v) : value_(std::move(v)), code_(ErrorCode::Ok) {}
    Result(ErrorCode code, std::string message) : code_(code), message_(std::move(message)) {}

    bool ok() const { return code_ == ErrorCode::Ok && value_.has_value(); }
    bool has_value() const { return value_.has_value(); }
    ErrorCode code() const { return code_; }
    const std::string& message() const { return message_; }

    const T& value() const {
        if (!value_) throw std::logic_error("Result::value() on non-ok result: " + message_);
        return *value_;
    }
    T& value() {
        if (!value_) throw std::logic_error("Result::value() on non-ok result");
        return *value_;
    }
    const T& operator*() const { return value(); }
    T& operator*() { return value(); }
    const T* operator->() const { return &value(); }

    const T& value_or(const T& fallback) const { return value_ ? *value_ : fallback; }

private:
    std::optional<T> value_;
    ErrorCode code_ = ErrorCode::Internal;
    std::string message_;
};

template <>
class Result<void> {
public:
    Result() : ok_(true), code_(ErrorCode::Ok) {}
    Result(ErrorCode code, std::string message) : ok_(false), code_(code), message_(std::move(message)) {}
    bool ok() const { return ok_ && code_ == ErrorCode::Ok; }
    ErrorCode code() const { return code_; }
    const std::string& message() const { return message_; }
private:
    bool ok_;
    ErrorCode code_;
    std::string message_;
};

template <typename T> Result<std::decay_t<T>> Ok(T&& v) { return Result<std::decay_t<T>>(std::forward<T>(v)); }
inline Result<void> OkVoid() { return Result<void>(); }
template <typename T> Result<T> Err(ErrorCode c, std::string msg) { return Result<T>(c, std::move(msg)); }
inline Result<void> ErrVoid(ErrorCode c, std::string msg) { return Result<void>(c, std::move(msg)); }

// ---------------------------------------------------------------------------
// 128-bit identity. Lossless: 64-bit values are stored with hi=0. Serialization
// preserves both words exactly. Parsing rejects invalid or non-canonical text.
// ---------------------------------------------------------------------------
struct Id128 {
    uint64_t hi = 0;
    uint64_t lo = 0;

    Id128() = default;
    Id128(uint64_t h, uint64_t l) : hi(h), lo(l) {}

    static Id128 fromU64(uint64_t v) { return Id128(0, v); }

    static std::optional<Id128> parse(std::string_view hex);
    std::string toHex() const;
    bool isZero() const { return hi == 0 && lo == 0; }

    bool operator==(const Id128& o) const { return hi == o.hi && lo == o.lo; }
    bool operator!=(const Id128& o) const { return !(*this == o); }
    bool operator<(const Id128& o) const { return std::tie(hi, lo) < std::tie(o.hi, o.lo); }
};

template <typename Tag>
class TypedId {
public:
    TypedId() = default;
    explicit TypedId(Id128 v) : v_(v) {}
    static TypedId fromU64(uint64_t v) { return TypedId(Id128::fromU64(v)); }
    static std::optional<TypedId> parse(std::string_view hex) {
        auto p = Id128::parse(hex);
        if (!p) return std::nullopt;
        return TypedId(*p);
    }
    const Id128& raw() const { return v_; }
    std::string toHex() const { return v_.toHex(); }
    bool isZero() const { return v_.isZero(); }
    bool operator==(const TypedId& o) const { return v_ == o.v_; }
    bool operator!=(const TypedId& o) const { return !(*this == o); }
    bool operator<(const TypedId& o) const { return v_ < o.v_; }
private:
    Id128 v_;
};

struct CompilationRequestIdTag {};
struct CompilationPlanIdTag {};
struct CompilationAttemptIdTag {};
struct ArtifactIdTag {};
struct SourceIdentityTag {};
struct IRIdentityTag {};
struct LogicalOperationTag {};
struct WorkerBootIdTag {};

using CompilationRequestId = TypedId<CompilationRequestIdTag>;
using CompilationPlanId = TypedId<CompilationPlanIdTag>;
using CompilationAttemptId = TypedId<CompilationAttemptIdTag>;
using ArtifactId = TypedId<ArtifactIdTag>;
using SourceIdentity = TypedId<SourceIdentityTag>;
using IRIdentity = TypedId<IRIdentityTag>;
using LogicalOperation = TypedId<LogicalOperationTag>;
using WorkerBootId = TypedId<WorkerBootIdTag>;

template <typename Tag> std::string_view identityKindName();
template <> inline std::string_view identityKindName<CompilationRequestIdTag>() { return "CompilationRequestId"; }
template <> inline std::string_view identityKindName<CompilationPlanIdTag>() { return "CompilationPlanId"; }
template <> inline std::string_view identityKindName<CompilationAttemptIdTag>() { return "CompilationAttemptId"; }
template <> inline std::string_view identityKindName<ArtifactIdTag>() { return "ArtifactId"; }
template <> inline std::string_view identityKindName<SourceIdentityTag>() { return "SourceIdentity"; }
template <> inline std::string_view identityKindName<IRIdentityTag>() { return "IRIdentity"; }
template <> inline std::string_view identityKindName<LogicalOperationTag>() { return "LogicalOperation"; }
template <> inline std::string_view identityKindName<WorkerBootIdTag>() { return "WorkerBootId"; }

using CoordinatorEpoch = uint64_t;
using CompilerGeneration = uint64_t;
using ToolchainGeneration = uint64_t;
using CacheGeneration = uint64_t;
using ArtifactGeneration = uint64_t;
using DeploymentGeneration = uint64_t;
using WorkerId = uint32_t;

using Digest = std::array<uint8_t, 32>;

class Sha256 {
public:
    Sha256();
    void update(const void* data, size_t len);
    void update(std::string_view s) { update(s.data(), s.size()); }
    Digest final();
    static Digest hash(const void* data, size_t len);
    static Digest hash(std::string_view s) { return hash(s.data(), s.size()); }
private:
    void transform(const uint8_t* block);
    uint32_t state_[8] = {};
    uint64_t bitLen_ = 0;
    uint8_t buffer_[64] = {};
    size_t bufferLen_ = 0;
};

std::string digestToHex(const Digest& d);
std::optional<Digest> digestFromHex(std::string_view hex);
std::ostream& operator<<(std::ostream& os, const Digest& d);

std::string bytesToHex(const uint8_t* p, size_t n);
std::string bytesToHex(std::string_view s);

// ---------------------------------------------------------------------------
// Clock. steady_clock for durations, system for wall-epoch timestamps (ms).
// ---------------------------------------------------------------------------
class Clock {
public:
    static int64_t nowMillis();
    static int64_t monotonicNanos();
    using duration = std::chrono::steady_clock::duration;
    static duration now();
};

} // namespace compilationfabric

namespace std {
template <> struct hash<compilationfabric::Id128> {
    size_t operator()(const compilationfabric::Id128& v) const noexcept {
        size_t h = std::hash<uint64_t>{}(v.hi);
        h ^= std::hash<uint64_t>{}(v.lo) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        return h;
    }
};
template <typename Tag> struct hash<compilationfabric::TypedId<Tag>> {
    size_t operator()(const compilationfabric::TypedId<Tag>& v) const noexcept {
        return std::hash<compilationfabric::Id128>{}(v.raw());
    }
};
} // namespace std