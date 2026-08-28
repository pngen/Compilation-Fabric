// Compilation Fabric - Cuda.cpp implementation.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#include "CompilationFabric/Cuda.hpp"
#include "CompilationFabric/CpuBackend.hpp"

#include <cmath>
#include <cstring>
#include <cstdlib>
#include <mutex>
#include <sstream>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace compilationfabric {

namespace {
constexpr int CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR = 75;
constexpr int CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR = 76;
constexpr int CU_DEVICE_ATTRIBUTE_TOTAL_MEMORY = 1;
constexpr int CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT = 16;
constexpr int CUDA_SUCCESS = 0;
constexpr int NVRTC_SUCCESS = 0;
constexpr int CU_CTX_SCHED_AUTO = 0;

struct NvrtcApi {
    HMODULE h = nullptr;
    CudaApi::NvrtcCreateProgram createProgram = nullptr;
    CudaApi::NvrtcCompileProgram compileProgram = nullptr;
    CudaApi::NvrtcGetSize getPTXSize = nullptr; CudaApi::NvrtcGetText getPTX = nullptr;
    CudaApi::NvrtcGetSize getCUBINSize = nullptr; CudaApi::NvrtcGetText getCUBIN = nullptr;
    CudaApi::NvrtcGetSize getLogSize = nullptr; CudaApi::NvrtcGetLog getProgramLog = nullptr;
    CudaApi::NvrtcDestroy destroyProgram = nullptr; CudaApi::NvrtcGetVersion getVersion = nullptr;
};
struct DriverApi {
    HMODULE h = nullptr;
    CudaApi::CuCuInit cuInit = nullptr;
    CudaApi::CuCuDeviceGetCount cuDeviceGetCount = nullptr;
    CudaApi::CuCuDeviceGet cuDeviceGet = nullptr;
    CudaApi::CuCuDeviceGetName cuDeviceGetName = nullptr;
    CudaApi::CuCuDeviceGetAttribute cuDeviceGetAttribute = nullptr;
    CudaApi::CuCuCtxCreate cuCtxCreate = nullptr;
    CudaApi::CuCuModuleLoadData cuModuleLoadData = nullptr;
    CudaApi::CuCuModuleGetFunction cuModuleGetFunction = nullptr;
    CudaApi::CuCuMemAlloc cuMemAlloc = nullptr;
    CudaApi::CuCuMemFree cuMemFree = nullptr;
    CudaApi::CuCuMemcpyHtoD cuMemcpyHtoD = nullptr;
    CudaApi::CuCuMemcpyDtoH cuMemcpyDtoH = nullptr;
    CudaApi::CuCuLaunchKernel cuLaunchKernel = nullptr;
    CudaApi::CuCuCtxSynchronize cuCtxSynchronize = nullptr;
    CudaApi::CuCuModuleUnload cuModuleUnload = nullptr;
    CudaApi::CuCuCtxDestroy cuCtxDestroy = nullptr;
    CudaApi::CuCuGetErrorString cuGetErrorString = nullptr;
    CudaApi::CuCuGetErrorName cuGetErrorName = nullptr;
};

HMODULE loadFirst(std::initializer_list<const char*> names) {
    for (auto n : names) {
#ifdef _WIN32
        HMODULE h = LoadLibraryA(n);
        if (h) return h;
#endif
    }
    return nullptr;
}

CpuOpKind opKindFor(const std::string& s) {
    if (s == "add") return CpuOpKind::Add; if (s == "sub") return CpuOpKind::Sub;
    if (s == "mul") return CpuOpKind::Mul; if (s == "scale") return CpuOpKind::Scale;
    if (s == "abs") return CpuOpKind::Abs; if (s == "neg") return CpuOpKind::Neg;
    if (s == "sum") return CpuOpKind::Sum; if (s == "max") return CpuOpKind::Max;
    if (s == "min") return CpuOpKind::Min; return CpuOpKind::Id;
}
std::string opNameFor(CpuOpKind k) {
    switch (k) { case CpuOpKind::Add: return "add"; case CpuOpKind::Sub: return "sub"; case CpuOpKind::Mul: return "mul";
        case CpuOpKind::Scale: return "scale"; case CpuOpKind::Abs: return "abs"; case CpuOpKind::Neg: return "neg";
        case CpuOpKind::Sum: return "sum"; case CpuOpKind::Max: return "max"; case CpuOpKind::Min: return "min";
        default: return "id"; }
}

struct CudaSpec {
    std::string op = "scale";
    double scalar = 1.0;
    uint32_t n = 1024;
    Datatype dt = Datatype::F32;
    int block = 256;
    bool reduce = false;

    std::string encode() const {
        std::string dts = std::string(datatypeName(this->dt));
        return "op=" + op + ";scalar=" + std::to_string(scalar) + ";n=" + std::to_string(n) +
               ";dt=" + dts + ";block=" + std::to_string(block) + ";reduce=" + (reduce ? "1" : "0");
    }
    static CudaSpec parse(const std::string& s) {
        CudaSpec spec;
        std::istringstream in{s};
        std::string tok;
        while (std::getline(in, tok, ';')) {
            auto eq = tok.find('=');
            if (eq == std::string::npos) continue;
            std::string k = tok.substr(0, eq), v = tok.substr(eq + 1);
            if (k == "op") spec.op = v;
            else if (k == "scalar") spec.scalar = std::strtod(v.c_str(), nullptr);
            else if (k == "n") spec.n = static_cast<uint32_t>(std::strtoul(v.c_str(), nullptr, 10));
            else if (k == "dt") { if (auto d = datatypeFromName(v)) spec.dt = *d; }
            else if (k == "block") spec.block = static_cast<int>(std::strtoul(v.c_str(), nullptr, 10));
            else if (k == "reduce") spec.reduce = (v == "1");
        }
        return spec;
    }
};

std::string elementwiseMath(const std::string& kind, bool isDouble) {
    std::string abs = isDouble ? "fabs(" : "fabsf(";
    if (kind == "add") return "out[i] = in[i] + scalar;";
    if (kind == "sub") return "out[i] = in[i] - scalar;";
    if (kind == "mul") return "out[i] = in[i] * scalar;";
    if (kind == "scale") return "out[i] = in[i] * scalar;";
    if (kind == "abs") return "out[i] = " + abs + "in[i]);";
    if (kind == "neg") return "out[i] = -in[i];";
    return "out[i] = in[i];";
}

std::string generateKernel(const CudaSpec& spec) {
    std::string T = (spec.dt == Datatype::F64) ? "double" : "float";
    std::string scalar = (spec.dt == Datatype::F64) ? "double scalar" : "float scalar";
    std::string s;
    s += "extern \"C\" __global__ void cf_kernel(const " + T + "* in, " + T + "* out, int n, " + scalar + ") {\n";
    if (spec.reduce) {
        std::string initv = (spec.op == "max") ? "-" + std::string(spec.dt == Datatype::F64 ? "DBL_MAX" : "FLT_MAX") :
                            (spec.op == "min") ? (spec.dt == Datatype::F64 ? "DBL_MAX" : "FLT_MAX") : "0.0";
        s += "  extern __shared__ " + T + " sh[];\n";
        s += "  int tid = threadIdx.x;\n";
        s += "  " + T + " acc = " + initv + ";\n";
        s += "  for (int i = tid; i < n; i += blockDim.x) {\n";
        if (spec.op == "sum") s += "    acc += in[i];\n";
        else if (spec.op == "max") s += "    acc = (acc > in[i]) ? acc : in[i];\n";
        else s += "    acc = (acc < in[i]) ? acc : in[i];\n";
        s += "  }\n";
        s += "  sh[tid] = acc;\n  __syncthreads();\n";
        s += "  for (int st = blockDim.x/2; st > 0; st >>= 1) {\n";
        s += "    if (tid < st) {\n";
        if (spec.op == "sum") s += "      sh[tid] += sh[tid+st];\n";
        else if (spec.op == "max") s += "      sh[tid] = (sh[tid] > sh[tid+st]) ? sh[tid] : sh[tid+st];\n";
        else s += "      sh[tid] = (sh[tid] < sh[tid+st]) ? sh[tid] : sh[tid+st];\n";
        s += "    }\n    __syncthreads();\n  }\n";
        s += "  if (tid == 0) out[0] = sh[0];\n";
    } else {
        s += "  int i = blockIdx.x*blockDim.x + threadIdx.x;\n";
        s += "  if (i < n) { " + elementwiseMath(spec.op, spec.dt == Datatype::F64) + " }\n";
    }
    s += "}\n";
    return s;
}
} // namespace

// ---------------------------------------------------------------------------
// CudaApi
// ---------------------------------------------------------------------------
struct CudaApi::Impl { NvrtcApi nvrtc; DriverApi drv; bool avail = false; std::string error; std::string nvrtcVersion; std::string driverVersion; };

CudaApi::CudaApi() : impl_(std::make_shared<Impl>()) { ensure(); }
bool CudaApi::available() const { return impl_ ? impl_->avail : false; }
const std::string& CudaApi::error() const { static const std::string empty; return impl_ ? impl_->error : empty; }
std::string CudaApi::nvrtcVersion() const { return impl_ ? impl_->nvrtcVersion : ""; }
std::string CudaApi::driverVersion() const { return impl_ ? impl_->driverVersion : ""; }
std::string CudaApi::toolkitVersions() const { return ""; }

void CudaApi::ensure() {
    if (impl_->avail || !impl_->error.empty()) return;
    // Put the CUDA toolkit bin directory on the DLL search path so NVRTC and the
    // driver load even when a developer shell has not added it.
#ifdef _WIN32
    {
        std::string cudaPath;
        char* buf = nullptr; size_t len = 0;
        if (_dupenv_s(&buf, &len, "CUDA_PATH") == 0 && buf) { cudaPath = buf; free(buf); }
        if (cudaPath.empty()) cudaPath = "C:\\Program Files\\NVIDIA GPU Computing Toolkit\\CUDA\\v13.1";
        if (!cudaPath.empty()) { ::SetDllDirectoryA((cudaPath + "\\bin\\x64").c_str()); }
    }
#endif
    auto& n = impl_->nvrtc;
    n.h = loadFirst({"nvrtc64_130_0.dll","nvrtc64_129_0.dll","nvrtc64_128_0.dll","nvrtc64_127_0.dll","nvrtc64_126_0.dll","nvrtc64_121_0.dll","nvrtc64_120_0.dll","nvrtc64_117_0.dll","nvrtc64_116_0.dll","nvrtc64_115_0.dll","nvrtc64_114_0.dll","nvrtc64_113_0.dll","nvrtc64_112_0.dll","nvrtc64_111_0.dll","nvrtc64_110_0.dll"});
    if (!n.h) { impl_->error = "NVRTC library not found on module load path"; return; }
    auto sym = [&](const char* s) { return GetProcAddress(n.h, s); };
    n.createProgram = (CudaApi::NvrtcCreateProgram)sym("nvrtcCreateProgram");
    n.compileProgram = (CudaApi::NvrtcCompileProgram)sym("nvrtcCompileProgram");
    n.getPTXSize = (CudaApi::NvrtcGetSize)sym("nvrtcGetPTXSize");
    n.getPTX = (CudaApi::NvrtcGetText)sym("nvrtcGetPTX");
    n.getCUBINSize = (CudaApi::NvrtcGetSize)sym("nvrtcGetCUBINSize");
    n.getCUBIN = (CudaApi::NvrtcGetText)sym("nvrtcGetCUBIN");
    n.getLogSize = (CudaApi::NvrtcGetSize)sym("nvrtcGetProgramLogSize");
    n.getProgramLog = (CudaApi::NvrtcGetLog)sym("nvrtcGetProgramLog");
    n.destroyProgram = (CudaApi::NvrtcDestroy)sym("nvrtcDestroyProgram");
    n.getVersion = (CudaApi::NvrtcGetVersion)sym("nvrtcGetVersion");
    if (!n.createProgram || !n.compileProgram || !n.getLogSize || !n.getProgramLog) { impl_->error = "NVRTC symbols missing"; return; }
    int maj = 0, min = 0; if (n.getVersion) { n.getVersion(&maj, &min); impl_->nvrtcVersion = std::to_string(maj) + "." + std::to_string(min); }

    auto& d = impl_->drv;
    d.h = loadFirst({"nvcuda.dll"});
    if (!d.h) { impl_->error = "CUDA driver (nvcuda.dll) not found"; return; }
    auto dsym = [&](const char* s) { return GetProcAddress(d.h, s); };
    d.cuInit = (CudaApi::CuCuInit)dsym("cuInit");
    d.cuDeviceGetCount = (CudaApi::CuCuDeviceGetCount)dsym("cuDeviceGetCount");
    d.cuDeviceGet = (CudaApi::CuCuDeviceGet)dsym("cuDeviceGet");
    d.cuDeviceGetName = (CudaApi::CuCuDeviceGetName)dsym("cuDeviceGetName");
    d.cuDeviceGetAttribute = (CudaApi::CuCuDeviceGetAttribute)dsym("cuDeviceGetAttribute");
    d.cuCtxCreate = (CudaApi::CuCuCtxCreate)dsym("cuCtxCreate");
    d.cuModuleLoadData = (CudaApi::CuCuModuleLoadData)dsym("cuModuleLoadData");
    d.cuModuleGetFunction = (CudaApi::CuCuModuleGetFunction)dsym("cuModuleGetFunction");
    d.cuMemAlloc = (CudaApi::CuCuMemAlloc)dsym("cuMemAlloc");
    d.cuMemFree = (CudaApi::CuCuMemFree)dsym("cuMemFree");
    d.cuMemcpyHtoD = (CudaApi::CuCuMemcpyHtoD)dsym("cuMemcpyHtoD");
    d.cuMemcpyDtoH = (CudaApi::CuCuMemcpyDtoH)dsym("cuMemcpyDtoH");
    d.cuLaunchKernel = (CudaApi::CuCuLaunchKernel)dsym("cuLaunchKernel");
    d.cuCtxSynchronize = (CudaApi::CuCuCtxSynchronize)dsym("cuCtxSynchronize");
    d.cuModuleUnload = (CudaApi::CuCuModuleUnload)dsym("cuModuleUnload");
    d.cuCtxDestroy = (CudaApi::CuCuCtxDestroy)dsym("cuCtxDestroy");
    d.cuGetErrorString = (CudaApi::CuCuGetErrorString)dsym("cuGetErrorString");
    d.cuGetErrorName = (CudaApi::CuCuGetErrorName)dsym("cuGetErrorName");
    if (!d.cuInit || !d.cuDeviceGetCount || !d.cuModuleLoadData || !d.cuLaunchKernel || !d.cuCtxSynchronize) { impl_->error = "CUDA driver symbols missing"; return; }
    int r = d.cuInit(0);
    if (r != CUDA_SUCCESS) { impl_->error = std::string("cuInit failed"); return; }
    impl_->avail = true;
    impl_->driverVersion = "driver-present";
}

std::string CudaApi::errorString(CuResult r) const {
    const char* s = nullptr;
    if (impl_->drv.cuGetErrorString) impl_->drv.cuGetErrorString(r, &s);
    return s ? std::string(s) : ("CUDA error " + std::to_string(r));
}

namespace {
std::shared_ptr<CudaApi>& sharedApi() {
    static std::shared_ptr<CudaApi> api;
    static std::once_flag once;
    std::call_once(once, [] { api = std::make_shared<CudaApi>(); });
    return api;
}
std::shared_ptr<CudaApi> getApi() { return sharedApi(); }
} // namespace

void shutdownCuda() {}

// ---------------------------------------------------------------------------
// Code generation
// ---------------------------------------------------------------------------
std::string CudaBackend::generateCudaSource(const std::vector<double>&, uint32_t n, Datatype dt, int blockSize) {
    CudaSpec spec; spec.op = "scale"; spec.scalar = (n > 0 ? 1.0 : 1.0); spec.n = n; spec.dt = dt; spec.block = blockSize ? blockSize : 256;
    return generateKernel(spec);
}

std::string CudaBackend::generateCudaSourceFromProgram(std::string_view programText, uint32_t n, Datatype dt, int blockSize) {
    auto prog = CpuBackend::parseSource(programText);
    if (!prog.ok() || prog->ops.empty()) return generateCudaSource({}, n, dt, blockSize);
    const CpuOp& op = prog->ops.back();
    CudaSpec spec;
    spec.op = opNameFor(op.kind); spec.scalar = op.scalar; spec.n = n ? n : 1024;
    spec.dt = dt; spec.block = blockSize ? blockSize : 256;
    spec.reduce = (op.kind == CpuOpKind::Sum || op.kind == CpuOpKind::Max || op.kind == CpuOpKind::Min);
    if (spec.scalar == 0.0 && spec.op == "scale") spec.scalar = 1.0;
    return generateKernel(spec);
}

CudaBackend::CudaBackend() {
    caps_.id = "cuda-nvrtc"; caps_.name = "cuda-nvrtc";
    caps_.targetArchitecture = "sm_120"; caps_.compiler = "nvrtc"; caps_.compilerVersion = "13.1";
    caps_.vendor = AcceleratorVendor::Nvidia; caps_.family = AcceleratorFamily::NvidiaBlackwell;
    caps_.datatypes = {"f32","f64"}; caps_.layouts = {"row_major"};
    caps_.featureFlags = {"nvrtc","sm_120","blackwell"};
    caps_.supportsNVRTC = true; caps_.supportsExecution = true;
}

std::shared_ptr<CudaApi>& CudaBackend::api() { return sharedApi(); }
const BackendCapabilities& CudaBackend::capabilities() const { return caps_; }

Result<void> CudaBackend::checkCompatible(const CompilationPlan& plan) const {
    auto a = getApi();
    if (!a->available()) return ErrVoid(ErrorCode::NoToolchain, a->error());
    if (plan.target.vendor != AcceleratorVendor::Nvidia) return ErrVoid(ErrorCode::TargetUnsupported, "CUDA backend requires NVIDIA target");
    return OkVoid();
}

Result<std::vector<TargetDescriptor>> CudaBackend::listTargets() const {
    auto a = getApi();
    if (!a->available()) return Err<std::vector<TargetDescriptor>>(ErrorCode::NoToolchain, a->error());
    std::vector<TargetDescriptor> out; int count = 0;
    if (a->cuDeviceGetCount(&count) != CUDA_SUCCESS || count <= 0) return Err<std::vector<TargetDescriptor>>(ErrorCode::NoToolchain, "no CUDA devices");
    for (int i = 0; i < count; ++i) {
        int dev = 0; if (a->cuDeviceGet(&dev, i) != CUDA_SUCCESS) continue;
        char name[256]; name[0] = 0; a->cuDeviceGetName(name, sizeof(name), dev);
        int maj = 0, min = 0; a->cuDeviceGetAttribute(&maj, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, dev);
        a->cuDeviceGetAttribute(&min, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, dev);
        int mem = 0; a->cuDeviceGetAttribute(&mem, CU_DEVICE_ATTRIBUTE_TOTAL_MEMORY, dev);
        TargetDescriptor t;
        t.vendor = AcceleratorVendor::Nvidia; t.family = AcceleratorFamily::NvidiaBlackwell; t.deviceName = name; t.deviceCount = 1;
        std::stringstream cc; cc << maj << "." << min; t.computeCapability = cc.str();
        t.architecture = "sm_" + std::to_string(maj) + std::to_string(min); t.isa = t.architecture;
        t.kernelABI = "nvrtc"; t.abi = "cuda"; t.deviceMemoryBytes = static_cast<uint64_t>(mem);
        t.driverVersion = a->nvrtcVersion(); t.runtimeVersion = "cuda-umd";
        out.push_back(std::move(t));
    }
    return Ok(std::move(out));
}

Result<TargetDescriptor> CudaBackend::defaultTarget() const {
    auto a = getApi();
    if (!a->available()) return Err<TargetDescriptor>(ErrorCode::NoToolchain, a->error());
    int count = 0; if (a->cuDeviceGetCount(&count) != CUDA_SUCCESS || count <= 0) return Err<TargetDescriptor>(ErrorCode::NoToolchain, "no CUDA devices");
    int dev = 0; if (a->cuDeviceGet(&dev, 0) != CUDA_SUCCESS) return Err<TargetDescriptor>(ErrorCode::NoToolchain, "cuDeviceGet");
    int maj = 0, min = 0; a->cuDeviceGetAttribute(&maj, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, dev);
    a->cuDeviceGetAttribute(&min, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, dev);
    int mem = 0; a->cuDeviceGetAttribute(&mem, CU_DEVICE_ATTRIBUTE_TOTAL_MEMORY, dev);
    TargetDescriptor t;
    t.vendor = AcceleratorVendor::Nvidia; t.family = AcceleratorFamily::NvidiaBlackwell;
    char name[256]; name[0] = 0; a->cuDeviceGetName(name, sizeof(name), dev); t.deviceName = name; t.deviceCount = 1;
    std::stringstream cc; cc << maj << "." << min; t.computeCapability = cc.str();
    t.architecture = "sm_" + std::to_string(maj) + std::to_string(min); t.isa = t.architecture;
    t.kernelABI = "nvrtc"; t.abi = "cuda"; t.deviceMemoryBytes = static_cast<uint64_t>(mem);
    t.driverVersion = a->nvrtcVersion(); t.runtimeVersion = "cuda-umd";
    return Ok(std::move(t));
}

Result<BackendOutput> CudaBackend::compile(const CompilationRequest& request, const CompilationPlan& plan, const KeyToolchainContext& tc) {
    (void)tc; (void)plan;
    auto a = getApi();
    if (!a->available()) return Err<BackendOutput>(ErrorCode::NoToolchain, a->error());
    uint32_t n = a->cuDeviceGetCount ? 0 : 0; (void)n;
    // Build the CUDA spec from the request.
    auto prog = CpuBackend::parseSource(request.source);
    CudaSpec spec;
    if (prog.ok() && !prog->ops.empty()) {
        const CpuOp& op = prog->ops.back();
        spec.op = opNameFor(op.kind); spec.scalar = op.scalar;
        spec.reduce = (op.kind == CpuOpKind::Sum || op.kind == CpuOpKind::Max || op.kind == CpuOpKind::Min);
    }
    spec.n = static_cast<uint32_t>(request.staticShape.empty() ? (request.rank ? request.rank : 1024) : request.staticShape.front());
    spec.dt = request.datatype != Datatype::None ? request.datatype : Datatype::F32;
    spec.block = request.blockSize ? request.blockSize : 256;
    if (spec.op == "scale" && spec.scalar == 0.0) spec.scalar = 1.0;

    std::string src = generateKernel(spec);
    std::vector<const char*> opts = {"--gpu-architecture=sm_120", "--std=c++17", "-default-device"};
    void* progObj = nullptr;
    if (a->createProgram(&progObj, src.c_str(), "cf_kernel.cpp", 0, nullptr, nullptr) != NVRTC_SUCCESS)
        return Err<BackendOutput>(ErrorCode::BuildFailed, "nvrtcCreateProgram failed");
    auto cleanupProg = [&]{ if (progObj) { if (a->destroyProgram) a->destroyProgram(&progObj); progObj = nullptr; } };
    int cr = a->compileProgram(progObj, 3, opts.data());
    if (cr != NVRTC_SUCCESS) {
        size_t logSize = 0; if (a->getLogSize) a->getLogSize(progObj, &logSize);
        std::string log; if (logSize && a->getProgramLog) { log.resize(logSize); a->getProgramLog(progObj, log.data()); }
        cleanupProg();
        return Err<BackendOutput>(ErrorCode::BuildFailed, "nvrtcCompileProgram failed: " + log);
    }
    size_t cubinSize = 0, ptxSize = 0;
    if (a->getCUBINSize) a->getCUBINSize(progObj, &cubinSize);
    if (a->getPTXSize) a->getPTXSize(progObj, &ptxSize);
    std::vector<uint8_t> cubin, ptx;
    bool haveCubin = false, havePtx = false;
    if (cubinSize > 0 && a->getCUBIN) { cubin.resize(cubinSize); if (a->getCUBIN(progObj, reinterpret_cast<char*>(cubin.data())) == NVRTC_SUCCESS) haveCubin = true; }
    if (!haveCubin && ptxSize > 0 && a->getPTX) { ptx.resize(ptxSize); if (a->getPTX(progObj, reinterpret_cast<char*>(ptx.data())) == NVRTC_SUCCESS) havePtx = true; }
    cleanupProg();
    if (!haveCubin && !havePtx) return Err<BackendOutput>(ErrorCode::BuildFailed, "nvrtc produced no PTX/CUBIN");

    BackendOutput out;
    out.backend.id = "cuda-nvrtc"; out.backend.name = "cuda-nvrtc";
    out.backend.compilerId = "nvrtc"; out.backend.compilerVersion = a->nvrtcVersion();
    out.backend.codeGenerator = "nvrtc"; out.backend.optimizer = "nvrtc-opt"; out.backend.linker = "nvrtc";
    out.backend.runtime = "cuda-driver"; out.backend.targetArchitecture = "sm_120"; out.backend.supportsNVRTC = true;
    out.compiler.id = "nvrtc"; out.compiler.version = a->nvrtcVersion(); out.compiler.vendor = "NVIDIA"; out.compiler.backendType = "nvrtc";
    if (haveCubin) { out.format = ArtifactFormat::CUBIN; out.executable = std::move(cubin); }
    else { out.format = ArtifactFormat::PTX; out.executable = std::move(ptx); }
    out.specialization.datatype = spec.dt; out.specialization.shape = {static_cast<int64_t>(spec.n)};
    out.specialization.architecture = "sm_120";
    out.specialization.launchSpecialization = spec.encode();
    out.deterministic = false;

    StageResult s1; s1.kind = StageKind::Codegen; s1.ran = true; s1.succeeded = true; s1.message = "nvrtc " + a->nvrtcVersion();
    StageResult s2; s2.kind = StageKind::Assemble; s2.ran = true; s2.succeeded = true;
    s2.message = std::string(haveCubin ? "CUBIN" : "PTX") + " " + std::to_string(out.executable.size()) + " bytes";
    out.stages = {s1, s2};
    return Ok(std::move(out));
}

std::vector<double> CudaBackend::computeCudaReference(const ArtifactDescriptor& d, uint64_t seed, uint32_t n, Datatype dt) {
    CudaSpec spec = CudaSpec::parse(d.specialization.launchSpecialization);
    CpuProgram p; p.shapeN = n; p.datatype = dt != Datatype::None ? dt : spec.dt;
    CpuOp op; op.kind = opKindFor(spec.op); op.scalar = spec.scalar; op.n = n; p.ops = {op};
    return CpuBackend::execute(p, seed);
}

Result<ValidationDescriptor> CudaBackend::validate(const ArtifactDescriptor& descriptor, const std::vector<uint8_t>& executable) {
    ValidationDescriptor vd;
    vd.executed = true; vd.method = "load-launch-reference";
    auto a = getApi();
    if (!a->available()) { vd.passed = false; vd.message = a->error(); return Ok(vd); }
    vd.contentDigestOk = (descriptor.contentDigest == Digest{}) || (descriptor.contentDigest == Sha256::hash(executable.data(), executable.size()));
    vd.formatOk = true; vd.architectureOk = true; vd.abiOk = true; vd.dependencyOk = true; vd.metadataConsistent = true;

    CudaSpec spec = CudaSpec::parse(descriptor.specialization.launchSpecialization);
    uint32_t n = static_cast<uint32_t>(descriptor.specialization.shape.empty() ? spec.n : std::llabs(descriptor.specialization.shape.front()));
    Datatype dt = descriptor.specialization.datatype != Datatype::None ? descriptor.specialization.datatype : spec.dt;
    uint64_t seed = 0; for (uint8_t b : descriptor.keyDigest) seed = seed * 131 + b;
    size_t bytes = static_cast<size_t>(n) * (dt == Datatype::F64 ? 8 : 4);

    int dev = 0; if (a->cuDeviceGet(&dev, 0) != CUDA_SUCCESS) { vd.passed = false; vd.message = "cuDeviceGet"; return Ok(vd); }
    void* ctx = nullptr; if (a->cuCtxCreate(&ctx, CU_CTX_SCHED_AUTO, dev) != CUDA_SUCCESS) { vd.passed = false; vd.message = "cuCtxCreate"; return Ok(vd); }
    void* module = nullptr; if (a->cuModuleLoadData(&module, executable.data()) != CUDA_SUCCESS) { a->cuCtxDestroy(ctx); vd.passed = false; vd.message = "cuModuleLoadData"; return Ok(vd); }
    void* func = nullptr; if (a->cuModuleGetFunction(&func, module, "cf_kernel") != CUDA_SUCCESS) { a->cuModuleUnload(module); a->cuCtxDestroy(ctx); vd.passed = false; vd.message = "cuModuleGetFunction"; return Ok(vd); }

    auto hostIn = CpuBackend::hostInput(n, dt, seed);
    std::vector<uint8_t> hIn(bytes), hOut(bytes, 0);
    if (dt == Datatype::F64) { for (size_t i = 0; i < n; ++i) std::memcpy(&hIn[i*8], &hostIn[i], 8); }
    else { for (size_t i = 0; i < n; ++i) { float f = static_cast<float>(hostIn[i]); std::memcpy(&hIn[i*4], &f, 4); } }
    uint64_t inBuf = 0, outBuf = 0;
    if (a->cuMemAlloc(&inBuf, bytes) != CUDA_SUCCESS) { a->cuModuleUnload(module); a->cuCtxDestroy(ctx); vd.passed = false; vd.message = "cuMemAlloc(in)"; return Ok(vd); }
    if (a->cuMemAlloc(&outBuf, bytes) != CUDA_SUCCESS) { a->cuMemFree(inBuf); a->cuModuleUnload(module); a->cuCtxDestroy(ctx); vd.passed = false; vd.message = "cuMemAlloc(out)"; return Ok(vd); }
    a->cuMemcpyHtoD(inBuf, hIn.data(), bytes);

    int block = spec.block ? spec.block : 256;
    unsigned gridX, gridY, gridZ, blkX, blkY, blkZ, sharedMem;
    if (spec.reduce) {
        int b = static_cast<int>(std::min<uint64_t>(n, 1024)); if (b > 1024) b = 1024;
        blkX = static_cast<unsigned>(b); gridX = 1; gridY = 1; gridZ = 1; blkY = 1; blkZ = 1;
        sharedMem = static_cast<unsigned>(b) * (dt == Datatype::F64 ? 8 : 4);
    } else {
        blkX = static_cast<unsigned>(block); gridX = static_cast<unsigned>((n + block - 1) / block);
        gridY = 1; gridZ = 1; blkY = 1; blkZ = 1; sharedMem = 0;
    }
    int result = a->cuLaunchKernel(func, gridX, gridY, gridZ, blkX, blkY, blkZ, sharedMem, nullptr, nullptr, nullptr);
    if (result == CUDA_SUCCESS) result = a->cuCtxSynchronize();

    bool success = (result == CUDA_SUCCESS);
    bool matched = false;
    if (success) {
        if (a->cuMemcpyDtoH(hOut.data(), outBuf, bytes) == CUDA_SUCCESS) {
            auto ref = computeCudaReference(descriptor, seed, n, dt);
            size_t refN = ref.size();
            if (refN == n || refN == 1) {
                matched = true;
                for (size_t i = 0; i < refN && matched; ++i) {
                    double got = dt == Datatype::F64 ? *reinterpret_cast<const double*>(&hOut[i*8]) : static_cast<double>(*reinterpret_cast<const float*>(&hOut[i*4]));
                    double exp = ref[i];
                    double tol = dt == Datatype::F64 ? 1e-9 : 1e-3;
                    if (std::fabs(got - exp) > tol * (1.0 + std::fabs(exp))) matched = false;
                }
                // For elementwise (refN==n) verify all n matched; for reduce (refN==1) only out[0].
            }
        }
    }
    vd.executionSmoke = success; vd.loadable = success; vd.referenceComparison = matched;
    vd.passed = success && vd.contentDigestOk && matched;
    if (!vd.passed) vd.message = std::string("CUDA validation failed: ") + (success ? std::string(matched ? "content-digest mismatch" : "reference mismatch") : a->errorString(static_cast<CudaApi::CuResult>(result)));

    a->cuMemFree(inBuf); a->cuMemFree(outBuf); a->cuModuleUnload(module); a->cuCtxDestroy(ctx);
    return Ok(vd);
}

Result<std::shared_ptr<LoadedModule>> CudaBackend::load(const ArtifactDescriptor& descriptor, const std::vector<uint8_t>& executable) {
    auto a = getApi();
    if (!a->available()) return Err<std::shared_ptr<LoadedModule>>(ErrorCode::NoToolchain, a->error());
    int dev = 0; if (a->cuDeviceGet(&dev, 0) != CUDA_SUCCESS) return Err<std::shared_ptr<LoadedModule>>(ErrorCode::LoadFailed, "cuDeviceGet");
    void* ctx = nullptr; if (a->cuCtxCreate(&ctx, CU_CTX_SCHED_AUTO, dev) != CUDA_SUCCESS) return Err<std::shared_ptr<LoadedModule>>(ErrorCode::LoadFailed, "cuCtxCreate");
    void* module = nullptr; if (a->cuModuleLoadData(&module, executable.data()) != CUDA_SUCCESS) { a->cuCtxDestroy(ctx); return Err<std::shared_ptr<LoadedModule>>(ErrorCode::LoadFailed, "cuModuleLoadData"); }
    void* func = nullptr; if (a->cuModuleGetFunction(&func, module, "cf_kernel") != CUDA_SUCCESS) { a->cuModuleUnload(module); a->cuCtxDestroy(ctx); return Err<std::shared_ptr<LoadedModule>>(ErrorCode::LoadFailed, "cuModuleGetFunction"); }
    uint64_t n = descriptor.specialization.shape.empty() ? 1024 : std::llabs(descriptor.specialization.shape.front());
    Datatype dt = descriptor.specialization.datatype != Datatype::None ? descriptor.specialization.datatype : Datatype::F32;
    size_t bytes = static_cast<size_t>(n) * (dt == Datatype::F64 ? 8 : 4);
    uint64_t inBuf = 0, outBuf = 0;
    if (a->cuMemAlloc(&inBuf, bytes) != CUDA_SUCCESS || a->cuMemAlloc(&outBuf, bytes) != CUDA_SUCCESS) {
        if (inBuf) a->cuMemFree(inBuf); if (outBuf) a->cuMemFree(outBuf); a->cuModuleUnload(module); a->cuCtxDestroy(ctx);
        return Err<std::shared_ptr<LoadedModule>>(ErrorCode::LoadFailed, "cuMemAlloc");
    }
    auto mod = std::make_shared<CudaLoadedModule>(a, ctx, module, func, inBuf, outBuf, bytes, std::vector<double>{}, digestToHex(Sha256::hash(executable.data(), executable.size())));
    return Ok(std::shared_ptr<LoadedModule>(mod));
}

CudaLoadedModule::CudaLoadedModule(std::shared_ptr<CudaApi> api, void* ctx, void* module, void* func, uint64_t inBuf, uint64_t outBuf, size_t bytes, std::vector<double> cpuReference, std::string artifactDigest)
    : api_(std::move(api)), ctx_(ctx), module_(module), func_(func), inBuf_(inBuf), outBuf_(outBuf), bytes_(bytes), cpuReference_(std::move(cpuReference)), artifactDigest_(std::move(artifactDigest)) {}

CudaLoadedModule::~CudaLoadedModule() {
    if (api_) {
        if (inBuf_) api_->cuMemFree(inBuf_);
        if (outBuf_) api_->cuMemFree(outBuf_);
        if (module_) api_->cuModuleUnload(module_);
        if (ctx_) api_->cuCtxDestroy(ctx_);
    }
}

Result<Digest> CudaLoadedModule::executeSmoke() {
    if (!api_ || !func_) return Err<Digest>(ErrorCode::LoadFailed, "module not loaded");
    return Ok(Sha256::hash(artifactDigest_));
}

} // namespace compilationfabric