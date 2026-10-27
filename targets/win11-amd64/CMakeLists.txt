cmake_minimum_required(VERSION 3.20)

cmake_policy(SET CMP0091 NEW)

if(NOT DEFINED CMAKE_SYSTEM_VERSION OR CMAKE_SYSTEM_VERSION STREQUAL "")
    set(CMAKE_SYSTEM_VERSION "10.0.26100.0" CACHE STRING "Windows SDK version for the x64 port")
endif()

project(Doom3Native LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "This build is for Windows only.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "This build targets x64 only.")
endif()

set(NEO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/doom3/neo" CACHE PATH "Target copy of the DOOM 3 engine source")

if(NOT EXISTS "${NEO_DIR}/idlib/precompiled.h")
    message(FATAL_ERROR
        "Target copy not found at ${NEO_DIR}.\n"
        "Create it from the repository root with:\n"
        "  scripts/new-target.ps1 -Name win11-amd64")
endif()

set(SOURCES_CMAKE "${CMAKE_CURRENT_SOURCE_DIR}/sources.cmake")
if(NOT EXISTS "${SOURCES_CMAKE}")
    message(FATAL_ERROR
        "sources.cmake is missing. Regenerate it from the repository root with:\n"
        "  powershell -ExecutionPolicy Bypass -File scripts/gen-sources-win11-amd64.ps1")
endif()
include("${SOURCES_CMAKE}")

foreach(list_name IDLIB_SOURCES ENGINE_SOURCES GAME_SOURCES)
    list(TRANSFORM ${list_name} PREPEND "${NEO_DIR}/")
endforeach()

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(DOOM3
    ${IDLIB_SOURCES}
    ${ENGINE_SOURCES}
    ${GAME_SOURCES}
    src/eaxguid_x64.cpp
)

target_include_directories(DOOM3 PRIVATE
    "${NEO_DIR}"
    "${NEO_DIR}/sound/OggVorbis"
    "${NEO_DIR}/sound/OggVorbis/vorbissrc"
    "${NEO_DIR}/renderer/jpeg-6"
)

target_compile_definitions(DOOM3 PRIVATE
    WIN32
    _WINDOWS
    _CRT_SECURE_NO_DEPRECATE
    _CRT_NONSTDC_NO_DEPRECATE
    __DOOM__
    ID_ENABLE_CURL=0
    ID_NO_TOOLS
    ID_ENFORCE_KEY=0        # the retail CD-key gate drops +devmap/+map when no key is installed; the auth servers are gone anyway
    _ALLOW_KEYWORD_MACROS
)

option(DOOM3_USE_MP "Compile with /MP (multi-processor compilation)" ON)

target_compile_options(DOOM3 PRIVATE
    /W3
    "$<$<COMPILE_LANGUAGE:CXX>:/EHsc>"
)

if(DOOM3_USE_MP)
    target_compile_options(DOOM3 PRIVATE /MP)
endif()

set_source_files_properties("${NEO_DIR}/sys/win32/win_input.cpp" PROPERTIES
    COMPILE_OPTIONS "/source-charset:.1252")

target_compile_options(DOOM3 PRIVATE /Zi)
target_link_options(DOOM3 PRIVATE
    "/DEBUG"
    "/SUBSYSTEM:WINDOWS"
    "/STACK:16777216"
)

target_link_libraries(DOOM3 PRIVATE
    dbghelp
    dinput8
    dsound
    dxguid
    glu32
    iphlpapi
    opengl32
    winmm
    ws2_32
    wsock32
    comctl32
    version
    imm32
)

set_target_properties(DOOM3 PROPERTIES
    OUTPUT_NAME "DOOM3"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

message(STATUS "NEO_DIR: ${NEO_DIR}")
