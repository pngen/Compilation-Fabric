# Compilation Fabric test suite.
set(COMPILATION_FABRIC_TESTS
    core_smoke
    concurrency
    persistence_recovery
    adversarial
    property
    cuda_backend
)
foreach(t ${COMPILATION_FABRIC_TESTS})
    add_executable(${t} ${t}.cpp)
    target_link_libraries(${t} PRIVATE CompilationFabric::CompilationFabric)
    target_include_directories(${t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${t} COMMAND ${t})
    # No timeout is set; tests must complete naturally.
endforeach()

# Atomic multiprocess closure driver (run by tests/atomic_closure.ps1, not gated by CTest).
add_executable(atomic_driver atomic_driver.cpp)
target_link_libraries(atomic_driver PRIVATE CompilationFabric::CompilationFabric)
target_include_directories(atomic_driver PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME atomic_closure COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File ${CMAKE_CURRENT_SOURCE_DIR}/atomic_closure.ps1)