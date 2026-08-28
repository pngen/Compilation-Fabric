// Compilation Fabric - Protocol.cpp implementation.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#include "CompilationFabric/Protocol.hpp"
#include "CompilationFabric/Request.hpp"

#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

namespace compilationfabric {

namespace {
constexpr size_t kEnvelopeSize = 8 + 4 + 16 + 8 + 8 + 16 + 16 + 16 + 8;
bool readEnvelope(CanonicalReader& r, ProtoEnvelope& e) {
    std::string_view es;
    if (!r.rawBlob(kEnvelopeSize, es)) return false;
    auto dec = ProtoEnvelope::decode(std::vector<uint8_t>(es.begin(), es.end()));
    if (!dec.ok()) return false;
    e = *dec; return true;
}
bool isValidType(uint32_t t) {
    return t >= static_cast<uint32_t>(MsgType::Register) && t <= static_cast<uint32_t>(MsgType::Control);
}
} // namespace

std::string_view msgTypeName(MsgType t) {
    switch (t) {
        case MsgType::Register: return "Register";
        case MsgType::Capabilities: return "Capabilities";
        case MsgType::Submit: return "Submit";
        case MsgType::DispatchCompile: return "DispatchCompile";
        case MsgType::CompileResult: return "CompileResult";
        case MsgType::Result: return "Result";
        case MsgType::CacheHit: return "CacheHit";
        case MsgType::Invalidate: return "Invalidate";
        case MsgType::Heartbeat: return "Heartbeat";
        case MsgType::Shutdown: return "Shutdown";
        case MsgType::Reject: return "Reject";
        case MsgType::Control: return "Control";
    }
    return "Unknown";
}

// ---------------------------------------------------------------------------
// Frame codec
// ---------------------------------------------------------------------------
std::vector<uint8_t> encodeFrameBody(const ProtoFrame& f) {
    CanonicalWriter w;
    w.u32(kProtocolVersion);
    w.u32(static_cast<uint32_t>(f.type));
    w.u32(f.flags);
    w.u32(f.seq);
    w.u64(f.payload.size());
    w.bytes(f.payload.data(), f.payload.size());
    return w.take();
}
Result<ProtoFrame> decodeFrameBody(const std::vector<uint8_t>& body) {
    CanonicalReader r(std::string_view(reinterpret_cast<const char*>(body.data()), body.size()));
    uint32_t v; if (!r.u32(v)) return Err<ProtoFrame>(ErrorCode::TruncatedFrame, "truncated protocol version");
    if (v != kProtocolVersion) return Err<ProtoFrame>(ErrorCode::ProtocolVersionMismatch, "protocol version mismatch");
    uint32_t type, flags, seq;
    if (!r.u32(type)) return Err<ProtoFrame>(ErrorCode::TruncatedFrame, "truncated message type");
    if (!isValidType(type)) return Err<ProtoFrame>(ErrorCode::UnknownMessageType, "unknown message type " + std::to_string(type));
    if (!r.u32(flags)) return Err<ProtoFrame>(ErrorCode::TruncatedFrame, "truncated flags");
    if (!r.u32(seq)) return Err<ProtoFrame>(ErrorCode::TruncatedFrame, "truncated seq");
    std::string_view pd; if (!r.bytesBlob(pd)) return Err<ProtoFrame>(ErrorCode::TruncatedFrame, "truncated payload");
    if (r.hasRemaining()) return Err<ProtoFrame>(ErrorCode::TrailingGarbage, "trailing garbage after frame");
    ProtoFrame f; f.type = static_cast<MsgType>(type); f.flags = flags; f.seq = seq;
    f.payload.assign(pd.begin(), pd.end());
    return Ok(std::move(f));
}

// ---------------------------------------------------------------------------
// Envelope
// ---------------------------------------------------------------------------
std::vector<uint8_t> ProtoEnvelope::encode() const {
    CanonicalWriter w;
    w.u64(epoch); w.u32(workerId); w.u128(bootId.raw());
    w.u64(cacheGen); w.u64(toolchainGen);
    w.u128(requestId.raw()); w.u128(attemptId.raw()); w.u128(artifactId.raw());
    w.u64(artifactGen);
    return w.take();
}
Result<ProtoEnvelope> ProtoEnvelope::decode(const std::vector<uint8_t>& bytes) {
    CanonicalReader r(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
    ProtoEnvelope e;
    if (!r.u64(e.epoch)) return Err<ProtoEnvelope>(ErrorCode::TruncatedFrame, "truncated epoch");
    if (!r.u32(e.workerId)) return Err<ProtoEnvelope>(ErrorCode::TruncatedFrame, "truncated worker id");
    Id128 boot; if (!r.u128(boot)) return Err<ProtoEnvelope>(ErrorCode::TruncatedFrame, "truncated boot id");
    e.bootId = WorkerBootId(boot);
    if (!r.u64(e.cacheGen)) return Err<ProtoEnvelope>(ErrorCode::TruncatedFrame, "truncated cache gen");
    if (!r.u64(e.toolchainGen)) return Err<ProtoEnvelope>(ErrorCode::TruncatedFrame, "truncated toolchain gen");
    Id128 rid, aid, arid;
    if (!r.u128(rid)) return Err<ProtoEnvelope>(ErrorCode::TruncatedFrame, "truncated request id");
    if (!r.u128(aid)) return Err<ProtoEnvelope>(ErrorCode::TruncatedFrame, "truncated attempt id");
    if (!r.u128(arid)) return Err<ProtoEnvelope>(ErrorCode::TruncatedFrame, "truncated artifact id");
    e.requestId = CompilationRequestId(rid); e.attemptId = CompilationAttemptId(aid); e.artifactId = ArtifactId(arid);
    if (!r.u64(e.artifactGen)) return Err<ProtoEnvelope>(ErrorCode::TruncatedFrame, "truncated artifact gen");
    if (r.hasRemaining()) return Err<ProtoEnvelope>(ErrorCode::TrailingGarbage, "trailing garbage in envelope");
    return Ok(std::move(e));
}
Json ProtoEnvelope::toJson() const {
    return Json::object({
        {"epoch", Json::number(static_cast<double>(epoch))},
        {"worker_id", Json::number(static_cast<double>(workerId))},
        {"worker_boot", Json::str(bootId.toHex())},
        {"cache_gen", Json::number(static_cast<double>(cacheGen))},
        {"toolchain_gen", Json::number(static_cast<double>(toolchainGen))},
        {"request_id", Json::str(requestId.toHex())},
        {"attempt_id", Json::str(attemptId.toHex())},
        {"artifact_id", Json::str(artifactId.toHex())},
        {"artifact_gen", Json::number(static_cast<double>(artifactGen))},
    });
}

// ---------------------------------------------------------------------------
// Payload codecs
// ---------------------------------------------------------------------------
std::vector<uint8_t> encodeRegister(const ProtoEnvelope& env, const Json& capabilities) {
    CanonicalWriter w;
    w.bytes(env.encode().data(), env.encode().size());
    std::string caps = capabilities.dump();
    w.string(caps);
    return w.take();
}
Json decodeRegister(const std::vector<uint8_t>& payload) {
    CanonicalReader r(std::string_view(reinterpret_cast<const char*>(payload.data()), payload.size()));
    ProtoEnvelope e; (void)readEnvelope(r, e);
    std::string caps;
    r.str(caps);
    if (auto j = Json::parse(caps)) return *j;
    return Json::object({});
}

std::vector<uint8_t> encodeSubmit(const ProtoEnvelope& env, const CompilationRequest& req) {
    CanonicalWriter w;
    w.bytes(env.encode().data(), env.encode().size());
    w.string(req.toJson().dump());
    return w.take();
}
Result<CompilationRequest> decodeSubmit(const std::vector<uint8_t>& payload) {
    CanonicalReader r(std::string_view(reinterpret_cast<const char*>(payload.data()), payload.size()));
    ProtoEnvelope e; (void)readEnvelope(r, e);
    std::string reqJson;
    if (!r.str(reqJson)) return Err<CompilationRequest>(ErrorCode::MalformedFrame, "truncated request payload");
    auto j = Json::parse(reqJson);
    if (!j) return Err<CompilationRequest>(ErrorCode::MalformedFrame, "request JSON parse failed");
    auto req = CompilationRequest::fromJson(*j);
    if (!req) return Err<CompilationRequest>(ErrorCode::MalformedFrame, "request decode failed");
    return Ok(std::move(*req));
}

std::vector<uint8_t> encodeDispatch(const ProtoEnvelope& env, const CompilationRequest& req, const CompilationKey& key) {
    CanonicalWriter w;
    w.bytes(env.encode().data(), env.encode().size());
    w.string(req.toJson().dump());
    auto kbytes = makeCanonicalFields(key.fields());
    w.u64(kbytes.size());
    w.bytes(kbytes.data(), kbytes.size());
    return w.take();
}
Result<std::pair<CompilationRequest, CompilationKey>> decodeDispatch(const std::vector<uint8_t>& payload) {
    CanonicalReader r(std::string_view(reinterpret_cast<const char*>(payload.data()), payload.size()));
    ProtoEnvelope e; (void)readEnvelope(r, e);
    std::string reqJson;
    if (!r.str(reqJson)) return Err<std::pair<CompilationRequest, CompilationKey>>(ErrorCode::MalformedFrame, "truncated dispatch request");
    auto j = Json::parse(reqJson);
    if (!j) return Err<std::pair<CompilationRequest, CompilationKey>>(ErrorCode::MalformedFrame, "dispatch JSON parse failed");
    auto req = CompilationRequest::fromJson(*j);
    if (!req) return Err<std::pair<CompilationRequest, CompilationKey>>(ErrorCode::MalformedFrame, "dispatch request decode failed");
    uint64_t keyLen; if (!r.u64(keyLen)) return Err<std::pair<CompilationRequest, CompilationKey>>(ErrorCode::MalformedFrame, "truncated key len");
    std::string_view kb; if (!r.rawBlob(keyLen, kb) || kb.size() != keyLen) return Err<std::pair<CompilationRequest, CompilationKey>>(ErrorCode::MalformedFrame, "truncated key");
    auto key = CompilationKey::fromCanonicalBytes(reinterpret_cast<const uint8_t*>(kb.data()), kb.size());
    if (!key) return Err<std::pair<CompilationRequest, CompilationKey>>(ErrorCode::MalformedFrame, "dispatch key decode failed");
    return Ok(std::make_pair(std::move(*req), *key));
}

std::vector<uint8_t> encodeCompileResult(const ProtoEnvelope& env, bool success, const Json& result) {
    CanonicalWriter w;
    w.bytes(env.encode().data(), env.encode().size());
    w.u8(success ? 1 : 0);
    std::string rjson = result.dump();
    w.string(rjson);
    return w.take();
}
Json decodeCompileResult(const std::vector<uint8_t>& payload, bool& success) {
    success = false;
    CanonicalReader r(std::string_view(reinterpret_cast<const char*>(payload.data()), payload.size()));
    ProtoEnvelope e; (void)readEnvelope(r, e);
    uint8_t s; if (!r.u8(s)) return Json::object({});
    success = (s != 0);
    std::string res;
    if (!r.str(res)) return Json::object({});
    if (auto j = Json::parse(res)) return *j;
    return Json::object({});
}

std::vector<uint8_t> encodeResult(const ProtoEnvelope& env, const CompilationResult& result) {
    CanonicalWriter w;
    w.bytes(env.encode().data(), env.encode().size());
    std::string rjson = result.toJson().dump();
    w.string(rjson);
    w.u64(result.artifactBytes.size());
    w.bytes(result.artifactBytes.data(), result.artifactBytes.size());
    return w.take();
}
Result<CompilationResult> decodeResult(const std::vector<uint8_t>& payload) {
    CanonicalReader r(std::string_view(reinterpret_cast<const char*>(payload.data()), payload.size()));
    ProtoEnvelope e; (void)readEnvelope(r, e);
    std::string rjson;
    if (!r.str(rjson)) return Err<CompilationResult>(ErrorCode::MalformedFrame, "truncated result json");
    auto j = Json::parse(rjson);
    if (!j) return Err<CompilationResult>(ErrorCode::MalformedFrame, "result JSON parse failed");
    CompilationResult res;
    if (auto* rid = j->get("request_id")) if (auto i = CompilationRequestId::parse(rid->asString())) res.requestId = *i;
    if (auto* p = j->get("plan_id")) if (auto i = CompilationPlanId::parse(p->asString())) res.planId = *i;
    if (auto* at = j->get("attempt_id")) if (auto i = CompilationAttemptId::parse(at->asString())) res.attemptId = *i;
    if (auto* ar = j->get("artifact_id")) if (auto i = ArtifactId::parse(ar->asString())) res.artifactId = *i;
    if (auto* g = j->get("generation")) res.generation = static_cast<ArtifactGeneration>(g->asNumber());

    if (auto* ad = j->get("artifact")) if (auto v = ArtifactDescriptor::fromJson(*ad)) res.artifact = *v;
    if (auto* reused = j->get("reused")) res.reused = reused->asBool();
    if (auto* v = j->get("validated")) res.validated = v->asBool();
    if (auto* d = j->get("deployable")) res.deployable = d->asBool();
    if (auto* rm = j->get("reference_matched")) res.referenceMatched = rm->asBool();
    if (auto* bu = j->get("backend_used")) res.backendUsed = bu->asString();
    uint64_t byteLen; if (!r.u64(byteLen)) return Err<CompilationResult>(ErrorCode::MalformedFrame, "truncated artifact len");
    std::string_view ab; if (!r.rawBlob(byteLen, ab) || ab.size() != byteLen) return Err<CompilationResult>(ErrorCode::MalformedFrame, "truncated artifact bytes");
    res.artifactBytes.assign(ab.begin(), ab.end());
    return Ok(std::move(res));
}

std::vector<uint8_t> encodeReject(const ProtoEnvelope& env, ErrorCode reason, const std::string& msg) {
    CanonicalWriter w;
    w.bytes(env.encode().data(), env.encode().size());
    w.u32(static_cast<uint32_t>(reason));
    w.string(msg);
    return w.take();
}
ErrorCode decodeRejectCode(const std::vector<uint8_t>& payload) {
    CanonicalReader r(std::string_view(reinterpret_cast<const char*>(payload.data()), payload.size()));
    ProtoEnvelope e; (void)readEnvelope(r, e);
    uint32_t c; if (!r.u32(c)) return ErrorCode::MalformedFrame;
    return static_cast<ErrorCode>(c);
}

std::vector<uint8_t> encodeControl(ControlKind kind) { CanonicalWriter w; w.u32(static_cast<uint32_t>(kind)); return w.take(); }
ControlKind decodeControl(const std::vector<uint8_t>& payload) { CanonicalReader r(std::string_view(reinterpret_cast<const char*>(payload.data()), payload.size())); uint32_t k; if (!r.u32(k)) return ControlKind::Ping; return static_cast<ControlKind>(k); }
std::vector<uint8_t> encodeControlReply(const std::string& json) { CanonicalWriter w; w.string(json); return w.take(); }
Result<std::string> decodeControlReply(const std::vector<uint8_t>& payload) { CanonicalReader r(std::string_view(reinterpret_cast<const char*>(payload.data()), payload.size())); std::string s; if (!r.str(s)) return Err<std::string>(ErrorCode::MalformedFrame, "bad control reply"); return Ok(s); }

// ---------------------------------------------------------------------------
// TCP
// ---------------------------------------------------------------------------
void initSockets() {
#ifdef _WIN32
    static bool done = false;
    if (done) return;
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    done = true;
#else
    (void)0;
#endif
}
void shutdownSockets() {
#ifdef _WIN32
    WSACleanup();
#else
    (void)0;
#endif
}

TcpSocket::TcpSocket(TcpSocket&& o) noexcept : fd_(o.fd_) { o.fd_ = -1; }
TcpSocket& TcpSocket::operator=(TcpSocket&& o) noexcept { close(); fd_ = o.fd_; o.fd_ = -1; return *this; }
TcpSocket::~TcpSocket() { close(); }
bool TcpSocket::valid() const { return fd_ >= 0; }
void TcpSocket::close() { if (fd_ >= 0) {
#ifdef _WIN32
    ::shutdown(fd_, SD_BOTH); ::closesocket(fd_);
#else
    ::shutdown(fd_, SHUT_RDWR); ::close(fd_);
#endif
    fd_ = -1; } }

Result<void> TcpSocket::connectTo(const std::string& host, uint16_t port) {
    initSockets();
    fd_ = static_cast<int>(::socket(AF_INET, SOCK_STREAM, 0));
    if (fd_ < 0) return ErrVoid(ErrorCode::IOError, "socket() failed");
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
    if (::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) { close(); return ErrVoid(ErrorCode::IOError, "connect failed to " + host + ":" + std::to_string(port)); }
    return OkVoid();
}
Result<void> TcpSocket::bindListen(uint16_t port) {
    initSockets();
    fd_ = static_cast<int>(::socket(AF_INET, SOCK_STREAM, 0));
    if (fd_ < 0) return ErrVoid(ErrorCode::IOError, "socket() failed");
    int yes = 1;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_addr.s_addr = INADDR_ANY; addr.sin_port = htons(port);
    if (::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) { close(); return ErrVoid(ErrorCode::IOError, "bind failed on port " + std::to_string(port)); }
    if (::listen(fd_, 32) != 0) { close(); return ErrVoid(ErrorCode::IOError, "listen failed"); }
    return OkVoid();
}
Result<TcpSocket> TcpSocket::accept() {
    sockaddr_in cli{}; int len = sizeof(cli);
    int cfd = static_cast<int>(::accept(fd_, reinterpret_cast<sockaddr*>(&cli), reinterpret_cast<socklen_t*>(&len)));
#ifdef _WIN32
    (void)cli;
#endif
    if (cfd < 0) return Err<TcpSocket>(ErrorCode::IOError, "accept failed");
    return Ok(TcpSocket(cfd));
}

namespace {
Result<void> sendAll(int fd, const uint8_t* data, size_t len) {
    size_t off = 0;
    while (off < len) {
        int sent = static_cast<int>(::send(fd, reinterpret_cast<const char*>(data + off), static_cast<int>(len - off), 0));
#ifdef _WIN32
        if (sent == SOCKET_ERROR) return ErrVoid(ErrorCode::IOError, "send failed");
#else
        if (sent < 0) return ErrVoid(ErrorCode::IOError, "send failed");
#endif
        off += static_cast<size_t>(sent);
    }
    return OkVoid();
}
Result<size_t> recvAll(int fd, uint8_t* data, size_t len) {
    size_t off = 0;
    while (off < len) {
        int got = static_cast<int>(::recv(fd, reinterpret_cast<char*>(data + off), static_cast<int>(len - off), 0));
#ifdef _WIN32
        if (got == 0) return Err<size_t>(ErrorCode::IOError, "connection closed");
        if (got == SOCKET_ERROR) return Err<size_t>(ErrorCode::IOError, "recv failed");
#else
        if (got == 0) return Err<size_t>(ErrorCode::IOError, "connection closed");
        if (got < 0) return Err<size_t>(ErrorCode::IOError, "recv failed");
#endif
        off += static_cast<size_t>(got);
    }
    return Ok(off);
}
} // namespace

Result<void> FramedChannel::sendFrame(const ProtoFrame& f) {
    auto body = encodeFrameBody(f);
    if (body.size() > kMaxFrameSize) return ErrVoid(ErrorCode::FrameTooLarge, "frame exceeds max size");
    uint8_t lenb[4];
    uint32_t len = static_cast<uint32_t>(body.size());
    lenb[0] = (len >> 24) & 0xFF; lenb[1] = (len >> 16) & 0xFF; lenb[2] = (len >> 8) & 0xFF; lenb[3] = len & 0xFF;
    if (auto r = sendAll(sock_.native(), lenb, 4); !r.ok()) return r;
    if (auto r = sendAll(sock_.native(), body.data(), body.size()); !r.ok()) return r;
    return OkVoid();
}
Result<ProtoFrame> FramedChannel::recvFrame() {
    uint8_t lenb[4];
    if (auto r = recvAll(sock_.native(), lenb, 4); !r.ok()) return Err<ProtoFrame>(r.code(), r.message());
    uint32_t len = (uint32_t(lenb[0]) << 24) | (uint32_t(lenb[1]) << 16) | (uint32_t(lenb[2]) << 8) | lenb[3];
    if (len == 0) return Err<ProtoFrame>(ErrorCode::MalformedFrame, "zero-length frame");
    if (len > kMaxFrameSize) return Err<ProtoFrame>(ErrorCode::OverSizedFrame, "frame exceeds maximum size");
    std::vector<uint8_t> body(len);
    if (auto r = recvAll(sock_.native(), body.data(), len); !r.ok()) return Err<ProtoFrame>(ErrorCode::TruncatedFrame, "truncated frame body: " + r.message());
    return decodeFrameBody(body);
}

} // namespace compilationfabric