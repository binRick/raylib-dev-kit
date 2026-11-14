#!/usr/bin/env bash
# Build the example for the browser (WebAssembly + WebGL 2) into dist/web/.
#   scripts/build-web.sh [--serve]     --serve: then serve it at http://127.0.0.1:8080/
set -euo pipefail
cd "$(dirname "$0")/.."
command -v emcmake > /dev/null || { echo "error: emscripten not found (brew install emscripten)" >&2; exit 1; }
# Homebrew's emcc needs Python 3.10+, which Xcode's python3 (3.9) shadows on macOS.
if [[ -z "${EMCC_PYTHON:-}" && -d /opt/homebrew/bin ]]; then
    py="$(ls /opt/homebrew/bin/python3.1[0-9] 2>/dev/null | sort -V | tail -1 || true)"
    [[ -n "$py" ]] && export EMCC_PYTHON="$py" EMSDK_PYTHON="$py"
fi
[[ -f build-web/CMakeCache.txt ]] || emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web -j "$(sysctl -n hw.ncpu 2>/dev/null || nproc)"
rm -rf dist/web && mkdir -p dist/web
cp build-web/index.{html,js,wasm,data} dist/web/
du -sh dist/web
if [[ "${1:-}" == "--serve" ]]; then
    echo "Serving http://127.0.0.1:8080/ (Ctrl-C to stop)"
    cd dist/web && exec python3 -m http.server 8080 --bind 127.0.0.1
fi
