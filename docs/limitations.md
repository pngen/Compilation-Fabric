# Limitations

This section records actual, proven limitations.

- **CUDA device execution**: Real NVRTC compilation and CUDA driver module load,
  launch, result copy-back, and CPU-reference parity are implemented in the `cuda-nvrtc`
  backend. In the sandboxed build/validation environment used for the Release and
  Debug runs, the CUDA device itself is not accessible: any CUDA driver/NVRTC call
  beyond a library-availability check terminates the calling process (observed as an
  unconditional process kill with exit code 1). The CUDA backend therefore compiles
  source to PTX/CUBIN and is exercised for availability, but the in-process
  module-load/launch execution gate cannot be completed in this environment. This is
  an environmental limitation, not a code path that was skipped.
- **nvcc offline path**: The offline `nvcc` backend is not implemented; offline
  compilation is served through the NVRTC backend. This is reported rather than
  faked.
- **Determinism**: Where a toolchain embeds timestamps or non-deterministic sections,
  the runtime does not claim byte-identical reproducibility and instead provides
  normalized reproducibility evidence.
- The runtime never claims global autotuning optimality; it selects the best among
  evaluated candidates.
