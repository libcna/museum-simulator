#!/bin/bash
# Runs the museum on a private virtual X display (never the live desktop) and writes a PNG.
#   tools/shot.sh OUT.png [museum options...]
# Software GL keeps it independent of the host GPU; the audio driver is a dummy.
set -e
out="$1"; shift
bin="$(dirname "$0")/../build/bin/cna-3d-museum"
SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 timeout 180 \
  xvfb-run -a -s '-screen 0 1920x1080x24' "$bin" --screenshot "$out" "$@" 2>&1 | grep -v '^\[INFO\]' || true
