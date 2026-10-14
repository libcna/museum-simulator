#!/usr/bin/env bash
# Visits every room (or the ones named) for a few frames each on a private virtual display and reports
# errors, exceptions and the museum's own resource counters. Exit status 0 only if nothing went wrong.
#   tools/tour.sh [-f FRAMES] [-p] [room-key ...]      -p: also operate every control of every room (smoke test)
set -u
cd "$(dirname "$0")/.." || exit 2
frames=12
press=0
while [ $# -gt 0 ]; do
  case "$1" in
    -f) frames=$2; shift 2 ;;
    -p) press=1; shift ;;
    *) break ;;
  esac
done
bin=${MUSEUM_BIN:-${BUILD_DIR:-build}/bin/cna-3d-museum}
if [ $# -gt 0 ]; then keys=("$@"); else mapfile -t keys < <("$bin" --list-rooms); fi
script=""
for k in "${keys[@]}"; do script+="goto:$k;wait:$frames;"; [ $press = 1 ] && script+="pressall;wait:4;"; done
script+="stats;quit"
echo "touring ${#keys[@]} rooms, $frames frames each ..."
start=$(date +%s)
MUSEUM_TIMEOUT=${MUSEUM_TIMEOUT:-1500} tools/run.sh "$script" --no-audio > "${BUILD_DIR:-build}/tour.log" 2>&1
status=$?
end=$(date +%s)
grep -E "\[museum\]\[(script|stats)\]|exception|ERROR|FAILED|INVALID|Assertion" "${BUILD_DIR:-build}/tour.log" | grep -v "^\[museum\]\[models\]" | grep -v "pressall: [0-9]* panel" | tail -15
caught=$(grep -c "\[museum\]\[exhibit-error\]" "${BUILD_DIR:-build}/tour.log")
if [ "$caught" -gt 0 ]; then
  echo "$caught exhibit exception(s) were caught (the museum carried on, but a room is broken on this renderer):"
  grep "\[museum\]\[exhibit-error\]" "${BUILD_DIR:-build}/tour.log" | cut -c1-240 | sort -u | head -10
  [ $status -eq 0 ] && status=3
fi
echo "exit status $status, $((end - start)) s"
exit $status
