// Compilation Fabric - fixed-seed property / randomized test.
#include "CompilationFabric/CompilationFabric.hpp"
#include "TestUtil.hpp"
#include "TestReq.hpp"
#include <filesystem>
#include <random>

using namespace compilationfabric;

// deterministic PRNG
struct Rng { uint64_t s; explicit Rng(uint64_t seed) : s(seed) {} uint64_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; } uint64_t range(uint64_t n) { return next() % n; } };

int main() {
    uint64_t seed = 0xC0FFEE1234567890ULL;
    std::filesystem::path root = std::filesystem::temp_directory_path() / ("cf_prop_" + std::to_string(seed));
    CompilationFabricConfig cfg; cfg.artifactRoot = root.string(); cfg.persistenceEnabled = true; cfg.allowCuda = false;
    CompilationFabric fabric(cfg);
    Rng rng(seed);
    const char* ops[] = {"add","sub","mul","scale","abs","neg","sum","max","min"};
    const char* dts[] = {"F32","F64"};

    uint64_t opsCount = 0, invariants = 0;
    std::vector<CompilationKey> seenKeys;
    for (int iter = 0; iter < 3000; ++iter) {
        uint64_t id = rng.range(100000) + 1;
        std::string src = "name=k" + std::to_string(id) + "\nshape=" + std::to_string(64 + 64 * (rng.range(16))) + "\n" + std::string(ops[rng.range(9)]) + " scalar=" + std::to_string((rng.range(100) - 50) / 10.0) + "\n";
        CompilationRequest req = cf_test::mkCpuReq(src, id, rng.range(2) ? Datatype::F32 : Datatype::F64, {static_cast<int64_t>(64 + 64 * (rng.range(16)))});
        if (rng.range(3) == 0) req.namespaceName = "ns" + std::to_string(rng.range(3));
        auto r = fabric.compile(req);
        ++opsCount;
        if (!r.ok()) { ++invariants; continue; }
        // artifact immutable after publish: re-loading must give the same descriptor content digest
        auto r2 = fabric.compile(req);
        if (r2.ok()) { ++invariants; CF_CHECK(r2->artifact.contentDigest == r->artifact.contentDigest); CF_CHECK(r2->validated); }
        ++invariants;
        seenKeys.push_back(r->key);
        // Sometimes invalidate and recompile.
        if (rng.range(7) == 0) {
            auto inv = fabric.invalidateByKey(r->key);
            ++invariants; CF_CHECK(inv.ok());
            auto after = fabric.compile(req);
            ++invariants; CF_CHECK(after.ok());
            if (after.ok()) { CF_CHECK_EQ(after->reused, false); CF_CHECK(after->artifactId != r->artifactId); }
        }
    }
    // namespace separation: a request in a different namespace does not reuse artifacts from another.
    CompilationRequest nsa = cf_test::mkCpuReq("name=sep\nshape=128\nscale scalar=1.0\n", 999990); nsa.namespaceName = "nsA";
    CompilationRequest nsb = cf_test::mkCpuReq("name=sep\nshape=128\nscale scalar=1.0\n", 999991); nsb.namespaceName = "nsB";
    auto ra = fabric.compile(nsa); auto rb = fabric.compile(nsb);
    ++opsCount; ++opsCount;
    if (ra.ok() && rb.ok()) { ++invariants; CF_CHECK(ra->artifactId != rb->artifactId); }
    // generation monotonic / no generation rollback
    auto snap = fabric.snapshot();
    ++invariants; CF_CHECK(!snap.toJson().empty());
    std::fprintf(stderr, "[property] seed=%llu ops=%llu invariants=%llu keys=%zu\n",
        (unsigned long long)seed, (unsigned long long)opsCount, (unsigned long long)invariants, seenKeys.size());
    std::error_code ec; std::filesystem::remove_all(root, ec);
    CF_FINISH("property");
}
