#!/bin/sh
# Builds the launcher's Linux package: an AppImage with mw3launcher, its art, the read-me and the notices, and the
# SDL3 and SDL3_ttf libraries it loads. No game is in it: the launcher downloads each game (docs/launcher.md, Games
# on demand). Its own build tree, build-linux-pkg, configured with MW3_PACKAGE. Vulkan, X11, Wayland and the audio
# servers are the system's (loaded by SDL at run time), as an AppImage expects. The names are every earlier
# launcher's, so the updater takes it.
# Usage: tools/linux/make-appimage.sh [PREFIX] [TOOLS]
#   PREFIX: where SDL3 and SDL3_ttf are installed (tools/deps/build_deps.sh linux's build-deps/linux/prefix;
#           default ../prefix); TOOLS: the folder whose appimage-tools/ holds the AppImage
#           tools, pinned and fetched by get_appimage_tools.sh (default ../deps).
set -e
cd "$(dirname "$0")/../.."
PREFIX=$(cd "${1:-../prefix}" && pwd)
TOOLS="${2:-../deps}"; mkdir -p "$TOOLS"; TOOLS=$(cd "$TOOLS" && pwd)
TOOLS="$TOOLS/appimage-tools"  # pinned by checksum; downloaded here when missing
sh tools/linux/get_appimage_tools.sh "$TOOLS"
export APPIMAGE_EXTRACT_AND_RUN=1  # no FUSE needed (WSL, containers)

rm -rf build-linux-pkg
cmake -S . -B build-linux-pkg -G Ninja -DCMAKE_BUILD_TYPE=Release -DMW3_PACKAGE=ON -DCMAKE_PREFIX_PATH="$PREFIX"
cmake --build build-linux-pkg --target mw3launcher

VER=$(cat build-linux-pkg/mw3_version.txt)  # the program's own (CMakeLists.txt: never older than a release)
APPDIR=build-linux-pkg/AppDir
mkdir -p "$APPDIR/usr/bin/assets/launcher/ui" "$APPDIR/usr/share/doc/mw3/licenses"
# The launcher is the AppImage's entry (mw3.desktop's Exec).
cp build-linux-pkg/mw3launcher "$APPDIR/usr/bin/"
EXES="-e $APPDIR/usr/bin/mw3launcher"
# The launcher's art (ours): beside the program, where SDL_GetBasePath finds it.
cp assets/launcher/ui/* "$APPDIR/usr/bin/assets/launcher/ui/"
cp assets/launcher/*.png assets/launcher/font.ttf assets/launcher/font_body.ttf assets/launcher/click.wav assets/launcher/hover.wav "$APPDIR/usr/bin/assets/launcher/"
cp assets/launcher/sources.json "$APPDIR/usr/bin/assets/launcher/"  # where it looks (docs/launcher.md)
mkdir -p "$APPDIR/usr/share/doc/mw3/licenses/saira-condensed"
cp assets/launcher/font-OFL.txt "$APPDIR/usr/share/doc/mw3/licenses/saira-condensed/OFL.txt"
mkdir -p "$APPDIR/usr/share/doc/mw3/licenses/rmlui"
cp third_party/rmlui/LICENSE.txt "$APPDIR/usr/share/doc/mw3/licenses/rmlui/"
cp third_party/rmlui/Include/RmlUi/Core/Containers/LICENSE.txt "$APPDIR/usr/share/doc/mw3/licenses/rmlui/LICENSE-containers.txt"
mkdir -p "$APPDIR/usr/share/doc/mw3/licenses/miniz"
cp extern/re-setup-kit/third_party/miniz/LICENSE "$APPDIR/usr/share/doc/mw3/licenses/miniz/"
mkdir -p "$APPDIR/usr/share/doc/mw3/licenses/monocypher"
cp extern/re-setup-kit/third_party/monocypher/LICENCE.md "$APPDIR/usr/share/doc/mw3/licenses/monocypher/"
cp tools/linux/README.txt tools/linux/THIRD-PARTY.txt "$APPDIR/usr/share/doc/mw3/"
[ -f LICENSE ] && cp LICENSE "$APPDIR/usr/share/doc/mw3/LICENSE.txt"
# Each library's licence files: from its source tree when the prefix is
# build_deps.sh's, else what the prefix installed.
if [ -d "$PREFIX/../work/src" ]; then
  for d in "$PREFIX"/../work/src/*/; do
    n=$(basename "$d"); mkdir -p "$APPDIR/usr/share/doc/mw3/licenses/$n"
    for f in "$d"LICENSE* "$d"COPYING* "$d"LICENSE.txt "$d"docs/LICENSE.TXT "$d"docs/FTL.TXT "$d"docs/GPLv2.TXT; do
      [ -f "$f" ] && cp "$f" "$APPDIR/usr/share/doc/mw3/licenses/$n/"
    done
  done
else
  for lib in SDL3 SDL3_ttf; do
    mkdir -p "$APPDIR/usr/share/doc/mw3/licenses/$lib"
    find "$PREFIX/share" -ipath "*$lib*" \( -iname "LICENSE*" -o -iname "COPYING*" \) -exec cp {} "$APPDIR/usr/share/doc/mw3/licenses/$lib/" \; 2>/dev/null || true
  done
fi

# The AppImage's own start program and the libfuse in it (LGPL): its notice and licences.
cp -R tools/linux/appimage-runtime "$APPDIR/usr/share/doc/mw3/licenses/appimage-runtime"
# libarchive, zlib and liblzma are linked in: build_deps.sh keeps their licences in the prefix.
[ -d "$PREFIX/share/licenses" ] && cp -R "$PREFIX/share/licenses/." "$APPDIR/usr/share/doc/mw3/licenses/"
# Named BattleTech in menus (2026-11-20), found by its games' names too (Keywords).
mkdir -p build-linux-pkg/desktop && DESKTOP=build-linux-pkg/desktop/mw3.desktop
sed -e 's/^Name=.*/Name=BattleTech/' -e "s/^Comment=.*/Comment=MechWarrior 3, Pirate's Moon and MechWarrior 1 rebuilt, with your own game files/" tools/linux/mw3.desktop > "$DESKTOP"
# The libraries they load (SDL's from PREFIX), the desktop entry and icon.
LD_LIBRARY_PATH="$PREFIX/lib:$LD_LIBRARY_PATH" "$TOOLS/linuxdeploy-x86_64.AppImage" --appdir "$APPDIR" $EXES \
  -d "$DESKTOP" -i tools/linux/mw3.png
# The launcher alone, as the entry (AppRun follows the desktop file's Exec): no game program in it.
[ -x "$APPDIR/usr/bin/mw3launcher" ] || { echo "no mw3launcher in the AppDir" >&2; exit 1; }
for p in mw3 mw3pm mw1 mw3import; do [ ! -e "$APPDIR/usr/bin/$p" ] || { echo "$p in the launcher's AppImage" >&2; exit 1; }; done
grep -qx 'Exec=mw3launcher' "$APPDIR"/*.desktop || { echo "the AppImage does not start the launcher" >&2; exit 1; }
# No local path or user name in anything shipped (release.md, Privacy).
python3 -I tools/deps/check_paths.py "$APPDIR" || exit 1
mkdir -p build-linux-pkg/dist
OUT="build-linux-pkg/dist/MW3-Rebuild-$VER-x86_64.AppImage"
ARCH=x86_64 "$TOOLS/appimagetool-x86_64.AppImage" --runtime-file "$TOOLS/runtime-x86_64" "$APPDIR" "$OUT"
echo "$OUT"
