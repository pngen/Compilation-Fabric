// Compilation Fabric - Toolchain.cpp implementation.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#include "CompilationFabric/Toolchain.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <fstream>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace compilationfabric {

namespace {
namespace fs = std::filesystem;
std::string trim(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
    size_t a = s.find_first_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a);
}
} // namespace

ToolchainProbe::ToolchainProbe() { cudaApi_ = CudaBackend::api(); }
ToolchainProbe::ToolchainProbe(std::shared_ptr<CudaApi> a) : cudaApi_(std::move(a)) {}

std::string ToolchainProbe::runCapture(std::string_view cmd) {
    using namespace std;
#ifdef _WIN32
    std::string full(cmd);
    full += " 2>&1";
    FILE* p = _popen(full.c_str(), "r");
    if (!p) return "";
    std::string out;
    char buf[2048];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), p)) > 0) out.append(buf, n);
    _pclose(p);
    return out;
#else
    (void)cmd; return "";
#endif
}

std::string ToolchainProbe::readFileVersion(std::string_view exePath) {
#ifdef _WIN32
    std::wstring wpath(exePath.begin(), exePath.end());
    DWORD handle = 0;
    DWORD size = GetFileVersionInfoSizeW(wpath.c_str(), &handle);
    if (size == 0) return "";
    std::vector<uint8_t> data(size);
    if (!GetFileVersionInfoW(wpath.c_str(), 0, size, data.data())) return "";
    VS_FIXEDFILEINFO* ffi = nullptr;
    UINT len = 0;
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&ffi), &len) || !ffi) return "";
    std::ostringstream os;
    os << (ffi->dwFileVersionMS >> 16) << "." << (ffi->dwFileVersionMS & 0xFFFF) << "."
       << (ffi->dwFileVersionLS >> 16) << "." << (ffi->dwFileVersionLS & 0xFFFF);
    return os.str();
#else
    (void)exePath; return "";
#endif
}

std::string ToolchainProbe::failureReason(std::string_view tool) const {
    if (tool == "msvc") return "MSVC cl.exe not found in the Visual Studio toolset";
    if (tool == "nvcc") return "nvcc.exe not found (CUDA toolkit not installed or not on CUDA_PATH)";
    if (tool == "nvrtc") return "NVRTC library (nvrtc64_*.dll) not found on the module load path";
    if (tool == "cuda-driver") return "CUDA driver (nvcuda.dll) not found or cuInit failed";
    if (tool == "cmake") return "cmake not found on PATH";
    if (tool == "ninja") return "ninja not found on PATH";
    return "unavailable";
}

ToolchainDescriptor ToolchainProbe::probe() const {
    ToolchainDescriptor t;

    // ---- CUDA toolkit (nvcc) ----
    std::string cudaPath;
#ifdef _WIN32
    std::error_code ec;
    { char* buf = nullptr; size_t len = 0; if (_dupenv_s(&buf, &len, "CUDA_PATH") == 0 && buf) { cudaPath = buf; free(buf); } else cudaPath = "C:\\Program Files\\NVIDIA GPU Computing Toolkit\\CUDA\\v13.1"; }
#endif
    fs::path nvccPath(""); if (!cudaPath.empty()) nvccPath = fs::path(cudaPath) / "bin" / "nvcc.exe";
    if (!nvccPath.empty() && fs::exists(nvccPath, ec)) {
        t.nvccPresent = true;
        t.nvcc.id = "nvcc"; t.nvcc.vendor = "NVIDIA"; t.nvcc.backendType = "offline";
        auto ver = runCapture("\"" + nvccPath.string() + "\" --version");
        std::istringstream in(ver); std::string ln;
        std::string rel;
        while (std::getline(in, ln)) { if (ln.find("release") != std::string::npos) rel = trim(ln.substr(ln.find("release"))); }
        t.nvcc.version = rel; if (rel.empty()) t.nvcc.version = readFileVersion(nvccPath.string());
        t.cudaToolkitPresent = true; t.cudaToolkitPath = cudaPath;
    } else {
        t.nvccPresent = false; t.nvcc.id = "nvcc"; t.nvcc.version = ""; t.cudaToolkitPresent = false;
    }

    // ---- NVRTC + CUDA driver via CudaApi ----
    if (cudaApi_ && cudaApi_->available()) {
        t.nvrtcPresent = true; t.nvrtcVersion = cudaApi_->nvrtcVersion();
        t.cudaDriverPresent = true; t.cudaDriverVersion = cudaApi_->driverVersion();
    } else {
        t.nvrtcPresent = false; t.nvrtcVersion = cudaApi_ ? cudaApi_->error() : "";
        t.cudaDriverPresent = false;
    }

    // ---- MSVC ----
    fs::path clPath;
    {
        std::vector<fs::path> candidates;
        fs::path base1("C:\\Program Files\\Microsoft Visual Studio\\2022");
        fs::path base2("C:\\Program Files (x86)\\Microsoft Visual Studio\\2022");
        for (auto& base : {base1, base2}) {
            if (fs::exists(base, ec)) {
                for (auto& ed : fs::directory_iterator(base, ec)) {
                    if (!ed.is_directory()) continue;
                    auto tc = ed.path() / "VC" / "Tools" / "MSVC";
                    if (fs::exists(tc, ec)) {
                        for (auto& vd : fs::directory_iterator(tc, ec)) {
                            auto p = vd.path() / "bin" / "Hostx64" / "x64" / "cl.exe";
                            if (fs::exists(p, ec)) candidates.push_back(p);
                        }
                    }
                }
            }
        }
        if (!candidates.empty()) clPath = candidates.front();
    }
    if (!clPath.empty()) {
        t.msvcPresent = true;
        t.msvc.id = "msvc"; t.msvc.vendor = "Microsoft"; t.msvc.backendType = "host";
        t.msvc.version = readFileVersion(clPath.string());
    } else {
        t.msvcPresent = false; t.msvc.id = "msvc"; t.msvc.vendor = "Microsoft"; t.msvc.version = "";
    }

    // ---- CMake / Ninja ----
    {
        auto cm = runCapture("cmake --version");
        if (!cm.empty()) { t.cmakePresent = true; t.cmakeVersion = trim(cm.substr(sizeof("cmake version") - 1)); }
        else t.cmakePresent = false;
        auto nj = runCapture("ninja --version");
        if (!trim(nj).empty()) { t.ninjaPresent = true; t.ninjaVersion = trim(nj); }
        else t.ninjaPresent = false;
    }

    // ---- Windows SDK ----
    {
        fs::path sdk("C:\\Program Files (x86)\\Windows Kits\\10\\Include");
        std::string best;
        if (fs::exists(sdk, ec)) {
            for (auto& d : fs::directory_iterator(sdk, ec)) {
                if (d.is_directory()) {
                    std::string n = d.path().filename().string();
                    if (!n.empty() && (n.size() >= 3) && (n[0] >= '0' && n[0] <= '9')) {
                        if (best.empty() || n > best) best = n;
                    }
                }
            }
        }
        if (!best.empty()) { t.windowsSdkPresent = true; t.windowsSdkVersion = best; }
        else { t.windowsSdkPresent = false; t.windowsSdkVersion = ""; }
    }
    return t;
}

} // namespace compilationfabric