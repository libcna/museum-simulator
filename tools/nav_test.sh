#!/usr/bin/env bash
# Navigation smoke test: rides the elevator to every floor and back, opens the directory on each floor, and checks
# from the museum's own statistics line that the visitor really arrived on the requested floor.
set -u
cd "$(dirname "$0")/.." || exit 2
bin=${MUSEUM_BIN:-${BUILD_DIR:-build}/bin/cna-3d-museum}
floors=$("$bin" --inventory | grep -o '"floors": [0-9]*' | grep -o '[0-9]*')
status=0
for ((f = 0; f < floors; ++f)); do
  tools/run.sh "ride:$f;wait:160;stats;directory:open;wait:5;directory:close;wait:3;quit" --no-audio > "${BUILD_DIR:-build}/nav.out" 2>&1 || { echo "FAIL floor $f: the run failed"; status=1; continue; }
  got=$(grep -o " floor [0-9]*  exhibit errors" "${BUILD_DIR:-build}/run.log" | tail -1 | grep -o "floor [0-9]*" | grep -o '[0-9]*')
  if [ "$got" = "$f" ]; then echo "ok    ride to floor $f"; else echo "FAIL  ride to floor $f arrived on floor ${got:-?}"; status=1; fi
done
exit $status
