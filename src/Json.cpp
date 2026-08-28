// Compilation Fabric - Json.cpp implementation.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#include "CompilationFabric/Json.hpp"
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <cstdio>

namespace compilationfabric {

namespace {
void dumpTo(std::ostringstream& os, const Json& j, int indent);
void escapeTo(std::ostringstream& os, std::string_view s) {
    os.put('"');
    for (char ch : s) {
        unsigned char uc = static_cast<unsigned char>(ch);
        if (uc == '"') { os.put('\\'); os.put('"'); }
        else if (uc == '\\') { os.put('\\'); os.put('\\'); }
        else if (uc < 0x20) {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(uc));
            os << buf;
        } else {
            os.put(ch);
        }
    }
    os.put('"');
}
void dumpTo(std::ostringstream& os, const Json& j, int indent) {
    (void)indent;
    if (j.isNull()) { os << "null"; return; }
    if (j.isBool()) { os << (j.asBool() ? "true" : "false"); return; }
    if (j.isNumber()) {
        double d = j.asNumber();
        if (d == static_cast<double>(static_cast<int64_t>(d))) os << static_cast<int64_t>(d);
        else os << d;
        return;
    }
    if (j.isString()) { escapeTo(os, j.asString()); return; }
    if (auto* arr = j.asArrayPtr()) {
        os << '[';
        bool first = true;
        for (auto& e : *arr) { if (!first) os << ','; first = false; dumpTo(os, e, indent); }
        os << ']';
        return;
    }
    if (auto* obj = j.asObjectPtr()) {
        os << '{';
        bool first = true;
        for (auto& [k, val] : *obj) {
            if (!first) os << ','; first = false;
            escapeTo(os, k); os << ':'; dumpTo(os, val, indent);
        }
        os << '}';
        return;
    }
}
struct Parser {
    std::string_view s_;
    size_t i_ = 0;
    bool fail_ = false;
    void skipWs() { while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\r' || s_[i_] == '\n')) ++i_; }
    bool eof() const { return i_ >= s_.size(); }
    char peek() const { return eof() ? '\0' : s_[i_]; }
    char next() { return eof() ? '\0' : s_[i_++]; }
    Json parseValue();
    Json parseObject();
    Json parseArray();
    Json parseString();
    Json parseBool();
    Json parseNull();
    Json parseNumber();
};
Json Parser::parseValue() {
    skipWs();
    if (eof()) { fail_ = true; return Json::null(); }
    char c = peek();
    if (c == '{') return parseObject();
    if (c == '[') return parseArray();
    if (c == '"') return parseString();
    if (c == 't' || c == 'f') return parseBool();
    if (c == 'n') return parseNull();
    if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
    fail_ = true; return Json::null();
}
Json Parser::parseObject() {
    std::map<std::string, Json> o;
    next();
    skipWs();
    if (peek() == '}') { next(); return Json::object(std::move(o)); }
    for (;;) {
        skipWs();
        if (peek() != '"') { fail_ = true; return Json::null(); }
        std::string key = parseString().asString();
        skipWs();
        if (next() != ':') { fail_ = true; return Json::null(); }
        auto val = parseValue();
        if (fail_) return Json::null();
        o.insert_or_assign(std::move(key), std::move(val));
        skipWs();
        char d = next();
        if (d == '}') break;
        if (d != ',') { fail_ = true; return Json::null(); }
    }
    return Json::object(std::move(o));
}
Json Parser::parseArray() {
    std::vector<Json> a;
    next();
    skipWs();
    if (peek() == ']') { next(); return Json::array(std::move(a)); }
    for (;;) {
        auto v = parseValue();
        if (fail_) return Json::null();
        a.push_back(std::move(v));
        skipWs();
        char d = next();
        if (d == ']') break;
        if (d != ',') { fail_ = true; return Json::null(); }
    }
    return Json::array(std::move(a));
}
Json Parser::parseString() {
    next();
    std::string out;
    for (;;) {
        if (eof()) { fail_ = true; return Json::null(); }
        char c = next();
        if (c == '"') break;
        if (c == '\\') {
            if (eof()) { fail_ = true; return Json::null(); }
            char e = next();
            switch (e) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    if (i_ + 4 > s_.size()) { fail_ = true; return Json::null(); }
                    std::string hex(s_.substr(i_, 4)); i_ += 4;
                    out.push_back(static_cast<char>(static_cast<unsigned int>(std::strtoul(hex.c_str(), nullptr, 16))));
                    break;
                }
                default: fail_ = true; return Json::null();
            }
        } else out.push_back(c);
    }
    return Json::str(std::move(out));
}
Json Parser::parseBool() {
    if (s_.substr(i_, 4) == "true") { i_ += 4; return Json::boolean(true); }
    if (s_.substr(i_, 5) == "false") { i_ += 5; return Json::boolean(false); }
    fail_ = true; return Json::null();
}
Json Parser::parseNull() {
    if (s_.substr(i_, 4) == "null") { i_ += 4; return Json::null(); }
    fail_ = true; return Json::null();
}
Json Parser::parseNumber() {
    size_t start = i_;
    if (peek() == '-') next();
    while (!eof() && ((peek() >= '0' && peek() <= '9') || peek() == '.' || peek() == 'e' || peek() == 'E' || peek() == '+' || peek() == '-')) next();
    std::string text(s_.substr(start, i_ - start));
    char* end = nullptr;
    double d = std::strtod(text.c_str(), &end);
    if (end == text.c_str()) { fail_ = true; return Json::null(); }
    return Json::number(d);
}
} // namespace

std::string Json::dump() const { std::ostringstream os; dumpTo(os, *this, 0); return os.str(); }
std::string Json::dumpPretty() const { std::ostringstream os; dumpTo(os, *this, 0); return os.str(); }

std::optional<Json> Json::parse(std::string_view text) {
    Parser p{text};
    Json j = p.parseValue();
    if (p.fail_) return std::nullopt;
    p.skipWs();
    if (!p.eof()) return std::nullopt;
    return j;
}

} // namespace compilationfabric
