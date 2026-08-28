# Protocol

Distributed message exchange uses real framed TCP. Each frame is body length
prefixed with a hard maximum size, a protocol version, a message type, and a strict
decoder that rejects unknown versions, unknown types, truncated frames, oversized
frames, and zero-length frames. Every message carries a typed authority envelope:
CoordinatorEpoch, WorkerId, WorkerBootId, CacheGeneration, ToolchainGeneration,
CompilationRequestId, CompilationAttemptId, ArtifactId, and ArtifactGeneration.

The runtime rejects stale epoch, stale worker boot, stale cache/toolchain
generation, stale attempt, stale artifact generation, duplicate completion,
completion after invalidation, and completion after replacement.
