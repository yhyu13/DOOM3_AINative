# Root build entry point for DOOM3_AINative.
#
#   cmake --preset win11-amd64
#   cmake --build --preset win11-amd64-release
#
# Output: build/bin/Release/DOOM3.exe and DOOM3XP.exe (x64, game linked statically; ALL builds both).
# DOOM3XP only: cmake --build --preset win11-amd64-xp-release
#
# The engine source is a read-only git submodule. Migration targets build from a git-ignored
# COPY (targets/<name>/doom3) that is created from the submodule and patched by the scripts in
# scripts/. This file runs those steps for you at configure time when the copy is missing, so a
# fresh clone needs: git clone --recurse-submodules, then the two commands above.
cmake_minimum_required(VERSION 3.20)

if(NOT WIN32)
    message(FATAL_ERROR "The win11-amd64 target is Windows-only. See docs/ROADMAP.md for other targets.")
endif()

set(DOOM3_TARGET "win11-amd64" CACHE STRING "Migration target under targets/ to build")
option(DOOM3_BOOTSTRAP_TARGET "Create and patch the target copy at configure time when it is missing" ON)

set(_target_dir "${CMAKE_CURRENT_SOURCE_DIR}/targets/${DOOM3_TARGET}")
set(_copy_marker "${_target_dir}/doom3/neo/idlib/precompiled.h")
set(_pristine_marker "${CMAKE_CURRENT_SOURCE_DIR}/doom3_orig/doom3/neo/idlib/precompiled.h")

if(NOT EXISTS "${_target_dir}/CMakeLists.txt")
    message(FATAL_ERROR "Unknown target '${DOOM3_TARGET}': ${_target_dir}/CMakeLists.txt not found.")
endif()

find_program(DOOM3_POWERSHELL NAMES powershell pwsh REQUIRED)

# Run one of the repo scripts; fail configure with the script's own output.
function(doom3_run_script what)
    execute_process(
        COMMAND "${DOOM3_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass -File ${ARGN}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        RESULT_VARIABLE _rc
        OUTPUT_VARIABLE _out
        ERROR_VARIABLE _err)
    message(STATUS "${what}: exit ${_rc}")
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "${what} failed (exit ${_rc}).\n${_out}\n${_err}")
    endif()
endfunction()

if(NOT EXISTS "${_copy_marker}")
    if(NOT DOOM3_BOOTSTRAP_TARGET)
        message(FATAL_ERROR "Target copy missing: ${_copy_marker}\n"
            "Run scripts/new-target.ps1 -Name ${DOOM3_TARGET} and scripts/apply-target-patches.ps1 -Name ${DOOM3_TARGET}, "
            "or configure with -DDOOM3_BOOTSTRAP_TARGET=ON.")
    endif()
    if(NOT EXISTS "${_pristine_marker}")
        find_program(DOOM3_GIT NAMES git REQUIRED)
        message(STATUS "Initialising submodule doom3_orig/doom3")
        execute_process(
            COMMAND "${DOOM3_GIT}" submodule update --init --recursive doom3_orig/doom3
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            RESULT_VARIABLE _rc)
        if(NOT _rc EQUAL 0 OR NOT EXISTS "${_pristine_marker}")
            message(FATAL_ERROR "Could not initialise the doom3_orig/doom3 submodule (exit ${_rc}). "
                "Clone with --recurse-submodules or run: git submodule update --init --recursive")
        endif()
    endif()
    # A copy directory without the marker is an interrupted earlier copy (scratch, git-ignored): replace it.
    set(_force "")
    if(EXISTS "${_target_dir}/doom3")
        set(_force -Force)
    endif()
    doom3_run_script("new-target" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/new-target.ps1" -Name "${DOOM3_TARGET}" ${_force})
    doom3_run_script("apply-target-patches" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/apply-target-patches.ps1" -Name "${DOOM3_TARGET}")
endif()

# CMAKE_SYSTEM_VERSION must be known before the first project() enables languages.
if(NOT DEFINED CMAKE_SYSTEM_VERSION OR CMAKE_SYSTEM_VERSION STREQUAL "")
    set(CMAKE_SYSTEM_VERSION "10.0.26100.0" CACHE STRING "Windows SDK version for the x64 port")
endif()

project(DOOM3_AINative LANGUAGES C CXX)

# One predictable place for the executable regardless of the generator: <build>/bin/<Config>/.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_subdirectory("targets/${DOOM3_TARGET}" "targets/${DOOM3_TARGET}")

# Convenience wrappers around the evidence scripts. They need game data (D3_DATA_DIR) and a
# desktop session, so they are not part of ALL.
set(_script_dir "${CMAKE_CURRENT_SOURCE_DIR}/scripts")
add_custom_target(verify
    COMMAND "${DOOM3_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass -File "${_script_dir}/verify-win11-amd64.ps1" -Exe "$<TARGET_FILE:DOOM3>"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS DOOM3
    COMMENT "scripts/verify-win11-amd64.ps1 (PASS/FAIL/SKIPPED per criterion)"
    USES_TERMINAL VERBATIM)
add_custom_target(smoke
    COMMAND "${DOOM3_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass -File "${_script_dir}/smoke-win11-amd64.ps1" -Exe "$<TARGET_FILE:DOOM3>"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS DOOM3
    COMMENT "scripts/smoke-win11-amd64.ps1 (needs D3_DATA_DIR)"
    USES_TERMINAL VERBATIM)
set_target_properties(verify smoke PROPERTIES EXCLUDE_FROM_ALL TRUE)
