#!/bin/bash
# Builds "MechWarrior 3.app" and its DMG for Mac players (docs/macos.md):
# a package build (MW3_PACKAGE: no development copy of the game in it), the
# libraries it loads copied into Contents/Frameworks with their paths
# rewritten to @rpath, the icon, Info.plist, every binary signed ad hoc
# (no identity, so no name in the signature: the releases are not Developer
# ID signed or notarized, docs/macos.md), then the DMG.
#
#   tools/macos/make_app.sh                          run from the repo root
#   tools/macos/make_app.sh --identity "<IDENTITY>"  opt-in, never for releases:
#        signs with that keychain identity and the hardened runtime instead
#   tools/macos/make_app.sh --deps                   the release form: universal
#        (arm64, x86_64), macOS 12 and later, against the libraries
#        tools/deps/build_deps.sh mac built (build-deps/mac/prefix), not Homebrew's
#
# Out: build-mac/dist/MechWarrior 3.app and
#      build-mac/dist/MW3-Rebuild-<git describe>-mac.dmg
#
# For the combined package (tools/package/combined.sh, docs/release.md):
#   MW3_PKG_PM_BIN=<mw3pm built on pm>  adds Pirate's Moon beside mw3 (bundled, signed and checked as mw3)
#   MW3_PKG_MW1_DIR=<mw1's build tree>  adds MechWarrior 1 Remake: mw1 beside the launcher, its mw1data/ in
#                                       Resources (where SDL_GetBasePath points mw1)
#   MW3_PKG_MW1_PORT_DIR=<staged port>  adds Elthial's MechWarrior (1989) SDL port (PLAY ORIGINAL), as
#                                       tools/package/mw1port/build_port.sh stages it: the program and its libraries
#                                       in MacOS/MechWarrior1989, MechWarrior.ini and licences/ in
#                                       Resources/MechWarrior1989 (2026-11-20)
#   MW3_PKG_STEM=<stem>                 the DMG's name: <stem>-<version>-mac.dmg (default MW3-Rebuild; MW3 alone is
#                                       MW3-alone, which the launcher's updater never takes)
#   MW3_PKG_KEEP_BUILD=1                reuse build-mac-pkg (a second package from the same build)
set -euo pipefail
cd "$(dirname "$0")/../.."
ID="-"  # ad hoc
DEPS=""
while [ $# -gt 0 ]; do
  case "$1" in
    --identity) ID="${2:?--identity needs a name}"; shift 2 ;;
    --deps) DEPS="$PWD/build-deps/mac/prefix"; shift ;;
    *) echo "unknown option $1" >&2; exit 2 ;;
  esac
done
if [ -n "$DEPS" ] && [ ! -d "$DEPS/lib" ]; then echo "no $DEPS: run tools/deps/build_deps.sh mac first" >&2; exit 1; fi
if [ -z "$DEPS" ]; then
  echo "note: Homebrew's libraries (this Mac's macOS, arm64 only); THIRD-PARTY.txt describes the --deps build: not for release" >&2
fi
NAME="MechWarrior 3"  # the app as installed (CFBundleName, the display name); the DMG keeps MW3-Rebuild-
BUNDLE_ID="com.owenmorgan.mw3rebuild"
VER="$(git describe --tags --match 'v[0-9]*' --always --dirty)"
OUT=build-mac/dist
APP="$OUT/$NAME.app"

PM_BIN="${MW3_PKG_PM_BIN:-}"
if [ -n "$PM_BIN" ] && [ ! -x "$PM_BIN" ]; then echo "MW3_PKG_PM_BIN: no program at $PM_BIN" >&2; exit 1; fi
MW1_DIR="${MW3_PKG_MW1_DIR:-}"
if [ -n "$MW1_DIR" ] && { [ ! -x "$MW1_DIR/mw1" ] || [ ! -f "$MW1_DIR/mw1data/zbd/mw1image.zbd" ]; }; then
  echo "MW3_PKG_MW1_DIR: no mw1 and mw1data/zbd/mw1image.zbd in $MW1_DIR" >&2; exit 1
fi
PORT_DIR="${MW3_PKG_MW1_PORT_DIR:-}"
if [ -n "$PORT_DIR" ] && { [ ! -x "$PORT_DIR/mechwarrior" ] || [ ! -f "$PORT_DIR/MechWarrior.ini" ] || [ ! -f "$PORT_DIR/licences/SOURCES.txt" ]; }; then
  echo "MW3_PKG_MW1_PORT_DIR: no mechwarrior, MechWarrior.ini and licences/SOURCES.txt in $PORT_DIR" >&2; exit 1
fi

# The build, from scratch, as package.cmd does on Windows.
[ -n "${MW3_PKG_KEEP_BUILD:-}" ] || rm -rf build-mac-pkg
CONF=(-G Ninja -DCMAKE_BUILD_TYPE=Release -DMW3_PACKAGE=ON)
if [ -n "$DEPS" ]; then
  CONF+=(-DCMAKE_PREFIX_PATH="$DEPS" -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0)
  export PKG_CONFIG_LIBDIR="$DEPS/lib/pkgconfig"  # its FFmpeg, not Homebrew's
fi
cmake -S . -B build-mac-pkg "${CONF[@]}" >/dev/null
cmake --build build-mac-pkg --target mw3 mw3launcher >/dev/null
# The movie importer beside mw3 (the first start converts the movies with it,
# first_run_import.h), on the FFmpeg build_deps.sh built; Homebrew's FFmpeg
# would bring dozens of libraries, so without --deps there is none.
if [ -n "$DEPS" ]; then cmake --build build-mac-pkg --target mw3import >/dev/null; fi

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Frameworks" "$APP/Contents/Resources"
# The launcher is the app's own executable; the game beside it, started by it
# and on its own (docs/games-layout.md).
cp build-mac-pkg/mw3launcher "$APP/Contents/MacOS/mw3launcher"
cp build-mac-pkg/mw3 "$APP/Contents/MacOS/mw3"
if [ -n "$DEPS" ]; then cp build-mac-pkg/mw3import "$APP/Contents/MacOS/mw3import"; fi
# The combined package: Pirate's Moon beside it (the launcher finds it there, launcher.md).
if [ -n "$PM_BIN" ]; then cp "$PM_BIN" "$APP/Contents/MacOS/mw3pm"; fi
# MechWarrior 1 Remake beside the launcher (it starts the launcher from its own folder), its data in Resources.
if [ -n "$MW1_DIR" ]; then
  cp "$MW1_DIR/mw1" "$APP/Contents/MacOS/mw1"
  cp -R "$MW1_DIR/mw1data" "$APP/Contents/Resources/mw1data"
  # Its read-me and notices (mw1/dist on its branch, beside the build tree), beside mw3-re's own.
  for f in README-MW1.txt THIRD-PARTY-MW1.txt; do
    if [ -f "$MW1_DIR/../mw1/dist/$f" ]; then cp "$MW1_DIR/../mw1/dist/$f" "$APP/Contents/Resources/$f"
    else echo "warning: no $f in mw1/dist (an mw1 before its notices): not for release" >&2; fi
  done
fi
# The launcher's art (ours, assets/launcher), where SDL's base path is: Resources.
mkdir -p "$APP/Contents/Resources/assets/launcher"
cp assets/launcher/*.png assets/launcher/font.ttf "$APP/Contents/Resources/assets/launcher/"

# Every library outside the system, followed through the ones they load:
# copied by their real file, named by their install name's file, and every
# reference to them rewritten to @rpath/<that name>.
python3 - "$APP" "$DEPS" <<'EOF'
import glob, os, shutil, subprocess, sys
app = sys.argv[1]
prefix = sys.argv[2] if len(sys.argv) > 2 else ''
exes = [os.path.join(app, 'Contents/MacOS', n) for n in ('mw3launcher', 'mw3', 'mw3pm', 'mw1', 'mw3import')]
exes = [e for e in exes if os.path.exists(e)]
fw = os.path.join(app, 'Contents/Frameworks')
def deps(f):
    out = subprocess.run(['otool', '-L', f], capture_output=True, text=True, check=True).stdout.splitlines()
    # A universal file's list has a "<file> (architecture x):" header per arch.
    return sorted({l.strip().split(' (')[0] for l in out if l.startswith(('\t', ' '))})
def foreign(p):
    if prefix and p.startswith('@rpath/'):  # build_deps.sh's libraries are named @rpath/<file>
        return True
    return not (p.startswith('/System/') or p.startswith('/usr/lib/') or p.startswith('@'))
def licences(real, name):
    dst = os.path.join(app, 'Contents/Resources/licenses', name)
    if prefix:
        # From the source tree it was built from (build-deps/mac/work/src/<name>-<version>).
        stem = name.split('.')[0].removeprefix('lib')
        srcs = [d for d in glob.glob(os.path.join(os.path.dirname(prefix), 'work/src/*'))
                if os.path.basename(d).lower().startswith(stem.lower() + '-')]
        if stem.startswith('av') or stem.startswith('sw'):
            srcs = glob.glob(os.path.join(os.path.dirname(prefix), 'work/src/ffmpeg-*'))
        if stem == 'SDL3_ttf':  # FreeType is built into it
            srcs += glob.glob(os.path.join(os.path.dirname(prefix), 'work/src/freetype-*'))
        for d in srcs:
            for n in os.listdir(d):
                if n.upper().startswith(('LICEN', 'COPYING')) and os.path.isfile(os.path.join(d, n)):
                    sub = os.path.join(dst, os.path.basename(d))
                    os.makedirs(sub, exist_ok=True)
                    shutil.copy2(os.path.join(d, n), sub)
            # FreeType's LICENSE.TXT points to the FTL it is used under.
            ftl = os.path.join(d, 'docs', 'FTL.TXT')
            if os.path.isfile(ftl):
                shutil.copy2(ftl, os.path.join(dst, os.path.basename(d)))
        return
    # Its licence files, from its keg (Cellar/<formula>/<version>/).
    keg = os.path.dirname(os.path.dirname(real))
    lic = os.path.join(app, 'Contents/Resources/licenses', os.path.basename(os.path.dirname(keg)))
    os.makedirs(lic, exist_ok=True)
    for n in os.listdir(keg):
        if n.upper().startswith(('LICEN', 'COPYING')):
            shutil.copy2(os.path.join(keg, n), lic)
# Homebrew's own libraries can name each other @rpath/<file> (brotli does):
# found beside the library that loads them, or in Homebrew's lib.
origin = {}
def brew_rpath(f, d):
    if prefix or not d.startswith('@rpath/'):
        return None
    for lib in (origin.get(f, ''), '/opt/homebrew/lib', '/usr/local/lib'):
        c = os.path.join(lib, os.path.basename(d)) if lib else ''
        if c and os.path.exists(c):
            return c
    return None
copied, todo = {}, list(exes)
while todo:
    f = todo.pop()
    for d in deps(f):
        found = brew_rpath(f, d)
        if (foreign(d) or found) and d not in copied:
            name = os.path.basename(d)
            dst = os.path.join(fw, name)
            if not os.path.exists(dst):
                src = found or (os.path.join(prefix, 'lib', name) if d.startswith('@rpath/') else d)
                if not os.path.exists(src):
                    sys.exit(f'{d}: not in {prefix}/lib')
                real = os.path.realpath(src)
                shutil.copy2(real, dst)
                origin[dst] = os.path.dirname(real)
                os.chmod(dst, 0o644)
                licences(real, name)
            copied[d] = name
            todo.append(dst)
for f in exes + [os.path.join(fw, n) for n in set(copied.values())]:
    args = []
    for d in deps(f):
        if d in copied:
            args += ['-change', d, '@rpath/' + copied[d]]
    if f not in exes:
        args += ['-id', '@rpath/' + os.path.basename(f)]
    if args:
        subprocess.run(['install_name_tool'] + args + [f], check=True, stderr=subprocess.DEVNULL)
# Only the bundle's rpath: a build against a prefix keeps its absolute lib path.
for exe in exes:
    lc = subprocess.run(['otool', '-l', exe], capture_output=True, text=True, check=True).stdout.split('\n')
    stale = {lc[i + 2].strip().split(' ')[1] for i, l in enumerate(lc) if 'cmd LC_RPATH' in l}  # each arch lists it
    for path in sorted(p for p in stale):
        subprocess.run(['install_name_tool', '-delete_rpath', path, exe], check=True)
    subprocess.run(['install_name_tool', '-add_rpath', '@executable_path/../Frameworks', exe], check=True)
def outside(p):
    return not (p.startswith('/System/') or p.startswith('/usr/lib/') or p.startswith('@'))
left = [d for f in exes + [os.path.join(fw, n) for n in set(copied.values())] for d in deps(f) if outside(d)]
if left:
    sys.exit('still outside the bundle: ' + ', '.join(sorted(set(left))))
print(f'{len(set(copied.values()))} libraries bundled:', ' '.join(sorted(set(copied.values()))))
EOF

# The icon: make_icon.py's drawing at each iconset size.
ICONSET="build-mac/mw3.iconset"
rm -rf "$ICONSET" && mkdir -p "$ICONSET"
python3 - "$ICONSET" <<'EOF'
import sys, pathlib, importlib.util
spec = importlib.util.spec_from_file_location('make_icon', 'tools/windows/make_icon.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
out = pathlib.Path(sys.argv[1])
for n in (16, 32, 128, 256, 512):
    (out / f'icon_{n}x{n}.png').write_bytes(m.png(n))
    (out / f'icon_{n}x{n}@2x.png').write_bytes(m.png(2 * n))
EOF
iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/mw3.icns"

cat > "$APP/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key><string>$NAME</string>
  <key>CFBundleDisplayName</key><string>$NAME</string>
  <key>CFBundleDevelopmentRegion</key><string>en</string>
  <key>LSHasLocalizedDisplayName</key><true/>
  <key>CFBundleIdentifier</key><string>$BUNDLE_ID</string>
  <key>CFBundleExecutable</key><string>mw3launcher</string>
  <key>CFBundleIconFile</key><string>mw3</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>$VER</string>
  <key>CFBundleVersion</key><string>$VER</string>
  <key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
  <key>LSMinimumSystemVersion</key><string>$(otool -l "$APP/Contents/MacOS/mw3" | awk '/LC_BUILD_VERSION/{b=1} b&&/minos/{print $2; exit}')</string>
  <key>LSApplicationCategoryType</key><string>public.app-category.games</string>
  <key>NSHighResolutionCapable</key><true/>
  <key>LSArchitecturePriority</key><array><string>arm64</string></array>
  <key>NSHumanReadableCopyright</key><string>A fan rebuild; MechWarrior is a trademark of its owners. Needs your own copy of MechWarrior 3.</string>
</dict>
</plist>
EOF

# The name shown (Finder, Dock, Launchpad, the menu bar): BattleTech for the combined package, which holds more than
# MW3 (2026-11-20). The folder and the plist keep "MechWarrior 3" (the updater's path; a localized name shows only
# when the plist's matches the folder's), so Spotlight finds it by both names.
SHOWN="$NAME"
[ -n "$PM_BIN" ] && SHOWN="BattleTech"
mkdir -p "$APP/Contents/Resources/en.lproj"
printf 'CFBundleName = "%s";\nCFBundleDisplayName = "%s";\n' "$SHOWN" "$SHOWN" > "$APP/Contents/Resources/en.lproj/InfoPlist.strings"

# The read-me and the notices beside the program.
cp tools/macos/README.txt "$APP/Contents/Resources/README.txt"
if [ -f LICENSE ]; then cp LICENSE "$APP/Contents/Resources/LICENSE.txt"; fi
mkdir -p "$APP/Contents/Resources/assets/fonts" "$APP/Contents/Resources/licenses/liberation" "$APP/Contents/Resources/licenses/anton"
cp assets/fonts/*.ttf "$APP/Contents/Resources/assets/fonts/"
cp assets/fonts/Liberation-OFL.txt "$APP/Contents/Resources/licenses/liberation/OFL.txt"
cp assets/fonts/Anton-OFL.txt "$APP/Contents/Resources/licenses/anton/OFL.txt"
mkdir -p "$APP/Contents/Resources/licenses/saira-condensed"
cp assets/launcher/font-OFL.txt "$APP/Contents/Resources/licenses/saira-condensed/OFL.txt"
cp tools/macos/THIRD-PARTY.txt "$APP/Contents/Resources/THIRD-PARTY.txt"
# Elthial's port: its code beside the launcher (found as MechWarrior1989/mechwarrior), its data and every licence
# and source line build_port.sh staged in Resources, the launcher pointing it at the ini (MECHWARRIOR_TIMING_CONFIG).
if [ -n "$PORT_DIR" ]; then
  mkdir -p "$APP/Contents/MacOS/MechWarrior1989" "$APP/Contents/Resources/MechWarrior1989"
  cp "$PORT_DIR/mechwarrior" "$PORT_DIR"/*.dylib "$APP/Contents/MacOS/MechWarrior1989/"
  cp "$PORT_DIR/MechWarrior.ini" "$APP/Contents/Resources/MechWarrior1989/"
  cp -R "$PORT_DIR/licences" "$APP/Contents/Resources/MechWarrior1989/licences"
  cp tools/package/mw1port/THIRD-PARTY-MW1989.txt "$APP/Contents/Resources/THIRD-PARTY-MW1989.txt"
fi
# libarchive, zlib and liblzma: linked in with --deps (build_deps.sh keeps
# their licences in the prefix); Homebrew's libarchive is a dylib, its
# licences copied with it above.
if [ -n "$DEPS" ]; then
  [ -d "$DEPS/share/licenses/libarchive-3.7.9" ] || { echo "no licences in $DEPS/share/licenses: run tools/deps/build_deps.sh mac again" >&2; exit 1; }
  cp -R "$DEPS/share/licenses/." "$APP/Contents/Resources/licenses/"
fi

# Signing: the libraries first, then the app. Ad hoc by default (the
# release form: macOS asks once before the first open, README.txt). With
# --identity: the hardened runtime and a secure timestamp too.
# tools/macos/mw3.entitlements grants nothing: the app needs no exception.
if [ "$ID" = "-" ]; then SIGN=(--force --sign -); else SIGN=(--force --timestamp --options runtime --sign "$ID"); fi
for f in "$APP"/Contents/Frameworks/*.dylib; do
  codesign "${SIGN[@]}" "$f"
done
# The game and the importer, second programs in Contents/MacOS, before the app that seals them.
codesign "${SIGN[@]}" --entitlements tools/macos/mw3.entitlements "$APP/Contents/MacOS/mw3"
for p in mw3import mw3pm mw1; do
  if [ -f "$APP/Contents/MacOS/$p" ]; then codesign "${SIGN[@]}" --entitlements tools/macos/mw3.entitlements "$APP/Contents/MacOS/$p"; fi
done
if [ -n "$PORT_DIR" ]; then  # its libraries, then the program
  for f in "$APP"/Contents/MacOS/MechWarrior1989/*.dylib; do codesign "${SIGN[@]}" "$f"; done
  codesign "${SIGN[@]}" --entitlements tools/macos/mw3.entitlements "$APP/Contents/MacOS/MechWarrior1989/mechwarrior"
fi
codesign "${SIGN[@]}" --entitlements tools/macos/mw3.entitlements "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"
# The package carries both programs, the launcher as the app's executable.
PROGRAMS="mw3launcher mw3"
if [ -n "$PM_BIN" ]; then PROGRAMS="$PROGRAMS mw3pm"; fi
if [ -n "$MW1_DIR" ]; then
  PROGRAMS="$PROGRAMS mw1"
  [ -f "$APP/Contents/Resources/mw1data/zbd/mw1image.zbd" ] || { echo "no mw1data in the app" >&2; exit 1; }
fi
if [ -n "$PORT_DIR" ]; then
  PROGRAMS="$PROGRAMS MechWarrior1989/mechwarrior"
  [ -f "$APP/Contents/Resources/MechWarrior1989/licences/MechWarrior-sdl-port-LICENSE.txt" ] || { echo "no port licence in the app" >&2; exit 1; }
fi
for p in $PROGRAMS; do [ -x "$APP/Contents/MacOS/$p" ] || { echo "no $p in the app" >&2; exit 1; }; done
[ "$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$APP/Contents/Info.plist")" = mw3launcher ] || { echo "the app does not start the launcher" >&2; exit 1; }

# The DMG: the app and a link to Applications, compressed, signed.
DMG="$OUT/${MW3_PKG_STEM:-MW3-Rebuild}-$VER-mac.dmg"
STAGE="build-mac/dmg"
rm -rf "$STAGE" "$DMG" && mkdir -p "$STAGE"
cp -R "$APP" "$STAGE/"
ln -s /Applications "$STAGE/Applications"
cp tools/macos/README.txt "$STAGE/Read me.txt"
# No local path or user name in anything shipped (release.md, Privacy).
python3 -I tools/deps/check_paths.py "$STAGE" || exit 1
hdiutil create -volname "$NAME" -srcfolder "$STAGE" -ov -format UDZO "$DMG" >/dev/null
if [ "$ID" != "-" ]; then codesign --force --timestamp --sign "$ID" "$DMG"; fi
echo "App: $APP"
echo "DMG: $DMG"
