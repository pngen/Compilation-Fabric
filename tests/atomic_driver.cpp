// Compilation Fabric - atomic closure DRIVER (two-phase).
// Phase 1: real compile/reuse, record coordinator epoch + worker-1 boot.
// Phase 2: (wrapper killed+restarted worker-1 as a NEW OS process) verify the
//   WorkerBootId changed, roll the coordinator epoch, replay stale authority
//   over real framed TCP, and prove fresh success + a second exact hit.
// Usage: atomic_driver <phase:1|2> <port> <statefile>
#include "CompilationFabric/CompilationFabric.hpp"
#include "CompilationFabric/Distributed.hpp"
#include "CompilationFabric/Protocol.hpp"
#include "TestUtil.hpp"
#include <cstdio>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <string>
#include <fstream>
#include <sstream>
using namespace compilationfabric;

static CompilationRequest mkReq(const std::string& src, uint64_t id) {
    CompilationRequest r; r.requestId = CompilationRequestId::fromU64(id); r.logicalOperation = LogicalOperation::fromU64(1);
    r.source = src; r.sourceDigest = Sha256::hash(src); r.sourceLanguage = "cf-src";
    r.datatype = Datatype::F32; r.rank = 1; r.staticShape = {256}; r.targetArchitecture = "host-x86_64"; r.backend = "cpu";
    return r;
}
static Json pollStats(const std::string& host, uint16_t port, int tries) {
    for (int i=0;i<tries;++i) { DistributedClient c(host,port); if (c.connect().ok()) { auto s=c.control(ControlKind::GetStats); if (s.ok()) return *s; } std::this_thread::sleep_for(std::chrono::milliseconds(60)); }
    return Json::object({});
}
static std::string bootOf(const Json& st, const std::string& id) {
    if (!st.get("worker_boots")) return ""; auto* wb = st.get("worker_boots")->asObjectPtr(); if (!wb) return ""; auto it = wb->find(id); return it==wb->end()?std::string():it->second.asString();
}
static void writeState(const std::string& file, const Json& j) { std::ofstream o(file); o << j.dump(); }
static Json readState(const std::string& file) { std::ifstream f(file); std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()); auto j = Json::parse(s); return j ? *j : Json::object({}); }
static void sendStale(uint16_t port, CoordinatorEpoch curEpoch, CacheGeneration curGen, ToolchainGeneration curTg, CoordinatorEpoch oldEpoch, uint64_t oldBootLo) {
    initSockets(); TcpSocket s; if (!s.connectTo("127.0.0.1", port).ok()) return; FramedChannel ch(std::move(s));
    WorkerBootId myBoot = WorkerBootId::fromU64(0xAAAA000000000005ULL);
    ProtoEnvelope reg; reg.epoch=curEpoch; reg.workerId=999; reg.bootId=myBoot; reg.cacheGen=curGen; reg.toolchainGen=curTg;
    ProtoFrame rf; rf.type=MsgType::Register; rf.payload=encodeRegister(reg, Json::object({{"boot_id", Json::str(myBoot.toHex())}})); ch.sendFrame(rf);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    auto one=[&](const ProtoEnvelope& e){ Json j=Json::object({}); j.set("request_id", Json::str(e.requestId.toHex())); ProtoFrame fr; fr.type=MsgType::CompileResult; fr.payload=encodeCompileResult(e,true,j); ch.sendFrame(fr); };
    { ProtoEnvelope e; e.epoch=oldEpoch; e.workerId=999; e.bootId=myBoot; e.cacheGen=curGen; e.toolchainGen=curTg; e.requestId=CompilationRequestId::fromU64(91); e.attemptId=CompilationAttemptId::fromU64(91); e.artifactId=ArtifactId::fromU64(91); e.artifactGen=1; one(e); }
    { ProtoEnvelope e; e.epoch=curEpoch; e.workerId=999; e.bootId=WorkerBootId::fromU64(oldBootLo); e.cacheGen=curGen; e.toolchainGen=curTg; e.requestId=CompilationRequestId::fromU64(92); e.attemptId=CompilationAttemptId::fromU64(92); e.artifactId=ArtifactId::fromU64(92); e.artifactGen=1; one(e); }
    { ProtoEnvelope e; e.epoch=curEpoch; e.workerId=999; e.bootId=myBoot; e.cacheGen=9999; e.toolchainGen=9999; e.requestId=CompilationRequestId::fromU64(93); e.attemptId=CompilationAttemptId::fromU64(93); e.artifactId=ArtifactId::fromU64(93); e.artifactGen=1; one(e); }
    { ProtoEnvelope e; e.epoch=curEpoch; e.workerId=999; e.bootId=myBoot; e.cacheGen=curGen; e.toolchainGen=curTg; e.requestId=CompilationRequestId::fromU64(94); e.attemptId=CompilationAttemptId::fromU64(94); e.artifactId=ArtifactId::fromU64(94); e.artifactGen=0; one(e); }
    { ProtoEnvelope e; e.epoch=curEpoch; e.workerId=999; e.bootId=myBoot; e.cacheGen=curGen; e.toolchainGen=curTg; e.requestId=CompilationRequestId::fromU64(95); e.attemptId=CompilationAttemptId::fromU64(95); e.artifactId=ArtifactId::fromU64(95); e.artifactGen=3; one(e); }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}
int main(int argc, char** argv) {
    if (argc < 4) { std::printf("usage: atomic_driver <1|2> <port> <state>\n"); return 2; }
    int phase = std::atoi(argv[1]); uint16_t port = static_cast<uint16_t>(std::atoi(argv[2])); std::string state = argv[3];
    Json init = pollStats("127.0.0.1", port, 300);
    if (!init.get("workers") || init.get("workers")->asNumber() < 2.0) { std::printf("atomic: workers < 2\n"); return 1; }
    DistributedClient client("127.0.0.1", port); if (!client.connect().ok()) { std::printf("atomic: connect failed\n"); return 1; }
    auto req = mkReq("name=atomic\nshape=256\nadd scalar=2.0\n", 5000);
    if (phase == 1) {
        auto r1 = client.submit(req); CF_BEGIN("atomic-phase1"); CF_CHECK(r1.ok()); if(!r1.ok()){ std::printf("submit1 failed\n"); return 1; }
        CF_CHECK_EQ(r1->reused, false); CF_CHECK_EQ(r1->validated, true);
        auto r2 = client.submit(req); CF_CHECK(r2.ok()); CF_CHECK_EQ(r2->reused, true);
        auto st = client.control(ControlKind::GetStats); CF_CHECK(st.ok());
        Json s = Json::object({}); s.set("epoch", Json::number(st->get("epoch")?st->get("epoch")->asNumber():1.0));
        s.set("boot1", Json::str(bootOf(*st, "1"))); s.set("gen", Json::number(st->get("cache_gen")?st->get("cache_gen")->asNumber():1.0));
        writeState(state, s);
        std::printf("phase1: compile+reuse ok, recorded epoch/boot\n"); CF_FINISH("atomic_phase1");
    } else {
        Json s = readState(state);
        double preEpoch = s.get("epoch")?s.get("epoch")->asNumber():1.0; std::string bootBefore = s.get("boot1")?s.get("boot1")->asString():"";
        auto st0 = client.control(ControlKind::GetStats); CF_CHECK(st0.ok());
        std::string bootAfter = bootOf(*st0, "1");
        CF_BEGIN("atomic-phase2");
        CF_CHECK(!bootBefore.empty()); CF_CHECK(!bootAfter.empty()); CF_CHECK(bootBefore != bootAfter); // new WorkerBootId
        auto roll = client.control(ControlKind::RollEpoch); CF_CHECK(roll.ok());
        auto st1 = client.control(ControlKind::GetStats); CF_CHECK(st1.ok());
        double postEpoch = st1->get("epoch")?st1->get("epoch")->asNumber():-1; CF_CHECK(postEpoch == preEpoch + 1);
        double pubBefore = st1->get("published_count")?st1->get("published_count")->asNumber():0; double curGen = st1->get("cache_gen")?st1->get("cache_gen")->asNumber():1.0;
        sendStale(port, static_cast<CoordinatorEpoch>(postEpoch), static_cast<CacheGeneration>(curGen), 1, static_cast<CoordinatorEpoch>(preEpoch), 0xAAA0000000000001ULL);
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        auto st2 = client.control(ControlKind::GetStats); CF_CHECK(st2.ok());
        double rejected = st2->get("rejected_stale")?st2->get("rejected_stale")->asNumber():-1; CF_CHECK(rejected >= 5);
        double pubAfter = st2->get("published_count")?st2->get("published_count")->asNumber():0; CF_CHECK(pubAfter == pubBefore);
        auto f1 = client.submit(req); CF_CHECK(f1.ok()); CF_CHECK_EQ(f1->validated, true);
        auto f2 = client.submit(req); CF_CHECK(f2.ok()); CF_CHECK_EQ(f2->reused, true);
        auto stE = client.control(ControlKind::GetStats); CF_CHECK(stE.ok());
        double pend = stE->get("pending_count")?stE->get("pending_count")->asNumber():-1; CF_CHECK(pend == 0);
        std::printf("phase2: bootChanged=1 rejected=%d pubSame=%d fresh=%d hit=%d\n", (int)rejected, (int)(pubAfter==pubBefore), (int)f1.ok(), (int)f2->reused);
        CF_FINISH("atomic_phase2");
    }
}