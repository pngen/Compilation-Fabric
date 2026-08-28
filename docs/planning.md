# Planning

Planning is deterministic for identical normalized inputs and policy state. A
CompilationPlan records the normalized request, the selected frontend, compiler,
backend, optimizer, linker, target device, specialization strategy, expected
artifact format, dependency set, planned stages, required toolchain capabilities,
expected validation/deployment method, reproducibility constraints, cache/reuse
policy, and provenance.

An explicit `reason` string explains why a toolchain/target/specialization path
was selected. An unknown backend preference is rejected with `NoBackend`.
