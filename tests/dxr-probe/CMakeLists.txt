cmake_minimum_required(VERSION 3.21)

project(doom3_dxr_probe LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# This project is deliberately separate from tests/dxr. The Q6 CPU harness there
# must keep linking no device, window or shader compiler; this probe is the one
# place that creates a D3D12 device, builds acceleration structures and
# dispatches a ray. It is evidence tooling, not a renderer.

set(DXC_EXE "" CACHE FILEPATH "Path to dxc.exe (Windows SDK or Vulkan SDK)")
if(NOT DXC_EXE)
    file(GLOB _sdk_dxc "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/bin/*/x64/dxc.exe")
    if(_sdk_dxc)
        list(SORT _sdk_dxc)
        list(GET _sdk_dxc -1 DXC_EXE)
    endif()
endif()
if(NOT DXC_EXE)
    find_program(DXC_EXE dxc)
endif()
if(NOT DXC_EXE)
    message(FATAL_ERROR "dxc.exe was not found; pass -DDXC_EXE=<path>")
endif()
message(STATUS "DXR probe shader compiler: ${DXC_EXE}")

set(DXR_PROBE_SHADER "${CMAKE_CURRENT_BINARY_DIR}/dxr_trivial.cso")
add_custom_command(
    OUTPUT "${DXR_PROBE_SHADER}"
    COMMAND "${DXC_EXE}" -T lib_6_3 -Fo "${DXR_PROBE_SHADER}"
            "${CMAKE_CURRENT_SOURCE_DIR}/dxr_trivial.hlsl"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/dxr_trivial.hlsl"
    COMMENT "Compiling dxr_trivial.hlsl for lib_6_3"
    VERBATIM)
add_custom_target(q6_dxr_probe_shader DEPENDS "${DXR_PROBE_SHADER}")

add_executable(q6_dxr_probe dxr_probe.cpp)
add_dependencies(q6_dxr_probe q6_dxr_probe_shader)
target_link_libraries(q6_dxr_probe PRIVATE d3d12 dxgi)
target_compile_features(q6_dxr_probe PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(q6_dxr_probe PRIVATE /W4 /permissive- /EHsc)
endif()
add_custom_command(
    TARGET q6_dxr_probe POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${DXR_PROBE_SHADER}" "$<TARGET_FILE_DIR:q6_dxr_probe>/dxr_trivial.cso"
    COMMENT "Copying dxr_trivial.cso next to q6_dxr_probe")
