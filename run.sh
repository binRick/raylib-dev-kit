#!/usr/bin/env bash
# Build the example for this machine (macOS or Linux) and run it. Arguments go to the example.
set -euo pipefail
cd "$(dirname "$0")"
cmake -S . -B build > /dev/null
cmake --build build -j "$(sysctl -n hw.ncpu 2>/dev/null || nproc)"
exec ./build/demo "$@"
