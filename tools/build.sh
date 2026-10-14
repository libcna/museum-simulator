#!/usr/bin/env bash
# Configure (first time only) and build the museum. Works from a clean checkout.
#
#   tools/build.sh [target ...]        default target: cna-3d-museum
#
# Environment (all optional):
#   BUILD_DIR            build tree, relative to the repository root (default: build)
#   MUSEUM_RENDERERS     semicolon list for CNA_GRAPHICS_RENDERERS, e.g. "OPENGLES3;OPENGL33;VULKAN;SOFTWARE";
#                        the first entry becomes the default renderer. Empty: CNA's platform default.
#   MUSEUM_CMAKE_ARGS    extra arguments for the configure step
#   RECONFIGURE=1        run cmake again even if the tree is already configured
#   VERBOSE=1            stream the full build output instead of only diagnostics
#   JOBS=N               parallel jobs (default: 4, the most this project compiles with; lower it for sanitizer builds when RAM is short)
#
# ccache is used when it is on PATH. Cache location and size follow the user's own ccache
# configuration (CCACHE_DIR, ccache.conf); nothing is overridden here.
set -u
cd "$(dirname "$0")/.." || exit 2
root=$PWD
build_dir=${BUILD_DIR:-build}
targets=("$@")
[ ${#targets[@]} -gt 0 ] || targets=(cna-3d-museum)

mkdir -p "$build_dir"

generator=()
command -v ninja >/dev/null 2>&1 && generator=(-G Ninja)

launchers=()
if command -v ccache >/dev/null 2>&1; then
  launchers=(-DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache)
else
  echo "note: ccache not found; building without a compiler cache (slower rebuilds)"
fi

renderer_args=()
if [ -n "${MUSEUM_RENDERERS:-}" ]; then
  default_renderer=${MUSEUM_RENDERERS%%;*}
  renderer_args=("-DCNA_GRAPHICS_RENDERERS=${MUSEUM_RENDERERS}" "-DCNA_GRAPHICS_RENDERER=${default_renderer}")
fi

if [ ! -f "$build_dir/CMakeCache.txt" ] || [ "${RECONFIGURE:-0}" = 1 ]; then
  echo "configuring $build_dir ..."
  # shellcheck disable=SC2086
  cmake -S . -B "$build_dir" "${generator[@]}" "${launchers[@]}" "${renderer_args[@]}" ${MUSEUM_CMAKE_ARGS:-} \
    > "$build_dir/configure.log" 2>&1 || { tail -30 "$build_dir/configure.log"; echo "configure FAILED"; exit 1; }
fi

if [ "${VERBOSE:-0}" = 1 ]; then
  cmake --build "$build_dir" --target "${targets[@]}" --parallel "${JOBS:-4}"
  status=$?
else
  cmake --build "$build_dir" --target "${targets[@]}" --parallel "${JOBS:-4}" \
    > "$build_dir/build.log" 2>&1
  status=$?
  # Errors always; warnings only from this project's own sources (CNA's third_party code is noisy).
  grep -E "error:|FAILED:|undefined reference|CMake Error|^(\.\./)*src/[^ ]*: warning:|^(\.\./)*tests/[^ ]*: warning:" \
    -A3 "$build_dir/build.log" | grep -v "^--$" | cut -c1-260 | head -"${LINES_MAX:-60}"
fi
if [ $status -eq 0 ]; then echo "build ok"; else echo "build FAILED (log: $build_dir/build.log)"; fi
exit $status
