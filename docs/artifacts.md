# Artifacts

Artifacts are strongly typed and immutable after validation and publication.
Operational metadata is separate. The runtime models ArtifactId, ArtifactGeneration,
ArtifactDescriptor, ArtifactFormat, ArtifactDigest, SourceDescriptor, IRDescriptor,
CompilerDescriptor, ToolchainDescriptor, BackendDescriptor, OptimizerDescriptor,
TargetDescriptor, SpecializationDescriptor, ValidationDescriptor,
DeploymentDescriptor, ProvenanceDescriptor, CompilationRequest, CompilationPlan,
CompilationResult, CompilationLease, and CompilationReservation.

Executable bytes are never mutated under an existing immutable artifact identity.
An artifact is eligible for reuse only when its complete semantic and toolchain
authority still agrees with the request.
