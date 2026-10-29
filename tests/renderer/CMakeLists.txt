cmake_minimum_required(VERSION 3.21)

project(doom3_rhi_q1a LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(Q1A_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(Q1A_HEADER_ROOT "${Q1A_SOURCE_ROOT}/src/renderer")

add_library(q1a_rhi STATIC
    "${Q1A_SOURCE_ROOT}/src/renderer/d3d12/rhi_d3d12.cpp"
    "${Q1A_SOURCE_ROOT}/src/renderer/d3d12/rhi_pipeline.cpp"
    "${Q1A_SOURCE_ROOT}/src/renderer/d3d12/rhi_compute.cpp"
    "${Q1A_SOURCE_ROOT}/src/renderer/d3d12/rhi_attachments.cpp"
    "${Q1A_SOURCE_ROOT}/src/renderer/d3d12/rhi_present.cpp"
    "${Q1A_SOURCE_ROOT}/src/renderer/d3d12/frame_resources.cpp"
    "${Q1A_SOURCE_ROOT}/src/renderer/rhi/frame_graph.cpp")
target_include_directories(q1a_rhi PUBLIC "${Q1A_HEADER_ROOT}")
target_compile_features(q1a_rhi PUBLIC cxx_std_17)
target_compile_definitions(q1a_rhi PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(q1a_rhi PUBLIC d3d12 dxgi d3dcompiler)
if(MSVC)
    target_compile_options(q1a_rhi PRIVATE /W4 /permissive- /EHsc)
endif()

function(add_q1a_test target source)
    add_executable(${target} "${source}")
    target_link_libraries(${target} PRIVATE q1a_rhi)
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc)
    endif()
endfunction()

# Public-seam fixtures. These translation units are scanned by
# scripts/validate-rhi-contract.py and must stay free of private vocabulary.
add_q1a_test(q1a_clear_test rhi_clear_test.cpp)
add_q1a_test(q1a_lifetime_test rhi_lifetime_test.cpp)

# Q1b public fixtures.
add_q1a_test(q1b_draw_test rhi_draw_test.cpp)
add_q1a_test(q1b_compute_test rhi_compute_test.cpp)
add_q1a_test(q1b_mrt_depth_test rhi_mrt_depth_test.cpp)
add_q1a_test(q1b_frame_resources_test rhi_frame_resources_test.cpp)
add_q1a_test(q1b_temporal_test rhi_temporal_test.cpp)
add_q1a_test(q1b_frame_graph_test frame_graph_test.cpp)
add_q1a_test(q1b_surface_test rhi_present_test.cpp)
add_q1a_test(q1b_frame_resources_private d3d12/rhi_frame_resources_private_test.cpp)
add_q1a_test(q1b_device_removal_test d3d12/rhi_device_removal_test.cpp)
add_q1a_test(q1b_surface_private d3d12/rhi_surface_private_test.cpp)
# D3D12-private companions. The directory name excludes them from the public
# scan; they are the only tests that name state, the journal and fence values.
add_q1a_test(q1a_d3d12_test d3d12/rhi_d3d12_test.cpp)
add_q1a_test(q1a_lifetime_private d3d12/rhi_lifetime_private_test.cpp)
add_q1a_test(q1a_diagnostics_test d3d12/rhi_diagnostics_test.cpp)

enable_testing()

set(Q1A_EVIDENCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/q1a-evidence")

add_test(NAME rhi.q1a.clear_readback COMMAND q1a_clear_test clear_readback)
add_test(NAME rhi.q1a.negative_controls COMMAND q1a_clear_test negative_controls)
add_test(NAME rhi.q1a.state_journal COMMAND q1a_d3d12_test state_journal)
add_test(NAME rhi.q1a.staging_pitch COMMAND q1a_d3d12_test staging_pitch)
add_test(NAME rhi.q1a.outstanding_release COMMAND q1a_lifetime_test)
add_test(NAME rhi.q1a.outstanding_release_private COMMAND q1a_lifetime_private)
add_test(NAME rhi.q1a.diagnostics COMMAND q1a_diagnostics_test)

add_test(NAME rhi.q1b.draw COMMAND q1b_draw_test)
add_test(NAME rhi.q1b.compute COMMAND q1b_compute_test)
add_test(NAME rhi.q1b.mrt_depth COMMAND q1b_mrt_depth_test)
add_test(NAME rhi.q1b.frame_resources COMMAND q1b_frame_resources_test)
add_test(NAME rhi.q1b.temporal COMMAND q1b_temporal_test)
add_test(NAME rhi.q1b.frame_graph COMMAND q1b_frame_graph_test)
add_test(NAME rhi.q1b.surface COMMAND q1b_surface_test)
add_test(NAME rhi.q1b.frame_resources_private COMMAND q1b_frame_resources_private)
add_test(NAME rhi.q1b.device_removal COMMAND q1b_device_removal_test)
add_test(NAME rhi.q1b.surface_private COMMAND q1b_surface_private)
set_tests_properties(
    rhi.q1a.clear_readback
    rhi.q1a.negative_controls
    rhi.q1a.state_journal
    rhi.q1a.staging_pitch
    rhi.q1a.outstanding_release
    rhi.q1a.outstanding_release_private
    rhi.q1a.diagnostics
    rhi.q1b.draw
    rhi.q1b.compute
    rhi.q1b.mrt_depth
    rhi.q1b.frame_resources
    rhi.q1b.temporal
    rhi.q1b.frame_graph
    rhi.q1b.surface
    rhi.q1b.frame_resources_private
    rhi.q1b.device_removal
    rhi.q1b.surface_private
    PROPERTIES
        SKIP_RETURN_CODE 77
        TIMEOUT 240
        ENVIRONMENT "RHI_EVIDENCE_DIR=${Q1A_EVIDENCE_DIR}"
)
