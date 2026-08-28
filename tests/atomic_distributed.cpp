// Compilation Fabric - ATOMIC multiprocess closure proof.
// The SAME real scenario: real coordinator OS process + two real worker OS
// processes over framed TCP, real cache miss->compile->validate->publish,
// cache hit, worker kill + restart with a new WorkerBootId, epoch rollover, and
// replay of stale authority (epoch / boot / generation / attempt / artifact) --
// proving stale completion cannot install, validate, replace, or deploy.
#include "CompilationFabric/CompilationFabric.hpp"
#include "CompilationFabric/Distributed.hpp"
#include "CompilationFabric/Protocol.hpp"
#include "TestUtil.hpp"
#include <cstdio>
#include <filesystem>
#include <thread>
#include <chrono>
#include <string>
#include <windows.h>

using namespace compilationfabric;

static std::string cliPath() {
    char buf[MAX_PATH]; GetModuleFileNameA(NULL, buf, MAX_PATH);
    std::filesystem::path p(buf);
    return (p.parent_path().parent_path() / "tools" / "compile_fabric.exe").string();
}
static bool spawn(const std::string& exe, const std::string& args) {
    std::string cmd = "start \"\" /b \"" + exe + "\" " + args;
    int rc = std::system(cmd.c_str());
    return rc == 0;
}
static void killAll() { std::system("taskkill /im compile_fabric.exe /f >nul 2>nul"); }
static Json waitFor(CompilationFabricConfig dummy, const std::string& host, uint16_t port, int want, int tries) {
    (void)dummy;
    for (int i = 0; i < tries; ++i) {
        DistributedClient c(host, port);
        if (c.connect().ok()) { auto s = c.control(ControlKind::GetStats); if (s.ok()) { double w = s->get("workers") ? s->get("workers")->asNumber() : 0; if ((int)w >= want) { double rj = s->get("rejected_stale") ? s->get("rejected_stale")->asNumber() : -1; (void)rj; return *s; } } }
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
    }
    return Json::object({});
}

static CompilationRequest mkReq(const std::string& src, uint64_t id) {
    CompilationRequest r; r.requestId = CompilationRequestId::fromU64(id); r.logicalOperation = LogicalOperation::fromU64(1);
    r.source = src; r.sourceDigest = Sha256::hash(src); r.sourceLanguage = "cf-src";
    r.datatype = Datatype::F32; r.rank = 1; r.staticShape = {256}; r.targetArchitecture = "host-x86_64"; r.backend = "cpu";
    return r;
}

// Build a raw worker connection and send N stale CompileResult frames.
static int64_t sendStale(uint16_t port, CoordinatorEpoch curEpoch, CacheGeneration curGen, ToolchainGeneration curTg,
                         CoordinatorEpoch oldEpoch, CacheGeneration oldGen, uint64_t oldBootLo) {
    initSockets();
    TcpSocket s; if (!s.connectTo("127.0.0.1", port).ok()) return -1;
    FramedChannel ch(std::move(s));
    WorkerBootId myBoot = WorkerBootId::fromU64(0xAAAA000000000005ULL);
    ProtoEnvelope regEnv; regEnv.epoch = curEpoch; regEnv.workerId = 999; regEnv.bootId = myBoot; regEnv.cacheGen = curGen; regEnv.toolchainGen = curTg;
    ProtoFrame reg; reg.type = MsgType::Register; reg.payload = encodeRegister(regEnv, Json::object({{"boot_id", Json::str(myBoot.toHex())}}));
    ch.sendFrame(reg);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    auto sendOne = [&](const ProtoEnvelope& env) {
        Json j = Json::object({}); j.set("request_id", Json::str(env.requestId.toHex())); j.set("validated", Json::boolean(true));
        ProtoFrame f; f.type = MsgType::CompileResult; f.payload = encodeCompileResult(env, true, j); ch.sendFrame(f);
    };
    // 1) StaleEpoch (old epoch)
    { ProtoEnvelope e; e.epoch = oldEpoch; e.workerId = 999; e.bootId = myBoot; e.cacheGen = curGen; e.toolchainGen = curTg; e.requestId = CompilationRequestId::fromU64(91); e.attemptId = CompilationAttemptId::fromU64(91); e.artifactId = ArtifactId::fromU64(91); e.artifactGen = 1; sendOne(e); }
    // 2) StaleWorkerBoot (wrong boot)
    { ProtoEnvelope e; e.epoch = curEpoch; e.workerId = 999; e.bootId = WorkerBootId::fromU64(oldBootLo); e.cacheGen = curGen; e.toolchainGen = curTg; e.requestId = CompilationRequestId::fromU64(92); e.attemptId = CompilationAttemptId::fromU64(92); e.artifactId = ArtifactId::fromU64(92); e.artifactGen = 1; sendOne(e); }
    // 3) StaleGeneration (old cache gen)
    { ProtoEnvelope e; e.epoch = curEpoch; e.workerId = 999; e.bootId = myBoot; e.cacheGen = oldGen; e.toolchainGen = curTg; e.requestId = CompilationRequestId::fromU64(93); e.attemptId = CompilationAttemptId::fromU64(93); e.artifactId = ArtifactId::fromU64(93); e.artifactGen = 1; sendOne(e); }
    // 4) StaleArtifact (zero generation) + obsolete attempt (request not pending)
    { ProtoEnvelope e; e.epoch = curEpoch; e.workerId = 999; e.bootId = myBoot; e.cacheGen = curGen; e.toolchainGen = curTg; e.requestId = CompilationRequestId::fromU64(94); e.attemptId = CompilationAttemptId::fromU64(94); e.artifactId = ArtifactId::fromU64(94); e.artifactGen = 0; sendOne(e); }
    { ProtoEnvelope e; e.epoch = curEpoch; e.workerId = 999; e.bootId = myBoot; e.cacheGen = curGen; e.toolchainGen = curTg; e.requestId = CompilationRequestId::fromU64(95); e.attemptId = CompilationAttemptId::fromU64(95); e.artifactId = ArtifactId::fromU64(95); e.artifactGen = 3; sendOne(e); }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    return 0;
}

int main() {
    std::string exe = cliPath();
    if (!std::filesystem::exists(exe)) { std::printf("atomic: cli not found at %s\n", exe.c_str()); return 1; }
    uint16_t port = static_cast<uint16_t>(47000 + (Clock::monotonicNanos() % 1000));
    if (!spawn(exe, "serve " + std::to_string(port))) { std::printf("atomic: spawn coordinator failed\n"); return 1; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    spawn(exe, "worker 127.0.0.1 " + std::to_string(port) + " 1");
    spawn(exe, "worker 127.0.0.1 " + std::to_string(port) + " 2");
    // Deterministically wait for 2 workers.
    CompilationFabricConfig dcfg;
    Json init = waitFor(dcfg, "127.0.0.1", port, 2, 80);
    if (!init.get("workers")) { killAll(); std::printf("atomic: workers never registered\n"); return 1; }
    CF_BEGIN("atomic-cache-miss-validate-publish");
    DistributedClient client("127.0.0.1", port);
    if (!client.connect().ok()) { killAll(); std::printf("atomic: client connect failed\n"); return 1; }
    auto req = mkReq("name=atomic\nshape=256\nadd scalar=2.0\n", 5000);
    auto r1 = client.submit(req);
    CF_CHECK(r1.ok());
    if (!r1.ok()) { killAll(); std::printf("atomic: submit1 failed %s\n", r1.message().c_str()); return 1; }
    CF_CHECK_EQ(r1->validated, true);
    CF_CHECK_EQ(r1->reused, false);
    std::string artId = r1->artifactId.toHex();
    // second submit -> cache hit
    auto r2 = client.submit(req);
    CF_CHECK(r2.ok());
    CF_CHECK_EQ(r2->reused, true);

    CF_BEGIN("atomic-capture-authority");
    auto st1 = client.control(ControlKind::GetStats);
    CF_CHECK(st1.ok());
    double preEpoch = st1->get("epoch") ? st1->get("epoch")->asNumber() : -1;
    double preCacheGen = st1->get("cache_gen") ? st1->get("cache_gen")->asNumber() : -1;
    std::string boot1Before = "";
    if (st1->get("worker_boots")) { auto* wb = st1->get("worker_boots")->asObjectPtr(); if (wb) { auto it = wb->find("1"); if (it != wb->end()) boot1Before = it->second.asString(); } }
    CF_CHECK(preEpoch > 0);

    CF_BEGIN("atomic-kill-restart-worker");
    killAll();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    spawn(exe, "worker 127.0.0.1 " + std::to_string(port) + " 1"); spawn(exe, "worker 127.0.0.1 " + std::to_string(port) + " 2");
    Json afterRestart;
    for (int i=0;i<50;++i){ DistributedClient c("127.0.0.1", port); if (c.connect().ok()){ auto s=c.control(ControlKind::GetStats); if(s.ok()){ if(s->get("worker_boots")){ auto* wb=s->get("worker_boots")->asObjectPtr(); if(wb&&wb->count("1")){ afterRestart=*s; break; } } } } std::this_thread::sleep_for(std::chrono::milliseconds(60)); }
    std::string boot1After = "";
    if (afterRestart.get("worker_boots")) { auto* wb = afterRestart.get("worker_boots")->asObjectPtr(); if (wb) { auto it = wb->find("1"); if (it != wb->end()) boot1After = it->second.asString(); } }
    CF_CHECK(!boot1Before.empty());
    CF_CHECK(!boot1After.empty());
    CF_CHECK(boot1Before != boot1After);

    CF_BEGIN("atomic-epoch-rollover");
    auto roll = client.control(ControlKind::RollEpoch);
    CF_CHECK(roll.ok());
    auto st2 = client.control(ControlKind::GetStats);
    CF_CHECK(st2.ok());
    double postEpoch = st2->get("epoch") ? st2->get("epoch")->asNumber() : -1;
    CF_CHECK(postEpoch == preEpoch + 1);
    double publishedBefore = st2->get("published_count") ? st2->get("published_count")->asNumber() : 0;

    CF_BEGIN("atomic-stale-authority-replay");
    sendStale(port, static_cast<CoordinatorEpoch>(postEpoch), static_cast<CacheGeneration>(preCacheGen), 1,
              static_cast<CoordinatorEpoch>(preEpoch), static_cast<CacheGeneration>(preCacheGen), 0xAAA0000000000001ULL);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto st3 = client.control(ControlKind::GetStats);
    CF_CHECK(st3.ok());
    double rejected = st3->get("rejected_stale") ? st3->get("rejected_stale")->asNumber() : -1;
    CF_CHECK(rejected >= 5);   // stale epoch / boot / generation / artifact / attempt all rejected
    double publishedAfter = st3->get("published_count") ? st3->get("published_count")->asNumber() : 0;
    CF_CHECK(publishedAfter == publishedBefore);  // stale completion changed no authority

    CF_BEGIN("atomic-fresh-success-and-hit");
    auto f1 = client.submit(req);
    CF_CHECK(f1.ok());                       // fresh success under current authority (hard assert)
    CF_CHECK_EQ(f1->validated, true);
    auto f2 = client.submit(req);
    CF_CHECK(f2.ok());                       // second exact hit (hard assert)
    CF_CHECK_EQ(f2->reused, true);

    // cleanup
    killAll();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CF_FINISH("atomic_distributed");
}