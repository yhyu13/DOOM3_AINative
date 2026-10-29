cmake_minimum_required(VERSION 3.21)

project(doom3_shader_resolver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SHADER_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(SHADER_SOURCE_DIR "${SHADER_REPO_ROOT}/src/renderer/shader")

file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS "${SHADER_SOURCE_DIR}/*.cpp")

add_library(shader_resolver STATIC ${SHADER_SOURCES})
target_include_directories(shader_resolver PUBLIC "${SHADER_SOURCE_DIR}")
target_compile_features(shader_resolver PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(shader_resolver PRIVATE /W4 /permissive- /EHsc)
endif()

add_executable(shader_resolver_test shader_resolver_test.cpp)
target_link_libraries(shader_resolver_test PRIVATE shader_resolver)
target_compile_features(shader_resolver_test PRIVATE cxx_std_17)
target_compile_definitions(shader_resolver_test PRIVATE SHADER_REPO_ROOT="${SHADER_REPO_ROOT}")
if(MSVC)
    target_compile_options(shader_resolver_test PRIVATE /W4 /permissive- /EHsc)
endif()

enable_testing()

add_test(NAME shader.resolver.vectors COMMAND shader_resolver_test vectors)
add_test(NAME shader.resolver.manifest_shipping COMMAND shader_resolver_test manifest_shipping)
add_test(NAME shader.resolver.phase2 COMMAND shader_resolver_test phase2)
add_test(NAME shader.resolver.diagnostics COMMAND shader_resolver_test diagnostics)
add_test(NAME shader.resolver.manifest_invalid COMMAND shader_resolver_test manifest_invalid)
add_test(NAME shader.resolver.material_atomic COMMAND shader_resolver_test material_atomic)

set_tests_properties(
    shader.resolver.vectors
    shader.resolver.manifest_shipping
    shader.resolver.phase2
    shader.resolver.diagnostics
    shader.resolver.manifest_invalid
    shader.resolver.material_atomic
    PROPERTIES
        SKIP_RETURN_CODE 77
        TIMEOUT 120
        WORKING_DIRECTORY "${SHADER_REPO_ROOT}"
)
