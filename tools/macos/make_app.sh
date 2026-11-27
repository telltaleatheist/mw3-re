#!/bin/bash
# Builds the launcher's app, "MechWarrior 3.app" (shown as BattleTech), and its DMG for Mac players (docs/launcher.md,
# Games on demand: the launcher downloads each game, so no game is in it): a package build (MW3_PACKAGE), the
# libraries it loads copied into Contents/Frameworks with their paths rewritten to @rpath, the icon, Info.plist,
# every Mach-O and the app signed, then the DMG, signed, notarized and stapled when the keychain has what that needs
# (docs/launcher.md, Mac signing). The names and the bundle ID are every earlier launcher's, so the updater takes it.
#
#   tools/macos/make_app.sh                          run from the repo root
#   tools/macos/make_app.sh --deps                   the release form: universal
#        (arm64, x86_64), macOS 12 and later, against the libraries
#        tools/deps/build_deps.sh mac built (build-deps/mac/prefix), not Homebrew's
#
# Signing (2026-11-26: releases are Developer ID signed and notarized from 1.7.0) is the setup kit's
# (extern/re-setup-kit/tools/macos/sign_notarize.sh, its header and README), from the environment, so no name or
# team is ever in the repo:
#   RE_SIGN_IDENTITY    the keychain's "Developer ID Application: ..." identity (its name or SHA-1): every Mach-O,
#                       the app and the DMG signed with it, the hardened runtime and a secure timestamp. Unset: ad
#                       hoc, as every release before 1.7.0 (macOS then asks once before the first open, README.txt).
#   RE_NOTARY_PROFILE   a notarytool keychain profile (xcrun notarytool store-credentials): the DMG notarized
#                       (submit --wait), its ticket stapled, Gatekeeper's verdict checked.
#   RE_ALLOW_UNSIGNED=1 a build that will not be notarized (no identity, no profile, or no such profile in the
#                       keychain): refused without it, so an unnotarized DMG never reaches a release. Development
#                       and check builds set it; their README.txt keeps the "Open Anyway" paragraph.
#   The MW3_SIGN_IDENTITY, MW3_NOTARY_PROFILE and MW3_ALLOW_UNSIGNED names are read too.
#   --identity "<IDENTITY>" is RE_SIGN_IDENTITY given on the command line.
#
# Out: build-mac/dist/MechWarrior 3.app and
#      build-mac/dist/MW3-Rebuild-<git describe>-mac.dmg
set -euo pipefail
cd "$(dirname "$0")/../.."
SIGNER=extern/re-setup-kit/tools/macos/sign_notarize.sh
DEPS=""
while [ $# -gt 0 ]; do
  case "$1" in
    --identity) export RE_SIGN_IDENTITY="${2:?--identity needs a name}"; shift 2 ;;
    --deps) DEPS="$PWD/build-deps/mac/prefix"; shift ;;
    *) echo "unknown option $1" >&2; exit 2 ;;
  esac
done
if [ -n "$DEPS" ] && [ ! -d "$DEPS/lib" ]; then echo "no $DEPS: run tools/deps/build_deps.sh mac first" >&2; exit 1; fi
# Notarized, or said why not, before anything is built.
[ -x "$SIGNER" ] || { echo "no $SIGNER: git submodule update --init" >&2; exit 1; }
MODE=$("$SIGNER" check)  # refused here unless it will be notarized or RE_ALLOW_UNSIGNED=1
NOTARIZE=0; [ "$MODE" = notarize ] && NOTARIZE=1
if [ -z "$DEPS" ]; then
  echo "note: Homebrew's libraries (this Mac's macOS, arm64 only); THIRD-PARTY.txt describes the --deps build: not for release" >&2
fi
NAME="MechWarrior 3"  # the app as installed (CFBundleName, the display name); the DMG keeps MW3-Rebuild-
BUNDLE_ID="com.owenmorgan.mw3rebuild"
OUT=build-mac/dist
APP="$OUT/$NAME.app"

# The build, from scratch, as package.cmd does on Windows.
rm -rf build-mac-pkg
CONF=(-G Ninja -DCMAKE_BUILD_TYPE=Release -DMW3_PACKAGE=ON)
if [ -n "$DEPS" ]; then
  CONF+=(-DCMAKE_PREFIX_PATH="$DEPS" -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0)
fi
cmake -S . -B build-mac-pkg "${CONF[@]}" >/dev/null
cmake --build build-mac-pkg --target mw3launcher >/dev/null
VER="$(cat build-mac-pkg/mw3_version.txt)"  # the program's own (CMakeLists.txt: never older than a release)

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Frameworks" "$APP/Contents/Resources"
# The launcher is the app's own executable; each game is its own download (docs/launcher.md, Games on demand).
cp build-mac-pkg/mw3launcher "$APP/Contents/MacOS/mw3launcher"
# The launcher's art (ours, assets/launcher), where SDL's base path is: Resources.
mkdir -p "$APP/Contents/Resources/assets/launcher/ui"
cp assets/launcher/ui/* "$APP/Contents/Resources/assets/launcher/ui/"
cp assets/launcher/*.png assets/launcher/font.ttf assets/launcher/font_body.ttf assets/launcher/click.wav assets/launcher/hover.wav "$APP/Contents/Resources/assets/launcher/"
cp assets/launcher/sources.json "$APP/Contents/Resources/assets/launcher/"  # where it looks (docs/launcher.md)
cp assets/launcher/launcher-settings.json "$APP/Contents/Resources/assets/launcher/"  # its settings' defaults (docs/launcher-config.md)

# Every library outside the system, followed through the ones they load:
# copied by their real file, named by their install name's file, and every
# reference to them rewritten to @rpath/<that name>.
python3 - "$APP" "$DEPS" <<'EOF'
import glob, os, shutil, subprocess, sys
app = sys.argv[1]
prefix = sys.argv[2] if len(sys.argv) > 2 else ''
exes = [os.path.join(app, 'Contents/MacOS', n) for n in ('mw3launcher',)]
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
        # Into licenses/<its source folder> (SDL3-3.4.20), as the Windows package names it.
        if not srcs:
            sys.exit(f'{name}: no source folder in {os.path.dirname(prefix)}/work/src for its licence')
        for d in srcs:
            sub = os.path.join(app, 'Contents/Resources/licenses', os.path.basename(d))
            for n in os.listdir(d):
                if n.upper().startswith(('LICEN', 'COPYING')) and os.path.isfile(os.path.join(d, n)):
                    os.makedirs(sub, exist_ok=True)
                    shutil.copy2(os.path.join(d, n), sub)
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
  <key>LSMinimumSystemVersion</key><string>$(otool -l "$APP/Contents/MacOS/mw3launcher" | awk '/LC_BUILD_VERSION/{b=1} b&&/minos/{print $2; exit}')</string>
  <key>LSApplicationCategoryType</key><string>public.app-category.games</string>
  <key>NSHighResolutionCapable</key><true/>
  <key>LSArchitecturePriority</key><array><string>arm64</string></array>
  <key>NSHumanReadableCopyright</key><string>A fan rebuild; MechWarrior is a trademark of its owners. Needs your own copy of MechWarrior 3.</string>
</dict>
</plist>
EOF

# The name shown (Finder, Dock, Launchpad, the menu bar): BattleTech (2026-11-20). The folder and the plist keep
# "MechWarrior 3" (the updater's path; a localized name shows only when the plist's matches the folder's), so
# Spotlight finds it by both names.
SHOWN="BattleTech"
mkdir -p "$APP/Contents/Resources/en.lproj"
printf 'CFBundleName = "%s";\nCFBundleDisplayName = "%s";\n' "$SHOWN" "$SHOWN" > "$APP/Contents/Resources/en.lproj/InfoPlist.strings"

# The read-me and the notices beside the program: its first-open paragraph the notarized build's or the other's.
mkdir -p build-mac
python3 -I - "$NOTARIZE" tools/macos/README.txt build-mac/README.txt <<'PY'
import sys
want = '[notarized]' if sys.argv[1] == '1' else '[unsigned]'
out, keep = [], True
for line in open(sys.argv[2]).read().splitlines(keepends=True):
    tag = line.strip()
    if tag in ('[notarized]', '[unsigned]', '[end]'):
        keep = tag in (want, '[end]')
        continue
    if keep:
        out.append(line)
open(sys.argv[3], 'w').write(''.join(out))
PY
cp build-mac/README.txt "$APP/Contents/Resources/README.txt"
if [ -f LICENSE ]; then cp LICENSE "$APP/Contents/Resources/LICENSE.txt"; fi
mkdir -p "$APP/Contents/Resources/licenses/saira-condensed"
cp assets/launcher/font-OFL.txt "$APP/Contents/Resources/licenses/saira-condensed/OFL.txt"
mkdir -p "$APP/Contents/Resources/licenses/rmlui"
cp third_party/rmlui/LICENSE.txt "$APP/Contents/Resources/licenses/rmlui/"
cp third_party/rmlui/Include/RmlUi/Core/Containers/LICENSE.txt "$APP/Contents/Resources/licenses/rmlui/LICENSE-containers.txt"
mkdir -p "$APP/Contents/Resources/licenses/miniz"
cp extern/re-setup-kit/third_party/miniz/LICENSE "$APP/Contents/Resources/licenses/miniz/"
mkdir -p "$APP/Contents/Resources/licenses/monocypher"
cp extern/re-setup-kit/third_party/monocypher/LICENCE.md "$APP/Contents/Resources/licenses/monocypher/"
cp tools/macos/THIRD-PARTY.txt "$APP/Contents/Resources/THIRD-PARTY.txt"
# libarchive, zlib, liblzma and FreeType: linked in with --deps (build_deps.sh
# keeps their licences in the prefix); Homebrew's are dylibs, their licences
# copied with them above. Only those: the prefix also has SDL3_ttf's (the Linux
# launcher's alone) and SDL3's, which is beside its dylib already.
if [ -n "$DEPS" ]; then
  for l in libarchive-3.7.9 freetype-2.14.3 xz-5.8.1 zlib-1.3.1; do
    [ -d "$DEPS/share/licenses/$l" ] || { echo "no $l licences in $DEPS/share/licenses: run tools/deps/build_deps.sh mac again" >&2; exit 1; }
    cp -R "$DEPS/share/licenses/$l" "$APP/Contents/Resources/licenses/"
  done
fi

# Signing: every Mach-O inside first (the libraries), then the app (the kit's script). With an identity: the
# hardened runtime and a secure timestamp. tools/macos/mw3.entitlements grants nothing: no JIT, nothing loaded at
# run time, no sandbox.
"$SIGNER" app "$APP" --entitlements tools/macos/mw3.entitlements
# The launcher alone, as the app's executable: no game program in it.
[ -x "$APP/Contents/MacOS/mw3launcher" ] || { echo "no mw3launcher in the app" >&2; exit 1; }
for p in mw3 mw3pm mw1 mw3import; do [ ! -e "$APP/Contents/MacOS/$p" ] || { echo "$p in the launcher's app" >&2; exit 1; }; done
[ "$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$APP/Contents/Info.plist")" = mw3launcher ] || { echo "the app does not start the launcher" >&2; exit 1; }

# The DMG: the app and a link to Applications, compressed, signed.
DMG="$OUT/MW3-Rebuild-$VER-mac.dmg"
STAGE="build-mac/dmg"
rm -rf "$STAGE" "$DMG" && mkdir -p "$STAGE"
cp -R "$APP" "$STAGE/"
ln -s /Applications "$STAGE/Applications"
cp build-mac/README.txt "$STAGE/Read me.txt"
# No local path or user name in anything shipped (release.md, Privacy).
python3 -I tools/deps/check_paths.py "$STAGE" || exit 1
hdiutil create -volname "$NAME" -srcfolder "$STAGE" -ov -format UDZO "$DMG" >/dev/null

# The DMG signed; when notarizing, its ticket stapled to it and to the app here, Gatekeeper's verdict on both, and
# a rejection stops the build with Apple's log (the kit's script).
"$SIGNER" dmg "$DMG" "$APP"
echo "App: $APP"
echo "DMG: $DMG"
