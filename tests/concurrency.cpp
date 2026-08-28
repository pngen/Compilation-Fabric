// Compilation Fabric - concurrency suite: single-flight, no duplicate compile ownership,
// no stale publish, no deadlock, no lease underflow.
#include "CompilationFabric/CompilationFabric.hpp"
#include "TestUtil.hpp"
#include "TestReq.hpp"
#include <thread>
#include <vector>
#include <filesystem>
#include <atomic>

using namespace compilationfabric;

int main() {
    CF_BEGIN("concurrency-single-flight");
    std::filesystem::path root = std::filesystem::temp_directory_path() / ("cf_concurrency_" + std::to_string(Clock::monotonicNanos()));
    CompilationFabricConfig cfg; cfg.artifactRoot = root.string(); cfg.persistenceEnabled = false; cfg.allowCuda = false;
    CompilationFabric fabric(cfg);
    const std::string src = "name=k\nshape=1024\nadd scalar=2.0\n";
    CompilationRequest req = cf_test::mkCpuReq(src, 42);
    const int kThreads = 8;
    const int kRepeats = 40;
    std::vector<CompilationResult> results(kThreads * kRepeats);
    std::atomic<int> ok{0};
    std::vector<std::thread> ts;
    for (int t = 0; t < kThreads; ++t) {
        ts.emplace_back([&, t]() {
            for (int i = 0; i < kRepeats; ++i) {
                auto r = fabric.compile(req);
                if (r.ok()) { results[t * kRepeats + i] = *r; ok.fetch_add(1); }
            }
        });
    }
    for (auto& th : ts) th.join();
    CF_CHECK_EQ(ok.load(), kThreads * kRepeats);
    // All results must share the identical artifact (single-flight dedup).
    ArtifactId firstId = results[0].artifactId;
    ArtifactGeneration firstGen = results[0].generation;
    bool allSame = true;
    for (auto& r : results) if (r.artifactId != firstId || r.generation != firstGen) allSame = false;
    CF_CHECK(allSame);
    // The compile happened exactly once for the identical key.
    Json stats = fabric.stats();
    CF_CHECK(stats.get("compiled_artifacts") != nullptr);
    double compiled = stats.get("compiled_artifacts")->asNumber();
    CF_CHECK_EQ(compiled, 1.0);
    double waiters = stats.get("singleflight_waiters") ? stats.get("singleflight_waiters")->asNumber() : 0;
    CF_CHECK(waiters >= 0);
    // exact reuse path: cache hits now dominate
    auto hit = fabric.compile(req);
    CF_CHECK(hit.ok());
    CF_CHECK_EQ(hit->reused, true);

    CF_BEGIN("concurrency-invalidate-recompile");
    // Invalidate; all future submits must recompile (never reuse the invalidated artifact).
    auto inv = fabric.invalidateByKey(hit->key);
    CF_CHECK(inv.ok());
    CompilationRequest req2 = cf_test::mkCpuReq(src, 42);
    std::vector<CompilationResult> results2(kThreads * kRepeats);
    std::atomic<int> ok2{0};
    std::vector<std::thread> ts2;
    for (int t = 0; t < kThreads; ++t) {
        ts2.emplace_back([&, t]() {
            for (int i = 0; i < kRepeats; ++i) {
                auto r = fabric.compile(req2);
                if (r.ok()) { results2[t * kRepeats + i] = *r; ok2.fetch_add(1); }
                else { results2[t * kRepeats + i].artifactId = ArtifactId(); }
            }
        });
    }
    for (auto& th : ts2) th.join();
    CF_CHECK(ok2.load() == kThreads * kRepeats); // every call returned a result
    // None may have reused the invalidated artifact; they must all share a single fresh artifact.
    ArtifactId firstId2 = results2[0].artifactId;
    bool noneInvalidated = true, allSame2 = true, anyFresh = false;
    for (auto& r : results2) {
        if (r.artifactId == firstId) noneInvalidated = false;   // never reused the invalidated artifact
        if (r.artifactId != firstId2) allSame2 = false;
        if (!r.reused) anyFresh = true;                          // a fresh compile occurred after invalidation
    }
    CF_CHECK(noneInvalidated);
    CF_CHECK(allSame2);
    CF_CHECK(anyFresh);
    CF_CHECK(results2[0].artifactId != firstId); // the post-invalidation artifact is a new generation

    CF_BEGIN("concurrency-leases");
    // Lease acquire/release must never underflow.
    auto lease = fabric.acquire(firstId2);
    CF_CHECK(lease.ok());
    if (lease.ok()) {
        for (int i = 0; i < 100; ++i) { auto l = fabric.acquire(firstId2); CF_CHECK(l.ok()); if (l.ok()) { auto rel = fabric.release(*l); CF_CHECK(rel.ok()); } }
        auto rel = fabric.release(*lease);
        CF_CHECK(rel.ok());
    }
    std::error_code ec; std::filesystem::remove_all(root, ec);
    CF_FINISH("concurrency");
}