#!/usr/bin/env bash
# Runs the Renderer Validation rooms (floor 9) on each given renderer and prints a PASS/FAIL matrix.
#   tools/renderer_check.sh [RENDERER ...]        default: every renderer the build contains that works here
# The checks are the museum's own rv-* rooms: every line "[museum][validate] <room> <check>: PASS|FAIL|INFO|SKIP"
# in the log is one cell of the matrix. Exit status: 0 when no renderer had a FAIL, 1 otherwise.
set -u
cd "$(dirname "$0")/.." || exit 2
renderers=("$@")
[ ${#renderers[@]} -gt 0 ] || renderers=(OPENGLES3 OPENGL33 SOFTWARE)
mapfile -t rooms < <(${MUSEUM_BIN:-${BUILD_DIR:-build}/bin/cna-3d-museum} --list-rooms | grep '^rv-')
script=""
for r in "${rooms[@]}"; do script+="goto:$r;wait:10;"; done
script+="quit"
out=${BUILD_DIR:-build}/renderer_check
mkdir -p "$out"
status=0
for r in "${renderers[@]}"; do
  CNA_GRAPHICS_RENDERER=$r MUSEUM_TIMEOUT=${MUSEUM_TIMEOUT:-600} tools/run.sh "$script" > "$out/$r.console" 2>&1
  rc=$?
  cp "${BUILD_DIR:-build}/run.log" "$out/$r.log"
  grep "validate\]" "$out/$r.log" | sed 's/^\[museum\]\[validate\] //' > "$out/$r.checks"
  pass=$(grep -c ": PASS" "$out/$r.checks"); fail=$(grep -c ": FAIL" "$out/$r.checks")
  info=$(grep -c ": INFO" "$out/$r.checks"); skip=$(grep -c ": SKIP" "$out/$r.checks")
  printf "%-10s exit %-3s  %3d pass  %3d fail  %2d info  %2d skip\n" "$r" "$rc" "$pass" "$fail" "$info" "$skip"
  [ "$fail" -gt 0 ] && status=1
  [ "$rc" -ne 0 ] && status=1
done
echo "per-renderer results: $out/<RENDERER>.checks"
exit $status
