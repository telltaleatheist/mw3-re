#!/bin/bash
# The libraries the launcher's package bundles, built from their official
# release sources (2026-10-30): SDL3, FreeType (static: RmlUi's text, and
# inside SDL3_ttf, which the Linux launcher and the tests use), and libarchive
# with zlib and liblzma (static, for a game's download and its files). Each
# tarball is pinned by its sha256 (checked against the release's own
# signature when the pin was made); a download that does not match is
# refused. The games' own packages build theirs in their repos (with
# SDL3_sound and FFmpeg); on Windows the launcher's come from
# tools/windows/build_deps_ttf.cmd, build_deps_archive.cmd and SDL3's release.
#
#   tools/deps/build_deps.sh mac     universal (arm64, x86_64), macOS 12 and later
#   tools/deps/build_deps.sh linux   this machine's architecture
#
# Out: build-deps/<platform>/prefix (include, lib, lib/cmake, lib/pkgconfig),
# which tools/macos/make_app.sh and the Linux build take as CMAKE_PREFIX_PATH.
set -euo pipefail
cd "$(dirname "$0")/../.."
PLATFORM="${1:?mac or linux}"
case "$PLATFORM" in mac|linux) ;; *) echo "mac or linux, not $PLATFORM" >&2; exit 2 ;; esac
ROOT="$PWD/build-deps"
DL="$ROOT/downloads"
PREFIX="$ROOT/$PLATFORM/prefix"
WORK="$ROOT/$PLATFORM/work"
JOBS="${JOBS:-$( (sysctl -n hw.ncpu 2>/dev/null || nproc) )}"  # JOBS=4 to share the machine

# name | url | sha256
PINS=(
  "SDL3-3.4.12.tar.gz|https://github.com/libsdl-org/SDL/releases/download/release-3.4.12/SDL3-3.4.12.tar.gz|f07b958a9ac5020fb7a44cadb957f658b2149c3c8abb4f63145fac9303249db7"
  "freetype-2.14.3.tar.xz|https://download.savannah.gnu.org/releases/freetype/freetype-2.14.3.tar.xz|36bc4f1cc413335368ee656c42afca65c5a3987e8768cc28cf11ba775e785a5f"
  "SDL3_ttf-3.2.2.tar.gz|https://github.com/libsdl-org/SDL_ttf/releases/download/release-3.2.2/SDL3_ttf-3.2.2.tar.gz|63547d58d0185c833213885b635a2c0548201cc8f301e6587c0be1a67e1e045d"
  "zlib-1.3.1.tar.gz|https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz|9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23"
  "xz-5.8.1.tar.xz|https://github.com/tukaani-project/xz/releases/download/v5.8.1/xz-5.8.1.tar.xz|0b54f79df85912504de0b14aec7971e3f964491af1812d83447005807513cd9e"
  "libarchive-3.7.9.tar.xz|https://www.libarchive.org/downloads/libarchive-3.7.9.tar.xz|ed8b5732e4cd6e30fae909fb945cad8ff9cb7be5c6cdaa3944ec96e4a200c04c"
)

sha256() { if command -v sha256sum >/dev/null; then sha256sum "$1" | cut -d' ' -f1; else shasum -a 256 "$1" | cut -d' ' -f1; fi; }

mkdir -p "$DL"
for p in "${PINS[@]}"; do
  IFS='|' read -r name url sum <<< "$p"
  [ -f "$DL/$name" ] || curl -sSfL -o "$DL/$name" "$url"
  got="$(sha256 "$DL/$name")"
  if [ "$got" != "$sum" ]; then echo "$name: sha256 $got, pinned $sum: refused" >&2; exit 1; fi
done
echo "sources: all ${#PINS[@]} match their pins"

rm -rf "$WORK" "$PREFIX"
mkdir -p "$WORK" "$PREFIX"
unpack() { mkdir -p "$WORK/src" && tar -xf "$DL/$1" -C "$WORK/src"; }

CMAKE_COMMON=(-G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_PREFIX_PATH="$PREFIX"
              -DCMAKE_INSTALL_LIBDIR=lib)
if [ "$PLATFORM" = mac ]; then
  CMAKE_COMMON+=(-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
                 -DCMAKE_INSTALL_NAME_DIR=@rpath)
else
  CMAKE_COMMON+=(-DCMAKE_INSTALL_RPATH='$ORIGIN')
fi
cmake_build() {  # source dir, then its options
  local src="$1"; shift
  cmake -S "$src" -B "$src/_build" "${CMAKE_COMMON[@]}" "$@" >/dev/null
  cmake --build "$src/_build" -j "$JOBS" >/dev/null
  cmake --install "$src/_build" >/dev/null
}

unpack SDL3-3.4.12.tar.gz
# Linux: XTest only feeds synthetic input; without its headers SDL refuses
# to configure X11.
SDL_EXTRA=()
[ "$PLATFORM" = linux ] && SDL_EXTRA+=(-DSDL_X11_XTEST=OFF)
cmake_build "$WORK/src/SDL3-3.4.12" -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF \
  ${SDL_EXTRA[@]+"${SDL_EXTRA[@]}"}
echo "built SDL3"

# FreeType inside SDL3_ttf (static, position-independent), with none of its
# optional libraries: the launcher's TrueType fonts need none of them. 2.14.3,
# as the Mac's Homebrew: TTF text depends on FreeType's version.
unpack freetype-2.14.3.tar.xz
cmake_build "$WORK/src/freetype-2.14.3" -DBUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DFT_DISABLE_ZLIB=ON -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_PNG=ON -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON
echo "built FreeType"

# SDL3_ttf without HarfBuzz (complex-script shaping) or PlutoSVG (SVG fonts):
# the launcher draws Latin text in its own TrueType fonts.
unpack SDL3_ttf-3.2.2.tar.gz
cmake_build "$WORK/src/SDL3_ttf-3.2.2" -DBUILD_SHARED_LIBS=ON -DSDLTTF_VENDORED=OFF -DSDLTTF_HARFBUZZ=OFF \
  -DSDLTTF_PLUTOSVG=OFF -DSDLTTF_SAMPLES=OFF
echo "built SDL3_ttf"

# The archives (a game's Windows zip, and through the setup kit whatever a
# game's files are picked from): libarchive reading zip, 7z, RAR4/5, tar and
# ISO 9660, with zlib (deflate) and liblzma (7z's LZMA/LZMA2, xz); all three
# static and position-independent, linked into the launcher, so the package
# carries no more libraries. No crypto, XML, bzip2, zstd, LZ4 or iconv:
# none of the games' downloads or sources needs them.
unpack zlib-1.3.1.tar.gz
cmake_build "$WORK/src/zlib-1.3.1" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DZLIB_BUILD_EXAMPLES=OFF
rm -f "$PREFIX"/lib/libz*.dylib "$PREFIX"/lib/libz.so*  # the static one only
echo "built zlib"
unpack xz-5.8.1.tar.xz
cmake_build "$WORK/src/xz-5.8.1" -DBUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DXZ_TOOL_XZ=OFF \
  -DXZ_TOOL_XZDEC=OFF -DXZ_TOOL_LZMADEC=OFF -DXZ_TOOL_LZMAINFO=OFF -DXZ_TOOL_SCRIPTS=OFF -DXZ_DOC=OFF -DXZ_NLS=OFF
echo "built liblzma"
unpack libarchive-3.7.9.tar.xz
cmake_build "$WORK/src/libarchive-3.7.9" -DBUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DENABLE_TAR=OFF -DENABLE_CPIO=OFF -DENABLE_CAT=OFF -DENABLE_UNZIP=OFF -DENABLE_TEST=OFF -DENABLE_INSTALL=ON \
  -DENABLE_ZLIB=ON -DENABLE_LZMA=ON -DENABLE_BZip2=OFF -DENABLE_ZSTD=OFF -DENABLE_LZ4=OFF -DENABLE_LIBB2=OFF \
  -DENABLE_OPENSSL=OFF -DENABLE_MBEDTLS=OFF -DENABLE_NETTLE=OFF -DENABLE_CNG=OFF -DENABLE_LIBXML2=OFF \
  -DENABLE_EXPAT=OFF -DENABLE_PCREPOSIX=OFF -DENABLE_PCRE2POSIX=OFF \
  -DENABLE_ICONV=OFF -DENABLE_ACL=OFF -DENABLE_XATTR=OFF -DENABLE_LZO=OFF
rm -f "$PREFIX"/lib/libarchive*.dylib "$PREFIX"/lib/libarchive.so*
echo "built libarchive"
# Their licences, for the packages (linked in, so no library file carries them); FreeType's too, static in the
# launcher (RmlUi's text): its LICENSE.TXT points to the FTL it is used under.
for l in "libarchive-3.7.9 COPYING" "zlib-1.3.1 LICENSE" "xz-5.8.1 COPYING COPYING.0BSD" "freetype-2.14.3 LICENSE.TXT docs/FTL.TXT"; do
  set -- $l; d=$1; shift
  mkdir -p "$PREFIX/share/licenses/$d"
  for f in "$@"; do cp "$WORK/src/$d/$f" "$PREFIX/share/licenses/$d/"; done
done
echo "prefix: $PREFIX"
