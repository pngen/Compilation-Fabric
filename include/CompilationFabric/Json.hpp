// Compilation Fabric - Minimal JSON value + parser/serializer.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#pragma once
#include "CompilationFabric/Core.hpp"
#include <map>
#include <variant>
#include <optional>

namespace compilationfabric {

class Json {
public:
    using Value = std::variant<std::nullptr_t, bool, double, std::string,
                               std::vector<Json>, std::map<std::string, Json>>;
    Value v = nullptr;

    // builders
    static Json null() { Json j; j.v = nullptr; return j; }
    static Json boolean(bool b) { Json j; j.v = b; return j; }
    static Json number(double d) { Json j; j.v = d; return j; }
    static Json integer(int64_t n) { Json j; j.v = static_cast<double>(n); return j; }
    static Json str(std::string s) { Json j; j.v = std::move(s); return j; }
    static Json array(std::vector<Json> a) { Json j; j.v = std::move(a); return j; }
    static Json object(std::map<std::string, Json> o) { Json j; j.v = std::move(o); return j; }

    bool isNull() const { return std::holds_alternative<std::nullptr_t>(v); }
    bool isBool() const { return std::holds_alternative<bool>(v); }
    bool isNumber() const { return std::holds_alternative<double>(v); }
    bool isString() const { return std::holds_alternative<std::string>(v); }
    bool isArray() const { return std::holds_alternative<std::vector<Json>>(v); }
    bool isObject() const { return std::holds_alternative<std::map<std::string, Json>>(v); }

    bool asBool(bool d = false) const { auto p = std::get_if<bool>(&v); return p ? *p : d; }
    double asNumber(double d = 0.0) const { auto p = std::get_if<double>(&v); return p ? *p : d; }
    const std::string& asString() const { static const std::string empty; auto p = std::get_if<std::string>(&v); return p ? *p : empty; }
    const std::vector<Json>* asArrayPtr() const { return std::get_if<std::vector<Json>>(&v); }
    const std::map<std::string, Json>* asObjectPtr() const { return std::get_if<std::map<std::string, Json>>(&v); }

    // object member convenience
    void set(std::string key, Json j) {
        if (!isObject()) v = std::map<std::string, Json>{};
        std::get<std::map<std::string, Json>>(v).insert_or_assign(std::move(key), std::move(j));
    }
    const Json* get(std::string_view key) const {
        auto* o = asObjectPtr();
        if (!o) return nullptr;
        auto it = o->find(std::string(key));
        return it == o->end() ? nullptr : &it->second;
    }

    std::string dump() const;              // compact JSON text
    std::string dumpPretty() const;        // indented JSON text
    static std::optional<Json> parse(std::string_view text);
};

} // namespace compilationfabric
