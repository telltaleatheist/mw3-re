# The macOS app

`tools/macos/make_app.sh` makes the release for Mac players:
`build-mac/dist/MechWarrior 3 Rebuild.app` and
`build-mac/dist/MW3-Rebuild-<git describe>-mac.dmg`. Run from the repo root
(about 40 s).

- A package build (`-DMW3_PACKAGE=ON`, Release, its own tree
  `build-mac-pkg/`): no development copy of the game compiled in.
- `Contents/MacOS/mw3`; every library outside the system, followed through
  the ones those load, copied into `Contents/Frameworks` and every
  reference rewritten to `@rpath/<name>`, with the rpath
  `@executable_path/../Frameworks`. The script stops if any reference is
  left outside the bundle. With `/opt/homebrew` unreadable
  (`sandbox-exec`), the app still starts c1m1; the development build does
  not even load.
- `Contents/Resources`: `mw3.icns` (our placeholder, `make_icon.py`'s
  drawing at each iconset size), the README, `LICENSE.txt`,
  `THIRD-PARTY.txt`, and each bundled library's licence files under
  `licenses/<formula>/`.
- `Info.plist`: bundle id `com.owenmorgan.mw3rebuild`, the version from
  git, `LSMinimumSystemVersion` from the binary (see below), games
  category.
- Signed **ad hoc** (`codesign -s -`): the code is sealed, but no identity
  or name is in the signature (Owen, 2026-10-30: the releases are not
  Developer ID signed and not notarized, for his privacy; he publishes as
  Owen Morgan). `tools/macos/mw3.entitlements` grants nothing: the app needs
  no exception. The DMG is not signed.
- An explicit opt-in, `--identity "<IDENTITY>"`, signs with a keychain
  identity, the hardened runtime and a secure timestamp instead; never used
  for releases.

The game's files, settings and saves go to the write dir,
`~/Library/Application Support/MicroProse/MechWarrior 3/`, as for the
development build; with no console, nothing else changes on the Mac (the
terminal shows the output when it is started from one).

## The first open

An app without a Developer ID signature and notarization is blocked by
Gatekeeper on its first open. The player opens it once by right-click (or
Control-click) > Open > Open, or after a blocked attempt, System Settings >
Privacy & Security > "Open Anyway"; macOS remembers it after that. The
README says so.

## Open: which macOS it runs on, and Intel Macs

The bundled libraries are Homebrew's bottles, built for this Mac's macOS
(26) and for arm64 only, so the app needs **macOS 26 or later on Apple
silicon**. To reach earlier macOS versions and Intel Macs, SDL3, SDL3_ttf
and SDL3_sound would be built from their release sources with
`CMAKE_OSX_DEPLOYMENT_TARGET` (e.g. 12.0) and
`CMAKE_OSX_ARCHITECTURES="arm64;x86_64"`, and our own build with the same.
SDL3_ttf's own vendored FreeType and HarfBuzz would then replace Homebrew's
FreeType, HarfBuzz, GLib, libintl, PCRE2, graphite2 and libpng (seven
libraries, two of them LGPL), leaving three. That needs a go to download and
build those sources (not done).

## Open: the movies

The first-run import (`mw3import`) needs FFmpeg's libavformat, libavcodec,
libswscale and libavutil: the Indeo 5 decoder and the MPEG-1 encoder. The
app does not include it yet, so on the Mac the game plays without its
movies unless a player converts them. Two ways:

- **Bundle a minimal FFmpeg** (LGPL 2.1, dynamic, FFmpeg's own code only):
  `--disable-everything` plus the AVI demuxer, the Indeo 5 decoder, the
  MPEG-1 video encoder and muxer, swscale and the file protocol (the
  configure line is in windows.md, Packaging). A few MB, built once per
  FFmpeg release we pick; the LGPL then asks for its notice, the source
  offer and replaceable dylibs, which the bundle already does for SDL. The
  inputs are only the player's own disc movies, so a pinned version needs no
  updates for features, and its exposure is small. It needs a go to build
  FFmpeg's downloaded source.
- **Ask Mac players to `brew install ffmpeg`**: nothing for us to build, but
  every player needs Homebrew, and `mw3import` would then link the
  installed version by its major soname (libavcodec.62): each FFmpeg major
  release in Homebrew would break it until we rebuilt it. That is a
  maintenance burden of its own, and the harder one for players.

The first is the less work over time.
