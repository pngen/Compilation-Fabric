// Compilation Fabric - Request.cpp implementation.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#include "CompilationFabric/Request.hpp"
#include "CompilationFabric/Canonical.hpp"

namespace compilationfabric {

namespace {
const Json* jget(const Json& j, std::string_view k) { return j.get(k); }
#define JSTR(j, key) (j.get(key) && j.get(key)->isString() ? j.get(key)->asString() : std::string())
#define JBOOL(j, key) (j.get(key) && j.get(key)->isBool() ? j.get(key)->asBool() : false)
#define JNUM64(j, key) (j.get(key) && j.get(key)->isNumber() ? static_cast<uint64_t>(j.get(key)->asNumber()) : 0)
#define JNUMI(j, key) (j.get(key) && j.get(key)->isNumber() ? static_cast<int64_t>(j.get(key)->asNumber()) : 0)
#define JNUMD(j, key) (j.get(key) && j.get(key)->isNumber() ? static_cast<double>(j.get(key)->asNumber()) : 0.0)

Json toStrArray(const std::vector<std::string>& v) {
    std::vector<Json> a; for (auto& s : v) a.push_back(Json::str(s));
    return Json::array(std::move(a));
}
std::vector<std::string> fromStrArray(const Json& j) {
    std::vector<std::string> out;
    if (auto* a = j.asArrayPtr()) for (auto& e : *a) out.push_back(e.asString());
    return out;
}

Id128 idFor(const Digest& d) {
    Id128 id;
    for (int i = 0; i < 16; ++i) { id.hi = (id.hi << 8) | d[i]; id.lo = (id.lo << 8) | d[i + 16]; }
    return id;
}
} // namespace

Json CompilationRequest::toJson() const {
    Json j = Json::object({});
    j.set("request_id", Json::str(requestId.toHex()));
    j.set("logical_operation", Json::str(logicalOperation.toHex()));
    j.set("source", Json::str(source));
    j.set("source_digest", Json::str(digestToHex(sourceDigest)));
    j.set("source_language", Json::str(sourceLanguage));
    j.set("ir_digest", Json::str(digestToHex(irDigest)));
    j.set("ir_format", Json::str(irFormat));
    j.set("datatype", Json::str(std::string(datatypeName(datatype))));
    j.set("layout", Json::str(std::string(layoutName(layout))));
    j.set("rank", Json::number(static_cast<double>(rank)));
    std::vector<Json> sh; for (auto e : staticShape) sh.push_back(Json::number(static_cast<double>(e)));
    j.set("static_shape", Json::array(std::move(sh)));
    j.set("symbolic_shape", Json::str(symbolicShape));
    j.set("alignment", Json::number(static_cast<double>(alignment)));
    j.set("quantization", Json::str(std::string(quantizationName(quantization))));
    j.set("precision", Json::str(std::string(precisionName(precision))));
    j.set("scalar_constants", Json::str(scalarConstants));
    j.set("launch_specialization", Json::str(launchSpecialization));
    j.set("target_architecture", Json::str(targetArchitecture));
    j.set("compute_capability", Json::str(computeCapability));
    j.set("isa", Json::str(isa));
    j.set("abi", Json::str(abi));
    j.set("kernel_abi", Json::str(kernelABI));
    j.set("backend", Json::str(backend));
    j.set("compiler_id", Json::str(compilerId));
    j.set("frontend", Json::str(frontend));
    j.set("feature_flags", Json::str(featureFlags));
    j.set("optimization_flags", Json::str(optimizationFlags));
    j.set("codegen_flags", Json::str(codegenFlags));
    j.set("debug_release", Json::str(std::string(debugReleaseName(debugRelease))));
    j.set("determinism", Json::str(std::string(determinismName(determinism))));
    j.set("reproducibility", Json::str(std::string(reproducibilityName(reproducibility))));
    std::vector<Json> di; for (auto& e : dependencyIdentities) di.push_back(Json::str(e.toHex()));
    j.set("dependency_identities", Json::array(std::move(di)));
    std::vector<Json> dg; for (auto e : dependencyGenerations) dg.push_back(Json::number(static_cast<double>(e)));
    j.set("dependency_generations", Json::array(std::move(dg)));
    j.set("model_revision", Json::str(modelRevision));
    j.set("namespace", Json::str(namespaceName));
    j.set("tenant", Json::str(tenant));
    j.set("autotune", Json::boolean(autotune));
    j.set("autotune_candidates", Json::number(static_cast<double>(autotuneCandidates)));
    j.set("autotune_seed", Json::number(static_cast<double>(autotuneSeed)));
    j.set("block_size", Json::number(static_cast<double>(blockSize)));
    j.set("unroll_factor", Json::number(static_cast<double>(unrollFactor)));
    j.set("offline_preferred", Json::boolean(offlinePreferred));
    return j;
}

std::optional<CompilationRequest> CompilationRequest::fromJson(const Json& j) {
    CompilationRequest r;
    if (auto i = CompilationRequestId::parse(JSTR(j, "request_id"))) r.requestId = *i;
    if (auto i = LogicalOperation::parse(JSTR(j, "logical_operation"))) r.logicalOperation = *i;
    r.source = JSTR(j, "source");
    if (auto d = digestFromHex(JSTR(j, "source_digest"))) r.sourceDigest = *d;
    r.sourceLanguage = JSTR(j, "source_language");
    if (auto d = digestFromHex(JSTR(j, "ir_digest"))) r.irDigest = *d;
    r.irFormat = JSTR(j, "ir_format");
    if (auto v = datatypeFromName(JSTR(j, "datatype"))) r.datatype = *v;
    if (auto v = layoutFromName(JSTR(j, "layout"))) r.layout = *v;
    r.rank = JNUM64(j, "rank");
    if (auto* sh = j.get("static_shape")) if (auto* a = sh->asArrayPtr()) for (auto& e : *a) r.staticShape.push_back(static_cast<int64_t>(e.asNumber()));
    r.symbolicShape = JSTR(j, "symbolic_shape");
    r.alignment = JNUM64(j, "alignment");
    if (auto v = quantizationFromName(JSTR(j, "quantization"))) r.quantization = *v;
    if (auto v = precisionFromName(JSTR(j, "precision"))) r.precision = *v;
    r.scalarConstants = JSTR(j, "scalar_constants"); r.launchSpecialization = JSTR(j, "launch_specialization");
    r.targetArchitecture = JSTR(j, "target_architecture"); r.computeCapability = JSTR(j, "compute_capability");
    r.isa = JSTR(j, "isa"); r.abi = JSTR(j, "abi"); r.kernelABI = JSTR(j, "kernel_abi");
    r.backend = JSTR(j, "backend"); r.compilerId = JSTR(j, "compiler_id"); r.frontend = JSTR(j, "frontend");
    r.featureFlags = JSTR(j, "feature_flags"); r.optimizationFlags = JSTR(j, "optimization_flags"); r.codegenFlags = JSTR(j, "codegen_flags");
    if (auto v = debugReleaseFromName(JSTR(j, "debug_release"))) r.debugRelease = *v;
    if (auto v = determinismFromName(JSTR(j, "determinism"))) r.determinism = *v;
    if (auto v = reproducibilityFromName(JSTR(j, "reproducibility"))) r.reproducibility = *v;
    if (auto* di = j.get("dependency_identities")) if (auto* a = di->asArrayPtr()) for (auto& e : *a) if (auto id = Id128::parse(e.asString())) r.dependencyIdentities.push_back(*id);
    if (auto* dg = j.get("dependency_generations")) if (auto* a = dg->asArrayPtr()) for (auto& e : *a) r.dependencyGenerations.push_back(static_cast<uint64_t>(e.asNumber()));
    r.modelRevision = JSTR(j, "model_revision");
    r.namespaceName = JSTR(j, "namespace"); r.tenant = JSTR(j, "tenant");
    r.autotune = JBOOL(j, "autotune"); r.autotuneCandidates = static_cast<int>(JNUM64(j, "autotune_candidates"));
    r.autotuneSeed = JNUM64(j, "autotune_seed");
    r.blockSize = static_cast<int>(JNUM64(j, "block_size")); r.unrollFactor = static_cast<int>(JNUM64(j, "unroll_factor"));
    r.offlinePreferred = JBOOL(j, "offline_preferred");
    return r;
}

Json CompilationPlan::toJson() const {
    Json j = Json::object({});
    j.set("plan_id", Json::str(planId.toHex()));
    j.set("request", request.toJson());
    j.set("reason", Json::str(reason));
    j.set("frontend", Json::str(frontend)); j.set("compiler", Json::str(compiler));
    j.set("backend", Json::str(backend)); j.set("optimizer", Json::str(optimizer));
    j.set("linker", Json::str(linker)); j.set("runtime", Json::str(runtime));
    j.set("target", target.toJson());
    j.set("specialization_strategy", Json::str(specializationStrategy));
    j.set("expected_format", Json::str(std::string(artifactFormatName(expectedFormat))));
    std::vector<Json> di; for (auto& e : dependencyIdentities) di.push_back(Json::str(e.toHex()));
    j.set("dependency_identities", Json::array(std::move(di)));
    std::vector<Json> dg; for (auto e : dependencyGenerations) dg.push_back(Json::number(static_cast<double>(e)));
    j.set("dependency_generations", Json::array(std::move(dg)));
    std::vector<Json> st; for (auto s : stages) st.push_back(Json::str(std::string(stageKindName(s))));
    j.set("stages", Json::array(std::move(st)));
    j.set("required_capabilities", toStrArray(requiredToolchainCapabilities));
    j.set("expected_validation_method", Json::str(expectedValidationMethod));
    j.set("expected_deployment_method", Json::str(expectedDeploymentMethod));
    j.set("reproducibility", Json::str(std::string(reproducibilityName(reproducibility))));
    j.set("cache_policy", Json::str(cachePolicy));
    j.set("created_at", Json::number(static_cast<double>(createdAt)));
    return j;
}
std::optional<CompilationPlan> CompilationPlan::fromJson(const Json& j) {
    CompilationPlan p;
    if (auto i = CompilationPlanId::parse(JSTR(j, "plan_id"))) p.planId = *i;
    if (auto* rq = j.get("request")) if (auto v = CompilationRequest::fromJson(*rq)) p.request = *v;
    p.reason = JSTR(j, "reason");
    p.frontend = JSTR(j, "frontend"); p.compiler = JSTR(j, "compiler"); p.backend = JSTR(j, "backend");
    p.optimizer = JSTR(j, "optimizer"); p.linker = JSTR(j, "linker"); p.runtime = JSTR(j, "runtime");
    if (auto* t = j.get("target")) if (auto v = TargetDescriptor::fromJson(*t)) p.target = *v;
    p.specializationStrategy = JSTR(j, "specialization_strategy");
    if (auto v = artifactFormatFromName(JSTR(j, "expected_format"))) p.expectedFormat = *v;
    if (auto* di = j.get("dependency_identities")) if (auto* a = di->asArrayPtr()) for (auto& e : *a) if (auto id = Id128::parse(e.asString())) p.dependencyIdentities.push_back(*id);
    if (auto* dg = j.get("dependency_generations")) if (auto* a = dg->asArrayPtr()) for (auto& e : *a) p.dependencyGenerations.push_back(static_cast<uint64_t>(e.asNumber()));
    if (auto* st = j.get("stages")) if (auto* a = st->asArrayPtr()) for (auto& e : *a) if (auto s = stageKindFromName(e.asString())) p.stages.push_back(*s);
    if (auto* rc = j.get("required_capabilities")) p.requiredToolchainCapabilities = fromStrArray(*rc);
    p.expectedValidationMethod = JSTR(j, "expected_validation_method"); p.expectedDeploymentMethod = JSTR(j, "expected_deployment_method");
    if (auto v = reproducibilityFromName(JSTR(j, "reproducibility"))) p.reproducibility = *v;
    p.cachePolicy = JSTR(j, "cache_policy"); p.createdAt = JNUMI(j, "created_at");
    return p;
}

Json CompilationResult::toJson() const {
    Json j = Json::object({});
    j.set("request_id", Json::str(requestId.toHex()));
    j.set("plan_id", Json::str(planId.toHex()));
    j.set("attempt_id", Json::str(attemptId.toHex()));
    j.set("artifact_id", Json::str(artifactId.toHex()));
    j.set("generation", Json::number(static_cast<double>(generation)));
    j.set("key", Json::str(key.toHex()));
    j.set("artifact", artifact.toJson());
    j.set("reused", Json::boolean(reused));
    j.set("validated", Json::boolean(validated));
    j.set("deployable", Json::boolean(deployable));
    j.set("reference_matched", Json::boolean(referenceMatched));
    j.set("compatibility_decision", Json::str(compatibilityDecision));
    j.set("error", Json::str(std::string(errorCodeName(error))));
    j.set("error_message", Json::str(errorMessage));
    j.set("compile_ms", Json::number(static_cast<double>(compileMs)));
    j.set("total_ms", Json::number(static_cast<double>(totalMs)));
    j.set("backend_used", Json::str(backendUsed));
    return j;
}

CompilationKey buildCompilationKey(const CompilationRequest& r, const CompilationPlan& p, const KeyToolchainContext& t) {
    CompilationKey k;
    k.logicalOperation(r.logicalOperation);
    k.sourceIdentity(SourceIdentity(idFor(r.sourceDigest)));
    k.sourceDigest(r.sourceDigest);
    k.sourceContent(r.source); // includes exact source content as semantic identity
    if (r.irDigest != Digest{}) { k.irIdentity(IRIdentity(idFor(r.irDigest))); k.irDigest(r.irDigest); }
    if (!r.irFormat.empty()) k.irFormat(r.irFormat);
    if (!t.frontend.empty()) k.frontend(t.frontend);
    if (!t.frontendVersion.empty()) k.frontendVersion(t.frontendVersion);
    if (!t.compiler.empty()) k.compiler(t.compiler);
    if (!t.compilerVersion.empty()) k.compilerVersion(t.compilerVersion);
    if (!t.backend.empty()) k.backend(t.backend);
    if (!t.backendVersion.empty()) k.backendVersion(t.backendVersion);
    if (!t.codeGenerator.empty()) k.codeGenerator(t.codeGenerator);
    if (!t.optimizer.empty()) k.optimizer(t.optimizer);
    if (!t.optimizerVersion.empty()) k.optimizerVersion(t.optimizerVersion);
    if (!t.linker.empty()) k.linker(t.linker);
    if (!t.linkerVersion.empty()) k.linkerVersion(t.linkerVersion);
    if (!t.runtime.empty()) k.runtime(t.runtime);
    if (!t.runtimeVersion.empty()) k.runtimeVersion(t.runtimeVersion);
    if (!t.driver.empty()) k.driver(t.driver);
    if (!t.driverVersion.empty()) k.driverVersion(t.driverVersion);
    // target
    k.acceleratorVendor(p.target.vendor);
    k.acceleratorFamily(p.target.family);
    std::string arch = !p.target.architecture.empty() ? p.target.architecture : r.targetArchitecture;
    if (!arch.empty()) k.targetArchitecture(arch);
    std::string cc = !p.target.computeCapability.empty() ? p.target.computeCapability : r.computeCapability;
    if (!cc.empty()) k.computeCapability(cc);
    std::string isa = !p.target.isa.empty() ? p.target.isa : r.isa;
    if (!isa.empty()) k.isa(isa);
    std::string abi = !p.target.abi.empty() ? p.target.abi : r.abi;
    if (!abi.empty()) k.abi(abi);
    if (!r.kernelABI.empty()) k.kernelABI(r.kernelABI);
    if (!p.target.kernelABI.empty()) k.kernelABI(p.target.kernelABI);
    if (!p.target.graphABI.empty()) k.graphABI(p.target.graphABI);
    // semantic specialization
    if (r.datatype != Datatype::None) k.datatype(r.datatype);
    else k.datatype(p.request.datatype);
    if (r.layout != Layout::None) k.layout(r.layout); else k.layout(p.request.layout);
    if (r.rank) k.rank(r.rank); else k.rank(p.request.rank);
    if (!r.staticShape.empty()) k.staticShape(r.staticShape); else k.staticShape(p.request.staticShape);
    if (!r.symbolicShape.empty()) k.symbolicShape(r.symbolicShape);
    if (r.alignment) k.alignment(r.alignment); else k.alignment(p.request.alignment);
    if (r.quantization != QuantizationMode::None) k.quantization(r.quantization);
    if (r.precision != PrecisionMode::None) k.precision(r.precision);
    if (!r.launchSpecialization.empty()) k.launchSpecialization(r.launchSpecialization);
    if (!r.scalarConstants.empty()) k.scalarConstants(r.scalarConstants);
    if (!r.featureFlags.empty()) k.featureFlags(r.featureFlags);
    if (!r.optimizationFlags.empty()) k.optimizationFlags(r.optimizationFlags);
    if (!r.codegenFlags.empty()) k.codegenFlags(r.codegenFlags);
    k.debugRelease(r.debugRelease);
    if (r.determinism != DeterminismMode::Unspecified) k.determinism(r.determinism);
    if (r.reproducibility != ReproducibilityMode::Unspecified) k.reproducibility(r.reproducibility);
    if (!t.environmentFingerprint.empty()) k.environmentFingerprint(t.environmentFingerprint);
    if (!r.dependencyIdentities.empty()) k.dependencyIdentities(r.dependencyIdentities);
    if (!r.dependencyGenerations.empty()) k.dependencyGenerations(r.dependencyGenerations);
    if (!r.modelRevision.empty()) k.modelRevision(r.modelRevision);
    k.runtimeCompatibilityGeneration(t.runtimeCompatGeneration);
    k.compilerPolicyGeneration(t.compilerPolicyGeneration);
    if (!r.namespaceName.empty()) k.namespaceName(r.namespaceName);
    if (!r.tenant.empty()) k.tenant(r.tenant);
    // model/operator revision
    return k;
}

} // namespace compilationfabric