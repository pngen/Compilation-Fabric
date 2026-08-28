// Compilation Fabric - Real toolchain discovery.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#pragma once
#include "CompilationFabric/Core.hpp"
#include "CompilationFabric/Descriptor.hpp"
#include "CompilationFabric/Backend.hpp"
#include "CompilationFabric/Cuda.hpp"
#include <string>

namespace compilationfabric {

// Probes MSVC, nvcc, NVRTC, the CUDA driver, CMake, Ninja, the Windows SDK, and
// backend capability. Never infers availability: presence is proven by actually
// locating the tool and, where possible, running a version probe. Exact failure
// reasons are recorded per tool.
class ToolchainProbe : public IToolchainProbe {
public:
    ToolchainProbe();
    explicit ToolchainProbe(std::shared_ptr<CudaApi> cudaApi);

    ToolchainDescriptor probe() const override;
    std::string failureReason(std::string_view tool) const override;

    // Helper used by the CLI/runtime to read a tool version from a file without
    // needing a configured developer shell.
    static std::string readFileVersion(std::string_view exePath);
    static std::string runCapture(std::string_view cmd);

private:
    std::shared_ptr<CudaApi> cudaApi_;
};

} // namespace compilationfabric
