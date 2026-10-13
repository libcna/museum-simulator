#!/bin/bash
# Incremental build of the museum in the shared build/ directory, with the shared ccache.
# Prints only diagnostics; the full log is build/build.log.
cd "$(dirname "$0")/.."
export CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv
if [ ! -f build/build.ninja ]; then
  cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache > build/configure.log 2>&1 || { tail -20 build/configure.log; exit 1; }
fi
cmake --build build --target cna-3d-museum --parallel "$(nproc)" > build/build.log 2>&1
status=$?
grep -E "error:|warning:|FAILED:|undefined reference" -A3 build/build.log | grep -v "^--$" | cut -c1-260 | head -${LINES_MAX:-60}
[ $status -eq 0 ] && echo "build ok" || echo "build FAILED"
exit $status
