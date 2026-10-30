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

# Q3 motion/history seams (T042/T044/T046). CPU-only public-seam tests: they
# use no device, window, shader or GPU work, so they link no native library.
add_library(q3_motion STATIC
    "${Q1A_SOURCE_ROOT}/src/renderer/rhi/motion_history.cpp"
    "${Q1A_SOURCE_ROOT}/src/renderer/rhi/skinned_history.cpp")
target_include_directories(q3_motion PUBLIC "${Q1A_HEADER_ROOT}")
target_compile_features(q3_motion PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(q3_motion PRIVATE /W4 /permissive- /EHsc)
endif()

function(add_q3_test target source)
    add_executable(${target} "${source}")
    target_link_libraries(${target} PRIVATE q3_motion)
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc)
    endif()
endfunction()

# Q3 production slices for T043/T045 (rendered-view history) and T047
# (post-reconstruction overlay composition). These are the real sources named
# by those tasks; the test-owned seam headers and the build-only stub were
# promoted into src/renderer/rhi/ by issue #62.
add_library(q3_production STATIC
    "${Q1A_SOURCE_ROOT}/src/renderer/rhi/view_history.cpp"
    "${Q1A_SOURCE_ROOT}/src/renderer/rhi/overlay_composition.cpp")
target_include_directories(q3_production PUBLIC "${Q1A_HEADER_ROOT}"
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(q3_production PUBLIC q3_motion)
target_compile_features(q3_production PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(q3_production PRIVATE /W4 /permissive- /EHsc)
endif()

function(add_q3_seam_test target source)
    add_executable(${target} "${source}")
    target_link_libraries(${target} PRIVATE q3_production)
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

# Q3 public-seam consumers of tests/renderer/motion-fixtures.md.
add_q3_test(q3_motion_reprojection_test motion_reprojection_test.cpp)
add_q3_test(q3_skinned_history_test skinned_history_test.cpp)
add_q3_test(q3_motion_validity_test motion_validity_test.cpp)

# T043/T045/T047. These assert the production behavior and link the real
# src/renderer/rhi slices. The former expected-red inversion was removed when
# the production slice landed; a regression is now a failing CTest, not a
# passing inverted one.
add_q3_seam_test(q3_view_history_test view_history_test.cpp)
add_q3_seam_test(q3_history_reset_test history_reset_test.cpp)
add_q3_seam_test(q3_overlay_composition_test overlay_composition_test.cpp)

# Q2 classic-path fixtures (T034/T037/T038). The scene adapter, the material plan and the
# light plan are backend-neutral, so these tests link no device, window or shader compiler.
# They still read the shipping manifest through the merged spec 008 resolver, because the
# draw gate is the resolver's decision and must not be re-implemented here.
add_library(q2_classic STATIC
    "${Q1A_SOURCE_ROOT}/src/renderer/rhi/engine_scene_adapter.cpp"
    "${Q1A_SOURCE_ROOT}/src/renderer/rhi/materials.cpp"
    "${Q1A_SOURCE_ROOT}/src/renderer/d3d12/lighting.cpp")
target_include_directories(q2_classic PUBLIC "${Q1A_HEADER_ROOT}")
target_compile_features(q2_classic PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(q2_classic PRIVATE /W4 /permissive- /EHsc)
endif()

add_library(q2_shader_resolver STATIC
    "${Q1A_SOURCE_ROOT}/src/renderer/shader/shader_resolver.cpp"
    "${Q1A_SOURCE_ROOT}/src/renderer/shader/shader_json.cpp")
target_include_directories(q2_shader_resolver PUBLIC
    "${Q1A_SOURCE_ROOT}/src/renderer/shader")
target_compile_features(q2_shader_resolver PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(q2_shader_resolver PRIVATE /W4 /permissive- /EHsc)
endif()

function(add_q2_test target source)
    add_executable(${target} "${source}")
    target_link_libraries(${target} PRIVATE q2_classic)
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc)
    endif()
endfunction()

add_q2_test(q2_engine_command_order_test engine_command_order_test.cpp)
add_q2_test(q2_classic_compatibility_test classic_compatibility_test.cpp)
add_q2_test(q2_classic_gui_test classic_gui_test.cpp)
target_link_libraries(q2_classic_compatibility_test PRIVATE q2_shader_resolver)
target_compile_definitions(q2_classic_compatibility_test PRIVATE
    Q2_MANIFEST_PATH="${Q1A_SOURCE_ROOT}/shaders/manifest/classic-programs.json")

# Q3 frame integration (T085 superproject slice). It consumes the Q2 ordered
# stream and writes the Q3 history, private pose and per-pixel motion seams. The
# engine-fork call sites are not part of this target; see evidence-q3.md.
add_library(q3_integration STATIC
    "${Q1A_SOURCE_ROOT}/src/renderer/rhi/frame_integration.cpp")
target_include_directories(q3_integration PUBLIC "${Q1A_HEADER_ROOT}"
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(q3_integration PUBLIC q2_classic q3_production q3_motion)
target_compile_features(q3_integration PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(q3_integration PRIVATE /W4 /permissive- /EHsc)
endif()

function(add_q3_integration_test target source)
    add_executable(${target} "${source}")
    target_link_libraries(${target} PRIVATE q3_integration)
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc)
    endif()
endfunction()

add_q3_integration_test(q3_frame_integration_test q3_integration_test.cpp)

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

add_test(NAME rhi.q3.motion_reprojection COMMAND q3_motion_reprojection_test)
add_test(NAME rhi.q3.skinned_history COMMAND q3_skinned_history_test)
add_test(NAME rhi.q3.motion_validity COMMAND q3_motion_validity_test)
set_tests_properties(
    rhi.q3.motion_reprojection
    rhi.q3.skinned_history
    rhi.q3.motion_validity
    PROPERTIES
        TIMEOUT 120
)

# T043/T045/T047 production tests. No WILL_FAIL: the exit code is the verdict.
add_test(NAME rhi.q3.view_history COMMAND q3_view_history_test)
add_test(NAME rhi.q3.history_reset COMMAND q3_history_reset_test)
add_test(NAME rhi.q3.overlay_composition COMMAND q3_overlay_composition_test)
add_test(NAME rhi.q3.frame_integration COMMAND q3_frame_integration_test)
set_tests_properties(
    rhi.q3.view_history
    rhi.q3.history_reset
    rhi.q3.overlay_composition
    rhi.q3.frame_integration
    PROPERTIES
        TIMEOUT 120
        LABELS "q3"
)

# Q2 classic-path fixtures. These are structural/classification checks: no legal demo data
# and no owned classic program exist, so image parity is SKIPPED in evidence-q2.md and is
# not claimed by any of these names.
add_test(NAME renderer.q2.command_order COMMAND q2_engine_command_order_test)
add_test(NAME renderer.q2.classic_compatibility COMMAND q2_classic_compatibility_test)
add_test(NAME renderer.q2.classic_gui COMMAND q2_classic_gui_test)
set_tests_properties(
    renderer.q2.command_order
    renderer.q2.classic_compatibility
    renderer.q2.classic_gui
    PROPERTIES
        TIMEOUT 120
        LABELS "q2"
)
