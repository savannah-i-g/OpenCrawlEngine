#!/usr/bin/env bash
# Build the Debian package (Ubuntu 24.04 target) via CPack.
# Output: build-release/opencrawlengine_<version>_amd64.deb
#
# Requires: a working build toolchain plus dpkg-dev (for dpkg-shlibdeps).
# Usage: scripts/make-deb.sh
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build="$here/build-release"

cmake -S "$here" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DOCE_SANITIZERS=OFF
cmake --build "$build"
( cd "$build" && cpack -G DEB )

echo "Package: $(ls "$build"/opencrawlengine_*_amd64.deb)"
