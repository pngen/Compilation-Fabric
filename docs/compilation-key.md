# Compilation Key

A CompilationKey is the canonical, typed identity of a compilation request. It is
computed deterministically and hashed with SHA-256.

## Encoding

Each typed semantic or toolchain field is encoded as a `KeyFieldEntry` whose
value is a canonical big-endian blob. The record is made canonical by sorting on
tag before hashing, so **field order can never change meaning**. The typed metadata
is retained in full for explainability; the digest is recomputed from the current
fields, so mutating typed metadata cannot leave a stale digest behind.

## Fields

The key encodes logical operation identity, source/IR identity and digest,
frontend/compiler/backend/codegen/optimizer/linker/runtime/driver identity and
version, accelerator vendor/family, target architecture, compute capability / ISA,
ABI, kernel and graph ABI, datatype, layout, rank, static/symbolic shape,
alignment, quantization, precision, launch specialization, scalar constants,
feature/codegen/optimization flags, debug-release mode, determinism,
reproducibility, environment fingerprint, dependency identities and generations,
model/operator revision, runtime and compiler policy generations, namespace, and
tenant.

## Guarantees

- Identical semantic inputs produce identical keys.
- Every relevant semantic/toolchain change alters compatibility identity.
- Repeated runs produce stable key encoding.
- Malformed encodings are rejected.
- 64-bit and 128-bit identities are lossless.
- Field order cannot change meaning.
