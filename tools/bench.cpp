// Compilation Fabric - measured benchmarks (real timings over real workloads).
#include "CompilationFabric/CompilationFabric.hpp"
#include <cstdio>
#include <filesystem>
#include <vector>
using namespace compilationfabric;

static double ms(int64_t a, int64_t b) { return double(b - a) / 1e6; }

int main() {
    std::filesystem::path root = std::filesystem::temp_directory_path() / ("cf_bench_" + std::to_string(Clock::monotonicNanos()));
    CompilationFabricConfig cfg; cfg.artifactRoot = root.string(); cfg.persistenceEnabled = true; cfg.allowCuda = false;
    CompilationFabric fabric(cfg);
    const int N = 2000;
    CompilationKey k;
    for (int i = 0; i < 20; ++i) { k.sourceDigest(Sha256::hash("src" + std::to_string(i))); k.compiler("cf-cpu"); k.datatype(Datatype::F32); k.staticShape({1024}); }

    auto t0 = Clock::monotonicNanos();
    volatile int sink = 0;
    for (int i = 0; i < N; ++i) { CompilationKey x; x.sourceDigest(Sha256::hash("s" + std::to_string(i))); x.compiler("cf-cpu"); x.datatype(Datatype::F32); x.staticShape({1024}); x.staticShape({1024, 1}); sink += (int)x.fieldCount(); }
    auto t1 = Clock::monotonicNanos();
    std::printf("key_construction: %d keys in %.2f ms => %.0f keys/s\n", N, ms(t0,t1), N*1e3/ms(t0,t1));

    t0 = Clock::monotonicNanos();
    for (int i = 0; i < N*10; ++i) { Digest d = Sha256::hash("payload" + std::to_string(i)); sink += d[0]; }
    t1 = Clock::monotonicNanos();
    std::printf("sha256: %d hashes in %.2f ms => %.0f hashes/s\n", N*10, ms(t0,t1), N*10*1e3/ms(t0,t1));

    // compile a pool of artifacts, then measure hit/miss lookup.
    auto t2 = Clock::monotonicNanos();
    std::vector<CompilationResult> arts;
    for (int i = 0; i < 200; ++i) {
        CompilationRequest r; r.requestId = CompilationRequestId::fromU64(i+1); r.logicalOperation = LogicalOperation::fromU64(1);
        r.source = "name=k\nshape=256\nadd scalar=" + std::to_string(i % 9) + "\n"; r.sourceDigest = Sha256::hash(r.source);
        r.datatype = Datatype::F32; r.rank = 1; r.staticShape = {256}; r.backend = "cpu";
        auto res = fabric.compile(r); if (res.ok()) arts.push_back(*res);
    }
    auto t3 = Clock::monotonicNanos();
    std::printf("compile: %d artifacts in %.2f ms => %.2f ms/compile\n", (int)arts.size(), ms(t2,t3), ms(t2,t3)/arts.size());

    // warm reuse lookups (cache hit)
    auto t4 = Clock::monotonicNanos();
    int hits = 0;
    for (int i = 0; i < N; ++i) {
        CompilationRequest r; r.requestId = CompilationRequestId::fromU64((i % 200)+1); r.logicalOperation = LogicalOperation::fromU64(1);
        r.source = "name=k\nshape=256\nadd scalar=" + std::to_string((i % 200) % 9) + "\n"; r.sourceDigest = Sha256::hash(r.source);
        r.datatype = Datatype::F32; r.rank = 1; r.staticShape = {256}; r.backend = "cpu";
        auto res = fabric.compile(r); if (res.ok() && res->reused) ++hits;
    }
    auto t5 = Clock::monotonicNanos();
    std::printf("cache_hit_lookup: %d lookups (%d hits) in %.2f ms => %.0f lookups/s\n", N, hits, ms(t4,t5), N*1e3/ms(t4,t5));

    // compatibility decisions
    auto t6 = Clock::monotonicNanos();
    int comps = 0;
    CompilationCompatibility compat;
    for (int i = 0; i < N; ++i) {
        CompilationKey a = k; a.datatype(i % 2 ? Datatype::F32 : Datatype::F64);
        CompilationKey b = k;
        for (int j = 0; j < 10; ++j) { auto d = compat.decide(a, b, arts[i % 200].artifact, cfg.compatibilityPolicy); (void)d; ++comps; }
    }
    auto t7 = Clock::monotonicNanos();
    std::printf("compatibility_decision: %d in %.2f ms => %.0f decisions/s\n", comps, ms(t6,t7), comps*1e3/ms(t6,t7));

    std::printf("sink=%d\n", sink);
    std::error_code ec; std::filesystem::remove_all(root, ec);
    return 0;
}
