# =============================================================================
# cna-3d-museum -- a walkable 3D museum that showcases the CNA framework.
# =============================================================================
# CNA is consumed the way cna-template consumes it: as a sibling checkout added
# with add_subdirectory() (CNA exports no CMake package). See README.md.
cmake_minimum_required(VERSION 3.23)

project(CnaMuseum
    VERSION 0.1.0
    DESCRIPTION "An interactive 3D museum demonstrating the CNA (C++ XNA 4.0) framework"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --- CNA and its siblings ------------------------------------------------------
# CNA resolves sharp-runtime (and easy-gl / meta-gl for the GL renderers)
# relative to its own source root, so all of them sit side by side:
#   <parent>/cna  <parent>/sharp-runtime  <parent>/easy-gl  <parent>/meta-gl
set(CNA_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../cna" CACHE PATH
    "Path to the CNA framework checkout")
if(NOT EXISTS "${CNA_ROOT_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "cna-3d-museum: CNA not found at '${CNA_ROOT_DIR}'.\n"
        "Clone it next to this project (see README.md, \"Build requirements\") or pass\n"
        "-DCNA_ROOT_DIR=/path/to/cna.")
endif()

# The renderer is CNA's own cache variable; nothing here is renderer-specific.
# Unset, CNA picks its platform default (OPENGLES3 on Linux). The museum uses
# only GraphicsCapability queries, so any 3D-capable renderer works.

# The museum needs neither CNA's tests, examples, networking nor video playback.
set(CNA_BUILD_TESTS    OFF CACHE BOOL   "Build CNA's own tests")
set(CNA_BUILD_EXAMPLES OFF CACHE BOOL   "Build CNA's example applications")
set(CNA_BUILD_C_API    OFF CACHE BOOL   "Build the CNA C API")
# Networking and GamerServices (Microsoft.Xna.Framework.Net / GamerServices) back the "Net and gamer services" wing. They pull in
# ENet and CNA's offline profile service; turn them off for a smaller build (those rooms then say so).
option(MUSEUM_NETWORKING "Demonstrate Microsoft.Xna.Framework.Net and GamerServices" ON)
set(CNA_ENABLE_NET     ${MUSEUM_NETWORKING} CACHE BOOL "Build CNA networking" FORCE)
set(CNA_ENABLE_GAMER_SERVICES ${MUSEUM_NETWORKING} CACHE BOOL "Build CNA GamerServices" FORCE)
# Video playback (VideoPlayer) needs FFmpeg's libraries. AUTO uses them when they are installed; the Video room says so when not.
set(MUSEUM_VIDEO "AUTO" CACHE STRING "Enable FFmpeg video playback (OFF, AUTO or ON)")
set(CNA_ENABLE_VIDEO "${MUSEUM_VIDEO}" CACHE STRING "Enable FFmpeg video playback" FORCE)
# CNA features the museum demonstrates: the CNAEXT graphics extensions (DebugDraw, CRT/ASCII/depth effects,
# shader packages), device/sensor extensions, and CNA's engine statistics counters. All three are optional:
# the exhibits that need them check for them and say so in the room when they are missing.
set(CNA_CNAEXT      ON    CACHE BOOL   "Build CNA's graphics extensions")
set(CNA_DEVICES     ON    CACHE BOOL   "Build CNA's device extensions")
set(CNA_DIAGNOSTICS STATS CACHE STRING "CNA diagnostics level (OFF|STATS|FULL)")
set(EASYGL_BUILD_TESTS    OFF CACHE BOOL "Build easy-gl tests")
set(EASYGL_BUILD_EXAMPLES OFF CACHE BOOL "Build easy-gl examples")
set(METAGL_BUILD_TESTS    OFF CACHE BOOL "Build meta-gl tests")

# CNA caches its vendored SDL build in its own checkout by default. Keep it in
# this build tree instead, keyed by toolchain, so the CNA checkout stays
# read-only and a rebuild of the museum never rebuilds SDL.
if(NOT DEFINED CNA_SDL_PREBUILT_ROOT)
    set(CNA_SDL_PREBUILT_ROOT
        "${CMAKE_BINARY_DIR}/cna-sdl-prebuilt-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_CXX_COMPILER_ID}"
        CACHE PATH "Persistent CNA SDL build cache, kept outside the CNA checkout")
endif()

# Link only the sharp-runtime components CNA itself needs.
include("${CNA_ROOT_DIR}/cmake/SharpRuntimeConsumption.cmake" OPTIONAL)
if(DEFINED CNA_SHARP_RUNTIME_DEFAULT_COMPONENTS AND NOT DEFINED SHARP_RUNTIME_COMPONENTS)
    set(SHARP_RUNTIME_COMPONENTS "${CNA_SHARP_RUNTIME_DEFAULT_COMPONENTS}"
        CACHE STRING "Sharp Runtime components required by CNA")
endif()

add_subdirectory("${CNA_ROOT_DIR}" cna EXCLUDE_FROM_ALL)

# --- the application -------------------------------------------------------------
# Everything except main() is a static library so tests link the code the
# application runs.
file(GLOB_RECURSE MUSEUM_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(REMOVE_ITEM MUSEUM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/Main.cpp")

add_library(museum_core STATIC ${MUSEUM_SOURCES})
target_include_directories(museum_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(museum_core PUBLIC CNA)
if(MUSEUM_NETWORKING AND TARGET CNA_Net AND TARGET CNA_GamerServices)
    target_link_libraries(museum_core PUBLIC CNA_Net CNA_GamerServices)
    target_compile_definitions(museum_core PUBLIC MUSEUM_NETWORKING=1)
endif()
# Microsoft.Xna.Framework.Design (the math type converters) is a separate CNA library.
if(TARGET cna_design)
    target_link_libraries(museum_core PUBLIC cna_design)
    target_compile_definitions(museum_core PUBLIC MUSEUM_DESIGN=1)
endif()
# CNA's content pipeline (IntermediateSerializer and the importers) is a build-time library; the Content Pipeline wing runs it in the game.
option(MUSEUM_PIPELINE "Link CNA's content pipeline library for the Pipeline in the Game room" ON)
if(MUSEUM_PIPELINE AND TARGET cna_content_pipeline)
    target_link_libraries(museum_core PUBLIC cna_content_pipeline)
    target_compile_definitions(museum_core PUBLIC MUSEUM_PIPELINE=1)
endif()
# The CNA revision this build was configured against, shown by the History rooms (read, never typed in).
execute_process(COMMAND git -C "${CNA_ROOT_DIR}" describe --always --dirty
                OUTPUT_VARIABLE MUSEUM_CNA_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT MUSEUM_CNA_REVISION)
    set(MUSEUM_CNA_REVISION "unknown")
endif()
target_compile_definitions(museum_core PUBLIC
    MUSEUM_CNA_REVISION="${MUSEUM_CNA_REVISION}"
    MUSEUM_VERSION="${PROJECT_VERSION}"
    MUSEUM_SOURCE_CONTENT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Content")

# Warnings apply to the museum's own code only. CNA's and sharp-runtime's public
# headers are not warning-clean under -Wextra/-Wpedantic, so their include
# directories are passed as -isystem (GCC and Clang then silence them).
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(museum_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion
        -Wno-sign-conversion -Wno-overloaded-virtual -Wno-missing-field-initializers)
    file(GLOB _museum_system_includes
        "${CNA_ROOT_DIR}/modules/*/include"
        "${CNA_ROOT_DIR}/modules/renderers/*/include"
        "${CNA_ROOT_DIR}/../sharp-runtime/modules/*/include")
    foreach(_dir IN LISTS _museum_system_includes)
        target_compile_options(museum_core PRIVATE "SHELL:-isystem ${_dir}")
    endforeach()
endif()

add_executable(cna-3d-museum src/Main.cpp)
target_link_libraries(cna-3d-museum PRIVATE museum_core)
if(TARGET SDL3::SDL3main)
    target_link_libraries(cna-3d-museum PRIVATE SDL3::SDL3main)
endif()
if(COMMAND cna_copy_renderer_runtime)
    cna_copy_renderer_runtime(cna-3d-museum)
endif()

# Put Content/ next to the executable so the build tree is self-contained.
add_custom_command(TARGET cna-3d-museum POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/Content" "$<TARGET_FILE_DIR:cna-3d-museum>/Content"
    COMMENT "Copying Content next to cna-3d-museum")

# CNA's offline achievement catalog is read from the title directory (next to the executable), not from Content/.
add_custom_command(TARGET cna-3d-museum POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/Content/GamerServices" "$<TARGET_FILE_DIR:cna-3d-museum>/GamerServices"
    COMMENT "Copying GamerServices/Achievements.json next to cna-3d-museum")

# Run the Content/Pipeline sources through CNA's own content compiler, once as .cnb (CNA's format) and once as
# .xnb (XNA's), next to the executable. The Content Pipeline rooms load the results and compare them with the
# loose source files. If the tool is not available the rooms say so instead of failing. When cross-compiling the tool is a
# foreign executable: it only runs here if CMAKE_CROSSCOMPILING_EMULATOR is set (for example to wine); otherwise the step is
# skipped and Content/Compiled can be copied from a native build tree (the files are the same on every platform).
if(TARGET cna_content_tool AND (NOT CMAKE_CROSSCOMPILING OR CMAKE_CROSSCOMPILING_EMULATOR))
    add_dependencies(cna-3d-museum cna_content_tool)
    foreach(_fmt cnb xnb)
        add_custom_command(TARGET cna-3d-museum POST_BUILD
            COMMAND "$<TARGET_FILE:cna_content_tool>" build "${CMAKE_CURRENT_SOURCE_DIR}/Content/Pipeline"
                -o "$<TARGET_FILE_DIR:cna-3d-museum>/Content/Compiled/${_fmt}" --format ${_fmt} --quiet
            COMMENT "Compiling Content/Pipeline to ${_fmt} with cna-content")
    endforeach()
endif()

if(WIN32 AND COMMAND cna_copy_sdl_runtime)
    cna_copy_sdl_runtime(cna-3d-museum)
endif()

# --- tests ---------------------------------------------------------------------------------------
# Structural tests of the museum's data (no window or graphics device needed): ctest --test-dir build
include(CTest)
add_executable(museum_tests EXCLUDE_FROM_ALL tests/museum_tests.cpp)
target_link_libraries(museum_tests PRIVATE museum_core)
add_test(NAME museum_structure COMMAND museum_tests)
