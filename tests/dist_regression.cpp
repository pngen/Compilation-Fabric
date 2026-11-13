// Compilation Fabric - coordinator regression scenarios against a real
// `compile_fabric serve` process over framed TCP. The test acts as clients and,
// where an exact interleaving is needed, as the workers themselves. Pass a
// scenario name to run only that scenario.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#include "CompilationFabric/CompilationFabric.hpp"
#include "CompilationFabric/Distributed.hpp"
#include "CompilationFabric/Protocol.hpp"
#include "TestUtil.hpp"
#include "TestReq.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>

using namespace compilationfabric;

namespace {

constexpr size_t kEnvBytes = 8 + 4 + 16 + 8 + 8 + 16 + 16 + 16 + 8;

HANDLE spawnCli(const std::string& args) {
    std::string cmd = "\"" + std::string(CF_CLI_PATH) + "\" " + args;
    std::vector<char> line(cmd.begin(), cmd.end());
    line.push_back('\0');
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(nullptr, line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        return nullptr;
    CloseHandle(pi.hThread);
    return pi.hProcess;
}

bool exited(HANDLE h) { return WaitForSingleObject(h, 0) == WAIT_OBJECT_0; }

void killWait(HANDLE& h) {
    if (!h) return;
    TerminateProcess(h, 1);
    WaitForSingleObject(h, INFINITE);
    CloseHandle(h);
    h = nullptr;
}

// A coordinator process (and any real workers); all terminated on scope exit.
struct Cluster {
    HANDLE coord = nullptr;
    std::vector<HANDLE> workers;
    uint16_t port = 0;
    ~Cluster() {
        for (auto& w : workers) killWait(w);
        killWait(coord);
    }
    void terminateCoordinator() { killWait(coord); }
};

bool startCoordinator(Cluster& cl) {
    static unsigned next = static_cast<unsigned>(GetCurrentProcessId()) % 997u;
    initSockets();
    for (;;) {
        cl.port = static_cast<uint16_t>(45000 + (next++ % 3000u));
        cl.coord = spawnCli("serve " + std::to_string(cl.port));
        if (!cl.coord) return false;
        for (;;) {
            if (exited(cl.coord)) break;   // bind failed: retry on another port
            TcpSocket probe;
            if (probe.connectTo("127.0.0.1", cl.port).ok()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        CloseHandle(cl.coord);
        cl.coord = nullptr;
    }
}

Json getStats(const Cluster& cl) {
    DistributedClient c("127.0.0.1", cl.port);
    if (!c.connect().ok()) return Json::object({});
    auto s = c.control(ControlKind::GetStats);
    return s.ok() ? *s : Json::object({});
}

std::string bootOf(const Json& st, const std::string& wid) {
    if (!st.get("worker_boots")) return "";
    auto* wb = st.get("worker_boots")->asObjectPtr();
    if (!wb) return "";
    auto it = wb->find(wid);
    return it == wb->end() ? std::string() : it->second.asString();
}

// The test registered as a worker.
struct FakeWorker {
    std::unique_ptr<FramedChannel> ch;
    WorkerId id = 0;
    WorkerBootId boot;

    bool connect(const Cluster& cl, WorkerId wid, uint64_t bootLo) {
        id = wid;
        boot = WorkerBootId::fromU64(bootLo);
        TcpSocket s;
        if (!s.connectTo("127.0.0.1", cl.port).ok()) return false;
        ch = std::make_unique<FramedChannel>(std::move(s));
        ProtoEnvelope env;
        env.epoch = 1; env.workerId = id; env.bootId = boot; env.cacheGen = 1; env.toolchainGen = 1;
        Json caps = Json::object({});
        caps.set("worker_id", Json::number(static_cast<double>(id)));
        caps.set("backend", Json::str("cpu"));
        caps.set("boot_id", Json::str(boot.toHex()));
        ProtoFrame reg;
        reg.type = MsgType::Register;
        reg.payload = encodeRegister(env, caps);
        return ch->sendFrame(reg).ok();
    }
    // Wait until the worker is listed with this boot (registration processed).
    void awaitRegistered(const Cluster& cl) const {
        while (bootOf(getStats(cl), std::to_string(id)) != boot.toHex())
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    bool nextDispatch(ProtoEnvelope& env) {
        for (;;) {
            auto f = ch->recvFrame();
            if (!f.ok()) return false;
            if (f->type != MsgType::DispatchCompile || f->payload.size() < kEnvBytes) continue;
            auto e = ProtoEnvelope::decode(std::vector<uint8_t>(f->payload.begin(), f->payload.begin() + kEnvBytes));
            if (!e.ok()) return false;
            env = *e;
            return true;
        }
    }
    // Answer an attempt as this worker. `as` overrides the claimed authority.
    bool answer(const ProtoEnvelope& dispatch, uint64_t artifact, bool success = true,
                const ProtoEnvelope* as = nullptr) {
        ProtoEnvelope out = as ? *as : dispatch;
        out.workerId = id; out.bootId = boot;
        out.requestId = dispatch.requestId; out.attemptId = dispatch.attemptId;
        out.artifactId = ArtifactId::fromU64(artifact); out.artifactGen = 1;
        Json j = Json::object({});
        j.set("request_id", Json::str(dispatch.requestId.toHex()));
        j.set("attempt_id", Json::str(dispatch.attemptId.toHex()));
        j.set("artifact_id", Json::str(out.artifactId.toHex()));
        j.set("generation", Json::number(1.0));
        j.set("validated", Json::boolean(success));
        j.set("deployable", Json::boolean(success));
        j.set("backend_used", Json::str("cpu"));
        ProtoFrame f;
        f.type = MsgType::CompileResult;
        f.payload = encodeCompileResult(out, success, j);
        return ch->sendFrame(f).ok();
    }
};

CompilationRequest req(const std::string& name, uint64_t id) {
    return cf_test::mkCpuReq("name=" + name + "\nshape=256\nadd scalar=2.0\n", id, Datatype::F32, {256});
}

}  // namespace

// An invalidation takes effect: the next submit compiles again.
static void scenario_invalidate_takes_effect() {
    CF_BEGIN("invalidate-takes-effect");
    Cluster cl;
    CF_CHECK(startCoordinator(cl));
    if (!cl.coord) return;
    FakeWorker w;
    CF_CHECK(w.connect(cl, 1, 0x1001));
    w.awaitRegistered(cl);
    std::atomic<bool> stop{false};
    std::thread auto_answer([&] {
        ProtoEnvelope d;
        uint64_t n = 0;
        while (!stop.load() && w.nextDispatch(d)) w.answer(d, 100 + (++n));
    });
    DistributedClient c("127.0.0.1", cl.port);
    CF_CHECK(c.connect().ok());
    auto r = req("inval", 11);
    auto r1 = c.submit(r);
    CF_CHECK(r1.ok());
    CF_CHECK(c.invalidate(r.requestId).ok());
    auto r2 = c.submit(r);
    CF_CHECK(r2.ok());
    if (r2.ok()) CF_CHECK_EQ(r2->reused, false);
    stop = true;
    cl.terminateCoordinator();
    auto_answer.join();
}

// A stale completion is not published, and its requester is answered.
static void scenario_stale_completion_answers_requester() {
    CF_BEGIN("stale-completion-answers-requester");
    Cluster cl;
    CF_CHECK(startCoordinator(cl));
    if (!cl.coord) return;
    FakeWorker w;
    CF_CHECK(w.connect(cl, 1, 0x2001));
    w.awaitRegistered(cl);
    DistributedClient c("127.0.0.1", cl.port);
    CF_CHECK(c.connect().ok());
    Result<CompilationResult> res = Err<CompilationResult>(ErrorCode::Internal, "not run");
    std::thread submitter([&] { res = c.submit(req("stale", 21)); });
    ProtoEnvelope d;
    CF_CHECK(w.nextDispatch(d));
    DistributedClient ctl("127.0.0.1", cl.port);
    CF_CHECK(ctl.connect().ok());
    CF_CHECK(ctl.control(ControlKind::RollEpoch).ok());
    CF_CHECK(w.answer(d, 201));          // echoes the dispatch epoch: stale
    ProtoEnvelope forged = d;
    forged.epoch = d.epoch + 1;          // claims the current epoch for the old attempt
    CF_CHECK(w.answer(d, 202, true, &forged));
    submitter.join();
    CF_CHECK(!res.ok());
    if (!res.ok()) CF_CHECK(res.code() == ErrorCode::StaleEpoch);
}

// A failed compile answers its requester.
static void scenario_failed_compile_answers_requester() {
    CF_BEGIN("failed-compile-answers-requester");
    Cluster cl;
    CF_CHECK(startCoordinator(cl));
    if (!cl.coord) return;
    FakeWorker w;
    CF_CHECK(w.connect(cl, 1, 0x3001));
    w.awaitRegistered(cl);
    DistributedClient c("127.0.0.1", cl.port);
    CF_CHECK(c.connect().ok());
    Result<CompilationResult> res = Err<CompilationResult>(ErrorCode::Internal, "not run");
    std::thread submitter([&] { res = c.submit(req("failed", 31)); });
    ProtoEnvelope d;
    CF_CHECK(w.nextDispatch(d));
    CF_CHECK(w.answer(d, 301, /*success=*/false));
    CF_CHECK(w.answer(d, 302));          // a later success for the same attempt
    submitter.join();
    CF_CHECK(!res.ok());
    if (!res.ok()) CF_CHECK(res.code() == ErrorCode::BuildFailed);
}

// A worker that disconnects with a compile in flight answers its requester.
static void scenario_disconnect_answers_requester() {
    CF_BEGIN("disconnect-answers-requester");
    Cluster cl;
    CF_CHECK(startCoordinator(cl));
    if (!cl.coord) return;
    FakeWorker w;
    CF_CHECK(w.connect(cl, 1, 0x6001));
    w.awaitRegistered(cl);
    DistributedClient c("127.0.0.1", cl.port);
    CF_CHECK(c.connect().ok());
    Result<CompilationResult> res = Err<CompilationResult>(ErrorCode::Internal, "not run");
    std::thread submitter([&] { res = c.submit(req("disconnect", 61)); });
    ProtoEnvelope d;
    CF_CHECK(w.nextDispatch(d));
    w.ch.reset();                        // the worker goes away mid-compile
    submitter.join();
    CF_CHECK(!res.ok());
    if (!res.ok()) CF_CHECK(res.code() == ErrorCode::NoBackend);
}

// Only the worker a compile was dispatched to completes it.
static void scenario_completion_only_from_dispatched_worker() {
    CF_BEGIN("completion-only-from-dispatched-worker");
    Cluster cl;
    CF_CHECK(startCoordinator(cl));
    if (!cl.coord) return;
    FakeWorker w1, w2;
    CF_CHECK(w1.connect(cl, 1, 0x4001));
    CF_CHECK(w2.connect(cl, 2, 0x4002));
    w1.awaitRegistered(cl);
    w2.awaitRegistered(cl);
    DistributedClient c("127.0.0.1", cl.port);
    CF_CHECK(c.connect().ok());
    Result<CompilationResult> res = Err<CompilationResult>(ErrorCode::Internal, "not run");
    std::thread submitter([&] { res = c.submit(req("dispatched", 41)); });
    ProtoEnvelope d;
    CF_CHECK(w1.nextDispatch(d));        // worker 1 is selected first
    CF_CHECK(w2.answer(d, 402));         // worker 2 forges the completion
    CF_CHECK(w1.answer(d, 401));
    submitter.join();
    CF_CHECK(res.ok());
    if (res.ok()) CF_CHECK(res->artifactId == ArtifactId::fromU64(401));
}

// Two clients using the same request id each receive their own result.
static void scenario_same_request_id_two_clients() {
    CF_BEGIN("same-request-id-two-clients");
    Cluster cl;
    CF_CHECK(startCoordinator(cl));
    if (!cl.coord) return;
    FakeWorker w;
    CF_CHECK(w.connect(cl, 1, 0x5001));
    w.awaitRegistered(cl);
    DistributedClient c1("127.0.0.1", cl.port), c2("127.0.0.1", cl.port);
    CF_CHECK(c1.connect().ok());
    CF_CHECK(c2.connect().ok());
    Result<CompilationResult> res1 = Err<CompilationResult>(ErrorCode::Internal, "not run");
    Result<CompilationResult> res2 = Err<CompilationResult>(ErrorCode::Internal, "not run");
    std::thread s1([&] { res1 = c1.submit(req("first", 5)); });
    ProtoEnvelope d1;
    CF_CHECK(w.nextDispatch(d1));
    std::thread s2([&] { res2 = c2.submit(req("second", 5)); });
    ProtoEnvelope d2;
    CF_CHECK(w.nextDispatch(d2));
    CF_CHECK(w.answer(d1, 501));
    CF_CHECK(w.answer(d2, 502));
    s2.join();
    const bool own2 = res2.ok() && res2->artifactId == ArtifactId::fromU64(502);
    CF_CHECK(own2);
    if (!own2) cl.terminateCoordinator();   // a misrouted result leaves c1 waiting
    s1.join();
    CF_CHECK(res1.ok());
    if (res1.ok()) CF_CHECK(res1->artifactId == ArtifactId::fromU64(501));
}

// A worker's old connection closing after it re-registered leaves the new
// registration in place.
static void scenario_reregistration_survives_old_close() {
    CF_BEGIN("reregistration-survives-old-close");
    Cluster cl;
    CF_CHECK(startCoordinator(cl));
    if (!cl.coord) return;
    FakeWorker oldConn, newConn;
    CF_CHECK(oldConn.connect(cl, 7, 0x7001));
    oldConn.awaitRegistered(cl);
    CF_CHECK(newConn.connect(cl, 7, 0x7002));
    newConn.awaitRegistered(cl);
    // Half-close the old connection and wait until the coordinator's handler
    // for it has finished (its side closes too).
    ::shutdown(static_cast<SOCKET>(oldConn.ch->socket().native()), SD_SEND);
    while (oldConn.ch->recvFrame().ok()) {}
    CF_CHECK_EQ(bootOf(getStats(cl), "7"), newConn.boot.toHex());
}

// Concurrent submits from many clients over one real worker all complete.
static void scenario_concurrent_dispatch() {
    CF_BEGIN("concurrent-dispatch");
    Cluster cl;
    CF_CHECK(startCoordinator(cl));
    if (!cl.coord) return;
    HANDLE wk = spawnCli("worker 127.0.0.1 " + std::to_string(cl.port) + " 1");
    CF_CHECK(wk != nullptr);
    if (!wk) return;
    cl.workers.push_back(wk);
    while (bootOf(getStats(cl), "1").empty()) {
        if (exited(wk)) { CF_CHECK_MSG(false, "worker exited before registering"); return; }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    constexpr int kClients = 8;
    constexpr int kPerClient = 15;
    std::atomic<int> failures{0};
    std::vector<std::thread> ts;
    for (int t = 0; t < kClients; ++t) {
        ts.emplace_back([&, t] {
            DistributedClient c("127.0.0.1", cl.port);
            if (!c.connect().ok()) { ++failures; return; }
            for (int i = 0; i < kPerClient; ++i) {
                auto r = c.submit(req("cc_" + std::to_string(t) + "_" + std::to_string(i),
                                      static_cast<uint64_t>(1000 + t * 100 + i)));
                if (!r.ok() || !r->validated) ++failures;
            }
        });
    }
    for (auto& th : ts) th.join();
    CF_CHECK_EQ(failures.load(), 0);
}

int main(int argc, char** argv) {
    struct Scenario { const char* name; void (*fn)(); };
    const Scenario all[] = {
        {"invalidate_takes_effect", scenario_invalidate_takes_effect},
        {"stale_completion_answers_requester", scenario_stale_completion_answers_requester},
        {"failed_compile_answers_requester", scenario_failed_compile_answers_requester},
        {"disconnect_answers_requester", scenario_disconnect_answers_requester},
        {"completion_only_from_dispatched_worker", scenario_completion_only_from_dispatched_worker},
        {"same_request_id_two_clients", scenario_same_request_id_two_clients},
        {"reregistration_survives_old_close", scenario_reregistration_survives_old_close},
        {"concurrent_dispatch", scenario_concurrent_dispatch},
    };
    int ran = 0;
    for (const auto& s : all) {
        if (argc > 1 && std::strcmp(argv[1], s.name) != 0) continue;
        s.fn();
        ++ran;
    }
    if (ran == 0) { std::printf("unknown scenario\n"); return 2; }
    CF_FINISH("dist_regression");
}
