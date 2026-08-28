// Compilation Fabric - Persistence.cpp implementation.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#include "CompilationFabric/Persistence.hpp"
#include <fstream>
#include <string_view>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace compilationfabric {

namespace {
constexpr std::string_view kMetaMagic = "CFMD";

bool atomicWriteBytes(const std::filesystem::path& dest, const std::vector<uint8_t>& bytes) {
    std::filesystem::path tmp = dest; tmp += ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        f.flush();
        if (!f.good()) return false;
    }
#ifdef _WIN32
    if (!MoveFileExW(tmp.c_str(), dest.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        // fall back to std::filesystem::rename
        std::error_code ec;
        std::filesystem::rename(tmp, dest, ec);
        return !ec;
    }
    return true;
#else
    std::error_code ec;
    std::filesystem::rename(tmp, dest, ec);
    return !ec;
#endif
}

std::vector<uint8_t> readAll(const std::filesystem::path& p, bool& ok) {
    ok = false;
    std::error_code ec;
    if (!std::filesystem::exists(p, ec)) return {};
    auto size = std::filesystem::file_size(p, ec);
    if (ec) return {};
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    std::vector<uint8_t> v(static_cast<size_t>(size));
    if (size) { f.read(reinterpret_cast<char*>(v.data()), static_cast<std::streamsize>(size)); if (!f.good()) return {}; }
    ok = true;
    return v;
}

std::vector<uint8_t> encodeMeta(const PersistedRecord& r) {
    CanonicalWriter w;
    w.string(kMetaMagic);
    w.u32(PersistenceStore::kVersion);
    auto keyBytes = makeCanonicalFields(r.key.fields());
    w.string(std::string_view(reinterpret_cast<const char*>(keyBytes.data()), keyBytes.size()));
    Json meta = r.descriptor.toJson();
    meta.set("invalidated", Json::boolean(r.invalidated));
    meta.set("superseded", Json::boolean(r.superseded));
    std::string json = meta.dump();
    w.string(json);
    w.bytes(r.descriptor.contentDigest.data(), 32);
    Digest h = Sha256::hash(w.data().data(), w.data().size());
    w.bytes(h.data(), 32);
    return w.take();
}

struct MetaParse {
    PersistedRecord record;
    bool ok = false;
    std::string error;
};

MetaParse decodeMeta(const std::vector<uint8_t>& bytes) {
    MetaParse out;
    CanonicalReader r(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
    std::string magic;
    if (!r.str(magic) || magic != kMetaMagic) { out.error = "unknown metadata magic"; return out; }
    uint32_t ver; if (!r.u32(ver)) { out.error = "truncated version"; return out; }
    if (ver != PersistenceStore::kVersion) { out.error = "unknown persistence version " + std::to_string(ver); return out; }
    if (r.remaining() < 32) { out.error = "truncated checksum"; return out; }
    std::string_view payload = std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size() - 32);
    Digest stored; for (int i = 0; i < 32; ++i) stored[i] = bytes[bytes.size() - 32 + i];
    Digest actual = Sha256::hash(payload.data(), payload.size());
    if (stored != actual) { out.error = "metadata checksum mismatch"; return out; }
    std::string kb;
    if (!r.str(kb)) { out.error = "truncated key"; return out; }
    auto keyOpt = CompilationKey::fromCanonicalBytes(reinterpret_cast<const uint8_t*>(kb.data()), kb.size());
    if (!keyOpt) { out.error = "invalid canonical key in metadata"; return out; }
    out.record.key = *keyOpt;
    std::string js;
    if (!r.str(js)) { out.error = "truncated metadata"; return out; }
    auto jsonOpt = Json::parse(js);
    if (!jsonOpt) { out.error = "metadata JSON parse failure"; return out; }
    auto desc = ArtifactDescriptor::fromJson(*jsonOpt);
    if (!desc) { out.error = "metadata descriptor parse failure"; return out; }
    out.record.descriptor = *desc;
    out.record.id = desc->id; out.record.generation = desc->generation;
    if (auto* inv = jsonOpt->get("invalidated")) out.record.invalidated = inv->asBool();
    if (auto* sup = jsonOpt->get("superseded")) out.record.superseded = sup->asBool();
    std::string_view cd; if (!r.rawBlob(32, cd)) { out.error = "truncated content digest"; return out; }
    out.ok = true;
    return out;
}
} // namespace

PersistenceStore::PersistenceStore() = default;
PersistenceStore::PersistenceStore(std::filesystem::path root) : root_(std::move(root)) {}

Result<void> PersistenceStore::open() {
    std::error_code ec;
    std::filesystem::create_directories(root_, ec);
    if (ec && !std::filesystem::exists(root_)) return ErrVoid(ErrorCode::IOError, "cannot create store root " + root_.string() + ": " + ec.message());
    return OkVoid();
}

std::filesystem::path metaPath(const std::filesystem::path& root, const ArtifactId& id, ArtifactGeneration gen) {
    return root / (id.toHex() + std::string("_") + std::to_string(gen) + std::string(".meta"));
}
std::filesystem::path binPath(const std::filesystem::path& root, const ArtifactId& id, ArtifactGeneration gen) {
    return root / (id.toHex() + std::string("_") + std::to_string(gen) + std::string(".bin"));
}

Result<void> PersistenceStore::store(const PersistedRecord& record) {
    if (root_.empty()) return ErrVoid(ErrorCode::PersistenceFailure, "store root not opened");
    auto meta = encodeMeta(record);
    if (!atomicWriteBytes(metaPath(root_, record.id, record.generation), meta))
        return ErrVoid(ErrorCode::PersistenceFailure, "atomic metadata write failed");
    if (!atomicWriteBytes(binPath(root_, record.id, record.generation), record.contentBytes))
        return ErrVoid(ErrorCode::PersistenceFailure, "atomic content write failed");
    return OkVoid();
}

Result<PersistedRecord> PersistenceStore::load(const ArtifactId& id, ArtifactGeneration gen) const {
    static const std::string meta = "";
    (void)meta;
    bool ok = false;
    auto metaBytes = readAll(metaPath(root_, id, gen), ok);
    if (!ok) return Err<PersistedRecord>(ErrorCode::NotFound, "metadata file missing for artifact " + id.toHex() + "_" + std::to_string(gen));
    auto parsed = decodeMeta(metaBytes);
    if (!parsed.ok) return Err<PersistedRecord>(ErrorCode::MetadataCorrupt, parsed.error);
    auto bin = readAll(binPath(root_, id, gen), ok);
    if (!ok) return Err<PersistedRecord>(ErrorCode::ArtifactTruncated, "content file missing for artifact " + id.toHex() + "_" + std::to_string(gen));
    // Verify content digest.
    Digest actual = Sha256::hash(bin.data(), bin.size());
    if (parsed.record.descriptor.contentDigest != Digest{} && actual != parsed.record.descriptor.contentDigest)
        return Err<PersistedRecord>(ErrorCode::ArtifactCorrupt, "content digest mismatch");
    if (parsed.record.id != id || parsed.record.generation != gen)
        return Err<PersistedRecord>(ErrorCode::MetadataCorrupt, "metadata id/generation mismatch");
    parsed.record.contentBytes = std::move(bin);
    return Ok(std::move(parsed.record));
}

Result<PersistedRecord> PersistenceStore::loadLatest(const ArtifactId& id) const {
    ArtifactGeneration latest = 0; bool any = false;
    for (auto& [aid, gen] : list()) if (aid == id && gen > latest) { latest = gen; any = true; }
    if (!any) return Err<PersistedRecord>(ErrorCode::NotFound, "no generations for artifact " + id.toHex());
    return load(id, latest);
}

std::vector<std::pair<ArtifactId, ArtifactGeneration>> PersistenceStore::list() const {
    std::vector<std::pair<ArtifactId, ArtifactGeneration>> out;
    std::error_code ec;
    if (!std::filesystem::exists(root_, ec)) return out;
    for (auto& e : std::filesystem::directory_iterator(root_, ec)) {
        if (!e.is_regular_file()) continue;
        std::string name = e.path().filename().string();
        if (name.size() < 5 || name.substr(name.size() - 5) != ".meta") continue;
        auto us = name.find('_');
        if (us == std::string::npos) continue;
        std::string idHex = name.substr(0, us);
        std::string genStr = name.substr(us + 1, name.size() - (us + 1) - 5);
        if (auto id = ArtifactId::parse(idHex)) {
            try { uint64_t g = std::strtoull(genStr.c_str(), nullptr, 10); out.push_back({*id, static_cast<ArtifactGeneration>(g)}); }
            catch (...) {}
        }
    }
    return out;
}

Result<void> PersistenceStore::remove(const ArtifactId& id, ArtifactGeneration gen) {
    std::error_code ec;
    std::filesystem::remove(metaPath(root_, id, gen), ec);
    std::filesystem::remove(binPath(root_, id, gen), ec);
    return OkVoid();
}

Result<PersistenceStore::RecoveryResult> PersistenceStore::recover() {
    RecoveryResult res;
    std::error_code ec;
    if (!std::filesystem::exists(root_, ec)) { std::filesystem::create_directories(root_, ec); }
    // orphan temp cleanup
    for (auto& e : std::filesystem::directory_iterator(root_, ec)) {
        std::string name = e.path().filename().string();
        if (!e.is_regular_file()) continue;
        if (name.size() >= 4 && name.substr(name.size() - 4) == ".tmp") {
            res.orphanTempRemoved.push_back(e.path());
            std::filesystem::remove(e.path(), ec);
        }
    }
    for (auto& [id, gen] : list()) {
        auto r = load(id, gen);
        if (!r.ok()) {
            PersistedRecord bad; bad.id = id; bad.generation = gen; bad.corrupted = true; bad.error = r.message();
            res.corrupted.push_back(std::move(bad));
            continue;
        }
        if (r->invalidated) res.invalidated.push_back(std::move(*r));
        else res.valid.push_back(std::move(*r));
    }
    return Ok(std::move(res));
}

} // namespace compilationfabric