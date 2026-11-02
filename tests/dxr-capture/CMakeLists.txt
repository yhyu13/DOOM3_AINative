cmake_minimum_required(VERSION 3.21)

project(doom3_dxr_capture LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Standalone T069 capture tool. It creates a D3D12 device, builds acceleration
# structures and runs a DXR path tracer; the Q6 CPU harness in tests/dxr stays
# device-free. The only production sources it consumes are the merged denoiser
# and the shared WIC PNG writer, both compiled read-only from their existing
# locations.
set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

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
message(STATUS "Q6 capture shader compiler: ${DXC_EXE}")

set(CAPTURE_SHADER "${CMAKE_CURRENT_BINARY_DIR}/pathtrace_capture.cso")
add_custom_command(
    OUTPUT "${CAPTURE_SHADER}"
    COMMAND "${DXC_EXE}" -T lib_6_3 -Fo "${CAPTURE_SHADER}"
            "${CMAKE_CURRENT_SOURCE_DIR}/pathtrace_capture.hlsl"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/pathtrace_capture.hlsl"
    COMMENT "Compiling pathtrace_capture.hlsl for lib_6_3"
    VERBATIM)
add_custom_target(q6_dxr_capture_shader DEPENDS "${CAPTURE_SHADER}")

add_executable(q6_dxr_capture
    dxr_capture.cpp
    capture_scene.cpp
    "${REPO_ROOT}/src/renderer/d3d12/pathtrace_denoise.cpp"
    "${REPO_ROOT}/src/renderer/rhi/motion_history.cpp"
    "${REPO_ROOT}/src/renderer/q4_harness/image_io.cpp")
add_dependencies(q6_dxr_capture q6_dxr_capture_shader)
target_include_directories(q6_dxr_capture PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${REPO_ROOT}/src/renderer"
    "${REPO_ROOT}/src/renderer/q4_harness")
target_link_libraries(q6_dxr_capture PRIVATE d3d12 dxgi windowscodecs bcrypt ole32)
target_compile_features(q6_dxr_capture PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(q6_dxr_capture PRIVATE /W4 /permissive- /EHsc)
endif()
add_custom_command(
    TARGET q6_dxr_capture POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CAPTURE_SHADER}" "$<TARGET_FILE_DIR:q6_dxr_capture>/pathtrace_capture.cso"
    COMMENT "Copying pathtrace_capture.cso next to q6_dxr_capture")
