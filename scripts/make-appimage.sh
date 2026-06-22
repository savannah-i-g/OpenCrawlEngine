#!/usr/bin/env bash
# Build a self-contained AppImage for generic Linux. The AppImage bundles the
# binary, its non-system shared libraries, the icon set, and a serif font, so it
# runs across distributions without an install step. It also runs on Ubuntu
# 24.04. Output: OpenCrawlEngine-x86_64.AppImage in the repository root.
#
# Requires: a working build toolchain and network access on first run (to fetch
# linuxdeploy). The host OpenGL driver is used, not bundled.
# Usage: scripts/make-appimage.sh
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build="$here/build-release"
appdir="$here/AppDir"
tools="$here/.cache/appimage-tools"

# 1. Release build.
cmake -S "$here" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DOCE_SANITIZERS=OFF
cmake --build "$build"

# 2. Stage an install tree under AppDir/usr.
rm -rf "$appdir"
DESTDIR="$appdir" cmake --install "$build" --prefix /usr

# 3. Bundle a libre serif so the intended look holds on minimal systems. The
#    font is added only here, at package time; no font binary lives in the repo.
fontdir="$appdir/usr/share/opencrawlengine/fonts"
mkdir -p "$fontdir"
for face in DejaVuSerif DejaVuSerif-Bold DejaVuSerif-Italic DejaVuSerif-BoldItalic; do
    src="/usr/share/fonts/truetype/dejavu/${face}.ttf"
    if [ -f "$src" ]; then
        cp "$src" "$fontdir/"
    else
        echo "warning: $src not found; the AppImage will fall back to the default font" >&2
    fi
done

# 4. Fetch linuxdeploy if needed, then bundle dependencies and emit the AppImage.
mkdir -p "$tools"
ld="$tools/linuxdeploy-x86_64.AppImage"
if [ ! -x "$ld" ]; then
    url="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
    echo "Fetching linuxdeploy..."
    curl -fSL -o "$ld" "$url"
    chmod +x "$ld"
fi

cd "$here"
# Extract-and-run avoids needing FUSE for the linuxdeploy AppImage itself.
export APPIMAGE_EXTRACT_AND_RUN=1
"$ld" --appdir "$appdir" \
    --desktop-file "$here/packaging/opencrawlengine.desktop" \
    --icon-file "$here/packaging/opencrawlengine.png" \
    --output appimage

echo "AppImage written to $here"
