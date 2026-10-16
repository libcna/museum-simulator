#!/usr/bin/env bash
# Navigation smoke test: rides the elevator to every floor, then physically walks every stair flight upward and
# the first flight downward. It checks the museum's own statistics line after each route.
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

# Every stair cell has the same local switchback: west up the lower run, north over the landing, east up the upper run.
for ((f = 0; f + 1 < floors; ++f)); do
  upper=$((f + 1))
  tools/run.sh "goto:stairs-$f-$upper;wait:8;walk:1,0,150;look:0,0;walk:1,0,50;look:-90,0;walk:1,0,190;wait:8;stats;quit" --no-audio > "${BUILD_DIR:-build}/nav.out" 2>&1 || { echo "FAIL stairs $f->$upper: the run failed"; status=1; continue; }
  got=$(grep -o " floor [0-9]*  exhibit errors" "${BUILD_DIR:-build}/run.log" | tail -1 | grep -o "floor [0-9]*" | grep -o '[0-9]*')
  if [ "$got" = "$upper" ]; then echo "ok    stairs from floor $f to $upper"; else echo "FAIL  stairs from floor $f to $upper arrived on floor ${got:-?}"; status=1; fi
done

# Reverse traversal exercises descending ground resolution rather than only the ascending profile.
if tools/run.sh 'goto:stairs-0-1;wait:2;at:4.4,-1.6,90,0;wait:5;walk:1,0,150;look:180,0;walk:1,0,50;look:-90,0;walk:1,0,190;wait:8;stats;quit' --no-audio > "${BUILD_DIR:-build}/nav.out" 2>&1; then
  got=$(grep -o " floor [0-9]*  exhibit errors" "${BUILD_DIR:-build}/run.log" | tail -1 | grep -o "floor [0-9]*" | grep -o '[0-9]*')
  if [ "$got" = 0 ]; then echo "ok    stairs from floor 1 to 0"; else echo "FAIL  stairs from floor 1 to 0 arrived on floor ${got:-?}"; status=1; fi
else
  echo "FAIL stairs 1->0: the run failed"
  status=1
fi
exit $status
