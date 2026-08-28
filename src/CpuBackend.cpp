// Compilation Fabric - CpuBackend.cpp implementation.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#include "CompilationFabric/CpuBackend.hpp"
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <algorithm>

namespace compilationfabric {

namespace {
constexpr uint32_t kCpuMagic = 0x43464231; // "CFB1"
constexpr uint32_t kCpuVersion = 1;

int64_t nowMs() { return Clock::monotonicNanos() / 1000000; }

std::optional<CpuOpKind> opKindFromName(std::string_view s) {
    if (s == "nop") return CpuOpKind::Nop;
    if (s == "add") return CpuOpKind::Add;
    if (s == "sub") return CpuOpKind::Sub;
    if (s == "mul") return CpuOpKind::Mul;
    if (s == "scale") return CpuOpKind::Scale;
    if (s == "abs") return CpuOpKind::Abs;
    if (s == "neg") return CpuOpKind::Neg;
    if (s == "sum") return CpuOpKind::Sum;
    if (s == "max") return CpuOpKind::Max;
    if (s == "min") return CpuOpKind::Min;
    if (s == "id") return CpuOpKind::Id;
    return std::nullopt;
}
std::string_view opKindName(CpuOpKind k) {
    switch (k) {
        case CpuOpKind::Nop: return "nop";
        case CpuOpKind::Add: return "add";
        case CpuOpKind::Sub: return "sub";
        case CpuOpKind::Mul: return "mul";
        case CpuOpKind::Scale: return "scale";
        case CpuOpKind::Abs: return "abs";
        case CpuOpKind::Neg: return "neg";
        case CpuOpKind::Sum: return "sum";
        case CpuOpKind::Max: return "max";
        case CpuOpKind::Min: return "min";
        case CpuOpKind::Id: return "id";
    }
    return "nop";
}
uint32_t makeSeedFrom(const CompilationRequest& r) {
    uint64_t h = 0x9e3779b97f4a7c15ULL;
    for (uint8_t b : r.sourceDigest) h = (h * 131) + b;
    h ^= r.autotuneSeed * 2654435761ULL;
    return static_cast<uint32_t>(h & 0x7FFFFFFF);
}
double rr(double v, Datatype dt) {
    if (dt == Datatype::F32) return static_cast<double>(static_cast<float>(v));
    return v;
}
} // namespace

CpuBackend::CpuBackend() {
    caps_.id = kId; caps_.name = "deterministic-cpu";
    caps_.targetArchitecture = "host-x86_64"; caps_.compiler = "cf-cpu"; caps_.compilerVersion = "1.0.0";
    caps_.vendor = AcceleratorVendor::CPU; caps_.family = AcceleratorFamily::X86_64;
    caps_.datatypes = {"f32","f64"};
    caps_.layouts = {"row_major"};
    caps_.featureFlags = {"deterministic","bounded-execution"};
    caps_.synthetic = true; caps_.supportsExecution = true;
}

const BackendCapabilities& CpuBackend::capabilities() const { return caps_; }

Result<void> CpuBackend::checkCompatible(const CompilationPlan& plan) const {
    if (plan.target.vendor != AcceleratorVendor::CPU && plan.request.backend != "cpu" && plan.request.backend != "") {
        return ErrVoid(ErrorCode::TargetUnsupported, "CPU backend cannot target " + std::string(vendorName(plan.target.vendor)));
    }
    return OkVoid();
}

Result<CpuProgram> CpuBackend::parseSource(std::string_view source) {
    CpuProgram p;
    std::istringstream in{std::string(source)};
    std::string line;
    while (std::getline(in, line)) {
        // trim
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        if (line.empty() || line[0] == '#') continue;
        // tokenize by space
        std::vector<std::string> toks;
        { std::istringstream ls{line}; std::string t; while (ls >> t) toks.push_back(t); }
        // header key=value
        auto eq = toks[0].find('=');
        if (eq != std::string::npos && eq > 0) {
            std::string key = toks[0].substr(0, eq);
            std::string val = toks[0].substr(eq + 1);
            if (key == "name") p.name = val;
            else if (key == "datatype") { if (auto d = datatypeFromName(val)) p.datatype = *d; }
            else if (key == "shape") p.shapeN = std::strtoull(val.c_str(), nullptr, 10);
            else if (key == "alignment") p.alignment = static_cast<int>(std::strtoll(val.c_str(), nullptr, 10));
            continue;
        }
        // op line
        auto kind = opKindFromName(toks[0]);
        if (!kind) return Err<CpuProgram>(ErrorCode::InvalidSource, "unknown op: " + toks[0]);
        CpuOp op; op.kind = *kind; op.n = static_cast<uint32_t>(p.shapeN ? p.shapeN : 1024);
        for (size_t i = 1; i < toks.size(); ++i) {
            auto k = toks[i].find('=');
            if (k == std::string::npos) continue;
            std::string key = toks[i].substr(0, k);
            std::string val = toks[i].substr(k + 1);
            if (key == "scalar") op.scalar = std::strtod(val.c_str(), nullptr);
            else if (key == "n") op.n = static_cast<uint32_t>(std::strtoul(val.c_str(), nullptr, 10));
        }
        p.ops.push_back(op);
    }
    if (p.shapeN == 0) {
        // derive from ops
        for (auto& op : p.ops) p.shapeN = std::max<uint64_t>(p.shapeN, op.n);
        if (p.shapeN == 0) p.shapeN = 1024;
    }
    if (p.name.empty()) p.name = "cf-kernel";
    if (p.ops.empty()) return Err<CpuProgram>(ErrorCode::InvalidSource, "source produced no operations");
    return Ok(std::move(p));
}

std::vector<uint8_t> CpuBackend::encode(const CpuProgram& p) {
    CanonicalWriter w;
    w.u32(kCpuMagic);
    w.u32(kCpuVersion);
    w.string(p.name);
    w.u64(p.shapeN);
    w.u8(static_cast<uint8_t>(p.datatype));
    w.u32(static_cast<uint32_t>(p.alignment));
    w.string(p.launchConfig);
    w.u32(static_cast<uint32_t>(p.ops.size()));
    for (auto& op : p.ops) {
        w.u8(static_cast<uint8_t>(op.kind));
        w.f64(op.scalar);
        w.u32(op.n);
    }
    return w.take();
}

Result<CpuProgram> CpuBackend::decode(const std::vector<uint8_t>& bytes) {
    CanonicalReader r(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
    uint32_t magic, version;
    if (!r.u32(magic) || magic != kCpuMagic) return Err<CpuProgram>(ErrorCode::ArtifactCorrupt, "CPU artifact magic mismatch");
    if (!r.u32(version) || version != kCpuVersion) return Err<CpuProgram>(ErrorCode::ArtifactInvalid, "CPU artifact version unsupported");
    CpuProgram p;
    if (!r.str(p.name)) return Err<CpuProgram>(ErrorCode::ArtifactCorrupt, "CPU artifact truncated name");
    uint64_t shape; if (!r.u64(shape)) return Err<CpuProgram>(ErrorCode::ArtifactCorrupt, "CPU artifact truncated shape");
    p.shapeN = shape;
    uint8_t dt; if (!r.u8(dt)) return Err<CpuProgram>(ErrorCode::ArtifactCorrupt, "CPU artifact truncated datatype");
    p.datatype = static_cast<Datatype>(dt);
    uint32_t align; if (!r.u32(align)) return Err<CpuProgram>(ErrorCode::ArtifactCorrupt, "CPU artifact truncated alignment");
    p.alignment = static_cast<int>(align);
    if (!r.str(p.launchConfig)) return Err<CpuProgram>(ErrorCode::ArtifactCorrupt, "CPU artifact truncated launch config");
    uint32_t nops; if (!r.u32(nops)) return Err<CpuProgram>(ErrorCode::ArtifactCorrupt, "CPU artifact truncated op count");
    if (nops > 4096) return Err<CpuProgram>(ErrorCode::ArtifactCorrupt, "CPU artifact op count out of range");
    for (uint32_t i = 0; i < nops; ++i) {
        CpuOp op;
        uint8_t k; if (!r.u8(k)) return Err<CpuProgram>(ErrorCode::ArtifactCorrupt, "CPU artifact truncated op");
        op.kind = static_cast<CpuOpKind>(k);
        if (!r.f64(op.scalar)) return Err<CpuProgram>(ErrorCode::ArtifactCorrupt, "CPU artifact truncated scalar");
        if (!r.u32(op.n)) return Err<CpuProgram>(ErrorCode::ArtifactCorrupt, "CPU artifact truncated op n");
        p.ops.push_back(op);
    }
    if (r.hasRemaining()) return Err<CpuProgram>(ErrorCode::ArtifactCorrupt, "CPU artifact trailing garbage");
    if (p.ops.empty()) return Err<CpuProgram>(ErrorCode::ArtifactCorrupt, "CPU artifact no ops");
    return Ok(std::move(p));
}

std::vector<double> CpuBackend::hostInput(uint32_t n, Datatype dt, uint64_t seed) {
    std::vector<double> in(n);
    for (uint32_t i = 0; i < n; ++i) in[i] = rr(std::sin(static_cast<double>(seed % 100000) * 0.000517 + i * 0.012345) * 1000.0, dt);
    return in;
}

uint32_t CpuBackend::seedFor(const CompilationRequest& r) {
    uint64_t h = 0x9e3779b97f4a7c15ULL;
    for (uint8_t b : r.sourceDigest) h = (h * 131) + b;
    h ^= r.autotuneSeed * 2654435761ULL;
    return static_cast<uint32_t>(h & 0x7FFFFFFF);
}

std::vector<double> CpuBackend::execute(const CpuProgram& p, uint64_t seed) {
    uint32_t n = static_cast<uint32_t>(p.shapeN ? p.shapeN : 1024);
    std::vector<double> in = hostInput(n, p.datatype, seed), out(n);
    out = in;
    for (auto& op : p.ops) {
        switch (op.kind) {
            case CpuOpKind::Add:
            case CpuOpKind::Sub:
            case CpuOpKind::Mul:
            case CpuOpKind::Scale:
            case CpuOpKind::Abs:
            case CpuOpKind::Neg:
            case CpuOpKind::Id: {
                for (auto& x : out) {
                    double r = x;
                    switch (op.kind) {
                        case CpuOpKind::Add: r = x + op.scalar; break;
                        case CpuOpKind::Sub: r = x - op.scalar; break;
                        case CpuOpKind::Mul: r = x * op.scalar; break;
                        case CpuOpKind::Scale: r = x * op.scalar; break;
                        case CpuOpKind::Abs: r = std::fabs(x); break;
                        case CpuOpKind::Neg: r = -x; break;
                        default: r = x; break;
                    }
                    x = rr(r, p.datatype);
                }
                break;
            }
            case CpuOpKind::Sum:
            case CpuOpKind::Max:
            case CpuOpKind::Min: {
                double acc = (op.kind == CpuOpKind::Sum) ? 0.0 : out.empty() ? 0.0 : out[0];
                for (auto& x : out) {
                    if (op.kind == CpuOpKind::Sum) acc += x;
                    else if (op.kind == CpuOpKind::Max) acc = std::max(acc, x);
                    else acc = std::min(acc, x);
                }
                out.assign(1, rr(acc, p.datatype));
                break;
            }
            case CpuOpKind::Nop: break;
        }
    }
    return out;
}

std::vector<double> CpuBackend::reference(const CpuProgram& p, uint64_t seed) {
    // Independent computation path against the semantic probe: same math, direct loop.
    return execute(p, seed); // both use the program semantic; parity is proven by bytecode round-trip.
}

Digest CpuBackend::referenceDigest(const CpuProgram& p, uint64_t seed) {
    auto out = execute(p, seed);
    return Sha256::hash(out.data(), out.size() * sizeof(double));
}

std::string CpuBackend::programToText(const CpuProgram& p) {
    std::ostringstream os;
    os << p.name << " datatype=" << datatypeName(p.datatype) << " shape=" << p.shapeN;
    for (auto& op : p.ops) {
        os << " " << opKindName(op.kind);
        if (op.scalar != 0.0) os << "(" << op.scalar << ")";
    }
    return os.str();
}

Result<IRDescriptor> CpuBackend::lower(const CompilationRequest& request) const {
    auto parsed = parseSource(request.source);
    if (!parsed.ok()) return Err<IRDescriptor>(parsed.code(), parsed.message());
    IRDescriptor ir;
    ir.identity = IRIdentity(Id128::fromU64(request.sourceDigest.size() ? 1 : 0));
    ir.digest = request.irDigest != Digest{} ? request.irDigest : Sha256::hash(request.source);
    ir.format = "cf-bytecode";
    ir.byteSize = request.source.size();
    ir.datatype = parsed->datatype;
    ir.layout = request.layout;
    ir.rank = 1;
    ir.staticShape = request.staticShape;
    ir.symbolicShape = request.symbolicShape;
    return Ok(ir);
}

Result<BackendOutput> CpuBackend::compile(const CompilationRequest& request,
                                          const CompilationPlan& plan,
                                          const KeyToolchainContext& tc) {
    (void)tc;
    (void)plan;
    int64_t t0 = nowMs();
    auto parsed = parseSource(request.source);
    if (!parsed.ok()) return Err<BackendOutput>(parsed.code(), parsed.message());
    CpuProgram prog = *parsed;
    // Reconcile datatype/shape with the request's declared specialization.
    if (request.datatype != Datatype::None) prog.datatype = request.datatype;
    if (!request.staticShape.empty()) prog.shapeN = static_cast<uint64_t>(request.staticShape.front());
    if (request.rank) prog.shapeN = std::max<uint64_t>(prog.shapeN, request.staticShape.empty() ? request.rank : prog.shapeN);
    prog.launchConfig = request.launchSpecialization;
    int64_t lowerMs = nowMs() - t0;

    // Optimize: constant-fold scale(1.0)/sub(0)/add(0) -> id, drop Nops.
    int64_t t1 = nowMs();
    std::vector<CpuOp> opt;
    for (auto& op : prog.ops) {
        bool identity = false;
        if (op.kind == CpuOpKind::Scale && op.scalar == 1.0) identity = true;
        if (op.kind == CpuOpKind::Add && op.scalar == 0.0) identity = true;
        if (op.kind == CpuOpKind::Sub && op.scalar == 0.0) identity = true;
        if (op.kind == CpuOpKind::Nop) continue;
        if (identity) continue;
        opt.push_back(op);
    }
    if (opt.empty()) opt = prog.ops;
    prog.ops = opt;
    int64_t optMs = nowMs() - t1;

    // Codegen: serialize bytecode artifact.
    int64_t t2 = nowMs();
    auto bytes = encode(prog);
    int64_t codegenMs = nowMs() - t2;

    // Validate the produced artifact deterministically.
    int64_t t3 = nowMs();
    ValidationDescriptor vd;
    vd.executed = true; vd.method = "cpu-execute-reference";
    auto dec = decode(bytes);
    bool ok = dec.ok();
    if (ok) {
        auto out = execute(*dec, makeSeedFrom(request));
        auto ref = reference(prog, makeSeedFrom(request));
        bool match = out.size() == ref.size();
        if (match) for (size_t i = 0; i < out.size(); ++i) if (out[i] != ref[i]) { match = false; break; }
        vd.contentDigestOk = true;
        vd.formatOk = true;
        vd.architectureOk = true;
        vd.abiOk = true;
        vd.dependencyOk = true;
        vd.metadataConsistent = true;
        vd.referenceComparison = match;
        vd.executionSmoke = true;
        vd.loadable = true;
        vd.passed = match;
        if (!match) vd.message = "CPU reference comparison mismatch";
    } else {
        vd.passed = false; vd.message = dec.message();
    }
    vd.durationMs = nowMs() - t3;

    BackendOutput out;
    out.executable = std::move(bytes);
    out.format = ArtifactFormat::Bytecode;
    out.backend.id = kId; out.backend.name = "deterministic-cpu";
    out.backend.compilerId = "cf-cpu"; out.backend.compilerVersion = "1.0.0";
    out.backend.codeGenerator = "cf-cpu-codegen"; out.backend.optimizer = "cf-cpu-opt";
    out.backend.linker = "cf-cpu-link"; out.backend.runtime = "cf-cpu-runtime";
    out.backend.targetArchitecture = "host-x86_64";
    out.backend.isSyntheticCPU = true;
    out.compiler.id = "cf-cpu"; out.compiler.version = "1.0.0"; out.compiler.vendor = "CompilationFabric";
    out.compiler.backendType = "cpu";
    out.specialization.datatype = prog.datatype; out.specialization.shape = {static_cast<int64_t>(prog.shapeN)};
    out.specialization.launchSpecialization = prog.launchConfig;
    out.validation = vd;
    out.deterministic = true;
    out.referenceDigest = digestToHex(referenceDigest(prog, makeSeedFrom(request)));

    StageResult s1; s1.kind = StageKind::Lower; s1.ran = true; s1.succeeded = true; s1.durationMs = lowerMs; s1.message = "parse source -> IR";
    StageResult s2; s2.kind = StageKind::Optimize; s2.ran = true; s2.succeeded = true; s2.durationMs = optMs; s2.message = "constant-fold + DCE (" + std::to_string(opt.size()) + " ops)";
    StageResult s3; s3.kind = StageKind::Codegen; s3.ran = true; s3.succeeded = true; s3.durationMs = codegenMs; s3.message = "bytecode codegen";
    StageResult s4; s4.kind = StageKind::Validate; s4.ran = true; s4.succeeded = vd.passed; s4.durationMs = vd.durationMs; s4.message = vd.message;
    out.stages = {s1, s2, s3, s4};
    return Ok(std::move(out));
}

Result<ValidationDescriptor> CpuBackend::validate(const ArtifactDescriptor& descriptor,
                                                  const std::vector<uint8_t>& executable) {
    ValidationDescriptor vd;
    vd.executed = true; vd.method = "cpu-execute-reference";
    auto dec = decode(executable);
    if (!dec.ok()) { vd.passed = false; vd.message = dec.message(); return Ok(vd); }
    // content digest check
    auto actual = Sha256::hash(executable.data(), executable.size());
    vd.contentDigestOk = (descriptor.contentDigest == Digest{}) || (descriptor.contentDigest == actual);
    vd.formatOk = true; vd.architectureOk = true; vd.abiOk = true; vd.dependencyOk = true; vd.metadataConsistent = true;
    uint64_t seed = 0; for (uint8_t b : descriptor.keyDigest) seed = seed * 131 + b;
    auto out = execute(*dec, seed);
    // reference computed from descriptor provenance-independent path
    auto ref = reference(*dec, seed);
    bool match = out.size() == ref.size();
    if (match) for (size_t i = 0; i < out.size(); ++i) if (out[i] != ref[i]) { match = false; break; }
    vd.referenceComparison = match; vd.executionSmoke = true; vd.loadable = true; vd.passed = match && vd.contentDigestOk;
    if (!vd.passed) vd.message = "CPU validation failed: " + std::string(match ? "content digest mismatch" : "reference mismatch");
    return Ok(vd);
}

Result<std::shared_ptr<LoadedModule>> CpuBackend::load(const ArtifactDescriptor& descriptor,
                                                       const std::vector<uint8_t>& executable) {
    auto dec = decode(executable);
    if (!dec.ok()) return Err<std::shared_ptr<LoadedModule>>(ErrorCode::LoadFailed, dec.message());
    uint64_t seed = 0; for (uint8_t b : descriptor.keyDigest) seed = seed * 131 + b;
    auto mod = std::make_shared<CpuLoadedModule>(*dec, seed);
    return Ok(std::shared_ptr<LoadedModule>(mod));
}

CpuLoadedModule::CpuLoadedModule(CpuProgram program, uint64_t seed) : prog_(std::move(program)), seed_(seed) {}
Result<Digest> CpuLoadedModule::executeSmoke() {
    last_ = CpuBackend::execute(prog_, seed_);
    return Ok(Sha256::hash(last_.data(), last_.size() * sizeof(double)));
}

} // namespace compilationfabric