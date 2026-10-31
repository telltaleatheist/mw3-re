#!/bin/bash
# Builds "MechWarrior 3 Rebuild.app" and its DMG for Mac players (docs/macos.md):
# a package build (MW3_PACKAGE: no development copy of the game in it), the
# libraries it loads copied into Contents/Frameworks with their paths
# rewritten to @rpath, the icon, Info.plist, every binary signed ad hoc
# (no identity, so no name in the signature: the releases are not Developer
# ID signed or notarized, docs/macos.md), then the DMG.
#
#   tools/macos/make_app.sh                          run from the repo root
#   tools/macos/make_app.sh --identity "<IDENTITY>"  opt-in, never for releases:
#        signs with that keychain identity and the hardened runtime instead
#
# Out: build-mac/dist/MechWarrior 3 Rebuild.app and
#      build-mac/dist/MW3-Rebuild-<git describe>-mac.dmg
set -euo pipefail
cd "$(dirname "$0")/../.."
ID="-"  # ad hoc
if [ "${1:-}" = "--identity" ] && [ -n "${2:-}" ]; then ID="$2"; fi
NAME="MechWarrior 3 Rebuild"
BUNDLE_ID="com.owenmorgan.mw3rebuild"
VER="$(git describe --tags --always --dirty)"
OUT=build-mac/dist
APP="$OUT/$NAME.app"

# The build, from scratch, as package.cmd does on Windows.
rm -rf build-mac-pkg
cmake -S . -B build-mac-pkg -G Ninja -DCMAKE_BUILD_TYPE=Release -DMW3_PACKAGE=ON >/dev/null
cmake --build build-mac-pkg --target mw3 >/dev/null

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Frameworks" "$APP/Contents/Resources"
cp build-mac-pkg/mw3 "$APP/Contents/MacOS/mw3"

# Every library outside the system, followed through the ones they load:
# copied by their real file, named by their install name's file, and every
# reference to them rewritten to @rpath/<that name>.
python3 - "$APP" <<'EOF'
import os, shutil, subprocess, sys
app = sys.argv[1]
exe = os.path.join(app, 'Contents/MacOS/mw3')
fw = os.path.join(app, 'Contents/Frameworks')
def deps(f):
    out = subprocess.run(['otool', '-L', f], capture_output=True, text=True, check=True).stdout.splitlines()[1:]
    return [l.strip().split(' (')[0] for l in out]
def foreign(p):
    return not (p.startswith('/System/') or p.startswith('/usr/lib/') or p.startswith('@'))
copied, todo = {}, [exe]
while todo:
    f = todo.pop()
    for d in deps(f):
        if foreign(d) and d not in copied:
            name = os.path.basename(d)
            dst = os.path.join(fw, name)
            if not os.path.exists(dst):
                real = os.path.realpath(d)
                shutil.copy2(real, dst)
                os.chmod(dst, 0o644)
                # Its licence files, from its keg (Cellar/<formula>/<version>/).
                keg = os.path.dirname(os.path.dirname(real))
                lic = os.path.join(app, 'Contents/Resources/licenses', os.path.basename(os.path.dirname(keg)))
                os.makedirs(lic, exist_ok=True)
                for n in os.listdir(keg):
                    if n.upper().startswith(('LICEN', 'COPYING')):
                        shutil.copy2(os.path.join(keg, n), lic)
            copied[d] = name
            todo.append(dst)
for f in [exe] + [os.path.join(fw, n) for n in set(copied.values())]:
    args = []
    for d in deps(f):
        if d in copied:
            args += ['-change', d, '@rpath/' + copied[d]]
    if f != exe:
        args += ['-id', '@rpath/' + os.path.basename(f)]
    if args:
        subprocess.run(['install_name_tool'] + args + [f], check=True, stderr=subprocess.DEVNULL)
subprocess.run(['install_name_tool', '-add_rpath', '@executable_path/../Frameworks', exe], check=True)
left = [d for f in [exe] + [os.path.join(fw, n) for n in set(copied.values())] for d in deps(f) if foreign(d)]
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
  <key>CFBundleIdentifier</key><string>$BUNDLE_ID</string>
  <key>CFBundleExecutable</key><string>mw3</string>
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

# The read-me and the notices beside the program.
cp tools/macos/README.txt "$APP/Contents/Resources/README.txt"
if [ -f LICENSE ]; then cp LICENSE "$APP/Contents/Resources/LICENSE.txt"; fi
cp tools/macos/THIRD-PARTY.txt "$APP/Contents/Resources/THIRD-PARTY.txt"

# Signing: the libraries first, then the app. Ad hoc by default (the
# release form: macOS asks once before the first open, README.txt). With
# --identity: the hardened runtime and a secure timestamp too.
# tools/macos/mw3.entitlements grants nothing: the app needs no exception.
if [ "$ID" = "-" ]; then SIGN=(--force --sign -); else SIGN=(--force --timestamp --options runtime --sign "$ID"); fi
for f in "$APP"/Contents/Frameworks/*.dylib; do
  codesign "${SIGN[@]}" "$f"
done
codesign "${SIGN[@]}" --entitlements tools/macos/mw3.entitlements "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"

# The DMG: the app and a link to Applications, compressed, signed.
DMG="$OUT/MW3-Rebuild-$VER-mac.dmg"
STAGE="build-mac/dmg"
rm -rf "$STAGE" "$DMG" && mkdir -p "$STAGE"
cp -R "$APP" "$STAGE/"
ln -s /Applications "$STAGE/Applications"
cp tools/macos/README.txt "$STAGE/Read me.txt"
hdiutil create -volname "$NAME" -srcfolder "$STAGE" -ov -format UDZO "$DMG" >/dev/null
if [ "$ID" != "-" ]; then codesign --force --timestamp --sign "$ID" "$DMG"; fi
echo "App: $APP"
echo "DMG: $DMG"
