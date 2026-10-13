#!/bin/bash
# Runs the museum on a private virtual X display with a test script (never on the live desktop).
#   tools/run.sh "pos:...;look:...;shot:out.png;quit" [museum options...]
set -e
script="$1"; shift
bin="$(dirname "$0")/../build/bin/cna-3d-museum"
SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 timeout 300 \
  xvfb-run -a -s '-screen 0 1920x1080x24' "$bin" --script "$script" --fixed-step 0.02 "$@" 2>&1 | grep -v '^\[INFO\]' || true
