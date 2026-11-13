// Compilation Fabric - regression scenarios: artifact identity after recovery,
// persisted invalidation, single-flight with a throwing backend, and CLI
// temp-directory safety. Pass a scenario name to run only that scenario.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#include "CompilationFabric/CompilationFabric.hpp"
#include "CompilationFabric/CpuBackend.hpp"
#include "TestUtil.hpp"
#include "TestReq.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

using namespace compilationfabric;
namespace fs = std::filesystem;

namespace {

fs::path scratch(const std::string& tag) {
    fs::path p = fs::path(CF_TEST_SCRATCH) / (tag + "_" + std::to_string(Clock::monotonicNanos()));
    fs::create_directories(p);
    return p;
}

CompilationFabricConfig persistentCfg(const fs::path& root) {
    CompilationFabricConfig cfg;
    cfg.artifactRoot = root.string(); cfg.persistenceEnabled = true; cfg.allowCuda = false;
    return cfg;
}

double statOf(const CompilationFabric& f, const char* name) {
    auto s = f.stats();
    auto* v = s.get(name);
    return v ? v->asNumber() : 0.0;
}

const char* kSrcA = "name=ra\nshape=512\nadd scalar=1.0\n";
const char* kSrcB = "name=rb\nshape=512\nmul scalar=3.0\n";

// A CPU backend whose compile waits at a gate and can throw once.
class GatedThrowingBackend : public ICompilerBackend {
public:
    const BackendCapabilities& capabilities() const override { return inner_.capabilities(); }
    std::string id() const override { return CpuBackend::kId; }
    Result<void> checkCompatible(const CompilationPlan& plan) const override { return inner_.checkCompatible(plan); }
    Result<BackendOutput> compile(const CompilationRequest& request, const CompilationPlan& plan,
                                  const KeyToolchainContext& tc) override {
        bool doThrow = false;
        {
            std::unique_lock<std::mutex> lk(mu);
            entered += 1;
            cv.notify_all();
            cv.wait(lk, [&] { return open; });
            doThrow = throwNext;
            throwNext = false;
        }
        if (doThrow) throw std::runtime_error("backend failure");
        return inner_.compile(request, plan, tc);
    }
    Result<ValidationDescriptor> validate(const ArtifactDescriptor& d, const std::vector<uint8_t>& e) override {
        return inner_.validate(d, e);
    }
    Result<std::shared_ptr<LoadedModule>> load(const ArtifactDescriptor& d, const std::vector<uint8_t>& e) override {
        return inner_.load(d, e);
    }

    std::mutex mu;
    std::condition_variable cv;
    bool open = false;
    bool throwNext = true;
    int entered = 0;

private:
    CpuBackend inner_;
};

}  // namespace

// After recovery, new artifacts never take a recovered artifact's identity
// (which also named, and overwrote, its persisted files).
static void scenario_recovered_ids_not_reused() {
    CF_BEGIN("recovered-ids-not-reused");
    auto root = scratch("ids");
    ArtifactId idA;
    ArtifactGeneration genA = 0;
    {
        CompilationFabric f(persistentCfg(root));
        auto r = f.compile(cf_test::mkCpuReq(kSrcA, 1));
        CF_CHECK(r.ok());
        if (!r.ok()) return;
        idA = r->artifactId; genA = r->generation;
    }
    {
        CompilationFabric f(persistentCfg(root));
        CF_CHECK(f.recover().ok());
        auto r = f.compile(cf_test::mkCpuReq(kSrcB, 2));
        CF_CHECK(r.ok());
        if (!r.ok()) return;
        CF_CHECK(r->artifactId != idA);
        CF_CHECK(r->generation > genA);
    }
    CompilationFabric f(persistentCfg(root));
    CF_CHECK(f.recover().ok());
    CF_CHECK_EQ(statOf(f, "recovered_artifacts"), 2.0);
    auto r = f.compile(cf_test::mkCpuReq(kSrcA, 1));
    CF_CHECK(r.ok());
    if (!r.ok()) return;
    CF_CHECK_EQ(r->reused, true);
    CF_CHECK(r->artifactId == idA);
}

// An invalidation is persisted: a restarted runtime does not reuse the artifact.
static void scenario_invalidation_survives_restart() {
    CF_BEGIN("invalidation-survives-restart");
    auto root = scratch("inval");
    {
        CompilationFabric f(persistentCfg(root));
        auto r = f.compile(cf_test::mkCpuReq(kSrcA, 1));
        CF_CHECK(r.ok());
        if (!r.ok()) return;
        CF_CHECK(f.invalidateById(r->artifactId).ok());
    }
    CompilationFabric f(persistentCfg(root));
    CF_CHECK(f.recover().ok());
    auto r = f.compile(cf_test::mkCpuReq(kSrcA, 1));
    CF_CHECK(r.ok());
    if (!r.ok()) return;
    CF_CHECK_EQ(r->reused, false);
}

// A throwing backend still resolves the single-flight entry: the waiter gets
// an error and the key can be compiled again.
static void scenario_singleflight_throwing_backend() {
    CF_BEGIN("singleflight-throwing-backend");
    CompilationFabricConfig cfg;
    cfg.artifactRoot = scratch("throw").string(); cfg.persistenceEnabled = false; cfg.allowCuda = false;
    CompilationFabric f(cfg);
    auto backend = std::make_shared<GatedThrowingBackend>();
    f.registerBackend(backend);
    const auto req = cf_test::mkCpuReq(kSrcA, 1);
    std::atomic<bool> ownerThrew{false};
    std::thread owner([&] {
        try { (void)f.compile(req); } catch (const std::exception&) { ownerThrew = true; }
    });
    {
        std::unique_lock<std::mutex> lk(backend->mu);
        backend->cv.wait(lk, [&] { return backend->entered == 1; });
    }
    bool waiterOk = true;
    std::thread waiter([&] { waiterOk = f.compile(req).ok(); });
    while (statOf(f, "singleflight_waiters") < 1.0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    {
        std::lock_guard<std::mutex> lk(backend->mu);
        backend->open = true;
    }
    backend->cv.notify_all();
    owner.join();
    waiter.join();
    CF_CHECK(ownerThrew.load());
    CF_CHECK(!waiterOk);
    CF_CHECK(f.compile(req).ok());
}

// The CLI creates its per-run root under the temp directory and never removes
// the temp directory itself.
static void scenario_cli_keeps_temp_directory() {
    CF_BEGIN("cli-keeps-temp-directory");
    const fs::path sandbox = scratch("tempdir");
    const fs::path canary = sandbox / "canary.txt";
    { std::ofstream o(canary); o << "keep"; }
    wchar_t oldTmp[MAX_PATH] = {}, oldTemp[MAX_PATH] = {};
    const DWORD hadTmp = GetEnvironmentVariableW(L"TMP", oldTmp, MAX_PATH);
    const DWORD hadTemp = GetEnvironmentVariableW(L"TEMP", oldTemp, MAX_PATH);
    // The child resolves its temp directory to the sandbox.
    SetEnvironmentVariableW(L"TMP", sandbox.wstring().c_str());
    SetEnvironmentVariableW(L"TEMP", sandbox.wstring().c_str());
    std::wstring cmd = L"\"" + fs::path(CF_CLI_PATH).wstring() + L"\" stats";
    std::vector<wchar_t> line(cmd.begin(), cmd.end());
    line.push_back(L'\0');
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    const BOOL started = CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                                        nullptr, &si, &pi);
    SetEnvironmentVariableW(L"TMP", hadTmp ? oldTmp : nullptr);
    SetEnvironmentVariableW(L"TEMP", hadTemp ? oldTemp : nullptr);
    CF_CHECK(started != 0);
    if (!started) return;
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CF_CHECK(fs::exists(canary));
    bool runRoot = false;   // proves the child used the sandbox as its temp directory
    std::error_code ec;
    for (auto& e : fs::directory_iterator(sandbox, ec))
        if (e.path().filename().string().rfind("cf_cli_", 0) == 0) runRoot = true;
    CF_CHECK(runRoot);
}

int main(int argc, char** argv) {
    struct Scenario { const char* name; void (*fn)(); };
    const Scenario all[] = {
        {"recovered_ids_not_reused", scenario_recovered_ids_not_reused},
        {"invalidation_survives_restart", scenario_invalidation_survives_restart},
        {"singleflight_throwing_backend", scenario_singleflight_throwing_backend},
        {"cli_keeps_temp_directory", scenario_cli_keeps_temp_directory},
    };
    int ran = 0;
    for (const auto& s : all) {
        if (argc > 1 && std::strcmp(argv[1], s.name) != 0) continue;
        s.fn();
        ++ran;
    }
    if (ran == 0) { std::printf("unknown scenario\n"); return 2; }
    std::error_code ec;
    fs::remove_all(CF_TEST_SCRATCH, ec);
    CF_FINISH("regression");
}
