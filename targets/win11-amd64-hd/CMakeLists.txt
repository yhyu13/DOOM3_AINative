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
        "  scripts/new-target.ps1 -Name win11-amd64-hd")
endif()

set(SOURCES_CMAKE "${CMAKE_CURRENT_SOURCE_DIR}/sources.cmake")
if(NOT EXISTS "${SOURCES_CMAKE}")
    message(FATAL_ERROR
        "sources.cmake is missing. Regenerate it from the repository root with:\n"
        "  powershell -ExecutionPolicy Bypass -File scripts/gen-sources-win11-amd64.ps1 -Neo targets/win11-amd64-hd/doom3/neo -OutFile targets/win11-amd64-hd/sources.cmake")
endif()
include("${SOURCES_CMAKE}")

foreach(list_name IDLIB_SOURCES ENGINE_SOURCES GAME_SOURCES D3XP_SOURCES)
    list(TRANSFORM ${list_name} PREPEND "${NEO_DIR}/")
endforeach()

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(DOOM3_USE_MP "Compile with /MP (multi-processor compilation)" ON)

# Settings shared by every translation unit of every executable.
add_library(doom3_common INTERFACE)
target_include_directories(doom3_common INTERFACE
    "${NEO_DIR}"
    "${NEO_DIR}/sound/OggVorbis"
    "${NEO_DIR}/sound/OggVorbis/vorbissrc"
    "${NEO_DIR}/renderer/jpeg-6"
)
target_compile_definitions(doom3_common INTERFACE
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
target_compile_options(doom3_common INTERFACE
    /W3
    "$<$<COMPILE_LANGUAGE:CXX>:/EHsc>"
    /Zi
)
if(DOOM3_USE_MP)
    target_compile_options(doom3_common INTERFACE /MP)
endif()

set_source_files_properties("${NEO_DIR}/sys/win32/win_input.cpp" PROPERTIES
    COMPILE_OPTIONS "/source-charset:.1252")

# idlib + engine: identical for both executables (the retail engine is one EXE that loads either
# game module; the only engine-visible difference, TIME_GROUP*, is unused by engine code).
add_library(doom3_engine OBJECT ${IDLIB_SOURCES} ${ENGINE_SOURCES} src/eaxguid_x64.cpp)
target_link_libraries(doom3_engine PRIVATE doom3_common)

# Game modules: compiled-in (no GAME_DLL / __DOOM_DLL__), so each executable owns exactly one
# set of the colliding globals (game, gameLocal, ...). Never link both into one EXE.
add_library(doom3_game_base OBJECT ${GAME_SOURCES})
target_link_libraries(doom3_game_base PRIVATE doom3_common)

# d3xp (Resurrection of Evil): _D3XP;CTF as in neo/_Game-d3xp.props, minus GAME_DLL.
add_library(doom3_game_d3xp OBJECT ${D3XP_SOURCES})
target_link_libraries(doom3_game_d3xp PRIVATE doom3_common)
target_compile_definitions(doom3_game_d3xp PRIVATE _D3XP CTF)

set(DOOM3_LINK_LIBS
    dbghelp dinput8 dsound dxguid glu32 iphlpapi opengl32 winmm
    ws2_32 wsock32 comctl32 version imm32
)

function(doom3_add_exe exe_name game_objects)
    add_executable(${exe_name} $<TARGET_OBJECTS:doom3_engine> $<TARGET_OBJECTS:${game_objects}>)
    target_link_options(${exe_name} PRIVATE "/DEBUG" "/SUBSYSTEM:WINDOWS" "/STACK:16777216")
    target_link_libraries(${exe_name} PRIVATE ${DOOM3_LINK_LIBS})
    set_target_properties(${exe_name} PROPERTIES
        OUTPUT_NAME "${exe_name}"
        LINKER_LANGUAGE CXX
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endfunction()

doom3_add_exe(DOOM3 doom3_game_base)
# DOOM3XP.exe: run with +set fs_game d3xp (CVAR_INIT; needs d3xp/pak000.pk4 from retail RoE).
doom3_add_exe(DOOM3XP doom3_game_d3xp)

message(STATUS "NEO_DIR: ${NEO_DIR}")
