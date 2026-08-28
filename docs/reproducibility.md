# Reproducibility

Reproducibility is modeled explicitly. The runtime records the source digest, IR
digest, compiler/toolchain versions, environment fingerprint, target architecture,
flags, dependency identities, artifact digest, build timestamps, attempt identity,
and deterministic/non-deterministic status, and supports **Strict**,
**BestEffort**, and **Unspecified** modes.

For deterministic paths, repeated builds with identical inputs and toolchain
produce identical canonical artifact bytes where the backend supports it. Where a
toolchain embeds timestamps or non-deterministic sections, the runtime does not
claim byte-identical reproducibility; it provides normalized reproducibility
evidence and labels the exact limitation.
