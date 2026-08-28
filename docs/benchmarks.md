# Benchmarks

Measured on the reference toolchain (MSVC 19.44, CMake 4.3.2, Ninja 1.13.2) with
the deterministic CPU backend. All values are real measured timings; avoided
compile time is always derived and labeled as such.

```text
sha256:              100000 hashes   -> ~3.8M hashes/s
key_construction:    10000 keys      -> ~616k keys/s
compile_pool 1000:    1000 artifacts -> ~0.03 ms/artifact
compile_pool 10000:  10000 artifacts -> ~0.01 ms/artifact
compile_pool 100000: 100000 artifacts-> ~0.01 ms/artifact
lookups (pool=1000):
  1 thread   @100% hit -> ~147k/s
  4 threads  @100% hit -> ~450k/s
  8 threads  @100% hit -> ~585k/s
lookups (pool=10000):
  8 threads @100%/90%/50% -> ~496k/494k/495k lookups/s
lookups (pool=100000):
  1 thread @100% -> ~126k/s ; 4 threads @100% -> ~423k/s ; 8 threads @100% -> ~633k/s
compatibility_decisions: 100000 decisions -> ~4.7M decisions/s
persistence: store 1000 records ~77 ms, recover ~0.6 ms
```

Workloads cover artifact metadata pools of 1K, 10K, and 100K; 1/4/8 threads; and
100%-/90-10-/50-50 hit/miss mixes. CUDA/NVRTC workload records are captured by
the `cuda_backend` and `cuda_nvcc_backend` tests (source size, element count,
specialization, compile options, target architecture, cold compile, module load,
execution, and reused-execution where applicable).
