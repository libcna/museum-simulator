#!/usr/bin/env bash
# Runs the museum on a private virtual X display with a test script and returns the museum's exit status.
#   tools/run.sh "pos:...;look:...;shot:out.png;quit" [museum options...]
# See MuseumGame::RunScript (src/Testing/ScriptRunner.cpp) for the script language.
set -u
source "$(dirname "$0")/_headless.sh"
[ $# -ge 1 ] || { echo "usage: tools/run.sh \"script\" [museum options...]" >&2; exit 2; }
script=$1; shift
root=$(cd "$(dirname "$0")/.." && pwd)
museum_headless "$root/${BUILD_DIR:-build}/run.log" --script "$script" --fixed-step 0.02 "$@"
exit $?
