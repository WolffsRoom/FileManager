#!/bin/sh
# Builds FileManager.vpk with the standard (hard-float) VitaSDK Docker image. Run from anywhere:
#   sh scripts/build.sh [Release|Debug]
set -e
CONFIG="${1:-Release}"
ROOT="$(cd "$(dirname "$0")/.."; pwd -W 2>/dev/null || pwd)"

MSYS_NO_PATHCONV=1 docker run --rm -v "$ROOT:/project" -w /project \
  vitasdk/vitasdk:latest sh -lc \
  "(command -v python3 >/dev/null || (apt-get install -y python3 >/dev/null 2>&1 || apk add --no-cache python3 >/dev/null)) && \
   cmake -S . -B build -DCMAKE_BUILD_TYPE=$CONFIG && cmake --build build -j8"

ls -la "$ROOT/build/FileManager.vpk"
