#!/usr/bin/env bash
# Takes the screenshots shown in README.md: real frames from the running museum (never mocked up), rendered on a
# private virtual display with the OPENGLES3 renderer, then recompressed as JPEG for the repository.
#   tools/readme_shots.sh            writes docs/images/*.jpg
set -u
cd "$(dirname "$0")/.." || exit 2
out=docs/images
tmp=${BUILD_DIR:-build}/readme_shots
mkdir -p "$out" "$tmp"
export CNA_GRAPHICS_RENDERER=${CNA_GRAPHICS_RENDERER:-OPENGLES3}
shots=(lobby vectors instancing lights shader mirror pbr fire debugdraw validation deck formats)
status=0
for v in "${shots[@]}"; do
  tools/shot.sh "$tmp/$v.png" --view "$v" --time 11 --no-audio --width 1280 --height 720 --fixed-step 0.02 --screenshot-frame 24 > "$tmp/$v.log" 2>&1 || { echo "FAILED $v"; status=1; continue; }
done
# The directory screen and the elevator button panel need the script runner.
tools/run.sh "goto:hub-0;wait:5;directory:open;wait:6;shot:$tmp/directory.png;directory:close;wait:3;quit" --time 11 --no-audio --fixed-step 0.02 > "$tmp/directory.log" 2>&1 || { echo "FAILED directory"; status=1; }
# Rooms whose screen is best seen from close by: step up to the screen first (the E key), then take the frame.
for pair in "sampler:model-sampler:0" "colours:vt-colors:1" "fontbuild:ct-fontbuild:1" "shadererrors:fx-errors:1" "meshbuilder:ct-meshbuilder:0"; do
  IFS=: read -r name room step <<< "$pair"
  act=""; [ "$step" = 1 ] && act="e;wait:30;"
  tools/run.sh "goto:$room;wait:40;${act}shot:$tmp/$name.png;quit" --time 11 --no-audio --no-hud --width 1280 --height 720 --fixed-step 0.02 > "$tmp/$name.log" 2>&1 || { echo "FAILED $name"; status=1; }
done
for f in "$tmp"/*.png; do
  n=$(basename "$f" .png)
  python3 -I -c "import sys; from PIL import Image; Image.open(sys.argv[1]).convert('RGB').save(sys.argv[2], quality=84, optimize=True)" "$f" "$out/$n.jpg"
done
ls -la "$out"
exit $status
