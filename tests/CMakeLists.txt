set(COMPILATION_FABRIC_TESTS
    core_smoke
    concurrency
    persistence_recovery
    adversarial
    property
)
add_executable(atomic_driver atomic_driver.cpp)
target_link_libraries(atomic_driver PRIVATE CompilationFabric::CompilationFabric)
target_include_directories(atomic_driver PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME atomic_closure COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File ${CMAKE_CURRENT_SOURCE_DIR}/atomic_closure.ps1)