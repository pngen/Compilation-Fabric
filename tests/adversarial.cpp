// Compilation Fabric - adversarial suite.
#include "CompilationFabric/CompilationFabric.hpp"
#include "CompilationFabric/Protocol.hpp"
#include "CompilationFabric/CpuBackend.hpp"
#include "TestUtil.hpp"
#include "TestReq.hpp"
#include <filesystem>
#include <vector>

using namespace compilationfabric;

int main() {
    std::filesystem::path root = std::filesystem::temp_directory_path() / ("cf_adv_" + std::to_string(Clock::monotonicNanos()));
    CompilationFabricConfig cfg; cfg.artifactRoot = root.string(); cfg.persistenceEnabled = false; cfg.allowCuda = false;
    CompilationFabric fabric(cfg);

    CF_BEGIN("adversarial-empty-source");
    CompilationRequest empty = cf_test::mkCpuReq("", 1);
    auto r1 = fabric.compile(empty);
    CF_CHECK(!r1.ok());  // empty source -> invalid

    CF_BEGIN("adversarial-malformed-source");
    CompilationRequest bad = cf_test::mkCpuReq("not-an-op\n##garbage\n", 2);
    auto r2 = fabric.compile(bad);
    CF_CHECK(!r2.ok());  // malformed source -> invalid

    CF_BEGIN("adversarial-no-backend");
    CompilationRequest nosuch = cf_test::mkCpuReq("name=k\nshape=64\nadd scalar=1.0\n", 3);
    nosuch.backend = "does-not-exist";
    auto r3 = fabric.compile(nosuch);
    CF_CHECK(!r3.ok());
    CF_CHECK(r3.code() == ErrorCode::NoBackend);

    CF_BEGIN("adversarial-malformed-key");
    std::vector<uint8_t> badbytes = {0x00, 0x01, 0x02, 0xFF};
    CF_CHECK(!CompilationKey::fromCanonicalBytes(badbytes.data(), badbytes.size()).has_value());
    std::vector<uint8_t> truncated = {0x00, 0x00, 0x00, 0x05, 0x01, 0x00};
    CF_CHECK(!CompilationKey::fromCanonicalBytes(truncated.data(), truncated.size()).has_value());

    CF_BEGIN("adversarial-compat-mismatches");
    CompilationRequest base = cf_test::mkCpuReq("name=k\nshape=1024\nadd scalar=2.0\n", 4);
    auto cr = fabric.compile(base);
    CF_CHECK(cr.ok());
    // datatype mismatch via the compatibility engine (only the datatype field differs)
    CompilationKey keyDt = cr->key; keyDt.datatype(Datatype::F64);
    CompilationCompatibility compat; auto dec = compat.decide(cr->key, keyDt, cr->artifact, cfg.compatibilityPolicy);
    CF_CHECK(!dec.reusable);
    CF_CHECK(dec.outcome == CompatibilityOutcome::RecompileRequiredDatatypeChange);
    // shape mismatch (only the static shape differs)
    CompilationKey keySh = cr->key; keySh.staticShape({2048});
    auto dec2 = compat.decide(cr->key, keySh, cr->artifact, cfg.compatibilityPolicy);
    CF_CHECK(!dec2.reusable);
    CF_CHECK(dec2.outcome == CompatibilityOutcome::RecompileRequiredShapeChange);
    // invalidated artifact lookup
    auto inv = fabric.invalidateByKey(cr->key);
    CF_CHECK(inv.ok());
    auto decInv = fabric.lookup(cr->key);
    CF_CHECK(decInv.ok());
    CF_CHECK(!decInv->first.reusable);
    CF_CHECK(decInv->first.outcome == CompatibilityOutcome::StaleArtifact);

    CF_BEGIN("adversarial-cpu-artifact-corrupt");
    CpuBackend cpu;
    auto prog = CpuBackend::parseSource("name=x\nshape=64\nscale scalar=1.0\n");
    CF_CHECK(prog.ok());
    auto enc = CpuBackend::encode(*prog);
    // corrupt one byte
    std::vector<uint8_t> badEnc = enc; badEnc[8] ^= 0xFF;
    CF_CHECK(!CpuBackend::decode(badEnc).ok());

    CF_BEGIN("adversarial-frame-decode");
    // zero-length frame
    std::vector<uint8_t> zero; // empty body -> truncated
    CF_CHECK(!decodeFrameBody(zero).ok());
    // unknown protocol version
    {
        CanonicalWriter w; w.u32(999); w.u32(static_cast<uint32_t>(MsgType::Heartbeat)); w.u32(0); w.u32(0); w.bytes(nullptr, 0);
        auto body = w.take();
        CF_CHECK(!decodeFrameBody(body).ok());
        CF_CHECK(decodeFrameBody(body).code() == ErrorCode::ProtocolVersionMismatch);
    }
    // unknown message type
    {
        CanonicalWriter w; w.u32(kProtocolVersion); w.u32(99990); w.u32(0); w.u32(0); w.bytes(nullptr, 0);
        auto body = w.take();
        CF_CHECK(!decodeFrameBody(body).ok());
        CF_CHECK(decodeFrameBody(body).code() == ErrorCode::UnknownMessageType);
    }
    // truncated frame body (header claims more than present)
    {
        std::vector<uint8_t> body = {0,0,0,1}; // only version bytes
        CF_CHECK(!decodeFrameBody(body).ok());
        CF_CHECK(decodeFrameBody(body).code() == ErrorCode::TruncatedFrame);
    }

    std::error_code ec; std::filesystem::remove_all(root, ec);
    CF_FINISH("adversarial");
}