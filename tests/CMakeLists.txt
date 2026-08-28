# Compilation Fabric test suite.
set(COMPILATION_FABRIC_TESTS
    core_smoke
    concurrency
    persistence_recovery
    adversarial
    property)
foreach(t ${COMPILATION_FABRIC_TESTS})
    add_executable(${t} ${t}.cpp)
    target_link_libraries(${t} PRIVATE CompilationFabric::CompilationFabric)
    target_include_directories(${t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${t} COMMAND ${t})
    # No timeout is set; tests must complete naturally.
endforeach()