#!/usr/bin/env bash
# Resource-lifetime check: tours every room once and three times and compares the peak memory and the number of
# registered textures. Rooms are built and destroyed as the visitor moves, so a second and third tour must not grow
# the process much (a leak would grow by about the same amount per tour). Exit status 1 when it grows more than LIMIT %.
set -u
cd "$(dirname "$0")/.." || exit 2
bin=${MUSEUM_BIN:-${BUILD_DIR:-build}/bin/cna-3d-museum}
limit=${LIMIT:-25}
command -v /usr/bin/time >/dev/null || { echo "needs /usr/bin/time"; exit 2; }
mapfile -t keys < <("$bin" --list-rooms)
run() {   # run N -> prints "peak_kb textures"
  local script="" r k
  for ((r = 0; r < $1; ++r)); do for k in "${keys[@]}"; do script+="goto:$k;wait:2;"; done; done
  script+="stats;quit"
  SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 /usr/bin/time -v xvfb-run -a -s "-screen 0 1920x1080x24" "$bin" --script "$script" --no-audio --fixed-step 0.02 > "${BUILD_DIR:-build}/leak$1.log" 2>&1
  echo "$(grep 'Maximum resident' "${BUILD_DIR:-build}/leak$1.log" | grep -o '[0-9]*$') $(grep -o 'textures [0-9]*' "${BUILD_DIR:-build}/leak$1.log" | tail -1 | grep -o '[0-9]*$')"
}
read -r peak1 tex1 < <(run 1)
read -r peak3 tex3 < <(run 3)
growth=$(( (peak3 - peak1) * 100 / peak1 ))
echo "one tour: peak $((peak1 / 1024)) MB, $tex1 textures;  three tours: peak $((peak3 / 1024)) MB, $tex3 textures;  growth $growth %"
if [ "$growth" -gt "$limit" ] || [ "$tex1" != "$tex3" ]; then echo "FAIL: memory or textures grew across tours"; exit 1; fi
echo "ok"
