# Toolchains

An **IToolchainProbe** discovers MSVC, nvcc, NVRTC, the CUDA driver, CMake, Ninja,
and the Windows SDK. Availability is proven by actually locating the tool and
running a version probe; it is never silently inferred. Missing or incompatible
tools produce exact failure reasons. Version parsing rejects invalid versions
rather than coercing them.

A CUDA target is advertised from real compiler/toolchain capability (the toolkit
supports `sm_120`), without requiring an active device context. Full device
query (name, memory) is exposed through the target probe.
