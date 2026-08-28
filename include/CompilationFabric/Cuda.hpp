// Compilation Fabric - CUDA backend via runtime-loaded NVRTC + CUDA driver.
//
// This backend compiles real CUDA source through NVRTC (PTX and/or CUBIN), loads
// the module with the CUDA driver, obtains the kernel, allocates device memory,
// launches the kernel, synchronizes, copies results back, compares against the
// deterministic CPU reference, and verifies cleanup. No CUDA headers are needed
// at build time: the API surface is reached through LoadLibrary/GetProcAddress,
// so the library remains fully vendor-neutral and builds without a CUDA toolkit.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#pragma once
#include "CompilationFabric/Backend.hpp"

namespace compilationfabric {

class CudaApi {
public:
    CudaApi();
    // Load and resolve the NVRTC + CUDA driver entry points. Idempotent.
    void ensure();
    bool available() const;
    const std::string& error() const;
    std::string nvrtcVersion() const;
    std::string driverVersion() const;
    std::string toolkitVersions() const;

    // NVRTC
    using NvrtcCreateProgram = int(*)(void**, const char*, const char*, int, const char* const*, const char* const*);
    using NvrtcCompileProgram = int(*)(void*, int, const char* const*);
    using NvrtcGetSize = int(*)(void*, size_t*);
    using NvrtcGetText = int(*)(void*, char*);
    using NvrtcGetLog = int(*)(void*, char*);
    using NvrtcDestroy = int(*)(void**);
    using NvrtcGetVersion = int(*)(int*, int*);
    NvrtcCreateProgram createProgram = nullptr;
    NvrtcCompileProgram compileProgram = nullptr;
    NvrtcGetSize getPTXSize = nullptr; NvrtcGetText getPTX = nullptr;
    NvrtcGetSize getCUBINSize = nullptr; NvrtcGetText getCUBIN = nullptr;
    NvrtcGetSize getLogSize = nullptr; NvrtcGetLog getProgramLog = nullptr;
    NvrtcDestroy destroyProgram = nullptr;
    NvrtcGetVersion getVersion = nullptr;

    // CUDA driver
    using CuResult = int;
    using CuCuInit = CuResult(*)(unsigned int);
    using CuCuDeviceGetCount = CuResult(*)(int*);
    using CuCuDeviceGet = CuResult(*)(int*, int);
    using CuCuDeviceGetName = CuResult(*)(char*, int, int);
    using CuCuDeviceGetAttribute = CuResult(*)(int*, int, int);
    using CuCuCtxCreate = CuResult(*)(void**, void*, unsigned int, int);
    using CuCuModuleLoadData = CuResult(*)(void**, const void*);
    using CuCuModuleGetFunction = CuResult(*)(void**, void*, const char*);
    using CuCuMemAlloc = CuResult(*)(uint64_t*, size_t);
    using CuCuMemFree = CuResult(*)(uint64_t);
    using CuCuMemcpyHtoD = CuResult(*)(uint64_t, const void*, size_t);
    using CuCuMemcpyDtoH = CuResult(*)(void*, uint64_t, size_t);
    using CuCuLaunchKernel = CuResult(*)(void*, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, void*, void**, void**);
    using CuCuCtxSynchronize = CuResult(*)(void);
    using CuCuModuleUnload = CuResult(*)(void*);
    using CuCuCtxDestroy = CuResult(*)(void*);
    using CuCuGetErrorString = CuResult(*)(CuResult, const char**);
    using CuCuGetErrorName = CuResult(*)(CuResult, const char**);

    CuCuInit cuInit = nullptr;
    CuCuDeviceGetCount cuDeviceGetCount = nullptr;
    CuCuDeviceGet cuDeviceGet = nullptr;
    CuCuDeviceGetName cuDeviceGetName = nullptr;
    CuCuDeviceGetAttribute cuDeviceGetAttribute = nullptr;
    CuCuCtxCreate cuCtxCreate = nullptr;
    CuCuModuleLoadData cuModuleLoadData = nullptr;
    CuCuModuleGetFunction cuModuleGetFunction = nullptr;
    CuCuMemAlloc cuMemAlloc = nullptr;
    CuCuMemFree cuMemFree = nullptr;
    CuCuMemcpyHtoD cuMemcpyHtoD = nullptr;
    CuCuMemcpyDtoH cuMemcpyDtoH = nullptr;
    CuCuLaunchKernel cuLaunchKernel = nullptr;
    CuCuCtxSynchronize cuCtxSynchronize = nullptr;
    CuCuModuleUnload cuModuleUnload = nullptr;
    CuCuCtxDestroy cuCtxDestroy = nullptr;
    CuCuGetErrorString cuGetErrorString = nullptr;
    CuCuGetErrorName cuGetErrorName = nullptr;

    std::string errorString(CuResult r) const;
private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
    bool available_ = false;
    std::string error_;
    std::string nvrtcVersion_;
    std::string driverVersion_;
    std::string toolkitVersions_;
};

// A loaded, deployable CUDA module.
class CudaLoadedModule : public LoadedModule {
public:
    CudaLoadedModule(std::shared_ptr<CudaApi> api, void* ctx, void* module, void* func,
                     uint64_t inBuf, uint64_t outBuf, size_t bytes, std::vector<double> cpuReference, std::string artifactDigest);
    ~CudaLoadedModule() override;
    std::string kind() const override { return "cuda-nvrtc"; }
    Result<Digest> executeSmoke() override;
    void* context() const { return ctx_; }
    void* function() const { return func_; }
private:
    std::shared_ptr<CudaApi> api_;
    void* ctx_ = nullptr; void* module_ = nullptr; void* func_ = nullptr;
    uint64_t inBuf_ = 0; uint64_t outBuf_ = 0; size_t bytes_ = 0;
    std::vector<double> cpuReference_;
    std::string artifactDigest_;
};

class CudaBackend : public ICompilerBackend, public ITargetProbe {
public:
    CudaBackend();
    ~CudaBackend() override = default;

    const BackendCapabilities& capabilities() const override;
    std::string id() const override { return "cuda-nvrtc"; }
    Result<void> checkCompatible(const CompilationPlan& plan) const override;
    Result<BackendOutput> compile(const CompilationRequest& request,
                                  const CompilationPlan& plan,
                                  const KeyToolchainContext& tc) override;
    Result<ValidationDescriptor> validate(const ArtifactDescriptor& descriptor,
                                          const std::vector<uint8_t>& executable) override;
    Result<std::shared_ptr<LoadedModule>> load(const ArtifactDescriptor& descriptor,
                                               const std::vector<uint8_t>& executable) override;

    // target discovery
    Result<std::vector<TargetDescriptor>> listTargets() const override;
    Result<TargetDescriptor> defaultTarget() const override;

    // Supplied by examples/tests: the CUDA source generator + CPU reference.
    static std::string generateCudaSource(const std::vector<double>& constants, uint32_t n, Datatype dt, int blockSize);
    static std::string generateCudaSourceFromProgram(std::string_view programText, uint32_t n, Datatype dt, int blockSize);
    // Reconstruct the deterministic CPU reference for a CUDA artifact produced from
    // a CF program specialization, used for load-launch parity validation.
    static std::vector<double> computeCudaReference(const ArtifactDescriptor& d, uint64_t seed, uint32_t n, Datatype dt);
    static std::shared_ptr<CudaApi>& api();

private:
    BackendCapabilities caps_;
};

// Free the shared CudaApi (used at process shutdown for clean driver teardown).
void shutdownCuda();

} // namespace compilationfabric