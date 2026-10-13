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
set(CNA_ENABLE_NET     OFF CACHE BOOL   "Build CNA networking")
set(CNA_ENABLE_VIDEO   OFF CACHE STRING "Enable FFmpeg video playback")
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
target_compile_definitions(museum_core PUBLIC
    MUSEUM_VERSION="${PROJECT_VERSION}"
    MUSEUM_SOURCE_CONTENT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Content")

# Warnings apply to the museum's own code only. CNA's and sharp-runtime's public
# headers are not warning-clean under -Wextra/-Wpedantic, so their include
# directories are passed as -isystem (GCC and Clang then silence them).
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(museum_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion
        -Wno-sign-conversion -Wno-overloaded-virtual)
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

if(WIN32 AND COMMAND cna_copy_sdl_runtime)
    cna_copy_sdl_runtime(cna-3d-museum)
endif()
