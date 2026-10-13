#!/usr/bin/env bash
# Visits every room (or the ones named) for a few frames each on a private virtual display and reports
# errors, exceptions and the museum's own resource counters. Exit status 0 only if nothing went wrong.
#   tools/tour.sh [-f FRAMES] [room-key ...]
set -u
cd "$(dirname "$0")/.." || exit 2
frames=12
if [ "${1:-}" = "-f" ]; then frames=$2; shift 2; fi
bin=${MUSEUM_BIN:-${BUILD_DIR:-build}/bin/cna-3d-museum}
if [ $# -gt 0 ]; then keys=("$@"); else mapfile -t keys < <("$bin" --list-rooms); fi
script=""
for k in "${keys[@]}"; do script+="goto:$k;wait:$frames;"; done
script+="stats;quit"
echo "touring ${#keys[@]} rooms, $frames frames each ..."
start=$(date +%s)
MUSEUM_TIMEOUT=${MUSEUM_TIMEOUT:-1500} tools/run.sh "$script" --no-audio > "${BUILD_DIR:-build}/tour.log" 2>&1
status=$?
end=$(date +%s)
grep -E "\[museum\]\[(script|stats)\]|exception|ERROR|FAILED|INVALID|Assertion" "${BUILD_DIR:-build}/tour.log" | grep -v "^\[museum\]\[models\]" | tail -15
echo "exit status $status, $((end - start)) s"
exit $status
