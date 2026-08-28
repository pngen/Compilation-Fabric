// Compilation Fabric - Descriptor.cpp implementation.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#include "CompilationFabric/Descriptor.hpp"

namespace compilationfabric {

std::string_view artifactFormatName(ArtifactFormat f) {
    switch (f) {
        case ArtifactFormat::Unknown: return "Unknown";
        case ArtifactFormat::Executable: return "Executable";
        case ArtifactFormat::Object: return "Object";
        case ArtifactFormat::StaticLibrary: return "StaticLibrary";
        case ArtifactFormat::SharedLibrary: return "SharedLibrary";
        case ArtifactFormat::PTX: return "PTX";
        case ArtifactFormat::CUBIN: return "CUBIN";
        case ArtifactFormat::Fatbinary: return "Fatbinary";
        case ArtifactFormat::Bytecode: return "Bytecode";
        case ArtifactFormat::IR: return "IR";
        case ArtifactFormat::Source: return "Source";
        case ArtifactFormat::Graph: return "Graph";
        case ArtifactFormat::KernelImage: return "KernelImage";
        case ArtifactFormat::Operational: return "Operational";
    }
    return "Unknown";
}
std::optional<ArtifactFormat> artifactFormatFromName(std::string_view s) {
    const std::pair<const char*, ArtifactFormat> arr[] = {
        {"Unknown",ArtifactFormat::Unknown},{"Executable",ArtifactFormat::Executable},{"Object",ArtifactFormat::Object},
        {"StaticLibrary",ArtifactFormat::StaticLibrary},{"SharedLibrary",ArtifactFormat::SharedLibrary},{"PTX",ArtifactFormat::PTX},
        {"CUBIN",ArtifactFormat::CUBIN},{"Fatbinary",ArtifactFormat::Fatbinary},{"Bytecode",ArtifactFormat::Bytecode},
        {"IR",ArtifactFormat::IR},{"Source",ArtifactFormat::Source},{"Graph",ArtifactFormat::Graph},
        {"KernelImage",ArtifactFormat::KernelImage},{"Operational",ArtifactFormat::Operational}};
    for (auto& p : arr) if (s == p.first) return p.second;
    return std::nullopt;
}

std::string_view stageKindName(StageKind k) {
    switch (k) {
        case StageKind::Normalize: return "normalize";
        case StageKind::Lower: return "lower";
        case StageKind::Optimize: return "optimize";
        case StageKind::Codegen: return "codegen";
        case StageKind::Assemble: return "assemble";
        case StageKind::Link: return "link";
        case StageKind::Validate: return "validate";
        case StageKind::Persist: return "persist";
        case StageKind::Deploy: return "deploy";
        case StageKind::Skip: return "skip";
        case StageKind::NotApplicable: return "not-applicable";
    }
    return "skip";
}
std::optional<StageKind> stageKindFromName(std::string_view s) {
    const std::pair<const char*, StageKind> arr[] = {
        {"normalize",StageKind::Normalize},{"lower",StageKind::Lower},{"optimize",StageKind::Optimize},
        {"codegen",StageKind::Codegen},{"assemble",StageKind::Assemble},{"link",StageKind::Link},
        {"validate",StageKind::Validate},{"persist",StageKind::Persist},{"deploy",StageKind::Deploy},
        {"skip",StageKind::Skip},{"not-applicable",StageKind::NotApplicable}};
    for (auto& p : arr) if (s == p.first) return p.second;
    return std::nullopt;
}

namespace {
const Json* jget(const Json& j, std::string_view k) { return j.get(k); }
#define JG(j, key) (j.get(key))
#define JSTR(j, key) (j.get(key) && j.get(key)->isString() ? j.get(key)->asString() : std::string())
#define JBOOL(j, key) (j.get(key) && j.get(key)->isBool() ? j.get(key)->asBool() : false)
#define JNUM(j, key) (j.get(key) && j.get(key)->isNumber() ? static_cast<uint64_t>(j.get(key)->asNumber()) : 0)
#define JSET(j, key, val) j.set(key, val)
} // namespace

// --------------------------- TargetDescriptor -------------------------------
Json TargetDescriptor::toJson() const {
    Json j = Json::object({});
    j.set("vendor", Json::str(std::string(vendorName(vendor))));
    j.set("family", Json::str(std::string(familyName(family))));
    j.set("architecture", Json::str(architecture));
    j.set("compute_capability", Json::str(computeCapability));
    j.set("isa", Json::str(isa));
    j.set("abi", Json::str(abi));
    j.set("calling_convention", Json::str(callingConvention));
    j.set("kernel_abi", Json::str(kernelABI));
    j.set("graph_abi", Json::str(graphABI));
    j.set("device_name", Json::str(deviceName));
    j.set("device_count", Json::number(deviceCount));
    j.set("device_memory_bytes", Json::number(static_cast<double>(deviceMemoryBytes)));
    j.set("driver_version", Json::str(driverVersion));
    j.set("runtime_version", Json::str(runtimeVersion));
    return j;
}
std::optional<TargetDescriptor> TargetDescriptor::fromJson(const Json& j) {
    TargetDescriptor t;
    if (auto v = vendorFromName(JSTR(j, "vendor"))) t.vendor = *v;
    if (auto f = familyFromName(JSTR(j, "family"))) t.family = *f;
    t.architecture = JSTR(j, "architecture");
    t.computeCapability = JSTR(j, "compute_capability");
    t.isa = JSTR(j, "isa");
    t.abi = JSTR(j, "abi");
    t.callingConvention = JSTR(j, "calling_convention");
    t.kernelABI = JSTR(j, "kernel_abi");
    t.graphABI = JSTR(j, "graph_abi");
    t.deviceName = JSTR(j, "device_name");
    t.deviceCount = static_cast<uint32_t>(JNUM(j, "device_count"));
    t.deviceMemoryBytes = JNUM(j, "device_memory_bytes");
    t.driverVersion = JSTR(j, "driver_version");
    t.runtimeVersion = JSTR(j, "runtime_version");
    return t;
}

// --------------------------- CompilerDescriptor -----------------------------
Json CompilerDescriptor::toJson() const {
    Json j = Json::object({});
    j.set("id", Json::str(id));
    j.set("version", Json::str(version));
    j.set("vendor", Json::str(vendor));
    j.set("backend_type", Json::str(backendType));
    return j;
}
std::optional<CompilerDescriptor> CompilerDescriptor::fromJson(const Json& j) {
    CompilerDescriptor c;
    c.id = JSTR(j, "id"); c.version = JSTR(j, "version"); c.vendor = JSTR(j, "vendor"); c.backendType = JSTR(j, "backend_type");
    return c;
}

// --------------------------- ToolchainDescriptor ----------------------------
Json ToolchainDescriptor::toJson() const {
    Json j = Json::object({});
    j.set("msvc_present", Json::boolean(msvcPresent));
    j.set("msvc", msvc.toJson());
    j.set("nvcc_present", Json::boolean(nvccPresent));
    j.set("nvcc", nvcc.toJson());
    j.set("nvrtc_present", Json::boolean(nvrtcPresent));
    j.set("nvrtc_version", Json::str(nvrtcVersion));
    j.set("cuda_driver_present", Json::boolean(cudaDriverPresent));
    j.set("cuda_driver_version", Json::str(cudaDriverVersion));
    j.set("cmake_present", Json::boolean(cmakePresent));
    j.set("cmake_version", Json::str(cmakeVersion));
    j.set("ninja_present", Json::boolean(ninjaPresent));
    j.set("ninja_version", Json::str(ninjaVersion));
    j.set("windows_sdk_present", Json::boolean(windowsSdkPresent));
    j.set("windows_sdk_version", Json::str(windowsSdkVersion));
    j.set("cuda_toolkit_present", Json::boolean(cudaToolkitPresent));
    j.set("cuda_toolkit_path", Json::str(cudaToolkitPath));
    return j;
}
std::optional<ToolchainDescriptor> ToolchainDescriptor::fromJson(const Json& j) {
    ToolchainDescriptor t;
    t.msvcPresent = JBOOL(j, "msvc_present");
    if (auto* m = j.get("msvc")) if (auto c = CompilerDescriptor::fromJson(*m)) t.msvc = *c;
    t.nvccPresent = JBOOL(j, "nvcc_present");
    if (auto* n = j.get("nvcc")) if (auto c = CompilerDescriptor::fromJson(*n)) t.nvcc = *c;
    t.nvrtcPresent = JBOOL(j, "nvrtc_present");
    t.nvrtcVersion = JSTR(j, "nvrtc_version");
    t.cudaDriverPresent = JBOOL(j, "cuda_driver_present");
    t.cudaDriverVersion = JSTR(j, "cuda_driver_version");
    t.cmakePresent = JBOOL(j, "cmake_present");
    t.cmakeVersion = JSTR(j, "cmake_version");
    t.ninjaPresent = JBOOL(j, "ninja_present");
    t.ninjaVersion = JSTR(j, "ninja_version");
    t.windowsSdkPresent = JBOOL(j, "windows_sdk_present");
    t.windowsSdkVersion = JSTR(j, "windows_sdk_version");
    t.cudaToolkitPresent = JBOOL(j, "cuda_toolkit_present");
    t.cudaToolkitPath = JSTR(j, "cuda_toolkit_path");
    return t;
}

// --------------------------- BackendDescriptor ------------------------------
Json BackendDescriptor::toJson() const {
    Json j = Json::object({});
    j.set("id", Json::str(id)); j.set("name", Json::str(name));
    j.set("compiler_id", Json::str(compilerId)); j.set("compiler_version", Json::str(compilerVersion));
    j.set("code_generator", Json::str(codeGenerator)); j.set("optimizer", Json::str(optimizer));
    j.set("linker", Json::str(linker)); j.set("runtime", Json::str(runtime));
    j.set("target_architecture", Json::str(targetArchitecture));
    j.set("supports_nvrtc", Json::boolean(supportsNVRTC));
    j.set("supports_offline", Json::boolean(supportsOffline));
    j.set("synthetic_cpu", Json::boolean(isSyntheticCPU));
    std::vector<Json> dts; for (auto& d : supportedDatatypes) dts.push_back(Json::str(d));
    j.set("supported_datatypes", Json::array(std::move(dts)));
    std::vector<Json> lts; for (auto& l : supportedLayouts) lts.push_back(Json::str(l));
    j.set("supported_layouts", Json::array(std::move(lts)));
    std::vector<Json> ff; for (auto& f : featureFlags) ff.push_back(Json::str(f));
    j.set("feature_flags", Json::array(std::move(ff)));
    j.set("capability_descriptor", Json::str(capabilityDescriptor));
    return j;
}
std::optional<BackendDescriptor> BackendDescriptor::fromJson(const Json& j) {
    BackendDescriptor b;
    b.id = JSTR(j, "id"); b.name = JSTR(j, "name"); b.compilerId = JSTR(j, "compiler_id");
    b.compilerVersion = JSTR(j, "compiler_version"); b.codeGenerator = JSTR(j, "code_generator");
    b.optimizer = JSTR(j, "optimizer"); b.linker = JSTR(j, "linker"); b.runtime = JSTR(j, "runtime");
    b.targetArchitecture = JSTR(j, "target_architecture");
    b.supportsNVRTC = JBOOL(j, "supports_nvrtc"); b.supportsOffline = JBOOL(j, "supports_offline");
    b.isSyntheticCPU = JBOOL(j, "synthetic_cpu");
    if (auto* d = j.get("supported_datatypes")) if (auto* a = d->asArrayPtr()) for (auto& e : *a) b.supportedDatatypes.push_back(e.asString());
    if (auto* l = j.get("supported_layouts")) if (auto* a = l->asArrayPtr()) for (auto& e : *a) b.supportedLayouts.push_back(e.asString());
    if (auto* f = j.get("feature_flags")) if (auto* a = f->asArrayPtr()) for (auto& e : *a) b.featureFlags.push_back(e.asString());
    b.capabilityDescriptor = JSTR(j, "capability_descriptor");
    return b;
}

// --------------------------- SourceDescriptor -------------------------------
Json SourceDescriptor::toJson() const {
    Json j = Json::object({});
    j.set("identity", Json::str(identity.toHex()));
    j.set("digest", Json::str(digestToHex(digest)));
    j.set("language", Json::str(language));
    j.set("format", Json::str(format));
    j.set("byte_size", Json::number(static_cast<double>(byteSize)));
    j.set("frontend", Json::str(frontend));
    return j;
}
std::optional<SourceDescriptor> SourceDescriptor::fromJson(const Json& j) {
    SourceDescriptor s;
    if (auto i = SourceIdentity::parse(JSTR(j, "identity"))) s.identity = *i;
    if (auto d = digestFromHex(JSTR(j, "digest"))) s.digest = *d;
    s.language = JSTR(j, "language"); s.format = JSTR(j, "format");
    s.byteSize = JNUM(j, "byte_size"); s.frontend = JSTR(j, "frontend");
    return s;
}

// --------------------------- IRDescriptor -----------------------------------
Json IRDescriptor::toJson() const {
    Json j = Json::object({});
    j.set("identity", Json::str(identity.toHex()));
    j.set("digest", Json::str(digestToHex(digest)));
    j.set("format", Json::str(format));
    j.set("byte_size", Json::number(static_cast<double>(byteSize)));
    j.set("rank", Json::number(static_cast<double>(rank)));
    j.set("datatype", Json::str(std::string(datatypeName(datatype))));
    j.set("layout", Json::str(std::string(layoutName(layout))));
    std::vector<Json> sh; for (auto e : staticShape) sh.push_back(Json::number(static_cast<double>(e)));
    j.set("static_shape", Json::array(std::move(sh)));
    j.set("symbolic_shape", Json::str(symbolicShape));
    return j;
}
std::optional<IRDescriptor> IRDescriptor::fromJson(const Json& j) {
    IRDescriptor ir;
    if (auto i = IRIdentity::parse(JSTR(j, "identity"))) ir.identity = *i;
    if (auto d = digestFromHex(JSTR(j, "digest"))) ir.digest = *d;
    ir.format = JSTR(j, "format"); ir.byteSize = JNUM(j, "byte_size"); ir.rank = JNUM(j, "rank");
    if (auto v = datatypeFromName(JSTR(j, "datatype"))) ir.datatype = *v;
    if (auto v = layoutFromName(JSTR(j, "layout"))) ir.layout = *v;
    if (auto* sh = j.get("static_shape")) if (auto* a = sh->asArrayPtr()) for (auto& e : *a) ir.staticShape.push_back(static_cast<int64_t>(e.asNumber()));
    ir.symbolicShape = JSTR(j, "symbolic_shape");
    return ir;
}

// --------------------------- SpecializationDescriptor -----------------------
Json SpecializationDescriptor::toJson() const {
    Json j = Json::object({});
    std::vector<Json> sh; for (auto e : shape) sh.push_back(Json::number(static_cast<double>(e)));
    j.set("shape", Json::array(std::move(sh)));
    j.set("datatype", Json::str(std::string(datatypeName(datatype))));
    j.set("layout", Json::str(std::string(layoutName(layout))));
    j.set("quantization", Json::str(std::string(quantizationName(quantization))));
    j.set("precision", Json::str(std::string(precisionName(precision))));
    j.set("scalar_constants", Json::str(scalarConstants));
    j.set("launch_specialization", Json::str(launchSpecialization));
    j.set("feature_flags", Json::str(featureFlags));
    j.set("architecture", Json::str(architecture));
    j.set("model_rev", Json::str(modelRev));
    return j;
}
std::optional<SpecializationDescriptor> SpecializationDescriptor::fromJson(const Json& j) {
    SpecializationDescriptor s;
    if (auto* sh = j.get("shape")) if (auto* a = sh->asArrayPtr()) for (auto& e : *a) s.shape.push_back(static_cast<int64_t>(e.asNumber()));
    if (auto v = datatypeFromName(JSTR(j, "datatype"))) s.datatype = *v;
    if (auto v = layoutFromName(JSTR(j, "layout"))) s.layout = *v;
    if (auto v = quantizationFromName(JSTR(j, "quantization"))) s.quantization = *v;
    if (auto v = precisionFromName(JSTR(j, "precision"))) s.precision = *v;
    s.scalarConstants = JSTR(j, "scalar_constants"); s.launchSpecialization = JSTR(j, "launch_specialization");
    s.featureFlags = JSTR(j, "feature_flags"); s.architecture = JSTR(j, "architecture"); s.modelRev = JSTR(j, "model_rev");
    return s;
}

// --------------------------- StageOutcome -----------------------------------
Json StageOutcome::toJson() const {
    Json j = Json::object({});
    j.set("kind", Json::str(std::string(stageKindName(kind))));
    j.set("ran", Json::boolean(ran)); j.set("succeeded", Json::boolean(succeeded));
    j.set("duration_ms", Json::number(static_cast<double>(durationMs))); j.set("message", Json::str(message));
    return j;
}

// --------------------------- ValidationDescriptor ---------------------------
Json ValidationDescriptor::toJson() const {
    Json j = Json::object({});
    j.set("executed", Json::boolean(executed)); j.set("passed", Json::boolean(passed)); j.set("method", Json::str(method));
    j.set("duration_ms", Json::number(static_cast<double>(durationMs))); j.set("message", Json::str(message));
    j.set("content_digest_ok", Json::boolean(contentDigestOk)); j.set("format_ok", Json::boolean(formatOk));
    j.set("architecture_ok", Json::boolean(architectureOk)); j.set("abi_ok", Json::boolean(abiOk));
    j.set("dependency_ok", Json::boolean(dependencyOk)); j.set("metadata_consistent", Json::boolean(metadataConsistent));
    j.set("loadable", Json::boolean(loadable)); j.set("execution_smoke", Json::boolean(executionSmoke));
    j.set("reference_comparison", Json::boolean(referenceComparison));
    return j;
}
std::optional<ValidationDescriptor> ValidationDescriptor::fromJson(const Json& j) {
    ValidationDescriptor v;
    v.executed = JBOOL(j, "executed"); v.passed = JBOOL(j, "passed"); v.method = JSTR(j, "method");
    v.durationMs = static_cast<int64_t>(JNUM(j, "duration_ms")); v.message = JSTR(j, "message");
    v.contentDigestOk = JBOOL(j, "content_digest_ok"); v.formatOk = JBOOL(j, "format_ok");
    v.architectureOk = JBOOL(j, "architecture_ok"); v.abiOk = JBOOL(j, "abi_ok");
    v.dependencyOk = JBOOL(j, "dependency_ok"); v.metadataConsistent = JBOOL(j, "metadata_consistent");
    v.loadable = JBOOL(j, "loadable"); v.executionSmoke = JBOOL(j, "execution_smoke");
    v.referenceComparison = JBOOL(j, "reference_comparison");
    return v;
}

// --------------------------- DeploymentDescriptor ---------------------------
Json DeploymentDescriptor::toJson() const {
    Json j = Json::object({});
    j.set("deployable", Json::boolean(deployable)); j.set("method", Json::str(method));
    j.set("runtime_compatible", Json::boolean(runtimeCompatible));
    j.set("architecture_validated", Json::boolean(architectureValidated));
    j.set("generation_validated", Json::boolean(generationValidated));
    j.set("last_load_duration_ms", Json::number(static_cast<double>(lastLoadDurationMs)));
    j.set("last_load_at", Json::number(static_cast<double>(lastLoadAt)));
    j.set("module_resident", Json::boolean(moduleResident));
    j.set("active_leases", Json::number(static_cast<double>(activeLeases)));
    return j;
}
std::optional<DeploymentDescriptor> DeploymentDescriptor::fromJson(const Json& j) {
    DeploymentDescriptor d;
    d.deployable = JBOOL(j, "deployable"); d.method = JSTR(j, "method");
    d.runtimeCompatible = JBOOL(j, "runtime_compatible"); d.architectureValidated = JBOOL(j, "architecture_validated");
    d.generationValidated = JBOOL(j, "generation_validated");
    d.lastLoadDurationMs = static_cast<int64_t>(JNUM(j, "last_load_duration_ms"));
    d.lastLoadAt = static_cast<int64_t>(JNUM(j, "last_load_at"));
    d.moduleResident = JBOOL(j, "module_resident"); d.activeLeases = JNUM(j, "active_leases");
    return d;
}

// --------------------------- ProvenanceDescriptor ---------------------------
Json ProvenanceDescriptor::toJson() const {
    Json j = Json::object({});
    j.set("request_id", Json::str(requestId.toHex()));
    j.set("plan_id", Json::str(planId.toHex()));
    j.set("attempt_id", Json::str(attemptId.toHex()));
    j.set("source_digest", Json::str(digestToHex(sourceDigest)));
    j.set("ir_digest", Json::str(digestToHex(irDigest)));
    j.set("toolchain_fingerprint", Json::str(toolchainFingerprint));
    j.set("environment_fingerprint", Json::str(environmentFingerprint));
    j.set("reproducibility", Json::str(std::string(reproducibilityName(reproducibility))));
    j.set("deterministic", Json::boolean(deterministic));
    j.set("compile_duration_ms", Json::number(static_cast<double>(compileDurationMs)));
    j.set("optimize_duration_ms", Json::number(static_cast<double>(optimizeDurationMs)));
    j.set("analyze_duration_ms", Json::number(static_cast<double>(analyzeDurationMs)));
    j.set("validate_duration_ms", Json::number(static_cast<double>(validateDurationMs)));
    j.set("link_duration_ms", Json::number(static_cast<double>(linkDurationMs)));
    j.set("deploy_duration_ms", Json::number(static_cast<double>(deployDurationMs)));
    j.set("created_at", Json::number(static_cast<double>(createdAt)));
    return j;
}
std::optional<ProvenanceDescriptor> ProvenanceDescriptor::fromJson(const Json& j) {
    ProvenanceDescriptor p;
    if (auto i = CompilationRequestId::parse(JSTR(j, "request_id"))) p.requestId = *i;
    if (auto i = CompilationPlanId::parse(JSTR(j, "plan_id"))) p.planId = *i;
    if (auto i = CompilationAttemptId::parse(JSTR(j, "attempt_id"))) p.attemptId = *i;
    if (auto d = digestFromHex(JSTR(j, "source_digest"))) p.sourceDigest = *d;
    if (auto d = digestFromHex(JSTR(j, "ir_digest"))) p.irDigest = *d;
    p.toolchainFingerprint = JSTR(j, "toolchain_fingerprint");
    p.environmentFingerprint = JSTR(j, "environment_fingerprint");
    if (auto v = reproducibilityFromName(JSTR(j, "reproducibility"))) p.reproducibility = *v;
    p.deterministic = JBOOL(j, "deterministic");
    p.compileDurationMs = static_cast<int64_t>(JNUM(j, "compile_duration_ms"));
    p.optimizeDurationMs = static_cast<int64_t>(JNUM(j, "optimize_duration_ms"));
    p.analyzeDurationMs = static_cast<int64_t>(JNUM(j, "analyze_duration_ms"));
    p.validateDurationMs = static_cast<int64_t>(JNUM(j, "validate_duration_ms"));
    p.linkDurationMs = static_cast<int64_t>(JNUM(j, "link_duration_ms"));
    p.deployDurationMs = static_cast<int64_t>(JNUM(j, "deploy_duration_ms"));
    p.createdAt = static_cast<int64_t>(JNUM(j, "created_at"));
    return p;
}

// --------------------------- ArtifactDescriptor -----------------------------
Json ArtifactDescriptor::toJson() const {
    Json j = Json::object({});
    j.set("id", Json::str(id.toHex()));
    j.set("generation", Json::number(static_cast<double>(generation)));
    j.set("format", Json::str(std::string(artifactFormatName(format))));
    j.set("content_digest", Json::str(digestToHex(contentDigest)));
    j.set("byte_size", Json::number(static_cast<double>(byteSize)));
    j.set("key_digest", Json::str(digestToHex(keyDigest)));
    j.set("state", Json::str(state));
    j.set("target", target.toJson());
    j.set("compiler", compiler.toJson());
    j.set("backend", backend.toJson());
    j.set("specialization", specialization.toJson());
    j.set("validation", validation.toJson());
    j.set("deployment", deployment.toJson());
    j.set("provenance", provenance.toJson());
    j.set("namespace", Json::str(namespaceName));
    j.set("tenant", Json::str(tenant));
    j.set("created_at", Json::number(static_cast<double>(createdAt)));
    j.set("last_access", Json::number(static_cast<double>(lastAccess)));
    j.set("reuse_count", Json::number(static_cast<double>(reuseCount)));
    return j;
}
std::optional<ArtifactDescriptor> ArtifactDescriptor::fromJson(const Json& j) {
    ArtifactDescriptor a;
    if (auto i = ArtifactId::parse(JSTR(j, "id"))) a.id = *i;
    a.generation = static_cast<ArtifactGeneration>(JNUM(j, "generation"));
    if (auto f = artifactFormatFromName(JSTR(j, "format"))) a.format = *f;
    if (auto d = digestFromHex(JSTR(j, "content_digest"))) a.contentDigest = *d;
    a.byteSize = JNUM(j, "byte_size");
    if (auto d = digestFromHex(JSTR(j, "key_digest"))) a.keyDigest = *d;
    a.state = JSTR(j, "state");
    if (auto* t = j.get("target")) if (auto v = TargetDescriptor::fromJson(*t)) a.target = *v;
    if (auto* t = j.get("compiler")) if (auto v = CompilerDescriptor::fromJson(*t)) a.compiler = *v;
    if (auto* t = j.get("backend")) if (auto v = BackendDescriptor::fromJson(*t)) a.backend = *v;
    if (auto* t = j.get("specialization")) if (auto v = SpecializationDescriptor::fromJson(*t)) a.specialization = *v;
    if (auto* t = j.get("validation")) if (auto v = ValidationDescriptor::fromJson(*t)) a.validation = *v;
    if (auto* t = j.get("deployment")) if (auto v = DeploymentDescriptor::fromJson(*t)) a.deployment = *v;
    if (auto* t = j.get("provenance")) if (auto v = ProvenanceDescriptor::fromJson(*t)) a.provenance = *v;
    a.namespaceName = JSTR(j, "namespace"); a.tenant = JSTR(j, "tenant");
    a.createdAt = static_cast<int64_t>(JNUM(j, "created_at"));
    a.lastAccess = static_cast<int64_t>(JNUM(j, "last_access"));
    a.reuseCount = JNUM(j, "reuse_count");
    return a;
}

} // namespace compilationfabric