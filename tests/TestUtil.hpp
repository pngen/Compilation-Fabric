// Compilation Fabric - Minimal deterministic test runner (no third-party deps).
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#pragma once
#include <cstdio>
#include <string>
#include <vector>
#include <functional>

namespace cf_test {
inline int& checks() { static int c = 0; return c; }
inline int& failures() { static int f = 0; return f; }
inline std::string& currentTest() { static std::string t = ""; return t; }

inline void begin(std::string name) { currentTest() = std::move(name); }
inline void ok(bool cond, const std::string& what, const char* file, int line) {
    checks() += 1;
    if (!cond) { failures() += 1; std::printf("FAIL [%s:%d] %s :: %s\n", file, line, currentTest().c_str(), what.c_str()); }
}
inline int finish(const char* suite) {
    std::printf("[%s] checks=%d failures=%d\n", suite, checks(), failures());
    return failures() == 0 ? 0 : 1;
}
} // namespace cf_test

#define CF_CHECK(cond) cf_test::ok((cond), #cond, __FILE__, __LINE__)
#define CF_CHECK_EQ(a, b) cf_test::ok((a) == (b), (std::string(#a) + " == " + #b), __FILE__, __LINE__)
#define CF_CHECK_MSG(cond, msg) cf_test::ok((cond), (msg), __FILE__, __LINE__)
#define CF_BEGIN(name) cf_test::begin(name)
#define CF_FINISH(name) return cf_test::finish(name)
