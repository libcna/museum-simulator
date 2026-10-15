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
  local script="" r k log script_file peak textures
  for ((r = 0; r < $1; ++r)); do for k in "${keys[@]}"; do script+="goto:$k;wait:2;"; done; done
  script+="stats;quit"
  log="${BUILD_DIR:-build}/leak$1.log"
  script_file="${BUILD_DIR:-build}/leak$1.script"
  printf '%s' "$script" > "$script_file"
  if ! SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 /usr/bin/time -v xvfb-run -a -s "-screen 0 1920x1080x24" "$bin" --script-file "$script_file" --no-audio --fixed-step 0.02 > "$log" 2>&1; then
    echo "leak check run $1 failed before comparison (log: $log)" >&2
    return 1
  fi
  read -r peak textures < <(awk '
    /\[museum\]\[stats\]/ {
      for (i = 1; i <= NF; ++i) if ($i == "textures") textures = $(i + 1)
    }
    /Maximum resident set size/ { peak = $NF }
    END { if (peak != "" && textures != "") print peak, textures }
  ' "$log")
  if [ -z "${peak:-}" ] || [ -z "${textures:-}" ]; then
    echo "leak check run $1 has no final stats or peak memory (log: $log)" >&2
    return 1
  fi
  printf '%s %s\n' "$peak" "$textures"
}
result1=$(run 1) || exit $?
result3=$(run 3) || exit $?
read -r peak1 tex1 <<< "$result1"
read -r peak3 tex3 <<< "$result3"
growth=$(( (peak3 - peak1) * 100 / peak1 ))
echo "one tour: peak $((peak1 / 1024)) MB, $tex1 textures;  three tours: peak $((peak3 / 1024)) MB, $tex3 textures;  growth $growth %"
if [ "$growth" -gt "$limit" ] || [ "$tex1" != "$tex3" ]; then echo "FAIL: memory or textures grew across tours"; exit 1; fi
echo "ok"
