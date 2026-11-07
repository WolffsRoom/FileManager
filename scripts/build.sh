#!/bin/sh
# Builds FileManager.vpk with the standard (hard-float) VitaSDK Docker image. Run from anywhere:
#   sh scripts/build.sh [Release|Debug] [version-label]
# Every successful VPK is copied to scripts/build/vpk/<version-label>/ with a
# timestamp. Existing packages are never overwritten.
set -e
CONFIG="${1:-Release}"
VERSION_LABEL="${2:-v0.4-test}"
ROOT="$(cd "$(dirname "$0")/.."; pwd -W 2>/dev/null || pwd)"

MSYS_NO_PATHCONV=1 docker run --rm -v "$ROOT:/project" -w /project \
  vitasdk/vitasdk:latest sh -lc \
  "(command -v python3 >/dev/null || (apt-get install -y python3 >/dev/null 2>&1 || apk add --no-cache python3 >/dev/null)) && \
   cmake -S . -B build -DCMAKE_BUILD_TYPE=$CONFIG && cmake --build build -j8"

ls -la "$ROOT/build/FileManager.vpk"

STAMP="$(date +%Y%m%d-%H%M%S)"
ARCHIVE_DIR="$ROOT/scripts/build/vpk/$VERSION_LABEL"
ARCHIVE_VPK="$ARCHIVE_DIR/FileManager-$VERSION_LABEL-$STAMP.vpk"
mkdir -p "$ARCHIVE_DIR"
cp -n "$ROOT/build/FileManager.vpk" "$ARCHIVE_VPK"
echo "Archived without replacing previous builds: $ARCHIVE_VPK"
