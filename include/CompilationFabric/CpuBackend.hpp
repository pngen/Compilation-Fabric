// Compilation Fabric - Deterministic synthetic CPU compilation backend.
//
// This backend is a real compiler for the bounded CF computation model: it
// lowers a source representation into an IR, optimizes it, code-generates a
// bytecode artifact, and can load+execute that artifact deterministically. It is
// used for testing/validation and as the deterministic CPU reference that CUDA
// artifacts are compared against. It is never a "copy input bytes" passthrough.
// Copyright 2026 Summon Software Labs. Licensed under Apache 2.0.
#pragma once
#include "CompilationFabric/Backend.hpp"

namespace compilationfabric {

enum class CpuOpKind : uint8_t { Nop, Add, Sub, Mul, Scale, Abs, Neg, Sum, Max, Min, Id };

struct CpuOp {
    CpuOpKind kind = CpuOpKind::Nop;
    double scalar = 0.0;
    uint32_t n = 0;
};

// The bounded CF computation program (IR).
struct CpuProgram {
    std::string name;
    std::vector<CpuOp> ops;
    Datatype datatype = Datatype::F32;
    uint64_t shapeN = 0;      // element count
    int alignment = 0;
    std::string launchConfig;
};

class CpuBackend : public ICompilerBackend {
public:
    static constexpr const char* kId = "cpu";
    CpuBackend();
    ~CpuBackend() override = default;

    const BackendCapabilities& capabilities() const override;
    std::string id() const override { return kId; }
    Result<void> checkCompatible(const CompilationPlan& plan) const override;
    Result<IRDescriptor> lower(const CompilationRequest& request) const override;
    Result<BackendOutput> compile(const CompilationRequest& request,
                                  const CompilationPlan& plan,
                                  const KeyToolchainContext& tc) override;
    Result<ValidationDescriptor> validate(const ArtifactDescriptor& descriptor,
                                          const std::vector<uint8_t>& executable) override;
    Result<std::shared_ptr<LoadedModule>> load(const ArtifactDescriptor& descriptor,
                                               const std::vector<uint8_t>& executable) override;

    // Public helpers used by parity tests + CUDA reference comparison.
    static Result<CpuProgram> parseSource(std::string_view source);
    static std::vector<uint8_t> encode(const CpuProgram& p);
    static Result<CpuProgram> decode(const std::vector<uint8_t>& bytes);
    static std::vector<double> execute(const CpuProgram& p, uint64_t seed);
    static std::vector<double> reference(const CpuProgram& p, uint64_t seed);
    static Digest referenceDigest(const CpuProgram& p, uint64_t seed);
    static std::vector<double> hostInput(uint32_t n, Datatype dt, uint64_t seed);
    static uint32_t seedFor(const CompilationRequest& r);
    static std::string programToText(const CpuProgram& p);

private:
    BackendCapabilities caps_;
};

// A loaded, executable CPU module that runs the compiled bytecode deterministically.
class CpuLoadedModule : public LoadedModule {
public:
    CpuLoadedModule(CpuProgram program, uint64_t seed);
    std::string kind() const override { return "cpu"; }
    Result<Digest> executeSmoke() override;
    const std::vector<double>& lastOutput() const { return last_; }
    const CpuProgram& program() const { return prog_; }
private:
    CpuProgram prog_;
    uint64_t seed_;
    std::vector<double> last_;
};

} // namespace compilationfabric