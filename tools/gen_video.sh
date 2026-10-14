#!/usr/bin/env bash
# Writes Content/Video/zoom.webm: a six-second clip for the Video room, made by FFmpeg's own Mandelbrot source
# (a zoom into the set) with the museum's arpeggio as its soundtrack. VP8 and Vorbis, both free formats, about 130 KB.
#
#     tools/gen_video.sh        # needs ffmpeg with libvpx and libvorbis, and Content/Audio/arpeggio.wav (tools/gen_assets.py)
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="$here/Content/Video"
mkdir -p "$out"
ffmpeg -hide_banner -loglevel error -y \
    -f lavfi -i "mandelbrot=size=320x180:rate=24:end_scale=0.05" \
    -i "$here/Content/Audio/arpeggio.wav" \
    -t 6 -map 0:v -map 1:a -c:v libvpx -b:v 150k -crf 30 -c:a libvorbis -ac 1 -b:a 32k -bitexact -fflags +bitexact \
    "$out/zoom.webm"
ls -l "$out/zoom.webm"
