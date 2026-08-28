// Compilation Fabric - atomic closure DRIVER.
// Connects to a running coordinator + workers (launched by a wrapper that keeps
// real OS processes alive), performs real compile/reuse, rolls the coordinator
// epoch, replays stale authority over real framed TCP, and asserts rejections,
// then proves fresh success and a second exact hit. Usage: atomic_driver <port>.
#include "CompilationFabric/CompilationFabric.hpp"
#include "CompilationFabric/Distributed.hpp"
#include "CompilationFabric/Protocol.hpp"
#include "TestUtil.hpp"
#include <cstdio>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <string>
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
static void sendStale(uint16_t port, CoordinatorEpoch curEpoch, CacheGeneration curGen, ToolchainGeneration curTg, CoordinatorEpoch oldEpoch, CacheGeneration oldGen, uint64_t oldBootLo) {
    (void)oldGen; (void)oldEpoch;
    initSockets(); TcpSocket s; if (!s.connectTo("127.0.0.1", port).ok()) return;
    FramedChannel ch(std::move(s));
    WorkerBootId myBoot = WorkerBootId::fromU64(0xAAAA000000000005ULL);
    ProtoEnvelope regEnv; regEnv.epoch = curEpoch; regEnv.workerId = 999; regEnv.bootId = myBoot; regEnv.cacheGen = curGen; regEnv.toolchainGen = curTg;
    ProtoFrame reg; reg.type = MsgType::Register; reg.payload = encodeRegister(regEnv, Json::object({{"boot_id", Json::str(myBoot.toHex())}}));
    ch.sendFrame(reg); std::this_thread::sleep_for(std::chrono::milliseconds(150));
    auto one = [&](const ProtoEnvelope& e){ Json j=Json::object({}); j.set("request_id", Json::str(e.requestId.toHex())); ProtoFrame f; f.type=MsgType::CompileResult; f.payload=encodeCompileResult(e,true,j); ch.sendFrame(f); };
    { ProtoEnvelope e; e.epoch=oldEpoch; e.workerId=999; e.bootId=myBoot; e.cacheGen=curGen; e.toolchainGen=curTg; e.requestId=CompilationRequestId::fromU64(91); e.attemptId=CompilationAttemptId::fromU64(91); e.artifactId=ArtifactId::fromU64(91); e.artifactGen=1; one(e); }
    { ProtoEnvelope e; e.epoch=curEpoch; e.workerId=999; e.bootId=WorkerBootId::fromU64(oldBootLo); e.cacheGen=curGen; e.toolchainGen=curTg; e.requestId=CompilationRequestId::fromU64(92); e.attemptId=CompilationAttemptId::fromU64(92); e.artifactId=ArtifactId::fromU64(92); e.artifactGen=1; one(e); }
    { ProtoEnvelope e; e.epoch=curEpoch; e.workerId=999; e.bootId=myBoot; e.cacheGen=9999; e.toolchainGen=9999; e.requestId=CompilationRequestId::fromU64(93); e.attemptId=CompilationAttemptId::fromU64(93); e.artifactId=ArtifactId::fromU64(93); e.artifactGen=1; one(e); }
    { ProtoEnvelope e; e.epoch=curEpoch; e.workerId=999; e.bootId=myBoot; e.cacheGen=curGen; e.toolchainGen=curTg; e.requestId=CompilationRequestId::fromU64(94); e.attemptId=CompilationAttemptId::fromU64(94); e.artifactId=ArtifactId::fromU64(94); e.artifactGen=0; one(e); }
    { ProtoEnvelope e; e.epoch=curEpoch; e.workerId=999; e.bootId=myBoot; e.cacheGen=curGen; e.toolchainGen=curTg; e.requestId=CompilationRequestId::fromU64(95); e.attemptId=CompilationAttemptId::fromU64(95); e.artifactId=ArtifactId::fromU64(95); e.artifactGen=3; one(e); }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}
int main(int argc, char** argv) {
    if (argc < 2) { std::printf("usage: atomic_driver <port>\n"); return 2; }
    uint16_t port = static_cast<uint16_t>(std::atoi(argv[1]));
    Json init = pollStats("127.0.0.1", port, 90);
    if (!init.get("workers") || init.get("workers")->asNumber() < 2.0) { std::printf("atomic: workers never >=2\n"); return 1; }
    DistributedClient client("127.0.0.1", port);
    if (!client.connect().ok()) { std::printf("atomic: connect failed\n"); return 1; }
    auto req = mkReq("name=atomic\nshape=256\nadd scalar=2.0\n", 5000);
    auto r1 = client.submit(req);
    CF_BEGIN("atomic-closure"); CF_CHECK(r1.ok()); if (!r1.ok()) { std::printf("submit1 failed: %s\n", r1.message().c_str()); return 1; }
    CF_CHECK_EQ(r1->reused, false); CF_CHECK_EQ(r1->validated, true);
    auto r2 = client.submit(req); CF_CHECK(r2.ok()); CF_CHECK_EQ(r2->reused, true);
    auto st1 = client.control(ControlKind::GetStats); CF_CHECK(st1.ok());
    double preEpoch = st1->get("epoch")?st1->get("epoch")->asNumber():-1; double preGen = st1->get("cache_gen")?st1->get("cache_gen")->asNumber():-1;
    auto roll = client.control(ControlKind::RollEpoch); CF_CHECK(roll.ok());
    auto st2 = client.control(ControlKind::GetStats); CF_CHECK(st2.ok());
    double postEpoch = st2->get("epoch")?st2->get("epoch")->asNumber():-1; CF_CHECK(postEpoch == preEpoch+1);
    double pubBefore = st2->get("published_count")?st2->get("published_count")->asNumber():0;
    sendStale(port, static_cast<CoordinatorEpoch>(postEpoch), static_cast<CacheGeneration>(preGen), 1,
              static_cast<CoordinatorEpoch>(preEpoch), static_cast<CacheGeneration>(preGen), 0xAAA0000000000001ULL);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    auto st3 = client.control(ControlKind::GetStats); CF_CHECK(st3.ok());
    double rejected = st3->get("rejected_stale")?st3->get("rejected_stale")->asNumber():-1; CF_CHECK(rejected >= 5);
    double pubAfter = st3->get("published_count")?st3->get("published_count")->asNumber():0; CF_CHECK(pubAfter == pubBefore);
    auto f1 = client.submit(req); CF_CHECK(f1.ok()); CF_CHECK_EQ(f1->validated, true);
    auto stE = client.control(ControlKind::GetStats); CF_CHECK(stE.ok());
    double pending = stE->get("pending_count") ? stE->get("pending_count")->asNumber() : -1;
    CF_CHECK(pending == 0);   // zero active builds after closure
    double wcnt = stE->get("workers") ? stE->get("workers")->asNumber() : -1;
    CF_CHECK(wcnt >= 2);
    auto f2 = client.submit(req); CF_CHECK(f2.ok()); CF_CHECK_EQ(f2->reused, true);
    std::printf("atomic_cluster: reused_hit=%d rejected_stale=%d fresh_ok=%d second_hit=%d\n", (int)r2->reused, (int)rejected, (int)f1.ok(), (int)f2->reused);
    CF_FINISH("atomic_driver");
}