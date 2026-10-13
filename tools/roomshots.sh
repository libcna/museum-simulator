#!/usr/bin/env bash
# Screenshot a list of rooms from a standard vantage point and join them into one contact sheet.
#   tools/roomshots.sh OUT.png key1 key2 ...        (env: AT="x,z,yaw,pitch" to change the viewpoint, COLS=3)
set -u
cd "$(dirname "$0")/.." || exit 2
out=$1; shift
at=${AT:-3.5,5.5,50,-8}
dir=${BUILD_DIR:-build}/shots/sheet
mkdir -p "$dir"; rm -f "$dir"/*.png
script=""
for k in "$@"; do script+="goto:$k;wait:8;at:$at;wait:6;shot:$dir/$k.png;"; done
tools/run.sh "${script}quit" --no-audio > "$dir/log.txt" 2>&1 || { tail -5 "$dir/log.txt"; exit 1; }
python3 - "$out" "$dir" "${COLS:-3}" "$@" <<'PY'
import sys
from PIL import Image
out, d, cols, keys = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4:]
w, h = 640, 360
rows = (len(keys) + cols - 1) // cols
sheet = Image.new("RGB", (cols * w, rows * h))
for i, k in enumerate(keys):
    try:
        im = Image.open(f"{d}/{k}.png").convert("RGB").resize((w, h))
    except Exception:
        continue
    sheet.paste(im, ((i % cols) * w, (i // cols) * h))
sheet.save(out)
PY
