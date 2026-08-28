# Benchmarks

The benchmark executable measures canonical key construction, SHA-256 digest, plan
construction, cache hit/miss lookup, compatibility decisions, single-flight
contention, metadata serialization, artifact persistence and recovery, CPU compile,
NVRTC cold compile (where device access is available), module load, CUDA validation
execution, artifact reuse, warm reuse, specialization compile, autotuning candidate
evaluation, and worker selection. Reported metrics include keys/s, lookups/s,
compatibility decisions/s, compile latency and throughput, persistence throughput,
recovery time, and NVRTC compile / module load latency. Avoided compile time is
always derived and labeled as such; metadata processing is never called
compilation throughput. Workloads range from 1K to 100K artifact metadata entries
across 1/4/8 threads and 100%/90-10/50-50 hit-miss mixes.
