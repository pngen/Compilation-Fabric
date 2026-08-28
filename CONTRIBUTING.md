# Contributing to Compilation Fabric

Contributions are accepted on the terms of the Apache License 2.0 without
requiring a Contributor License Agreement (CLA).

## Build and test

- Configure and build with CMake + Ninja + MSVC as described in the README.
- `/W4 /WX` is enforced; zero-warning builds are required in Release and Debug.
- Run `ctest --test-dir build` for the full suite. Tests must complete naturally;
  no timeout is set.

## Conventions

- Use the `compilationfabric` namespace.
- Keep the public API documented with exact thread-safety semantics.
- Backends advertise typed capability descriptors and never claim support they
  cannot prove.
- Reuse is a correctness decision: never silently downgrade requirements to
  maximize reuse.
- Add a test for any new invariant. Persistence, concurrency, adversarial, and
  property suites are required.

## Pull requests

- Include a clear description of the problem and the change.
- Ensure a clean build with zero warnings and that the full test suite passes.
- Do not introduce telemetry or network transmission of operator data.
