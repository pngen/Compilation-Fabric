// Compilation Fabric - core smoke test: key semantics, CPU compile/reuse/recompile/invalidate.
#include "CompilationFabric/CompilationFabric.hpp"
#include "TestUtil.hpp"
#include <filesystem>

using namespace compilationfabric;

static CompilationRequest makeReq(const std::string& src, uint64_t id, Datatype dt = Datatype::F32,
                                  std::vector<int64_t> shape = {1024}) {
    CompilationRequest r;
    r.requestId = CompilationRequestId::fromU64(id);
    r.logicalOperation = LogicalOperation::fromU64(1);
    r.source = src;
    r.sourceDigest = Sha256::hash(src);
    r.sourceLanguage = "cf-src";
    r.datatype = dt; r.rank = 1; r.staticShape = std::move(shape);
    r.targetArchitecture = "host-x86_64";
    r.backend = "cpu";
    r.reproducibility = ReproducibilityMode::Strict;
    return r;
}

static const char* kSource = "name=vec_add\nshape=1024\nadd scalar=2.0\n";

int main() {
    CF_BEGIN("key-identity");
    auto tc = KeyToolchainContext{};
    tc.compiler = "cf-cpu"; tc.compilerVersion = "1.0.0"; tc.backend = "cpu"; tc.frontend = "cf-frontend";
    CompilationRequest r1 = makeReq(kSource, 1);
    CompilationRequest r2 = makeReq(kSource, 1);
    CF_CHECK(buildCompilationKey(r1, CompilationPlan{}, tc) == buildCompilationKey(r2, CompilationPlan{}, tc));
    CompilationRequest r3 = makeReq("name=vec_add\nshape=1024\nadd scalar=3.0\n", 1);
    CF_CHECK(buildCompilationKey(r1, CompilationPlan{}, tc) != buildCompilationKey(r3, CompilationPlan{}, tc));
    CompilationRequest r4 = makeReq(kSource, 1, Datatype::F64);
    CF_CHECK(buildCompilationKey(r1, CompilationPlan{}, tc) != buildCompilationKey(r4, CompilationPlan{}, tc));
    CompilationKey a, b;
    a.sourceDigest(r1.sourceDigest); a.sourceContent(r1.source); a.compiler("cf-cpu");
    b.compiler("cf-cpu"); b.sourceContent(r1.source); b.sourceDigest(r1.sourceDigest);
    CF_CHECK(a == b);
    CF_CHECK(a.digest() == b.digest());
    std::vector<uint8_t> bad = {0xFF, 0x00, 0x01, 0x02};
    CF_CHECK(!CompilationKey::fromCanonicalBytes(bad).has_value());
    Id128 id128 = Id128(0x1234567890ABCDEFULL, 0x0FEDCBA987654321ULL);
    auto back = Id128::parse(id128.toHex());
    CF_CHECK(back.has_value() && *back == id128);

    CF_BEGIN("cpu-compile-reuse-recompile");
    std::filesystem::path root = std::filesystem::temp_directory_path() / "cf_smoke" / ("run_" + std::to_string(Clock::monotonicNanos()));
    CompilationFabricConfig cfg;
    cfg.artifactRoot = root.string(); cfg.persistenceEnabled = true; cfg.persistOnCompile = true;
    cfg.allowCuda = true;
    CompilationFabric fabric(cfg);
    auto plan = fabric.plan(r1);
    CF_CHECK(plan.ok());
    CF_CHECK_EQ(plan->backend, std::string("cpu"));
    auto cr = fabric.compile(r1);
    CF_CHECK(cr.ok());
    if (cr.ok()) {
        CF_CHECK_EQ(cr->validated, true);
        CF_CHECK_EQ(cr->deployable, true);
        CF_CHECK_EQ(cr->artifact.validation.passed, true);
        // deploy + lease on a valid artifact
        auto mod = fabric.deploy(cr->artifactId);
        CF_CHECK(mod.ok());
        auto lease = fabric.acquire(cr->artifactId);
        CF_CHECK(lease.ok());
        if (lease.ok()) { auto rel = fabric.release(*lease); CF_CHECK(rel.ok()); }
        // exact reuse
        auto reuse = fabric.compile(r1);
        CF_CHECK(reuse.ok());
        CF_CHECK_EQ(reuse->reused, true);
        CF_CHECK_EQ(reuse->artifactId, cr->artifactId);
        // source change -> recompile, different artifact
        CompilationRequest changed = makeReq("name=vec_add\nshape=1024\nadd scalar=5.0\n", 2);
        auto plan2 = fabric.plan(changed);
        auto tc2 = KeyToolchainContext{}; tc2.compiler="cf-cpu"; tc2.backend="cpu"; tc2.frontend="cf-frontend";
        CompilationKey changedKey = buildCompilationKey(changed, *plan2, tc2);
        auto dec = fabric.lookup(changedKey);
        CF_CHECK(dec.ok());
        CF_CHECK_EQ(dec->first.reusable, false);
        auto rc = fabric.compile(changed);
        CF_CHECK(rc.ok());
        CF_CHECK(rc->artifactId != cr->artifactId);
        auto dec2 = fabric.lookup(rc->key);
        CF_CHECK(dec2.ok());
        CF_CHECK_EQ(dec2->first.reusable, true);
        // invalidate original; deployment now rejected; next compile recompiles.
        auto inv = fabric.invalidateByKey(cr->key);
        CF_CHECK(inv.ok());
        auto deployAfter = fabric.deploy(cr->artifactId);
        CF_CHECK(!deployAfter.ok());
        auto afterInv = fabric.compile(r1);
        CF_CHECK(afterInv.ok());
        CF_CHECK_EQ(afterInv->reused, false);
        CF_CHECK(afterInv->artifactId != cr->artifactId);
        auto snap = fabric.snapshot();
        CF_CHECK(!snap.toJson().empty());
    }
    std::error_code ec;
    std::filesystem::remove_all(root.parent_path(), ec);
    CF_FINISH("core_smoke");
}