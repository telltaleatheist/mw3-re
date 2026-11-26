#!/bin/sh
# The AppImage tools by version and checksum (linuxdeploy, appimagetool and the
# type-2 runtime the AppImages start with, whose notice is
# tools/linux/appimage-runtime). Downloads what is missing or differs.
# Usage: tools/linux/get_appimage_tools.sh DIR   (DIR gets the three files)
set -e
DIR="$1"; [ -n "$DIR" ] || { echo "get_appimage_tools.sh: no folder given" >&2; exit 1; }
mkdir -p "$DIR"
get() {  # name | url | sha256
  if [ -f "$DIR/$1" ] && [ "$(sha256sum "$DIR/$1" | cut -d' ' -f1)" = "$3" ]; then return 0; fi
  curl -sSfL -o "$DIR/$1.part" "$2"
  got=$(sha256sum "$DIR/$1.part" | cut -d' ' -f1)
  if [ "$got" != "$3" ]; then rm -f "$DIR/$1.part"; echo "$1: sha256 $got, pinned $3: refused" >&2; exit 1; fi
  mv -f "$DIR/$1.part" "$DIR/$1"; chmod +x "$DIR/$1"
}
get linuxdeploy-x86_64.AppImage https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d
get appimagetool-x86_64.AppImage https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-x86_64.AppImage ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0
get runtime-x86_64 https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-x86_64 2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d
