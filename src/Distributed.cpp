// Compilation Fabric - Distributed.cpp implementation.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#include "CompilationFabric/Distributed.hpp"
#include "CompilationFabric/CpuBackend.hpp"
#include "CompilationFabric/Cuda.hpp"

#include <thread>
#include <chrono>
#include <filesystem>

namespace compilationfabric {

namespace {
constexpr size_t kEnvSize = 8 + 4 + 16 + 8 + 8 + 16 + 16 + 16 + 8;
ProtoEnvelope readEnv(const uint8_t* p, size_t n) {
    ProtoEnvelope e;
    if (n < kEnvSize) return e;
    auto dec = ProtoEnvelope::decode(std::vector<uint8_t>(p, p + kEnvSize));
    if (dec.ok()) e = *dec;
    return e;
}
CompilationResult resultFromJson(const Json& j) {
    CompilationResult res;
    if (auto* rid = j.get("request_id")) if (auto i = CompilationRequestId::parse(rid->asString())) res.requestId = *i;
    if (auto* at = j.get("attempt_id")) if (auto i = CompilationAttemptId::parse(at->asString())) res.attemptId = *i;
    if (auto* ar = j.get("artifact_id")) if (auto i = ArtifactId::parse(ar->asString())) res.artifactId = *i;
    if (auto* g = j.get("generation")) res.generation = static_cast<ArtifactGeneration>(g->asNumber());
    if (auto* ad = j.get("artifact")) if (auto v = ArtifactDescriptor::fromJson(*ad)) res.artifact = *v;
    if (auto* v = j.get("validated")) res.validated = v->asBool();
    if (auto* d = j.get("deployable")) res.deployable = d->asBool();
    if (auto* rm = j.get("reference_matched")) res.referenceMatched = rm->asBool();
    if (auto* bu = j.get("backend_used")) res.backendUsed = bu->asString();
    if (auto* em = j.get("error_message")) res.errorMessage = em->asString();
    return res;
}
} // namespace

// ---------------------------------------------------------------------------
// Worker
// ---------------------------------------------------------------------------
DistributedWorker::DistributedWorker(std::string host, uint16_t port, WorkerId id, bool preferCuda)
    : host_(std::move(host)), port_(port), id_(id), preferCuda_(preferCuda) {}

int DistributedWorker::run() {
    initSockets();
    TcpSocket sock;
    auto cc = sock.connectTo(host_, port_);
    FramedChannel ch(std::move(sock));

    WorkerBootId boot = WorkerBootId::fromU64(Clock::monotonicNanos());
    ProtoEnvelope env; env.epoch = 1; env.workerId = id_; env.bootId = boot; env.cacheGen = 1; env.toolchainGen = 1;
    Json caps = Json::object({});
    caps.set("worker_id", Json::number(static_cast<double>(id_)));
    caps.set("backend", Json::str(preferCuda_ ? "cuda-nvrtc" : "cpu"));
    caps.set("target_architecture", Json::str(preferCuda_ ? "sm_120" : "host-x86_64"));
    caps.set("compiler", Json::str(preferCuda_ ? "nvrtc" : "cf-cpu"));
    caps.set("cuda", Json::boolean(CudaBackend::api()->available()));
    caps.set("boot_id", Json::str(boot.toHex()));

    ProtoFrame reg; reg.type = MsgType::Register; reg.payload = encodeRegister(env, caps);
    if (!ch.sendFrame(reg).ok()) return 1;

    std::filesystem::path root = std::filesystem::temp_directory_path() / ("cf_worker_" + std::to_string(id_) + "_" + std::to_string(Clock::monotonicNanos()));
    std::error_code ec0; std::filesystem::create_directories(root, ec0);
    CpuBackend cpu;

    for (;;) {
        auto fr = ch.recvFrame();
        if (!fr.ok()) { std::error_code ec; std::filesystem::remove_all(root, ec); return 1; }
        ProtoFrame f = *fr;
        if (f.type == MsgType::Shutdown) break;
        if (f.type == MsgType::Heartbeat) { ProtoFrame hb; hb.type = MsgType::Heartbeat; ch.sendFrame(hb); continue; }
        if (f.type != MsgType::DispatchCompile) continue;
        ProtoEnvelope dispEnv = readEnv(f.payload.data(), f.payload.size());
        auto dr = decodeDispatch(f.payload);
        CompilationRequest req = dr->first;
        CompilationKey key = dr->second;
        CompilationPlan plan; plan.backend = "cpu"; plan.frontend = "cf-frontend"; plan.optimizer = "cf-cpu-opt";
        plan.linker = "cf-cpu-link"; plan.runtime = "cf-cpu-runtime"; plan.target.vendor = AcceleratorVendor::CPU;
        plan.target.family = AcceleratorFamily::X86_64; plan.target.architecture = "host-x86_64"; plan.target.isa = "x86_64"; plan.target.abi = "sysv64";
        plan.expectedFormat = ArtifactFormat::Bytecode; plan.expectedValidationMethod = "cpu-execute-reference"; plan.expectedDeploymentMethod = "cpu-execute";
        KeyToolchainContext tc; tc.compiler="cf-cpu"; tc.backend="cpu"; tc.frontend="cf-frontend"; tc.compilerVersion="1.0.0";
        CompilationResult res;
        bool okFlag = false;
        {
            auto bo = cpu.compile(req, plan, tc);

            if (bo.ok()) {
                res = CompilationResult{}; res.requestId = req.requestId; res.key = key;
                res.attemptId = dispEnv.attemptId;
                res.artifactId = ArtifactId::fromU64(Clock::monotonicNanos() & 0x7FFFFFFFFFFFFFFFULL);
                res.generation = 1; res.validated = bo->validation.passed; res.deployable = bo->validation.passed;
                res.referenceMatched = bo->validation.referenceComparison;
                res.backendUsed = "cpu"; res.artifactBytes = bo->executable;
                res.artifact.backend = bo->backend; res.artifact.specialization = bo->specialization;
                res.artifact.validation = bo->validation;
                res.artifact.contentDigest = Sha256::hash(bo->executable.data(), bo->executable.size());
                res.error = bo->validation.passed ? ErrorCode::Ok : ErrorCode::ValidationFailed;
                okFlag = bo->validation.passed;
            }
        }
        ProtoEnvelope outEnv; outEnv.epoch = dispEnv.epoch; outEnv.workerId = id_; outEnv.bootId = boot;
        outEnv.cacheGen = dispEnv.cacheGen; outEnv.toolchainGen = dispEnv.toolchainGen;
        outEnv.requestId = req.requestId; outEnv.attemptId = dispEnv.attemptId; outEnv.artifactId = res.artifactId; outEnv.artifactGen = res.generation;
        ProtoFrame crf; crf.type = MsgType::CompileResult; crf.payload = encodeCompileResult(outEnv, okFlag, res.toJson());
        ch.sendFrame(crf);     }
    std::error_code ec; std::filesystem::remove_all(root, ec);
    return 0;
}

// ---------------------------------------------------------------------------
// Coordinator
// ---------------------------------------------------------------------------
DistributedCoordinator::DistributedCoordinator(uint16_t port, std::string artifactRoot,
        CoordinatorEpoch epoch, CacheGeneration cacheGen, ToolchainGeneration toolchainGen)
    : port_(port), root_(std::move(artifactRoot)), epoch_(epoch), cacheGen_(cacheGen), toolchainGen_(toolchainGen) {
    CompilationFabricConfig fc; fc.artifactRoot = root_; fc.persistenceEnabled = false; fc.allowCuda = false;
    fabric_ = std::make_unique<CompilationFabric>(fc);
}
DistributedCoordinator::~DistributedCoordinator() { shutdownSockets(); }
void DistributedCoordinator::rollEpoch() { epoch_.fetch_add(1); }
void DistributedCoordinator::bumpCacheGeneration() { cacheGen_.fetch_add(1); }
void DistributedCoordinator::bumpToolchainGeneration() { toolchainGen_.fetch_add(1); }
size_t DistributedCoordinator::workerCount() const { std::lock_guard<std::mutex> l(mtx_); return workers_.size(); }

Json DistributedCoordinator::stats() const {
    Json j = Json::object({});
    j.set("workers", Json::number(static_cast<double>(workerCount())));
    j.set("completed", Json::number(static_cast<double>(completed_.load())));
    j.set("published_count", Json::number(static_cast<double>(published_.size())));
    j.set("rejected_stale", Json::number(static_cast<double>(rejectedStale_.load())));
    j.set("dispatched", Json::number(static_cast<double>(dispatched_.load())));
    j.set("duplicate_suppressed", Json::number(static_cast<double>(duplicateSuppressed_.load())));
    j.set("epoch", Json::number(static_cast<double>(epoch_.load())));
    j.set("cache_gen", Json::number(static_cast<double>(cacheGen_.load())));
    j.set("toolchain_gen", Json::number(static_cast<double>(toolchainGen_.load())));
    {
        std::lock_guard<std::mutex> l(mtx_);
        Json wb = Json::object({});
        for (auto& [wid, caps] : workers_) { std::string b = caps.get("boot_id") && caps.get("boot_id")->isString() ? caps.get("boot_id")->asString() : ""; wb.set(std::to_string(wid), Json::str(b)); }
        j.set("worker_boots", std::move(wb));
    }
    return j;
}

ErrorCode DistributedCoordinator::checkAuthority(const ProtoEnvelope& env, bool forWorker) const {
    if (env.epoch != epoch_.load()) return ErrorCode::StaleEpoch;
    if (env.cacheGen != cacheGen_.load()) return ErrorCode::StaleGeneration;
    if (env.toolchainGen != toolchainGen_.load()) return ErrorCode::StaleGeneration;
    if (forWorker) {
        std::lock_guard<std::mutex> l(mtx_);
        auto it = workers_.find(env.workerId);
        if (it == workers_.end()) return ErrorCode::StaleWorkerBoot;
        const std::string regBoot = it->second.get("boot_id") && it->second.get("boot_id")->isString() ? it->second.get("boot_id")->asString() : "";
        if (regBoot != env.bootId.toHex()) return ErrorCode::StaleWorkerBoot;
    }
    return ErrorCode::Ok;
}

bool DistributedCoordinator::rejectStalePublication(const ProtoEnvelope& env, const std::string& /*context*/) {
    if (env.epoch != epoch_.load()) { rejectedStale_.fetch_add(1); return true; }
    if (env.cacheGen != cacheGen_.load() || env.toolchainGen != toolchainGen_.load()) { rejectedStale_.fetch_add(1); return true; }
    {
        std::lock_guard<std::mutex> l(mtx_);
        auto it = workers_.find(env.workerId);
        if (it == workers_.end() || !it->second.get("boot_id") || !it->second.get("boot_id")->isString() || it->second.get("boot_id")->asString() != env.bootId.toHex()) { rejectedStale_.fetch_add(1); return true; }
        // Stale / obsolete artifact generation: zero or non-monotonic.
        if (env.artifactGen == 0) { rejectedStale_.fetch_add(1); return true; }
        auto gi = artifactGen_.find(env.artifactId.toHex());
        if (gi != artifactGen_.end() && env.artifactGen <= gi->second) { rejectedStale_.fetch_add(1); return true; }
    }
    return false;
}

void DistributedCoordinator::handleWorker(FramedChannel& ch, const ProtoFrame& regFrame) {
    if (regFrame.payload.size() < kEnvSize) return;
    ProtoEnvelope env = readEnv(regFrame.payload.data(), regFrame.payload.size());
    if (checkAuthority(env, false) != ErrorCode::Ok) { rejectedStale_.fetch_add(1); return; }
    Json caps = decodeRegister(regFrame.payload);
    {
        std::lock_guard<std::mutex> l(mtx_);
        workers_[env.workerId] = caps;
        workerChannels_[env.workerId] = &ch;

    }
    for (;;) {
        auto fr = ch.recvFrame();
        if (!fr.ok()) { std::lock_guard<std::mutex> l(mtx_); workers_.erase(env.workerId); workerChannels_.erase(env.workerId); return; }
        ProtoFrame f = *fr;
        if (f.type == MsgType::Shutdown) break;
        if (f.type == MsgType::Heartbeat) { ProtoFrame hb; hb.type = MsgType::Heartbeat; ch.sendFrame(hb); continue; }
        if (f.type != MsgType::CompileResult) continue;
        if (f.payload.size() < kEnvSize) continue;
        ProtoEnvelope env2 = readEnv(f.payload.data(), f.payload.size());
        bool success = false;
        Json resultJson = decodeCompileResult(f.payload, success);
        if (!success) continue;
        std::string keyStr = env2.requestId.toHex();
        std::string pubKey = keyStr;
        { std::lock_guard<std::mutex> l(mtx_); auto ki = reqToKey_.find(keyStr); if (ki != reqToKey_.end()) pubKey = ki->second; }
        completed_.fetch_add(1);
        {
            std::lock_guard<std::mutex> l(mtx_);
            published_[pubKey] = resultJson;
            auto gi = artifactGen_.find(env2.artifactId.toHex());
            if (gi == artifactGen_.end() || env2.artifactGen > gi->second) artifactGen_[env2.artifactId.toHex()] = env2.artifactGen;
            auto pi = pending_.find(keyStr);
            if (pi != pending_.end() && pi->second) { pi->second->set_value(resultFromJson(resultJson)); pending_.erase(pi); }
            else duplicateSuppressed_.fetch_add(1);
        }
    }
}

void DistributedCoordinator::handleClient(FramedChannel& ch, const ProtoFrame& firstFrame) {
    handleClientFrame(ch, firstFrame);
    for (;;) {
        auto fr = ch.recvFrame();
        if (!fr.ok()) return;
        handleClientFrame(ch, *fr);
    }
}

void DistributedCoordinator::handleClientFrame(FramedChannel& ch, const ProtoFrame& f) {
    if (f.type == MsgType::Control) {
        auto kind = decodeControl(f.payload);
        Json rj = Json::object({});
        if (kind == ControlKind::RollEpoch) { rollEpoch(); rj.set("epoch", Json::number(static_cast<double>(epoch_.load()))); }
        else if (kind == ControlKind::SetCacheGen) { bumpCacheGeneration(); rj.set("cache_gen", Json::number(static_cast<double>(cacheGen_.load()))); }
        else if (kind == ControlKind::SetToolchainGen) { bumpToolchainGeneration(); rj.set("toolchain_gen", Json::number(static_cast<double>(toolchainGen_.load()))); }
        else if (kind == ControlKind::GetStats) { rj = stats(); }
        ProtoFrame rf; rf.type = MsgType::Control; rf.payload = encodeControlReply(rj.dump());
        ch.sendFrame(rf); return;
    }
    if (f.type == MsgType::Heartbeat) { ProtoFrame hb; hb.type = MsgType::Heartbeat; ch.sendFrame(hb); return; }
    if (f.type == MsgType::Shutdown) return;
    if (f.type != MsgType::Submit && f.type != MsgType::Invalidate) return;
    ProtoEnvelope env = readEnv(f.payload.data(), f.payload.size());
    if (f.type == MsgType::Invalidate) {
        std::lock_guard<std::mutex> l(mtx_); published_.erase(env.requestId.toHex());
        ProtoFrame ack; ack.type = MsgType::Reject; ack.payload = encodeReject(env, ErrorCode::Ok, "invalidated"); ch.sendFrame(ack);
        return;
    }
    auto dr = decodeSubmit(f.payload);
    if (!dr.ok()) { ProtoFrame rej; rej.type = MsgType::Reject; ProtoEnvelope e; rej.payload = encodeReject(e, dr.code(), dr.message()); ch.sendFrame(rej); return; }
    CompilationRequest req = *dr;
    CompilationPlan plan; bool havePlan = false;
    if (auto p = fabric_->plan(req); p.ok()) { plan = *p; havePlan = true; }
    (void)havePlan;
    KeyToolchainContext tc; tc.compiler="cf-cpu"; tc.backend="cpu"; tc.frontend="cf-frontend";
    CompilationKey key = buildCompilationKey(req, plan, tc);
    std::string keyHexStr = key.toHex();
    { std::lock_guard<std::mutex> l(mtx_); reqToKey_[req.requestId.toHex()] = keyHexStr; }
    {
        std::lock_guard<std::mutex> l(mtx_);
        auto hit = published_.find(keyHexStr);
        if (hit != published_.end()) {
            CompilationResult cached = resultFromJson(hit->second);
            cached.reused = true;
            ProtoEnvelope rEnv; rEnv.requestId = req.requestId;
            ProtoFrame rf; rf.type = MsgType::Result; rf.payload = encodeResult(rEnv, cached);
            ch.sendFrame(rf); return;
        }
    }
    WorkerId selected = 0; bool haveWorker = false;
    bool wantCuda = (plan.backend == "cuda-nvrtc");
    {
        std::lock_guard<std::mutex> l(mtx_);
        for (auto& [wid, caps] : workers_) {
            std::string bk = caps.get("backend") && caps.get("backend")->isString() ? caps.get("backend")->asString() : "cpu";
            if (wantCuda) { if (bk == "cuda-nvrtc") { selected = wid; haveWorker = true; break; } }
            else { if (bk == "cpu") { selected = wid; haveWorker = true; break; } }
        }
    }
    if (!haveWorker) { ProtoEnvelope e; e.requestId = req.requestId; ProtoFrame rej; rej.type = MsgType::Reject; rej.payload = encodeReject(e, ErrorCode::NoBackend, "no capable worker"); ch.sendFrame(rej); return; }
    auto promise = std::make_shared<std::promise<CompilationResult>>();
    std::string reqKey = req.requestId.toHex();
    { std::lock_guard<std::mutex> l(mtx_); pending_[reqKey] = promise; }
    ProtoEnvelope env2; env2.epoch = epoch_.load(); env2.workerId = selected; env2.cacheGen = cacheGen_.load(); env2.toolchainGen = toolchainGen_.load();
    env2.requestId = req.requestId; env2.attemptId = CompilationAttemptId::fromU64(dispatched_.fetch_add(1) + 1);
    FramedChannel* wch = nullptr;
    { std::lock_guard<std::mutex> l(mtx_); auto wi = workerChannels_.find(selected); if (wi != workerChannels_.end()) wch = wi->second; }
    if (!wch) { std::lock_guard<std::mutex> l(mtx_); pending_.erase(reqKey); ProtoFrame rej; rej.type = MsgType::Reject; rej.payload = encodeReject(env2, ErrorCode::NoBackend, "worker gone"); ch.sendFrame(rej); return; }
    ProtoFrame disp; disp.type = MsgType::DispatchCompile; disp.payload = encodeDispatch(env2, req, key);
    if (!wch->sendFrame(disp).ok()) { std::lock_guard<std::mutex> l(mtx_); pending_.erase(reqKey); ProtoFrame rej; rej.type = MsgType::Reject; rej.payload = encodeReject(env2, ErrorCode::IOError, "dispatch failed"); ch.sendFrame(rej); return; }
    auto fut = promise->get_future();
    CompilationResult res = fut.get();
    ProtoEnvelope rEnv; rEnv.requestId = req.requestId;
    ProtoFrame rf; rf.type = (res.error == ErrorCode::Ok) ? MsgType::Result : MsgType::Reject;
    if (res.error == ErrorCode::Ok) rf.payload = encodeResult(rEnv, res);
    else rf.payload = encodeReject(rEnv, res.error, res.errorMessage);
    ch.sendFrame(rf);
}

void DistributedCoordinator::handleConnection(TcpSocket sock) {
    FramedChannel ch(std::move(sock));
    auto fr = ch.recvFrame();
    if (!fr.ok()) return;
    if (!fr.ok()) return;
    ProtoFrame first = *fr;
    if (first.type == MsgType::Register) handleWorker(ch, first);
    else if (first.type == MsgType::Submit) handleClient(ch, first);
}

Result<void> DistributedCoordinator::run() {
    initSockets();
    TcpSocket listener;
    auto b = listener.bindListen(port_);
    if (!b.ok()) return ErrVoid(b.code(), "coordinator bind failed: " + b.message());
    std::vector<std::thread> threads;
    (void)threads;
    for (;;) {
        auto acc = listener.accept();
        if (!acc.ok()) { std::this_thread::sleep_for(std::chrono::milliseconds(20)); continue; }
        std::thread t([this, s = std::move(acc.value()) ]() mutable { handleConnection(std::move(s)); });
        t.detach();
    }
}

// ---------------------------------------------------------------------------
// Client
// ---------------------------------------------------------------------------
DistributedClient::DistributedClient(std::string host, uint16_t port) : host_(std::move(host)), port_(port) {}
Result<void> DistributedClient::connect() {
    initSockets();
    auto r = sock_.connectTo(host_, port_);
    if (!r.ok()) return ErrVoid(r.code(), r.message());
    connected_ = true;
    return OkVoid();
}
void DistributedClient::close() { if (connected_) sock_.close(); connected_ = false; }

Result<CompilationResult> DistributedClient::roundTrip(const ProtoFrame& send) {
    FramedChannel ch(std::move(sock_));
    if (!ch.sendFrame(send).ok()) return Err<CompilationResult>(ErrorCode::IOError, "send failed");
    auto fr = ch.recvFrame();
    if (!fr.ok()) { sock_ = std::move(ch.socket()); return Err<CompilationResult>(fr.code(), fr.message()); }
    ProtoFrame resp = *fr;
    if (resp.type == MsgType::Reject) {
        ErrorCode code = decodeRejectCode(resp.payload);
        sock_ = std::move(ch.socket());
        return Err<CompilationResult>(code, "coordinator rejected");
    }
    auto res = decodeResult(resp.payload);
    sock_ = std::move(ch.socket());
    if (!res.ok()) return Err<CompilationResult>(res.code(), res.message());
    return res;
}

Result<CompilationResult> DistributedClient::submit(const CompilationRequest& req) {
    seq_ += 1;
    ProtoEnvelope env; env.requestId = req.requestId; env.attemptId = CompilationAttemptId::fromU64(seq_);
    ProtoFrame f; f.type = MsgType::Submit; f.seq = seq_; f.payload = encodeSubmit(env, req);
    return roundTrip(f);
}
Result<Json> DistributedClient::control(ControlKind kind) {
    seq_ += 1;
    ProtoFrame f; f.type = MsgType::Control; f.seq = seq_; f.payload = encodeControl(kind);
    FramedChannel ch(std::move(sock_));
    if (!ch.sendFrame(f).ok()) return Err<Json>(ErrorCode::IOError, "send failed");
    auto fr = ch.recvFrame();
    sock_ = std::move(ch.socket());
    if (!fr.ok()) return Err<Json>(ErrorCode::IOError, fr.message());
    if (fr->type != MsgType::Control) return Err<Json>(ErrorCode::Internal, "bad control response");
    auto s = decodeControlReply(fr->payload);
    if (!s.ok()) return Err<Json>(s.code(), s.message());
    auto j = Json::parse(*s);
    if (!j) return Err<Json>(ErrorCode::Internal, "control response parse failed");
    return Ok(*j);
}

Result<void> DistributedClient::invalidate(const CompilationRequestId& id) {
    seq_ += 1;
    ProtoEnvelope env; env.requestId = id;
    ProtoFrame f; f.type = MsgType::Invalidate; f.seq = seq_; f.payload = encodeReject(env, ErrorCode::Ok, "invalidate");
    FramedChannel ch(std::move(sock_));
    if (!ch.sendFrame(f).ok()) return ErrVoid(ErrorCode::IOError, "send failed");
    auto fr = ch.recvFrame();
    sock_ = std::move(ch.socket());
    if (!fr.ok()) return ErrVoid(ErrorCode::IOError, fr.message());
    return OkVoid();
}

} // namespace compilationfabric