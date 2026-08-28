// Compilation Fabric - Distributed compilation control plane.
//
// A coordinator accepts real framed-TCP connections from compile workers and a
// client/driver. Workers advertise capabilities (backend, target, toolchain
// versions, CUDA capability). The coordinator selects workers by explicit
// capability, enforces single-flight deduplication, validates authority on every
// message (epoch / worker boot / cache & toolchain generation / attempt /
// artifact generation), and rejects stale authority with a structured reason.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#pragma once
#include "CompilationFabric/Core.hpp"
#include "CompilationFabric/Protocol.hpp"
#include "CompilationFabric/Fabric.hpp"
#include "CompilationFabric/Json.hpp"
#include "CompilationFabric/Request.hpp"
#include <vector>
#include <string>
#include <atomic>
#include <future>
#include <map>
#include <mutex>
#include <memory>
#include <condition_variable>


namespace compilationfabric {

class DistributedWorker {
public:
    DistributedWorker(std::string host, uint16_t port, WorkerId id, bool preferCuda = false);
    int run();
private:
    std::string host_;
    uint16_t port_;
    WorkerId id_;
    bool preferCuda_;
};

class DistributedCoordinator {
public:
    DistributedCoordinator(uint16_t port, std::string artifactRoot,
                           CoordinatorEpoch epoch = 1, CacheGeneration cacheGen = 1,
                           ToolchainGeneration toolchainGen = 1);
    ~DistributedCoordinator();

    Result<void> run();
    void rollEpoch();
    void bumpCacheGeneration();
    void bumpToolchainGeneration();
    CoordinatorEpoch epoch() const { return epoch_.load(); }
    CacheGeneration cacheGen() const { return cacheGen_.load(); }
    ToolchainGeneration toolchainGen() const { return toolchainGen_.load(); }
    size_t workerCount() const;

    Json stats() const;

private:
    void handleConnection(TcpSocket sock);
    void handleWorker(FramedChannel& ch, const ProtoFrame& regFrame);
    void handleClient(FramedChannel& ch, const ProtoFrame& firstFrame);
    void handleClientFrame(FramedChannel& ch, const ProtoFrame& f);
    ErrorCode checkAuthority(const ProtoEnvelope& env, bool forWorker) const;
    bool rejectStalePublication(const ProtoEnvelope& env, const std::string& context);
    void dispatchToWorker(CompilationResult& out, const CompilationRequest& req, const CompilationKey& key);

    uint16_t port_;
    std::string root_;
    std::atomic<CoordinatorEpoch> epoch_;
    std::atomic<CacheGeneration> cacheGen_;
    std::atomic<ToolchainGeneration> toolchainGen_;
    mutable std::mutex mtx_;
    std::map<WorkerId, Json> workers_;
    std::map<WorkerId, FramedChannel*> workerChannels_;
    std::map<std::string, CompilationAttemptId> inflight_;
    std::map<std::string, std::shared_ptr<std::promise<CompilationResult>>> pending_;
    std::map<std::string, std::string> reqToKey_;
    std::map<std::string, Json> published_;
    std::map<std::string, ArtifactGeneration> artifactGen_;
    std::unique_ptr<CompilationFabric> fabric_;
    std::atomic<uint64_t> completed_{0};
    std::atomic<uint64_t> rejectedStale_{0};
    std::atomic<uint64_t> dispatched_{0};
    std::atomic<uint64_t> duplicateSuppressed_{0};
};

class DistributedClient {
public:
    DistributedClient(std::string host, uint16_t port);
    Result<void> connect();
    Result<CompilationResult> submit(const CompilationRequest& req);
    Result<void> invalidate(const CompilationRequestId& id);
    Result<Json> control(ControlKind kind);
    void close();
private:
    std::string host_; uint16_t port_;
    TcpSocket sock_;
    bool connected_ = false;
    uint32_t seq_ = 0;
    Result<CompilationResult> roundTrip(const ProtoFrame& send);
};

} // namespace compilationfabric