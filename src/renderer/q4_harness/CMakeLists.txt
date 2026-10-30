# Standalone Q4 D3D12 capture harness.
#
#   cmake -S src/renderer/q4_harness -B build-q4-harness -G "Visual Studio 17 2022" -A x64
#   cmake --build build-q4-harness --config Release -- -m
#
# The harness links the existing RHI backend sources for device/adapter
# selection and the Q4 portable slices for the material/lighting/indirect/
# residency/reconstruction hooks. It is not part of the engine build and does
# not modify the target or root CMakeLists.
cmake_minimum_required(VERSION 3.21)

project(doom3_q4_harness LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(FATAL_ERROR "The Q4 D3D12 capture harness is Windows-only.")
endif()

# Match the repository convention: <build>/bin/<Config>/.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

get_filename_component(DOOM3_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(DOOM3_RENDERER "${DOOM3_REPO_ROOT}/src/renderer")

add_executable(q4_harness
    main.cpp
    harness.cpp
    harness_pipeline.cpp
    harness_render.cpp
    mesh.cpp
    content.cpp
    dds.cpp
    image_io.cpp
    "${DOOM3_RENDERER}/d3d12/rhi_d3d12.cpp"
    "${DOOM3_RENDERER}/d3d12/rhi_pipeline.cpp"
    "${DOOM3_RENDERER}/d3d12/rhi_compute.cpp"
    "${DOOM3_RENDERER}/d3d12/rhi_attachments.cpp"
    "${DOOM3_RENDERER}/d3d12/rhi_present.cpp"
    "${DOOM3_RENDERER}/d3d12/frame_resources.cpp"
    "${DOOM3_RENDERER}/rhi/frame_graph.cpp"
    "${DOOM3_RENDERER}/rhi/materials.cpp"
    "${DOOM3_RENDERER}/rhi/residency.cpp"
    "${DOOM3_RENDERER}/rhi/reconstruction.cpp"
    "${DOOM3_RENDERER}/rhi/overlay_composition.cpp"
    "${DOOM3_RENDERER}/d3d12/hybrid_opaque.cpp"
    "${DOOM3_RENDERER}/d3d12/lighting.cpp"
    "${DOOM3_RENDERER}/d3d12/indirect_lighting.cpp"
    "${DOOM3_RENDERER}/shader/shader_json.cpp")

target_include_directories(q4_harness PRIVATE "${DOOM3_RENDERER}")
target_compile_definitions(q4_harness PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_features(q4_harness PRIVATE cxx_std_17)
target_link_libraries(q4_harness PRIVATE d3d12 dxgi d3dcompiler windowscodecs ole32)
if(MSVC)
    target_compile_options(q4_harness PRIVATE /W4 /permissive- /EHsc)
endif()

add_custom_command(TARGET q4_harness POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:q4_harness>/shaders"
    COMMENT "Copying Q4 HLSL next to the harness executable")
