# Validation

Every artifact must pass validation before eligibility. A successful compile
command does not imply a valid artifact. Validation covers content digest, format,
architecture, ABI, dependency, metadata consistency, loadability, execution smoke,
and deterministic reference comparison. For CUDA artifacts, validation performs a
real module load, kernel lookup, launch, device synchronization, result copy-back,
CPU-reference comparison, and cleanup verification.
