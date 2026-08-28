// Compilation Fabric - Framed TCP protocol + message codec.
//
// Wire framing: [u32 bodyLength][body]. body = [u32 version][u32 type][u32 flags]
// [u32 seq][payload...]. bodyLength is fixed-width (4 bytes) and has a hard
// maximum (kMaxFrameSize). The decoder strictly validates the length bound, the
// protocol version, and the message type. Identities are carried losslessly as
// 128-bit fields.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#pragma once
#include "CompilationFabric/Core.hpp"
#include "CompilationFabric/Json.hpp"
#include "CompilationFabric/Canonical.hpp"
#include "CompilationFabric/Request.hpp"
#include <vector>
#include <string>
#include <cstdint>

namespace compilationfabric {

constexpr uint32_t kProtocolVersion = 1;
constexpr uint32_t kMaxFrameSize = 1u << 20;   // 1 MiB hard maximum
constexpr uint32_t kMaxPayload = kMaxFrameSize;

enum class MsgType : uint32_t {
    Register = 1,            // Worker -> Coordinator capability/identity advertisement
    Capabilities,            // Worker -> Coordinator
    Submit,                  // Client -> Coordinator (a compile request)
    DispatchCompile,         // Coordinator -> Worker
    CompileResult,           // Worker -> Coordinator
    Result,                  // Coordinator -> Client
    CacheHit,                // Coordinator -> Client
    Invalidate,              // Client -> Coordinator
    Heartbeat,               // keepalive
    Shutdown,
    Reject,                  // Coordinator -> sender (stale_epoch, stale_worker_boot, ...)
    Control,                 // control-plane (roll epoch, stats)
};
std::string_view msgTypeName(MsgType t);

// Control-plane messages (operator/test control), carried as a Control frame.
enum class ControlKind : uint32_t { RollEpoch = 1, GetStats = 2, Ping = 3, SetCacheGen = 4, SetToolchainGen = 5 };
std::vector<uint8_t> encodeControl(ControlKind kind);
ControlKind decodeControl(const std::vector<uint8_t>& payload);
std::vector<uint8_t> encodeControlReply(const std::string& json);
Result<std::string> decodeControlReply(const std::vector<uint8_t>& payload);


struct ProtoFrame {
    MsgType type = MsgType::Heartbeat;
    uint32_t flags = 0;
    uint32_t seq = 0;
    std::vector<uint8_t> payload;
};

// Canonical encode/decode of a frame body (everything after the length prefix).
std::vector<uint8_t> encodeFrameBody(const ProtoFrame& f);
Result<ProtoFrame> decodeFrameBody(const std::vector<uint8_t>& body);

// Security + authority envelope carried by every message.
struct ProtoEnvelope {
    CoordinatorEpoch epoch = 0;
    WorkerId workerId = 0;
    WorkerBootId bootId;
    CacheGeneration cacheGen = 0;
    ToolchainGeneration toolchainGen = 0;
    CompilationRequestId requestId;
    CompilationAttemptId attemptId;
    ArtifactId artifactId;
    ArtifactGeneration artifactGen = 0;
    std::vector<uint8_t> encode() const;
    static Result<ProtoEnvelope> decode(const std::vector<uint8_t>& bytes);
    Json toJson() const;
};

// Message payloads.
std::vector<uint8_t> encodeRegister(const ProtoEnvelope& env, const Json& capabilities);
Json decodeRegister(const std::vector<uint8_t>& payload); // returns capabilities json
std::vector<uint8_t> encodeSubmit(const ProtoEnvelope& env, const CompilationRequest& req);
Result<CompilationRequest> decodeSubmit(const std::vector<uint8_t>& payload);
std::vector<uint8_t> encodeDispatch(const ProtoEnvelope& env, const CompilationRequest& req, const CompilationKey& key);
Result<std::pair<CompilationRequest, CompilationKey>> decodeDispatch(const std::vector<uint8_t>& payload);
std::vector<uint8_t> encodeCompileResult(const ProtoEnvelope& env, bool success, const Json& result);
Json decodeCompileResult(const std::vector<uint8_t>& payload, bool& success);
std::vector<uint8_t> encodeResult(const ProtoEnvelope& env, const CompilationResult& result);
Result<CompilationResult> decodeResult(const std::vector<uint8_t>& payload);
std::vector<uint8_t> encodeReject(const ProtoEnvelope& env, ErrorCode reason, const std::string& msg);
// Returns the rejection ErrorCode by scanning the payload.
ErrorCode decodeRejectCode(const std::vector<uint8_t>& payload);

// ---------------------------------------------------------------------------
// TCP transport (Windows sockets). Not thread-safe per socket.
// ---------------------------------------------------------------------------
class TcpSocket {
public:
    TcpSocket() = default;
    TcpSocket(TcpSocket&& o) noexcept;
    TcpSocket& operator=(TcpSocket&& o) noexcept;
    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;
    ~TcpSocket();
    Result<void> connectTo(const std::string& host, uint16_t port);
    Result<void> bindListen(uint16_t port);
    Result<TcpSocket> accept();
    bool valid() const;
    void close();
    int native() const { return fd_; }
private:
    TcpSocket(int fd) : fd_(fd) {}
    int fd_ = -1;
};

class FramedChannel {
public:
    explicit FramedChannel(TcpSocket sock) : sock_(std::move(sock)) {}
    Result<void> sendFrame(const ProtoFrame& f);
    Result<ProtoFrame> recvFrame();
    bool valid() const { return sock_.valid(); }
    void close() { sock_.close(); }
    TcpSocket& socket() { return sock_; }
private:
    TcpSocket sock_;
};

// Ensures Winsock is initialized (call once at process start).
void initSockets();
void shutdownSockets();

} // namespace compilationfabric