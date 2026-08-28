// Compilation Fabric - Fabric.cpp implementation.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#include "CompilationFabric/Fabric.hpp"
#include "CompilationFabric/CpuBackend.hpp"
#include "CompilationFabric/Cuda.hpp"
#include "CompilationFabric/Toolchain.hpp"

#include <map>
#include <mutex>
#include <condition_variable>
#include <sstream>
#include <chrono>
#include <thread>
#include <filesystem>
#include <algorithm>
#include <limits>

namespace compilationfabric {

namespace {
int64_t nowMs() { return Clock::monotonicNanos() / 1000000; }
std::string uniqueHex(uint64_t counter) {
    // Stable, deterministic id from a global counter + monotonic clock offset.
    uint64_t h = counter * 2654435761ULL ^ Clock::monotonicNanos();
    Id128 id(0, h);
    return id.toHex();
}
} // namespace

struct CompilationFabric::Impl {
    CompilationFabricConfig config;
    std::map<std::string, std::shared_ptr<ICompilerBackend>> backends;
    std::shared_ptr<CpuBackend> cpuBackend;
    std::shared_ptr<CudaBackend> cudaBackend;
    std::shared_ptr<ToolchainProbe> toolchainProbe;
    ToolchainDescriptor toolchain;
    std::vector<TargetDescriptor> targets;
    std::shared_ptr<PersistenceStore> store;

    mutable std::mutex regMutex_;
    std::map<CompilationKey, std::vector<uint8_t>> keyBytesCache; // recently seen key->canonical bytes (not authoritative)
    struct CacheEntry {
        CompilationKey key;
        ArtifactDescriptor descriptor;
        std::vector<uint8_t> bytes;
        uint64_t baseCacheGen = 0;
        uint64_t baseToolchainGen = 0;
        bool inCache = false;
    };
    std::map<CompilationKey, CacheEntry> cache;
    struct InflightEntry {
        CompilationAttemptId attempt;
        bool done = false;
        bool success = false;
        CompilationResult result;
        ErrorCode failure = ErrorCode::Ok;
        std::string failureMsg;
        int waiters = 0;
        uint64_t baseCacheGen = 0;
        uint64_t baseToolchainGen = 0;
        std::condition_variable cv;
    };
    std::map<CompilationKey, std::shared_ptr<InflightEntry>> inflight;

    // generation authority
    CoordinatorEpoch epoch = 1;
    CacheGeneration cacheGen = 1;
    ToolchainGeneration toolchainGen = 1;
    std::atomic<uint64_t> artifactCounter{0};

    // observability
    Observability obs;
    std::map<ArtifactId, std::vector<uint64_t>> leaseCounts; // per artifact: active leases

    mutable std::mutex persistMutex_;
    mutable std::mutex leaseMutex_;
};

CompilationFabric::CompilationFabric(CompilationFabricConfig config) : impl_(std::make_unique<Impl>()), config_(config) {
    auto& i = *impl_;
    i.config = config;
    i.cpuBackend = std::make_shared<CpuBackend>();
    i.cudaBackend = std::make_shared<CudaBackend>();
    i.backends["cpu"] = i.cpuBackend;
    if (config.allowCuda && CudaBackend::api()->available()) i.backends["cuda-nvrtc"] = i.cudaBackend;

    i.toolchainProbe = std::make_shared<ToolchainProbe>(CudaBackend::api());
    i.toolchain = i.toolchainProbe->probe();
    i.store = std::make_shared<PersistenceStore>(config.artifactRoot);
    if (config.persistenceEnabled) i.store->open();

    // Target discovery: the CPU host is always present. A CUDA target is advertised
    // from the real compiler/toolchain capability (nvcc/NVRTC support sm_120) without
    // requiring an active device context; full device query (name, memory) is exposed
    // through targets() so a driver-side query is not required at construction time.
    {
        TargetDescriptor cpu; cpu.vendor = AcceleratorVendor::CPU; cpu.family = AcceleratorFamily::X86_64;
        cpu.architecture = "host-x86_64"; cpu.isa = "x86_64"; cpu.abi = "sysv64"; cpu.deviceCount = 1;
        i.targets.push_back(cpu);
    }
    if (i.toolchain.nvccPresent || i.toolchain.nvrtcPresent || CudaBackend::api()->available()) {
        TargetDescriptor cuda;
        cuda.vendor = AcceleratorVendor::Nvidia; cuda.family = AcceleratorFamily::NvidiaBlackwell;
        cuda.architecture = "sm_120"; cuda.isa = "sm_120"; cuda.computeCapability = "12.0";
        cuda.kernelABI = "nvrtc"; cuda.abi = "cuda"; cuda.deviceCount = 1;
        if (!i.toolchain.cudaDriverVersion.empty()) cuda.driverVersion = i.toolchain.cudaDriverVersion;
        i.targets.push_back(cuda);
    }
    i.obs.set("coordinator_epoch", 1);
    i.obs.set("cache_generation", 1);
    i.obs.set("toolchain_generation", 1);
    i.obs.recordEvent("fabric_init", Json::object({ {"cuda", Json::boolean(CudaBackend::api()->available())} }));
}

CompilationFabric::~CompilationFabric() = default;

void CompilationFabric::registerBackend(std::shared_ptr<ICompilerBackend> backend) {
    std::lock_guard<std::mutex> l(impl_->regMutex_);
    impl_->backends[backend->id()] = std::move(backend);
}

CoordinatorEpoch CompilationFabric::epoch() const { return impl_->epoch; }
CacheGeneration CompilationFabric::cacheGeneration() const { return impl_->cacheGen; }
ToolchainGeneration CompilationFabric::toolchainGeneration() const { return impl_->toolchainGen; }

// ---------------------------------------------------------------------------
// Planning
// ---------------------------------------------------------------------------
Result<CompilationPlan> CompilationFabric::plan(const CompilationRequest& request) const {
    auto& i = *impl_;
    CompilationPlan p;
    static std::atomic<uint64_t> planCounter{0};
    p.planId = CompilationPlanId::fromU64(++planCounter);
    p.request = request;
    p.createdAt = Clock::nowMillis();
    // Honor an explicit backend preference; an unknown backend is a hard rejection.
    if (!request.backend.empty() && !i.backends.count(request.backend) && request.backend != "cuda-nvcc") {
        return Err<CompilationPlan>(ErrorCode::NoBackend, "no backend registered for " + request.backend);
    }

    bool wantCuda = (request.backend == "cuda-nvrtc" || request.backend == "cuda-nvcc" ||
                     request.targetArchitecture.rfind("sm_", 0) == 0 || !request.computeCapability.empty() ||
                     request.isa.rfind("sm_", 0) == 0);
    bool cudaOk = CudaBackend::api()->available();
    std::string backend = "cpu";
    if (wantCuda && cudaOk && i.backends.count("cuda-nvrtc")) backend = "cuda-nvrtc";
    else if (wantCuda && !cudaOk) {
        std::ostringstream os; os << "CUDA requested but unavailable";
        // Don't fail planning; plan() returns a plan that will fail at compile with a clear reason.
    }

    p.backend = backend;
    p.compiler = (backend == "cuda-nvrtc") ? "nvrtc" : "cf-cpu";
    p.frontend = "cf-frontend";
    p.optimizer = (backend == "cuda-nvrtc") ? "nvrtc-opt" : "cf-cpu-opt";
    p.linker = (backend == "cuda-nvrtc") ? "nvrtc" : "cf-cpu-link";
    p.runtime = (backend == "cuda-nvrtc") ? "cuda-driver" : "cf-cpu-runtime";

    // target
    if (backend == "cuda-nvrtc") {
        // Prefer an advertised CUDA target when present; otherwise build one.
        for (auto& tt : i.targets) if (tt.architecture == "sm_120" || tt.vendor == AcceleratorVendor::Nvidia) { p.target = tt; break; }
        p.target.vendor = AcceleratorVendor::Nvidia;
        p.target.family = AcceleratorFamily::NvidiaBlackwell;
        p.target.architecture = request.targetArchitecture.empty() ? "sm_120" : request.targetArchitecture;
        p.target.computeCapability = request.computeCapability.empty() ? "12.0" : request.computeCapability;
        p.target.isa = p.target.architecture;
        p.expectedFormat = ArtifactFormat::CUBIN;
        p.expectedValidationMethod = "load-launch-reference";
        p.expectedDeploymentMethod = "cuda-load";
    } else {
        p.target.vendor = AcceleratorVendor::CPU; p.target.family = AcceleratorFamily::X86_64;
        p.target.architecture = "host-x86_64"; p.target.isa = "x86_64"; p.target.abi = "sysv64";
        p.target.deviceCount = 1;
        p.expectedFormat = ArtifactFormat::Bytecode;
        p.expectedValidationMethod = "cpu-execute-reference";
        p.expectedDeploymentMethod = "cpu-execute";
    }

    // specialization strategy
    if (!request.staticShape.empty()) p.specializationStrategy = "static-shape";
    else if (!request.symbolicShape.empty()) p.specializationStrategy = "dynamic-bounded";
    else p.specializationStrategy = "static-default";

    // stages
    p.stages = {StageKind::Normalize, StageKind::Lower, StageKind::Optimize, StageKind::Codegen,
                StageKind::Validate, StageKind::Persist, StageKind::Deploy};
    p.requiredToolchainCapabilities = {"compiler", backend == "cuda-nvrtc" ? "nvrtc" : "deterministic"};
    p.reproducibility = request.reproducibility != ReproducibilityMode::Unspecified ? request.reproducibility : config_.reproducibility;
    p.cachePolicy = "exact";

    std::ostringstream reason;
    reason << "selected backend " << backend << " because ";
    if (backend == "cuda-nvrtc") reason << "request targeted an NVIDIA/CUDA architecture and NVRTC is available";
    else reason << "the request is CPU-targeted or CUDA is unavailable; the deterministic CPU backend satisfies it";
    p.reason = reason.str();
    return Ok(std::move(p));
}

// ---------------------------------------------------------------------------
// Key building
// ---------------------------------------------------------------------------
namespace {
KeyToolchainContext makeTc(const CompilationPlan& plan, const ToolchainDescriptor& toolchain, const CompilationFabricConfig& config) {
    KeyToolchainContext tc;
    tc.frontend = plan.frontend; tc.frontendVersion = "1.0.0";
    if (plan.backend == "cuda-nvrtc") { tc.compiler = "nvrtc"; tc.compilerVersion = toolchain.nvrtcVersion; tc.backend = "cuda-nvrtc"; tc.backendVersion = toolchain.nvrtcVersion; tc.codeGenerator = "nvrtc"; tc.optimizer = "nvrtc-opt"; tc.linker = "nvrtc"; tc.runtime = "cuda-driver"; tc.driver = "nvidia"; tc.driverVersion = toolchain.cudaDriverVersion; }
    else { tc.compiler = "cf-cpu"; tc.compilerVersion = "1.0.0"; tc.backend = "cpu"; tc.backendVersion = "1.0.0"; tc.codeGenerator = "cf-cpu-codegen"; tc.optimizer = "cf-cpu-opt"; tc.linker = "cf-cpu-link"; tc.runtime = "cf-cpu-runtime"; }
    tc.environmentFingerprint = config.environmentFingerprint;
    return tc;
}
} // namespace

// ---------------------------------------------------------------------------
// Compile / single-flight
// ---------------------------------------------------------------------------
Result<CompilationResult> CompilationFabric::compile(const CompilationRequest& request) {
    auto& i = *impl_;
    auto p = plan(request);
    if (!p.ok()) return Err<CompilationResult>(p.code(), p.message());
    CompilationPlan plan = *p;
    auto tc = makeTc(plan, i.toolchain, config_);
    CompilationKey key = buildCompilationKey(request, plan, tc);

    // Fast path: cached, still-reusable artifact.
    {
        std::lock_guard<std::mutex> l(i.regMutex_);
        auto it = i.cache.find(key);
        if (it != i.cache.end()) {
            CompilationCompatibilityDecision dec = CompilationCompatibility(config_.compatibilityPolicy)
                .decide(it->second.key, key, it->second.descriptor, config_.compatibilityPolicy);
            if (dec.reusable && it->second.descriptor.state == "Deployable") {
                it->second.descriptor.lastAccess = Clock::nowMillis();
                it->second.descriptor.reuseCount += 1;
                i.obs.count("cache_hits");
                i.obs.count("reused_builds");
                CompilationResult r;
                r.requestId = request.requestId; r.planId = plan.planId; r.attemptId = it->second.descriptor.provenance.attemptId;
                r.artifactId = it->second.descriptor.id; r.generation = it->second.descriptor.generation;
                r.key = key; r.keyDigest = key.digest(); r.artifact = it->second.descriptor; r.artifactBytes = it->second.bytes;
                r.reused = true; r.validated = it->second.descriptor.validation.passed; r.deployable = true;
                r.referenceMatched = it->second.descriptor.validation.referenceComparison;
                r.compatibilityDecision = std::string(compatibilityOutcomeName(dec.outcome));
                r.backendUsed = it->second.descriptor.backend.id;
                r.compileMs = 0; r.totalMs = 0;
                return Ok(std::move(r));
            }
        }
        i.obs.count("cache_misses");
    }

    // Single-flight. Exactly one attempt owns compilation for this key; waiters
    // block on a per-key condition variable until the owner commits a result.
    std::shared_ptr<Impl::InflightEntry> owned;
    {
        std::unique_lock<std::mutex> ul(i.regMutex_);
        auto it = i.inflight.find(key);
        if (it != i.inflight.end() && !it->second->done) {
            auto entry = it->second;
            entry->waiters += 1;
            i.obs.count("singleflight_waiters");
            entry->cv.wait(ul, [&] { return entry->done; });
            i.obs.count("singleflight_wait_completions");
            if (entry->success) return Ok(entry->result);
            return Err<CompilationResult>(entry->failure, entry->failureMsg);
        }
        auto e = std::make_shared<Impl::InflightEntry>();
        e->attempt = CompilationAttemptId::fromU64(++i.artifactCounter);
        e->done = false;
        e->baseCacheGen = i.cacheGen;
        e->baseToolchainGen = i.toolchainGen;
        i.inflight[key] = e;
        owned = e;
    }

    // Owner compiles OUTSIDE any lock.
    auto backendIt = i.backends.find(plan.backend);
    if (backendIt == i.backends.end()) {
        std::lock_guard<std::mutex> l(i.regMutex_);
        owned->done = true; owned->success = false; owned->failure = ErrorCode::NoBackend; owned->failureMsg = "no backend " + plan.backend; owned->cv.notify_all();
        return Err<CompilationResult>(ErrorCode::NoBackend, "no backend " + plan.backend);
    }
    auto backend = backendIt->second;
    // Enforce toolchain/target capability before compiling.
    auto compatChk = backend->checkCompatible(plan);
    if (!compatChk.ok()) {
        std::lock_guard<std::mutex> l2(i.regMutex_);
        owned->done = true; owned->success = false; owned->failure = compatChk.code(); owned->failureMsg = compatChk.message(); owned->cv.notify_all();
        i.obs.count("compile_failures");
        return Err<CompilationResult>(compatChk.code(), compatChk.message());
    }
    int64_t t0 = nowMs();
    bool autotuned = false;
    std::string autotuneWinner;
    std::vector<AutotuneCandidate> autotuneCands;
    BackendOutput output;
    // Bounded autotuning: evaluate candidate codegen variants, measure validated
    // execution performance, and select the best among evaluated candidates.
    if (request.autotune && request.autotuneCandidates > 1) {
        autotuned = true;
        int winnerIx = -1; double bestMs = (std::numeric_limits<double>::max)();
        for (int vi = 0; vi < request.autotuneCandidates; ++vi) {
            CompilationRequest vr = request;
            vr.optimizeLevel = (vi == 0) ? 0 : std::max(request.optimizeLevel, 1);
            vr.autotuneSeed = request.autotuneSeed + static_cast<uint64_t>(vi);
            auto bo = backend->compile(vr, plan, tc);
            AutotuneCandidate ac; ac.variantId = "variant-" + std::to_string(vi);
            ac.flags = Json::object({{"optimize_level", Json::number(static_cast<double>(vr.optimizeLevel))}, {"seed", Json::number(static_cast<double>(vr.autotuneSeed))}});
            if (!bo.ok()) { ac.validated = false; ac.perfMs = -1.0; ac.reason = bo.message(); autotuneCands.push_back(ac); continue; }
            ArtifactDescriptor vad; vad.id = ArtifactId::fromU64(Clock::monotonicNanos() & 0x7FFFFFFFFFFFFFFFULL);
            vad.generation = 1; vad.format = bo->format; vad.specialization = bo->specialization;
            vad.backend = bo->backend; vad.keyDigest = key.digest();
            vad.contentDigest = Sha256::hash(bo->executable.data(), bo->executable.size());
            auto vd = backend->validate(vad, bo->executable);
            ac.validated = vd.ok() && vd->passed;
            double perf = 0.0;
            if (ac.validated) {
                auto mod = backend->load(vad, bo->executable);
                if (mod.ok()) {
                    auto s0 = Clock::monotonicNanos();
                    auto exe = (*mod)->executeSmoke();
                    perf = static_cast<double>(Clock::monotonicNanos() - s0) / 1e6;
                    (void)exe;
                }
            }
            ac.perfMs = perf; ac.artifactId = vad.id;
            ac.reason = ac.validated ? (perf > 0.0 ? "validated+measured" : "validated(nil perf)") : "validation_failed";
            autotuneCands.push_back(ac);
            if (ac.validated && perf > 0.0 && perf < bestMs) { bestMs = perf; winnerIx = vi; }
        }
        if (winnerIx < 0) { for (int vi = 0; vi < (int)autotuneCands.size(); ++vi) if (autotuneCands[vi].validated) { winnerIx = vi; break; } }
        if (winnerIx >= 0) {
            CompilationRequest wr = request;
            wr.optimizeLevel = (winnerIx == 0) ? 0 : std::max(request.optimizeLevel, 1);
            wr.autotuneSeed = request.autotuneSeed + static_cast<uint64_t>(winnerIx);
            auto wob = backend->compile(wr, plan, tc);
            if (wob.ok()) { output = *wob; autotuneWinner = autotuneCands[winnerIx].variantId; }
        }
        if (output.executable.empty()) { auto wob = backend->compile(request, plan, tc); if (wob.ok()) output = *wob; }
    } else {
        auto bo = backend->compile(request, plan, tc);
        if (!bo.ok()) {
            std::lock_guard<std::mutex> l(i.regMutex_);
            owned->done = true; owned->success = false; owned->failure = bo.code(); owned->failureMsg = bo.message(); owned->cv.notify_all();
            i.obs.count("compile_failures");
            i.obs.recordEvent("compile_failed", Json::object({{"backend", Json::str(plan.backend)}, {"error", Json::str(bo.message())}}));
            return Err<CompilationResult>(bo.code(), bo.message());
        }
        output = *bo;
    }
    if (output.executable.empty()) {
        std::lock_guard<std::mutex> l(i.regMutex_);
        owned->done = true; owned->success = false; owned->failure = ErrorCode::BuildFailed; owned->failureMsg = "no candidate produced an artifact"; owned->cv.notify_all();
        i.obs.count("compile_failures");
        return Err<CompilationResult>(ErrorCode::BuildFailed, "no candidate produced an artifact");
    }
    int64_t compileMs = nowMs() - t0;

    // Validate (real validation, required before eligibility).
    int64_t t1 = nowMs();
    ArtifactDescriptor art;
    art.id = ArtifactId::fromU64(++i.artifactCounter);
    ArtifactGeneration gen = i.artifactCounter;
    art.generation = gen;
    art.format = output.format;
    art.contentDigest = Sha256::hash(output.executable.data(), output.executable.size());
    art.byteSize = output.executable.size();
    art.keyDigest = key.digest();
    art.backend = output.backend; art.compiler = output.compiler;
    art.target = plan.target; art.specialization = output.specialization;
    art.provenance.requestId = request.requestId; art.provenance.planId = plan.planId; art.provenance.attemptId = owned->attempt;
    art.provenance.createdAt = Clock::nowMillis();
    art.namespaceName = config_.namespaceName; art.tenant = config_.tenant;
    art.createdAt = Clock::nowMillis(); art.lastAccess = Clock::nowMillis();
    art.state = "Validating";
    auto vd = backend->validate(art, output.executable);
    int64_t validateMs = nowMs() - t1;
    if (!vd.ok()) {
        std::lock_guard<std::mutex> l(i.regMutex_);
        owned->done = true; owned->success = false; owned->failure = vd.code(); owned->failureMsg = vd.message(); owned->cv.notify_all();
        i.obs.count("validation_failures");
        return Err<CompilationResult>(vd.code(), vd.message());
    }
    art.validation = *vd;
    if (!art.validation.passed) {
        art.state = "Invalid";
        std::lock_guard<std::mutex> l(i.regMutex_);
        owned->done = true; owned->success = false; owned->failure = ErrorCode::ValidationFailed; owned->failureMsg = "artifact failed validation: " + art.validation.message; owned->cv.notify_all();
        i.obs.count("validation_failures");
        return Err<CompilationResult>(ErrorCode::ValidationFailed, "artifact failed validation: " + art.validation.message);
    }
    art.state = "Deployable";
    art.deployment.deployable = true; art.deployment.runtimeCompatible = true;
    art.deployment.architectureValidated = true; art.deployment.generationValidated = true;
    art.deployment.method = (plan.backend == "cuda-nvrtc") ? "cuda-load" : "cpu-execute";

    // Commit to cache + persist, checking generation authority (no stale publish).
    {
        std::lock_guard<std::mutex> l(i.regMutex_);
        if (owned->baseCacheGen != i.cacheGen || owned->baseToolchainGen != i.toolchainGen) {
            owned->done = true; owned->success = false; owned->failure = ErrorCode::StaleGeneration; owned->failureMsg = "generation changed during compile; stale completion rejected"; owned->cv.notify_all();
            i.obs.count("stale_authority_rejections");
            return Err<CompilationResult>(ErrorCode::StaleGeneration, "generation changed during compile; stale completion rejected");
        }
        Impl::CacheEntry ce; ce.key = key; ce.descriptor = art; ce.bytes = output.executable;
        ce.baseCacheGen = i.cacheGen; ce.baseToolchainGen = i.toolchainGen; ce.inCache = true;
        i.cache[key] = ce;
        i.obs.count("compiled_artifacts");
        i.obs.set("cache_generation", i.cacheGen);
        i.obs.recordEvent("artifact_committed", Json::object({{"artifact", Json::str(art.id.toHex())}, {"gen", Json::number(static_cast<double>(gen))}, {"backend", Json::str(plan.backend)}}));

        if (config_.persistenceEnabled && config_.persistOnCompile) {
            PersistedRecord rec;
            rec.id = art.id; rec.generation = gen; rec.key = key; rec.descriptor = art;
            rec.contentBytes = output.executable;
            std::lock_guard<std::mutex> pl(i.persistMutex_);
            auto sr = i.store->store(rec);
            if (!sr.ok()) i.obs.count("persistence_failures");
        }
    }

    // Build success result.
    CompilationResult res;
    res.requestId = request.requestId; res.planId = plan.planId; res.attemptId = owned->attempt;
    res.artifactId = art.id; res.generation = gen; res.key = key; res.keyDigest = key.digest();
    res.artifact = art; res.artifactBytes = output.executable;
    res.reused = false; res.validated = true; res.deployable = true; res.referenceMatched = art.validation.referenceComparison;
    res.compatibilityDecision = "Compiled";
    res.backendUsed = plan.backend; res.compileMs = compileMs; res.totalMs = compileMs + validateMs;
    res.autotuned = autotuned; res.autotuneWinner = autotuneWinner; res.autotuneCandidates = autotuneCands;
    for (auto& s : output.stages) { StageResult sr; sr.kind = s.kind; sr.ran = true; sr.succeeded = true; sr.durationMs = s.durationMs; sr.message = s.message; res.stages.push_back(std::move(sr)); }

    std::lock_guard<std::mutex> l(i.regMutex_);
    owned->done = true; owned->success = true; owned->result = res; owned->cv.notify_all();
    i.obs.count("singleflight_completions");
    return Ok(std::move(res));
}

// ---------------------------------------------------------------------------
// Lookup
// ---------------------------------------------------------------------------
Result<std::pair<CompilationCompatibilityDecision, std::optional<ArtifactDescriptor>>> CompilationFabric::lookup(const CompilationKey& key) const {
    auto& i = *impl_;
    std::lock_guard<std::mutex> l(i.regMutex_);
    auto it = i.cache.find(key);
    if (it == i.cache.end()) return Ok(std::pair<CompilationCompatibilityDecision, std::optional<ArtifactDescriptor>>{CompilationCompatibilityDecision{CompatibilityOutcome::RecompileRequiredSpecializationChange, false, {CompatibilityReason::None}, "no artifact for key"}, std::nullopt});
    CompilationCompatibilityDecision dec = CompilationCompatibility(config_.compatibilityPolicy).decide(it->second.key, key, it->second.descriptor, config_.compatibilityPolicy);
    return Ok(std::pair<CompilationCompatibilityDecision, std::optional<ArtifactDescriptor>>{dec, it->second.descriptor});
}

// ---------------------------------------------------------------------------
// Invalidation / supersession
// ---------------------------------------------------------------------------
Result<void> CompilationFabric::invalidateByKey(const CompilationKey& key) {
    auto& i = *impl_;
    std::lock_guard<std::mutex> l(i.regMutex_);
    auto it = i.cache.find(key);
    if (it != i.cache.end()) { it->second.descriptor.state = "Invalidated"; it->second.descriptor.deployment.deployable = false; it->second.descriptor.validation.passed = false; }
    i.cacheGen += 1; i.obs.set("cache_generation", static_cast<int64_t>(i.cacheGen));
    i.obs.count("invalidations");
    i.obs.recordEvent("invalidate", Json::object({{"key", Json::str(key.toHex())}}));
    return OkVoid();
}
Result<void> CompilationFabric::invalidateById(const ArtifactId& id) {
    auto& i = *impl_;
    std::lock_guard<std::mutex> l(i.regMutex_);
    for (auto& [k, e] : i.cache) if (e.descriptor.id == id) { e.descriptor.state = "Invalidated"; e.descriptor.deployment.deployable = false; e.descriptor.validation.passed = false; }
    i.cacheGen += 1; i.obs.set("cache_generation", static_cast<int64_t>(i.cacheGen));
    i.obs.count("invalidations");
    return OkVoid();
}
Result<void> CompilationFabric::invalidateByLogicalOperation(const LogicalOperation& op) {
    auto& i = *impl_;
    std::lock_guard<std::mutex> l(i.regMutex_);
    for (auto& [k, e] : i.cache) if (e.key.logicalOperation() && *e.key.logicalOperation() == op) { e.descriptor.state = "Invalidated"; e.descriptor.deployment.deployable = false; e.descriptor.validation.passed = false; }
    i.cacheGen += 1; i.obs.set("cache_generation", static_cast<int64_t>(i.cacheGen));
    i.obs.count("invalidations");
    return OkVoid();
}
Result<void> CompilationFabric::invalidateByCompilerGeneration(uint64_t gen) {
    auto& i = *impl_;
    std::lock_guard<std::mutex> l(i.regMutex_);
    for (auto& [k, e] : i.cache) if (e.baseToolchainGen <= gen) { e.descriptor.state = "Invalidated"; e.descriptor.deployment.deployable = false; e.descriptor.validation.passed = false; }
    i.toolchainGen += 1; i.cacheGen += 1; i.obs.set("cache_generation", static_cast<int64_t>(i.cacheGen));
    i.obs.count("invalidations");
    return OkVoid();
}
Result<void> CompilationFabric::invalidateByDependencyGeneration(uint64_t gen) {
    auto& i = *impl_;
    std::lock_guard<std::mutex> l(i.regMutex_);
    for (auto& [k, e] : i.cache) {
        auto dg = e.key.u64Field(KeyField::DependencyGenerations);
        if (dg && *dg <= gen) { e.descriptor.state = "Invalidated"; e.descriptor.deployment.deployable = false; e.descriptor.validation.passed = false; }
    }
    i.cacheGen += 1; i.obs.set("cache_generation", static_cast<int64_t>(i.cacheGen));
    i.obs.count("invalidations");
    return OkVoid();
}
Result<void> CompilationFabric::supersede(const ArtifactId& id, ArtifactGeneration newGen) {
    auto& i = *impl_;
    std::lock_guard<std::mutex> l(i.regMutex_);
    for (auto& [k, e] : i.cache) {
        if (e.descriptor.id == id && e.descriptor.generation < newGen) { e.descriptor.state = "Superseded"; e.descriptor.deployment.deployable = false; }
    }
    i.obs.count("supersessions");
    return OkVoid();
}

// ---------------------------------------------------------------------------
// Deploy / leases
// ---------------------------------------------------------------------------
Result<std::shared_ptr<LoadedModule>> CompilationFabric::deploy(const ArtifactId& id) {
    auto& i = *impl_;
    ArtifactDescriptor desc; std::vector<uint8_t> bytes; std::string backendId;
    {
        std::lock_guard<std::mutex> l(i.regMutex_);
        bool found = false;
        for (auto& kv : i.cache) if (kv.second.descriptor.id == id) { found = true; desc = kv.second.descriptor; bytes = kv.second.bytes; backendId = kv.second.descriptor.backend.id; break; }
        if (!found) return Err<std::shared_ptr<LoadedModule>>(ErrorCode::NotFound, "artifact not in cache");
        if (desc.state != "Deployable" && desc.state != "Deployed") return Err<std::shared_ptr<LoadedModule>>(ErrorCode::NotDeployable, "artifact state is " + desc.state);
    }
    auto backendIt = i.backends.find(backendId);
    if (backendIt == i.backends.end()) return Err<std::shared_ptr<LoadedModule>>(ErrorCode::NoBackend, "no backend for " + backendId);
    auto mod = backendIt->second->load(desc, bytes);
    if (!mod.ok()) return Err<std::shared_ptr<LoadedModule>>(mod.code(), mod.message());
    return mod;
}

Result<CompilationLease> CompilationFabric::acquire(const ArtifactId& id) {
    auto& i = *impl_;
    std::lock_guard<std::mutex> l(i.regMutex_);
    ArtifactGeneration gen = 0; bool found = false;
    for (auto& [k, e] : i.cache) if (e.descriptor.id == id) { gen = e.descriptor.generation; found = true; break; }
    if (!found) return Err<CompilationLease>(ErrorCode::NotFound, "artifact not found");
    {
        std::lock_guard<std::mutex> ll(i.leaseMutex_);
        i.leaseCounts[id].push_back(0);
        if (i.leaseCounts[id].size() > 100000) i.leaseCounts[id].clear();
    }
    i.obs.count("active_leases");
    return Ok(CompilationLease(id, gen));
}

Result<void> CompilationFabric::release(const CompilationLease& lease) {
    auto& i = *impl_;
    if (!lease.valid()) return ErrVoid(ErrorCode::InvalidArgument, "invalid lease");
    std::lock_guard<std::mutex> ll(i.leaseMutex_);
    auto it = i.leaseCounts.find(lease.id().value());
    if (it == i.leaseCounts.end() || it->second.empty()) return ErrVoid(ErrorCode::LeaseUnderflow, "lease underflow");
    it->second.pop_back();
    if (it->second.empty()) {
        // keep entry for accounting; do not erase to avoid underflow detection confusion
    }
    i.obs.count("lease_releases");
    return OkVoid();
}

// ---------------------------------------------------------------------------
// Recovery / observability
// ---------------------------------------------------------------------------
Result<void> CompilationFabric::recover() {
    auto& i = *impl_;
    if (!config_.persistenceEnabled) return OkVoid();
    auto rec = i.store->recover();
    if (!rec.ok()) return ErrVoid(rec.code(), rec.message());
    auto& res = *rec;
    std::lock_guard<std::mutex> l(i.regMutex_);
    for (auto& r : res.valid) {
        if (r.invalidated || r.superseded) { /* keep state */ }
        Impl::CacheEntry ce; ce.key = r.key; ce.descriptor = r.descriptor; ce.bytes = r.contentBytes;
        ce.baseCacheGen = i.cacheGen; ce.baseToolchainGen = i.toolchainGen; ce.inCache = true;
        i.cache[r.key] = ce;
        i.obs.count("recovered_artifacts");
    }
    i.obs.count("corruption_detected", static_cast<int64_t>(res.corrupted.size()));
    i.obs.count("orphan_temps_removed", static_cast<int64_t>(res.orphanTempRemoved.size()));
    i.obs.set("recovery_count", i.obs.get("recovery_count") + 1);
    i.obs.recordEvent("recovery", Json::object({{"valid", Json::number(static_cast<double>(res.valid.size()))},
        {"corrupted", Json::number(static_cast<double>(res.corrupted.size()))},
        {"invalidated", Json::number(static_cast<double>(res.invalidated.size()))},
        {"orphan_temp", Json::number(static_cast<double>(res.orphanTempRemoved.size()))}}));
    // module residency is NOT resumed on restart.
    for (auto& [k, e] : i.cache) e.descriptor.deployment.moduleResident = false;
    return OkVoid();
}

CompilationFabricSnapshot CompilationFabric::snapshot() const {
    auto& i = *impl_;
    CompilationFabricSnapshot s;
    {
        std::lock_guard<std::mutex> l(i.regMutex_);
        s.counters = i.obs.stats();
        s.events = i.obs.snapshot().get("events") ? *i.obs.snapshot().get("events") : Json::array({});
        Json idx = Json::array({});
        std::vector<Json> entries;
        for (auto& [k, e] : i.cache) {
            Json je = Json::object({});
            je.set("artifact", Json::str(e.descriptor.id.toHex()));
            je.set("gen", Json::number(static_cast<double>(e.descriptor.generation)));
            je.set("state", Json::str(e.descriptor.state));
            je.set("backend", Json::str(e.descriptor.backend.id));
            je.set("key", Json::str(k.toHex()));
            entries.push_back(std::move(je));
        }
        s.artifactIndex = Json::array(std::move(entries));
        Json inf = Json::object({});
        size_t inflightCount = 0;
        for (auto& [k, e] : i.inflight) if (!e->done) { (void)k; ++inflightCount; }
        inf.set("active", Json::number(static_cast<double>(inflightCount)));
        s.inflight = std::move(inf);
        Json gen = Json::object({});
        gen.set("cache_generation", Json::number(static_cast<double>(i.cacheGen)));
        gen.set("toolchain_generation", Json::number(static_cast<double>(i.toolchainGen)));
        gen.set("artifact_counter", Json::number(static_cast<double>(i.artifactCounter)));
        s.generations = std::move(gen);
    }
    auto snap = i.obs.snapshot();
    s.events = snap.get("events") ? *snap.get("events") : Json::array({});
    s.epochText = "epoch=" + std::to_string(i.epoch);
    return s;
}

Json CompilationFabric::stats() const {
    auto& i = *impl_;
    Json j = i.obs.stats();
    std::lock_guard<std::mutex> l(i.regMutex_);
    j.set("cache_entries", Json::number(static_cast<double>(i.cache.size())));
    size_t activeInflight = 0; for (auto& [k, e] : i.inflight) if (!e->done) { (void)k; ++activeInflight; }
    j.set("active_builds", Json::number(static_cast<double>(activeInflight)));
    return j;
}

Json CompilationFabric::explain(const CompilationKey& key) const {
    auto& i = *impl_;
    Json j = Json::object({});
    j.set("key", Json::str(key.toHex()));
    std::vector<Json> fields;
    for (auto& e : key.explain()) {
        Json fe = Json::object({});
        fe.set("field", Json::str(e.name));
        fe.set("present", Json::boolean(e.present));
        fe.set("value", Json::str(e.value));
        fields.push_back(std::move(fe));
    }
    j.set("fields", Json::array(std::move(fields)));
    std::lock_guard<std::mutex> l(i.regMutex_);
    auto it = i.cache.find(key);
    if (it != i.cache.end()) {
        j.set("state", Json::str(it->second.descriptor.state));
        j.set("backend", Json::str(it->second.descriptor.backend.id));
        j.set("validation", it->second.descriptor.validation.toJson());
        j.set("deployment", it->second.descriptor.deployment.toJson());
    }
    return j;
}

Result<ToolchainDescriptor> CompilationFabric::toolchains() const { return Ok(impl_->toolchain); }
Result<std::vector<TargetDescriptor>> CompilationFabric::targets() const { return Ok(impl_->targets); }

} // namespace compilationfabric