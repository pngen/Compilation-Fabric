cmake_minimum_required(VERSION 3.24)

project(CompilationFabric
    VERSION 1.0.0
    DESCRIPTION "Vendor-neutral runtime for orchestrating AI compilation, specialization, code generation, artifact production, validation, reproducibility, caching, invalidation, and accelerator-targeted deployment."
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

option(COMPILATION_FABRIC_BUILD_TESTS "Build Compilation Fabric tests" ON)
option(COMPILATION_FABRIC_BUILD_EXAMPLES "Build Compilation Fabric examples" ON)
option(COMPILATION_FABRIC_BUILD_CLI "Build Compilation Fabric CLI / distributed binaries" ON)

if(MSVC)
    add_compile_options(/W4 /WX /permissive- /Zc:__cplusplus /utf-8 /EHsc)
    add_compile_options(/external:anglebrackets /external:W0)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

file(GLOB COMPILATION_FABRIC_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)


add_library(CompilationFabric ${COMPILATION_FABRIC_SOURCES})
add_library(CompilationFabric::CompilationFabric ALIAS CompilationFabric)

target_include_directories(CompilationFabric
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

if(WIN32)
    target_link_libraries(CompilationFabric PRIVATE ws2_32 version advapi32)
endif()

set_target_properties(CompilationFabric PROPERTIES
    OUTPUT_NAME "compilationfabric"
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    POSITION_INDEPENDENT_CODE ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS CompilationFabric
    EXPORT CompilationFabricTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(DIRECTORY include/CompilationFabric
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilationFabricConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/CompilationFabricConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CompilationFabric)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/generated/CompilationFabricConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/generated/CompilationFabricConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/generated/CompilationFabricConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CompilationFabric)

install(EXPORT CompilationFabricTargets
    FILE CompilationFabricTargets.cmake
    NAMESPACE CompilationFabric::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CompilationFabric)

if(COMPILATION_FABRIC_BUILD_CLI)
    add_subdirectory(tools)
endif()
if(COMPILATION_FABRIC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
if(COMPILATION_FABRIC_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()