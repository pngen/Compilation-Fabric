# Architecture

Compilation Fabric decomposes compilation orchestration into typed subsystems
behind clean interfaces. The library is vendor-neutral: backends advertise
capabilities rather than being assumed.

## Components

- **CompilationFabric** orchestrates requests, planning, caching, single-flight,
  invalidation, generation authority, and deployment eligibility.
- **CompilationKey** is the canonical typed identity of a compilation request.
- **CompilationCompatibility** decides whether an artifact is eligible for reuse.
- **Lifecycle** guards artifact/attempt state transitions.
- Persistence + Recovery provide versioned, checksummed, atomic artifact storage.
- Backends implement Frontend/Compiler/CodeGenerator/Optimizer/Linker/Validator/
  Loader/Probe boundaries.

## Backends

- `cpu` — deterministic synthetic CPU backend that lowers a source into an IR,
  optimizes it, code-generates a bytecode artifact, validates it against a
  deterministic reference, and can load + execute it.
- `cuda-nvrtc` — NVRTC compilation to PTX/CUBIN with the CUDA driver for
  module load, kernel launch, result copy-back, and CPU-reference parity.

No fake AMD/Intel backends are present. Vendor neutrality means clean interfaces
and typed capability descriptors, not pretending unsupported hardware exists.
