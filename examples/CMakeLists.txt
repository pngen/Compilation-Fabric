# Compilation Fabric runnable examples.
file(GLOB EXAMPLE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/example_*.cpp)
foreach(src ${EXAMPLE_SOURCES})
    get_filename_component(name ${src} NAME_WE)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE CompilationFabric::CompilationFabric)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME example_${name} COMMAND ${name})
endforeach()
