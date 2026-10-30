cmake_minimum_required(VERSION 3.21)

project(doom3_dxr_q6 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(DXR_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(DXR_HEADER_ROOT "${DXR_SOURCE_ROOT}/src/renderer")

# Q6 CPU reference slices (spec 004 T063-T068). These translation units hold
# backend-neutral arithmetic: no device, window, shader compiler or native
# library is linked, and no vendor ray-tracing API is called. The D3D12-named
# directory mirrors the Q2/Q4 layout; it does not make this target a device
# target.
add_library(q6_dxr STATIC
    "${DXR_SOURCE_ROOT}/src/renderer/rhi/motion_history.cpp"
    "${DXR_SOURCE_ROOT}/src/renderer/d3d12/raytracing_capability.cpp"
    "${DXR_SOURCE_ROOT}/src/renderer/d3d12/raytracing_scene.cpp"
    "${DXR_SOURCE_ROOT}/src/renderer/d3d12/pathtrace_material.cpp"
    "${DXR_SOURCE_ROOT}/src/renderer/d3d12/pathtrace.cpp"
    "${DXR_SOURCE_ROOT}/src/renderer/d3d12/pathtrace_denoise.cpp")
target_include_directories(q6_dxr PUBLIC "${DXR_HEADER_ROOT}")
target_compile_features(q6_dxr PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(q6_dxr PRIVATE /W4 /permissive- /EHsc)
endif()

function(add_q6_test target source)
    add_executable(${target} "${source}")
    target_link_libraries(${target} PRIVATE q6_dxr)
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc)
    endif()
endfunction()

add_q6_test(q6_raytracing_capability_test raytracing_capability_test.cpp)
add_q6_test(q6_pathtrace_static_test pathtrace_static_test.cpp)
add_q6_test(q6_pathtrace_dynamic_test pathtrace_dynamic_test.cpp)
add_q6_test(q6_pathtrace_material_test pathtrace_material_test.cpp)
add_q6_test(q6_pathtrace_denoise_test pathtrace_denoise_test.cpp)

enable_testing()

add_test(NAME renderer.q6.raytracing_capability COMMAND q6_raytracing_capability_test)
add_test(NAME renderer.q6.pathtrace_static COMMAND q6_pathtrace_static_test)
add_test(NAME renderer.q6.pathtrace_dynamic COMMAND q6_pathtrace_dynamic_test)
add_test(NAME renderer.q6.pathtrace_material COMMAND q6_pathtrace_material_test)
add_test(NAME renderer.q6.pathtrace_denoise COMMAND q6_pathtrace_denoise_test)
set_tests_properties(
    renderer.q6.raytracing_capability
    renderer.q6.pathtrace_static
    renderer.q6.pathtrace_dynamic
    renderer.q6.pathtrace_material
    renderer.q6.pathtrace_denoise
    PROPERTIES
        TIMEOUT 300
        LABELS "q6"
)
