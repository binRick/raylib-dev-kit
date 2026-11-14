#!/usr/bin/env bash
# One launcher for every dev mode. The modes, their items and their keys come from the game
# binary itself (kit_dev.h), so this script never needs editing when you add one.
#   ./dev.sh                    list the modes
#   ./dev.sh sounds             the sound board
#   ./dev.sh props barrel       a gallery, starting on one item
#   ./dev.sh models --shots out/    save one picture per item, then quit
# Everything the run prints is kept in logs/dev-<mode>.log, and the run ends with a count of
# the warnings and errors in it (raylib reports a missing file as a warning and carries on).
set -uo pipefail
cd "$(dirname "$0")"
BIN=./build/demo    # <- your game's binary

cmake -S . -B build > /dev/null 2>&1 && cmake --build build -j "$(sysctl -n hw.ncpu 2>/dev/null || nproc)" 2>&1 | grep -E "error|warning: " -A3
[[ -x $BIN ]] || { echo "build failed: run 'cmake --build build' to see why" >&2; exit 1; }
if [[ $# -eq 0 || $1 == -h || $1 == --help ]]; then exec "$BIN" --dev; fi

mkdir -p logs
log="logs/dev-$1.log"
[[ -f $log ]] && mv -f "$log" "${log%.log}.prev.log"
"$BIN" --dev "$@" 2>&1 | tee "$log" | grep --line-buffered -E "WARNING|ERROR|unknown" 
status=${PIPESTATUS[0]}
echo "[dev $1] exit $status   $(grep -c -E 'WARNING|ERROR' "$log") warning/error lines   log: $log"
exit "$status"
