cmake_minimum_required(VERSION 3.21)

# Stand-alone project for the optional-provider extension tests (spec 004 T059).
#
# It is deliberately separate from tests/renderer/CMakeLists.txt: the ext tests
# need no device, window or shader compiler, and the shared test project is
# edited by other sessions. Configure and run with:
#
#   cmake -S tests/renderer/ext -B build-ext-provider -G "Visual Studio 17 2022" -A x64
#   cmake --build build-ext-provider --config Release -- -m
#   ctest --test-dir build-ext-provider -C Release --no-tests=error --output-on-failure
#
# The provider source is compiled with the same warning level as the other
# renderer fixtures and links no backend library.

project(doom3_renderer_ext LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# tests/renderer/ext -> tests/renderer -> tests -> repository root.
set(EXT_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
set(EXT_HEADER_ROOT "${EXT_SOURCE_ROOT}/src/renderer")

add_library(ext_provider STATIC
    "${EXT_SOURCE_ROOT}/src/renderer/ext/provider.cpp")
target_include_directories(ext_provider PUBLIC "${EXT_HEADER_ROOT}")
target_compile_features(ext_provider PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(ext_provider PRIVATE /W4 /permissive- /EHsc)
endif()

add_executable(provider_offpath_test provider_offpath_test.cpp)
target_link_libraries(provider_offpath_test PRIVATE ext_provider)
# The shared runner header lives one level up, in tests/renderer.
target_include_directories(provider_offpath_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_compile_features(provider_offpath_test PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(provider_offpath_test PRIVATE /W4 /permissive- /EHsc)
endif()

enable_testing()

add_test(NAME provider.off_path COMMAND provider_offpath_test)
set_tests_properties(provider.off_path PROPERTIES
    TIMEOUT 120
    LABELS "ext")
