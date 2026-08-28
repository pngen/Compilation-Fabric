# Limitations

This section records actual, proven limitations of Compilation Fabric 1.0.0.

- **nvcc offline backend**: the offline `nvcc` backend is not implemented as a
  separate driver. Offline compilation is served through the NVRTC backend
  (NVRTC compiles CUDA source to PTX/CUBIN for the target architecture). This is
  reported rather than faked.
- **Determinism of external toolchains**: where a toolchain embeds timestamps or
  non-deterministic sections, the runtime does not claim byte-identical
  reproducibility; it records normalized reproducibility evidence and labels the
  exact limitation. The deterministic CPU backend and the Compilation Fabric
  artifact pipeline itself are deterministic.
- **Autotuning scope**: the bounded autotuner selects the best among the
  evaluated candidate variants and records exact evidence; it never claims global
  optimality.
- **Distributed worker authority**: the coordinator enforces strict authority on
  worker messages (epoch, worker boot, cache/toolchain generation, artifact
  generation, and attempt freshness). Worker capabilities are advertised by the
  worker and trusted as the basis for dispatch; there is no remote attestation of
  a worker's toolchain beyond its self-reported capabilities.

CUDA device execution on the RTX 5090 (NVRTC compilation to PTX/CUBIN for
`sm_120`, CUDA module load, kernel lookup, device allocation, launch,
synchronization, copy-back, and CPU-reference parity) is verified to run to
completion in this environment through the real `cuda-nvrtc` backend.
