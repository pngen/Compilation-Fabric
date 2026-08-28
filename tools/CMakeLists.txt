# Compilation Fabric CLI + distributed binaries.
add_executable(compile_fabric cli/main.cpp)
set_target_properties(compile_fabric PROPERTIES OUTPUT_NAME "compile_fabric")
target_link_libraries(compile_fabric PRIVATE CompilationFabric::CompilationFabric)

add_executable(compile_fabric_bench bench.cpp)
set_target_properties(compile_fabric_bench PROPERTIES OUTPUT_NAME "compile_fabric_bench")
target_link_libraries(compile_fabric_bench PRIVATE CompilationFabric::CompilationFabric)