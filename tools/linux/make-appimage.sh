#!/bin/sh
# Builds the Linux package (docs/linux.md, Packaging): an AppImage with mw3,
# mw3import (the movie importer, on FFmpeg's libraries), the launcher's art, the
# read-me and the notices, and the SDL3, SDL3_ttf and SDL3_sound libraries
# they load. Nothing from the game. Its own build tree, build-linux-pkg,
# configured with MW3_PACKAGE (no development copy of the game in the
# programs). Vulkan, X11, Wayland and the audio servers are the system's
# (loaded by SDL at run time), as an AppImage expects.
# Usage: tools/linux/make-appimage.sh [PREFIX] [TOOLS]
#   PREFIX: where SDL3, SDL3_ttf, SDL3_sound and FFmpeg are installed
#           (tools/deps/build_deps.sh linux's build-deps/linux/prefix;
#           default ../prefix); TOOLS: where linuxdeploy-x86_64.AppImage and
#           appimagetool-x86_64.AppImage are (default ../deps).
# For the combined package (tools/package/combined.sh, docs/release.md):
#   MW3_PKG_PM_BIN=<mw3pm built on pm>  adds Pirate's Moon beside mw3 (its libraries gathered as mw3's)
#   MW3_PKG_MW1_PORT_DIR=<staged port>  adds Elthial's MechWarrior (1989) SDL port (PLAY ORIGINAL) as
#                                       tools/package/mw1port/build_port.sh stages it, in usr/bin/MechWarrior1989 (its
#                                       libraries beside it by $ORIGIN, not through linuxdeploy; 2026-11-20)
#   MW3_PKG_MW1_DIR=<mw1's build tree>  adds MechWarrior 1 Remake: mw1 and its mw1data/ beside the launcher
#   MW3_PKG_STEM=<stem>                 the AppImage's name: <stem>-<version>-x86_64.AppImage (default MW3-Rebuild;
#                                       MW3 alone is MW3-alone, which the launcher's updater never takes)
#   MW3_PKG_KEEP_BUILD=1                reuse build-linux-pkg's build (a second package from it)
set -e
cd "$(dirname "$0")/../.."
PREFIX=$(cd "${1:-../prefix}" && pwd)
TOOLS=$(cd "${2:-../deps}" && pwd)
export APPIMAGE_EXTRACT_AND_RUN=1  # no FUSE needed (WSL, containers)
# FFmpeg's libraries from the prefix too (tools/deps/build_deps.sh linux:
# the minimal LGPL build mw3import needs), found through pkg-config.
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

if [ -n "${MW3_PKG_PM_BIN:-}" ] && [ ! -x "$MW3_PKG_PM_BIN" ]; then echo "MW3_PKG_PM_BIN: no program at $MW3_PKG_PM_BIN" >&2; exit 1; fi
if [ -n "${MW3_PKG_MW1_DIR:-}" ] && { [ ! -x "$MW3_PKG_MW1_DIR/mw1" ] || [ ! -f "$MW3_PKG_MW1_DIR/mw1data/zbd/mw1image.zbd" ]; }; then
  echo "MW3_PKG_MW1_DIR: no mw1 and mw1data/zbd/mw1image.zbd in $MW3_PKG_MW1_DIR" >&2; exit 1
fi
# A package without the movie importer is not one to release (v1.0.5's AppImage went out without it: the prefix
# given had SDL and no FFmpeg, and this script went on). So FFmpeg must be in PREFIX, before anything is built;
# MW3_PKG_NO_IMPORT=1 says a package without movies is meant (a quick check, never a release).
if [ -z "${MW3_PKG_NO_IMPORT:-}" ]; then
  for lib in libavformat libavcodec libswscale libavutil; do
    if [ ! -f "$PREFIX/lib/pkgconfig/$lib.pc" ]; then
      echo "make-appimage.sh: no $lib in $PREFIX (no lib/pkgconfig/$lib.pc): mw3import cannot be built, and the package would have no movie importer." >&2
      echo "  PREFIX is tools/deps/build_deps.sh linux's build-deps/linux/prefix (SDL and FFmpeg); MW3_PKG_NO_IMPORT=1 builds without the importer." >&2
      exit 1
    fi
  done
fi
if [ -z "${MW3_PKG_KEEP_BUILD:-}" ]; then rm -rf build-linux-pkg; else rm -rf build-linux-pkg/AppDir; fi
cmake -S . -B build-linux-pkg -G Ninja -DCMAKE_BUILD_TYPE=Release -DMW3_PACKAGE=ON -DCMAKE_PREFIX_PATH="$PREFIX"
cmake --build build-linux-pkg --target mw3 mw3launcher
if [ -z "${MW3_PKG_NO_IMPORT:-}" ]; then
  cmake --build build-linux-pkg --target mw3import
  [ -x build-linux-pkg/mw3import ] || { echo "make-appimage.sh: mw3import was not built" >&2; exit 1; }
else
  echo "MW3_PKG_NO_IMPORT: the package has no movie importer (not for release)"
fi

VER=$(git describe --tags --match 'v[0-9]*' --always --dirty 2>/dev/null || echo dev)
APPDIR=build-linux-pkg/AppDir
mkdir -p "$APPDIR/usr/bin/assets/launcher" "$APPDIR/usr/share/doc/mw3/licenses"
# The launcher is the AppImage's entry (mw3.desktop's Exec); the game beside it.
cp build-linux-pkg/mw3launcher build-linux-pkg/mw3 "$APPDIR/usr/bin/"
EXES="-e $APPDIR/usr/bin/mw3launcher -e $APPDIR/usr/bin/mw3"
if [ -x build-linux-pkg/mw3import ]; then
  cp build-linux-pkg/mw3import "$APPDIR/usr/bin/"
  EXES="$EXES -e $APPDIR/usr/bin/mw3import"
fi
# The combined package: Pirate's Moon beside it (the launcher finds it there, launcher.md).
if [ -n "${MW3_PKG_PM_BIN:-}" ]; then
  cp "$MW3_PKG_PM_BIN" "$APPDIR/usr/bin/mw3pm"
  EXES="$EXES -e $APPDIR/usr/bin/mw3pm"
fi
# Elthial's port beside the launcher, whole as staged (program, libraries, MechWarrior.ini, licences/).
PORT_DIR="${MW3_PKG_MW1_PORT_DIR:-}"
if [ -n "$PORT_DIR" ]; then
  [ -x "$PORT_DIR/mechwarrior" ] && [ -f "$PORT_DIR/licences/SOURCES.txt" ] || { echo "MW3_PKG_MW1_PORT_DIR: no staged port in $PORT_DIR" >&2; exit 1; }
  cp -R "$PORT_DIR" "$APPDIR/usr/bin/MechWarrior1989"
  cp tools/package/mw1port/THIRD-PARTY-MW1989.txt "$APPDIR/usr/share/doc/mw3/"
fi
# MechWarrior 1 Remake, its data beside it (SDL_GetBasePath()/mw1data).
if [ -n "${MW3_PKG_MW1_DIR:-}" ]; then
  cp "$MW3_PKG_MW1_DIR/mw1" "$APPDIR/usr/bin/mw1"
  cp -R "$MW3_PKG_MW1_DIR/mw1data" "$APPDIR/usr/bin/mw1data"
  # Its read-me and notices (mw1/dist on its branch), with mw3-re's in usr/share/doc/mw3.
  mkdir -p "$APPDIR/usr/share/doc/mw3"
  for f in README-MW1.txt THIRD-PARTY-MW1.txt; do
    if [ -f "$MW3_PKG_MW1_DIR/../mw1/dist/$f" ]; then cp "$MW3_PKG_MW1_DIR/../mw1/dist/$f" "$APPDIR/usr/share/doc/mw3/$f"
    else echo "warning: no $f in mw1/dist (an mw1 before its notices): not for release" >&2; fi
  done
  EXES="$EXES -e $APPDIR/usr/bin/mw1"
fi
# The launcher's art (ours): beside the program, where SDL_GetBasePath finds it.
cp assets/launcher/*.png assets/launcher/font.ttf "$APPDIR/usr/bin/assets/launcher/"
mkdir -p "$APPDIR/usr/bin/assets/fonts" "$APPDIR/usr/share/doc/mw3/licenses/liberation" "$APPDIR/usr/share/doc/mw3/licenses/anton"
cp assets/fonts/*.ttf "$APPDIR/usr/bin/assets/fonts/"
cp assets/fonts/Liberation-OFL.txt "$APPDIR/usr/share/doc/mw3/licenses/liberation/OFL.txt"
cp assets/fonts/Anton-OFL.txt "$APPDIR/usr/share/doc/mw3/licenses/anton/OFL.txt"
mkdir -p "$APPDIR/usr/share/doc/mw3/licenses/saira-condensed"
cp assets/launcher/font-OFL.txt "$APPDIR/usr/share/doc/mw3/licenses/saira-condensed/OFL.txt"
cp tools/linux/README.txt tools/windows/dist/THIRD-PARTY.txt "$APPDIR/usr/share/doc/mw3/"
[ -f LICENSE ] && cp LICENSE "$APPDIR/usr/share/doc/mw3/LICENSE.txt"
# Each library's licence files: from its source tree when the prefix is
# build_deps.sh's (FFmpeg installs none), else what the prefix installed.
if [ -d "$PREFIX/../work/src" ]; then
  for d in "$PREFIX"/../work/src/*/; do
    n=$(basename "$d"); mkdir -p "$APPDIR/usr/share/doc/mw3/licenses/$n"
    for f in "$d"LICENSE* "$d"COPYING* "$d"LICENSE.txt "$d"docs/LICENSE.TXT "$d"docs/FTL.TXT "$d"docs/GPLv2.TXT; do
      [ -f "$f" ] && cp "$f" "$APPDIR/usr/share/doc/mw3/licenses/$n/"
    done
  done
else
  for lib in SDL3 SDL3_ttf SDL3_sound; do
    mkdir -p "$APPDIR/usr/share/doc/mw3/licenses/$lib"
    find "$PREFIX/share" -ipath "*$lib*" \( -iname "LICENSE*" -o -iname "COPYING*" \) -exec cp {} "$APPDIR/usr/share/doc/mw3/licenses/$lib/" \; 2>/dev/null || true
  done
fi

# libarchive, zlib and liblzma are linked in: build_deps.sh keeps their licences in the prefix.
[ -d "$PREFIX/share/licenses" ] && cp -R "$PREFIX/share/licenses/." "$APPDIR/usr/share/doc/mw3/licenses/"
# The combined package is named BattleTech in menus (it holds more than MW3; 2026-11-20), found by its
# games' names too (Keywords).
DESKTOP=tools/linux/mw3.desktop
if [ -n "${MW3_PKG_PM_BIN:-}" ]; then
  mkdir -p build-linux-pkg/desktop && DESKTOP=build-linux-pkg/desktop/mw3.desktop
  sed -e 's/^Name=.*/Name=BattleTech/' -e "s/^Comment=.*/Comment=MechWarrior 3, Pirate's Moon and MechWarrior 1 rebuilt, with your own game files/" tools/linux/mw3.desktop > "$DESKTOP"
fi
# The libraries they load (SDL's from PREFIX), the desktop entry and icon.
LD_LIBRARY_PATH="$PREFIX/lib:$LD_LIBRARY_PATH" "$TOOLS/linuxdeploy-x86_64.AppImage" --appdir "$APPDIR" $EXES \
  -d "$DESKTOP" -i tools/linux/mw3.png
# Both programs, the launcher as the entry (AppRun follows the desktop file's Exec).
PROGRAMS="mw3launcher mw3"
[ -n "${MW3_PKG_PM_BIN:-}" ] && PROGRAMS="$PROGRAMS mw3pm"
if [ -n "${MW3_PKG_MW1_DIR:-}" ]; then
  PROGRAMS="$PROGRAMS mw1"
  [ -f "$APPDIR/usr/bin/mw1data/zbd/mw1image.zbd" ] || { echo "no mw1data in the AppDir" >&2; exit 1; }
fi
[ -z "$PORT_DIR" ] || PROGRAMS="$PROGRAMS MechWarrior1989/mechwarrior"
for p in $PROGRAMS; do [ -x "$APPDIR/usr/bin/$p" ] || { echo "no $p in the AppDir" >&2; exit 1; }; done
grep -qx 'Exec=mw3launcher' "$APPDIR"/*.desktop || { echo "the AppImage does not start the launcher" >&2; exit 1; }
# No local path or user name in anything shipped (release.md, Privacy).
python3 -I tools/deps/check_paths.py "$APPDIR" || exit 1
mkdir -p build-linux-pkg/dist
OUT="build-linux-pkg/dist/${MW3_PKG_STEM:-MW3-Rebuild}-$VER-x86_64.AppImage"
ARCH=x86_64 "$TOOLS/appimagetool-x86_64.AppImage" "$APPDIR" "$OUT"
echo "$OUT"
