// Compilation Fabric - Key.cpp implementation.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#include "CompilationFabric/Key.hpp"

#include <algorithm>
#include <sstream>

namespace compilationfabric {

namespace {
std::vector<uint8_t> encId(Id128 v) { CanonicalWriter w; w.u128(v); return w.take(); }
std::vector<uint8_t> encDigest(Digest v) { CanonicalWriter w; w.bytes(v.data(), 32); return w.take(); }
std::vector<uint8_t> encString(std::string_view v) { CanonicalWriter w; w.string(v); return w.take(); }
std::vector<uint8_t> encU8(uint8_t v) { CanonicalWriter w; w.u8(v); return w.take(); }
std::vector<uint8_t> encU64(uint64_t v) { CanonicalWriter w; w.u64(v); return w.take(); }
std::vector<uint8_t> encShape(const std::vector<int64_t>& s) {
    CanonicalWriter w; w.u32(static_cast<uint32_t>(s.size()));
    for (auto e : s) w.u64(static_cast<uint64_t>(e));
    return w.take();
}
std::vector<uint8_t> encIds(const std::vector<Id128>& ids) {
    CanonicalWriter w; w.u32(static_cast<uint32_t>(ids.size()));
    for (auto& i : ids) w.u128(i);
    return w.take();
}
std::vector<uint8_t> encU64s(const std::vector<uint64_t>& vs) {
    CanonicalWriter w; w.u32(static_cast<uint32_t>(vs.size()));
    for (auto v : vs) w.u64(v);
    return w.take();
}

bool decBytes(std::string_view s, std::string& out) {
    CanonicalReader r(s); std::string_view b;
    if (!r.bytesBlob(b)) return false;
    out.assign(b);
    return r.consumed() == s.size();
}
bool decU64(std::string_view s, uint64_t& out) {
    CanonicalReader r(s);
    if (!r.u64(out)) return false;
    return r.consumed() == s.size();
}
bool decU8(std::string_view s, uint8_t& out) {
    CanonicalReader r(s);
    if (!r.u8(out)) return false;
    return r.consumed() == s.size();
}
bool decU128(std::string_view s, Id128& out) {
    CanonicalReader r(s);
    if (!r.u128(out)) return false;
    return r.consumed() == s.size();
}
bool decShape(std::string_view s, std::vector<int64_t>& out) {
    CanonicalReader r(s);
    uint32_t n;
    if (!r.u32(n)) return false;
    for (uint32_t i = 0; i < n; ++i) {
        uint64_t v; if (!r.u64(v)) return false;
        out.push_back(static_cast<int64_t>(v));
    }
    return r.consumed() == s.size();
}

std::string renderValue(KeyField f, const std::vector<uint8_t>& v) {
    auto s = std::string_view(reinterpret_cast<const char*>(v.data()), v.size());
    std::string str;
    if (decBytes(s, str)) return str;   // length-prefixed string / blob
    uint64_t u64v;
    if (decU64(s, u64v)) {
        if (f == KeyField::Datatype) return std::string(datatypeName(static_cast<Datatype>(u64v)));
        if (f == KeyField::Layout) return std::string(layoutName(static_cast<Layout>(u64v)));
        if (f == KeyField::Quantization) return std::string(quantizationName(static_cast<QuantizationMode>(u64v)));
        if (f == KeyField::Precision) return std::string(precisionName(static_cast<PrecisionMode>(u64v)));
        if (f == KeyField::Determinism) return std::string(determinismName(static_cast<DeterminismMode>(u64v)));
        if (f == KeyField::Reproducibility) return std::string(reproducibilityName(static_cast<ReproducibilityMode>(u64v)));
        if (f == KeyField::AcceleratorVendor) return std::string(vendorName(static_cast<AcceleratorVendor>(u64v)));
        if (f == KeyField::AcceleratorFamily) return std::string(familyName(static_cast<AcceleratorFamily>(u64v)));
        if (f == KeyField::DebugRelease) return std::string(debugReleaseName(static_cast<DebugReleaseMode>(u64v)));
        return std::to_string(u64v);
    }
    uint8_t u8v;
    if (decU8(s, u8v)) {
        if (f == KeyField::Datatype) return std::string(datatypeName(static_cast<Datatype>(u8v)));
        if (f == KeyField::Layout) return std::string(layoutName(static_cast<Layout>(u8v)));
        if (f == KeyField::Quantization) return std::string(quantizationName(static_cast<QuantizationMode>(u8v)));
        if (f == KeyField::Precision) return std::string(precisionName(static_cast<PrecisionMode>(u8v)));
        if (f == KeyField::Determinism) return std::string(determinismName(static_cast<DeterminismMode>(u8v)));
        if (f == KeyField::Reproducibility) return std::string(reproducibilityName(static_cast<ReproducibilityMode>(u8v)));
        if (f == KeyField::AcceleratorVendor) return std::string(vendorName(static_cast<AcceleratorVendor>(u8v)));
        if (f == KeyField::AcceleratorFamily) return std::string(familyName(static_cast<AcceleratorFamily>(u8v)));
        if (f == KeyField::DebugRelease) return std::string(debugReleaseName(static_cast<DebugReleaseMode>(u8v)));
        return std::to_string(u8v);
    }
    Id128 idv;
    if (decU128(s, idv)) return idv.toHex();
    if (v.size() == 32) return "0x" + bytesToHex(v.data(), v.size()); // digest
    std::vector<int64_t> sh;
    if (decShape(s, sh)) {
        std::ostringstream os; os << "[";
        for (size_t i = 0; i < sh.size(); ++i) { if (i) os << ","; os << sh[i]; }
        os << "]";
        return os.str();
    }
    return "0x" + bytesToHex(v.data(), v.size());
}
} // namespace

const KeyFieldEntry* CompilationKey::findField(KeyField f) const {
    uint8_t t = keyFieldTag(f);
    for (auto& e : fields_) if (e.tag == t) return &e;
    return nullptr;
}
KeyFieldEntry* CompilationKey::findField(KeyField f) {
    uint8_t t = keyFieldTag(f);
    for (auto& e : fields_) if (e.tag == t) return &e;
    return nullptr;
}
void CompilationKey::put(std::vector<uint8_t> value, KeyField f) {
    uint8_t t = keyFieldTag(f);
    fields_.erase(std::remove_if(fields_.begin(), fields_.end(),
                 [&](const KeyFieldEntry& e) { return e.tag == t; }), fields_.end());
    KeyFieldEntry e; e.tag = t; e.value = std::move(value);
    // Keep fields_ sorted by tag so operator==/operator< agree with the canonical
    // order, and a key reconstructed from canonical bytes compares equal to a
    // setter-built key with the same semantic content.
    auto it = std::lower_bound(fields_.begin(), fields_.end(), e,
        [](const KeyFieldEntry& a, const KeyFieldEntry& b) { return a.tag < b.tag; });
    fields_.insert(it, std::move(e));
}
void CompilationKey::setId(KeyField f, Id128 v) { put(encId(v), f); }
void CompilationKey::setDigest(KeyField f, Digest v) { put(encDigest(v), f); }
void CompilationKey::setString(KeyField f, std::string_view v) { put(encString(v), f); }
void CompilationKey::setU8(KeyField f, uint8_t v) { put(encU8(v), f); }
void CompilationKey::setU64(KeyField f, uint64_t v) { put(encU64(v), f); }
void CompilationKey::setShape(KeyField f, const std::vector<int64_t>& s) { put(encShape(s), f); }
void CompilationKey::setIds(KeyField f, const std::vector<Id128>& ids) { put(encIds(ids), f); }
void CompilationKey::setU64s(KeyField f, const std::vector<uint64_t>& vs) { put(encU64s(vs), f); }

std::optional<uint8_t> CompilationKey::getU8(KeyField f) const {
    auto* e = findField(f); if (!e) return std::nullopt;
    uint8_t v; if (!decU8(std::string_view(reinterpret_cast<const char*>(e->value.data()), e->value.size()), v)) return std::nullopt;
    return v;
}
std::optional<uint64_t> CompilationKey::getU64(KeyField f) const {
    auto* e = findField(f); if (!e) return std::nullopt;
    uint64_t v; if (!decU64(std::string_view(reinterpret_cast<const char*>(e->value.data()), e->value.size()), v)) return std::nullopt;
    return v;
}
std::optional<std::string> CompilationKey::getString(KeyField f) const {
    auto* e = findField(f); if (!e) return std::nullopt;
    std::string s; if (!decBytes(std::string_view(reinterpret_cast<const char*>(e->value.data()), e->value.size()), s)) return std::nullopt;
    return s;
}
std::optional<Digest> CompilationKey::getDigest(KeyField f) const {
    auto* e = findField(f); if (!e) return std::nullopt;
    CanonicalReader r(std::string_view(reinterpret_cast<const char*>(e->value.data()), e->value.size()));
    std::string_view s; if (!r.bytesBlob(s)) return std::nullopt;
    if (s.size() != 32) return std::nullopt;
    Digest d; std::memcpy(d.data(), s.data(), 32); return d;
}
std::optional<std::vector<int64_t>> CompilationKey::getShape(KeyField f) const {
    auto* e = findField(f); if (!e) return std::nullopt;
    std::vector<int64_t> out;
    if (!decShape(std::string_view(reinterpret_cast<const char*>(e->value.data()), e->value.size()), out)) return std::nullopt;
    return out;
}
std::optional<Id128> CompilationKey::getU128(KeyField f) const {
    auto* e = findField(f); if (!e) return std::nullopt;
    Id128 v; if (!decU128(std::string_view(reinterpret_cast<const char*>(e->value.data()), e->value.size()), v)) return std::nullopt;
    return v;
}

Digest CompilationKey::digest() const {
    auto bytes = makeCanonicalFields(fields_);
    return Sha256::hash(bytes.data(), bytes.size());
}

bool CompilationKey::operator==(const CompilationKey& o) const {
    if (fields_.size() != o.fields_.size()) return false;
    for (auto& e : fields_) {
        const KeyFieldEntry* oe = o.findField(static_cast<KeyField>(e.tag));
        if (!oe || oe->value != e.value) return false;
    }
    return true;
}
bool CompilationKey::operator<(const CompilationKey& o) const {
    return std::lexicographical_compare(fields_.begin(), fields_.end(), o.fields_.begin(), o.fields_.end(),
        [](const KeyFieldEntry& a, const KeyFieldEntry& b) {
            if (a.tag != b.tag) return a.tag < b.tag;
            return a.value < b.value;
        });
}

std::vector<CompilationKey::FieldExplain> CompilationKey::explain() const {
    std::vector<FieldExplain> out;
    for (auto& e : fields_) {
        KeyField f = static_cast<KeyField>(e.tag);
        FieldExplain fe;
        fe.name = std::string(keyFieldName(f));
        fe.present = true;
        fe.value = renderValue(f, e.value);
        out.push_back(std::move(fe));
    }
    std::sort(out.begin(), out.end(), [](const FieldExplain& a, const FieldExplain& b){ return a.name < b.name; });
    return out;
}

std::string CompilationKey::explainText() const {
    std::ostringstream os;
    os << "CompilationKey(" << toHex() << ")\n";
    for (auto& e : explain()) {
        os << "  " << (e.present ? "present" : "absent") << " " << e.name;
        if (e.present) os << " = " << e.value;
        os << "\n";
    }
    return os.str();
}

std::optional<CompilationKey> CompilationKey::fromCanonicalBytes(const uint8_t* p, size_t n) {
    auto parsed = parseKeyFields(p, n);
    if (!parsed.ok) return std::nullopt;
    CompilationKey k;
    k.fields_ = std::move(parsed.fields);
    return k;
}

std::ostream& operator<<(std::ostream& os, const CompilationKey& k) { os << k.toHex(); return os; }

} // namespace compilationfabric