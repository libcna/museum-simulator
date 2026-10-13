#!/usr/bin/env bash
# Runs the museum on a private virtual X display and writes one PNG; fails if the PNG was not produced.
#   tools/shot.sh OUT.png [museum options...]
set -u
source "$(dirname "$0")/_headless.sh"
[ $# -ge 1 ] || { echo "usage: tools/shot.sh OUT.png [museum options...]" >&2; exit 2; }
out=$1; shift
root=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$(dirname "$out")"
rm -f "$out"
museum_headless "$root/${BUILD_DIR:-build}/shot.log" --screenshot "$out" "$@"
status=$?
if [ $status -eq 0 ] && [ ! -s "$out" ]; then
  echo "error: the museum exited normally but wrote no screenshot: $out" >&2
  status=3
fi
exit $status
