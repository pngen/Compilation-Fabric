# Specialization

Specialization dimensions are explicit: shape, datatype, layout, quantization,
precision, scalar constants, launch configuration, architecture, feature flags,
and operator/model revision. Each is encoded into the CompilationKey, so distinct
specialization creates distinct artifact identity. Semantically equivalent
specialization can reuse only when policy proves equivalence. Invalid or
incompatible specialization is rejected; dynamic-shape policy is bounded and
explicit; a stale specialization cannot overwrite a newer artifact.
