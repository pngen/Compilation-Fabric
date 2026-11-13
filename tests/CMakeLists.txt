# Compilation Fabric test suite.
set(COMPILATION_FABRIC_TESTS
    core_smoke
    concurrency
    persistence_recovery
    adversarial
    property
    cuda_backend
    cuda_nvcc_backend
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
if(TARGET compile_fabric)
    # The closure runs this build's CLI and driver; its state file stays in this build tree.
    add_test(NAME atomic_closure COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File ${CMAKE_CURRENT_SOURCE_DIR}/atomic_closure.ps1
        -Cli $<TARGET_FILE:compile_fabric> -Driver $<TARGET_FILE:atomic_driver> -State ${CMAKE_CURRENT_BINARY_DIR}/atomic_state.json)

    # Regression scenarios: library recovery/persistence/single-flight and CLI
    # temp-directory safety, and coordinator scenarios against `compile_fabric serve`.
    foreach(t regression dist_regression)
        add_executable(${t} ${t}.cpp)
        target_link_libraries(${t} PRIVATE CompilationFabric::CompilationFabric)
        target_include_directories(${t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        target_compile_definitions(${t} PRIVATE
            CF_CLI_PATH="$<TARGET_FILE:compile_fabric>"
            CF_TEST_SCRATCH="${CMAKE_CURRENT_BINARY_DIR}/${t}_scratch")
        add_test(NAME ${t} COMMAND ${t})
    endforeach()
endif()