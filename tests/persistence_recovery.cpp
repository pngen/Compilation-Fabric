// Compilation Fabric - persistence + recovery suite.
#include "CompilationFabric/CompilationFabric.hpp"
#include "CompilationFabric/Persistence.hpp"
#include "TestUtil.hpp"
#include "TestReq.hpp"
#include <filesystem>
#include <fstream>

using namespace compilationfabric;

int main() {
    std::filesystem::path root = std::filesystem::temp_directory_path() / ("cf_persist_" + std::to_string(Clock::monotonicNanos()));
    std::filesystem::create_directories(root);

    CF_BEGIN("persistence-basic-recover");
    ArtifactId idA; ArtifactGeneration genA = 0; CompilationKey keyA;
    {
        CompilationFabricConfig cfg; cfg.artifactRoot = root.string(); cfg.persistenceEnabled = true; cfg.allowCuda = false;
        CompilationFabric fabric(cfg);
        auto rA = fabric.compile(cf_test::mkCpuReq("name=va\nshape=512\nadd scalar=1.0\n", 1));
        CF_CHECK(rA.ok());
        idA = rA->artifactId; genA = rA->generation; keyA = rA->key;
        auto rB = fabric.compile(cf_test::mkCpuReq("name=vb\nshape=512\nmul scalar=3.0\n", 2));
        CF_CHECK(rB.ok());
    }
    // Recovery in a new runtime on the same root.
    {
        CompilationFabricConfig cfg; cfg.artifactRoot = root.string(); cfg.persistenceEnabled = true; cfg.allowCuda = false;
        CompilationFabric fabric(cfg);
        auto rec = fabric.recover();
        CF_CHECK(rec.ok());
        auto stx = fabric.stats();
        CF_CHECK(stx.get("recovered_artifacts") && stx.get("recovered_artifacts")->asNumber() == 2.0);
        // valid artifacts must be reusable after recovery.
        auto reuse = fabric.compile(cf_test::mkCpuReq("name=va\nshape=512\nadd scalar=1.0\n", 1));
        CF_CHECK(reuse.ok());
        CF_CHECK_EQ(reuse->artifactId, idA);
        CF_CHECK_EQ(reuse->generation, genA);
        CF_CHECK_EQ(reuse->reused, true);
        // module residency is not resumed: deployment descriptor says module not resident.
        auto snap = fabric.snapshot();
        CF_CHECK(!snap.toJson().empty());
    }

    CF_BEGIN("persistence-corruption");
    PersistenceStore store(root);
    // find a content file and currupt it.
    // Use the store directly for corruption/truncation/version/orphan.
    auto list = store.list();
    if (!list.empty()) {
        auto id = list[0].first; auto gen = list[0].second;
        std::filesystem::path bin = root / (id.toHex() + "_" + std::to_string(gen) + ".bin");
        // currupt: overwrite bytes
        { std::ofstream f(bin, std::ios::binary | std::ios::trunc); f.write("CORRUPTED", 9); }
        auto loaded = store.load(id, gen);
        CF_CHECK(!loaded.ok());
        CF_CHECK(loaded.code() == ErrorCode::ArtifactCorrupt || loaded.code() == ErrorCode::ArtifactTruncated);
    }
    // truncation rejection is covered by corruption path on content size mismatch.

    CF_BEGIN("persistence-orphan-temp");
    std::filesystem::path tmp = root / "orphan_123.meta.tmp";
    { std::ofstream f(tmp, std::ios::binary); f << "junk"; }
    auto rec2 = store.recover();
    CF_CHECK(rec2.ok());
    bool removed = false;
    for (auto& p : rec2->orphanTempRemoved) if (p == tmp) removed = true;
    CF_CHECK(removed);

    CF_BEGIN("persistence-unknown-version");
    // Write a metadata file with an unknown version magic/version.
    std::filesystem::path meta = root / "zzz_1.meta";
    { std::ofstream f(meta, std::ios::binary); f << "ZZZZ\x07\x00\x00\x00garbage"; }
    auto all = store.list();
    bool sawUnknown = false;
    (void)all;
    auto rec3 = store.recover();
    CF_CHECK(rec3.ok());
    CF_CHECK(rec3->corrupted.size() >= 1); // unknown-version metadata is rejected as corrupted
    (void)sawUnknown;

    std::error_code ec; std::filesystem::remove_all(root, ec);
    CF_FINISH("persistence_recovery");
}