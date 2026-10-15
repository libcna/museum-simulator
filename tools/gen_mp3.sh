#!/usr/bin/env bash
# Writes the small MP3 source used by the content-pipeline audio room.
#
#     tools/gen_mp3.sh        # needs ffmpeg with libmp3lame and Content/Audio/arpeggio.wav
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source="$here/Content/Audio/arpeggio.wav"
out="$here/Content/Audio/arpeggio.mp3"
ffmpeg -hide_banner -loglevel error -y -i "$source" -map 0:a:0 -c:a libmp3lame -b:a 64k \
    -map_metadata -1 -id3v2_version 0 -write_xing 0 -fflags +bitexact -flags:a +bitexact "$out"
ls -l "$out"
