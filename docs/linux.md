# Linux

The rebuild on Linux: the same program as on the Mac and Windows (SDL3, its
GPU API through Vulkan), built with GCC, packaged as an AppImage. Checked in
WSL2 (Ubuntu 24.04) on Owen's PC (2026-10-31).

## Building

SDL3 is too new for most distributions' packages (Ubuntu 24.04 has none), so
SDL3, SDL3_ttf and SDL3_sound are built from their release sources into a
prefix, at the Mac build's versions:

| Library | Version | Source |
| --- | --- | --- |
| SDL3 | 3.4.12 | github.com/libsdl-org/SDL, release-3.4.12 |
| SDL3_ttf | 3.2.2 | github.com/libsdl-org/SDL_ttf, release-3.2.2 (`-DSDLTTF_VENDORED=ON`: its own FreeType and HarfBuzz, fetched by its `external/download.sh`) |
| SDL3_sound | 3.2.0 | github.com/icculus/SDL_sound, v3.2.0 |

SDL3 needs the usual development headers for its X11, Wayland and audio
back ends (the SDL wiki's README-linux lists them); without libXtst's,
configure SDL with `-DSDL_X11_XTEST=OFF` (only synthetic input uses it).
The shaders' SPIR-V comes from `glslangValidator` (glslang-tools), found by
CMake (`MW3_HAVE_SPIRV`, sdl_render3d.glsl).

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH=$HOME/mw3-linux/prefix -DMW3_GAME_DIR=<an MW3 1.2 install>
cmake --build build
```

`mw3import` (the movies' and the music's importer) needs FFmpeg's shared
libraries (libavformat, libavcodec, libswscale, libavutil and their -dev
packages); without them it is not built and the game is. On Linux the
movies need it: the 32-bit Indeo helper (`mw3indeo`) is Windows-only.

### What GCC needed that clang did not

- `<cstdlib>` and `<cmath>` included where `std::malloc`, `std::fabs` and
  `std::sqrt` are used (libc++ brought them in through other headers).
- A captureless lambda against `nullptr` in `?:` (`on ? [](...){...} :
  nullptr`): GCC does not convert the lambda to the pointer type; a unary `+`
  makes it the function pointer.
- `mw3core` is linked into the differential tests' shared probe, so it is
  built position-independent (`POSITION_INDEPENDENT_CODE`; the Mac's
  default).

## Where things go

- The write dir (settings, pilots, saves, the imported movies and music):
  `SDL_GetPrefPath("MicroProse", "MechWarrior 3")`, which is
  `~/.local/share/MicroProse/MechWarrior 3/` (XDG_DATA_HOME's).
- The install finder (sdl_install.cpp, `install_candidates`) looks, besides
  mw3.cfg's `install=` and the program's own folder, in a Windows install
  under Wine, Proton or Lutris: `$WINEPREFIX` (else `~/.wine`), each Steam
  `compatdata/<id>/pfx` (MW3 is not sold on Steam, so a non-Steam game's
  Proton prefix; also the Flatpak Steam's), and each `~/Games/<name>`
  prefix (Lutris), at `drive_c/Program Files (x86)/MicroProse/MechWarrior 3`
  and the usual hand-picked folders; and `~/Games/MechWarrior 3` and the
  like for a copied install.
- The launcher's font: `assets/launcher/font.ttf`, else DejaVu Sans
  Condensed Bold; the game's own fonts come from the install.
- The MechWarrior (1989) entry looks for `mechwarrior` (no `.exe`) beside
  `mw3` (docs/launcher.md).

## Packaging

`tools/linux/make-appimage.sh [PREFIX] [TOOLS]` builds
`build-linux-pkg/dist/MW3-Rebuild-<version>-x86_64.AppImage`: its own build
tree with `MW3_PACKAGE`, then linuxdeploy (the libraries `mw3` and
`mw3import` load, SDL's from PREFIX) and appimagetool, both from their
GitHub releases (continuous). In the AppDir:

- `usr/bin/mw3`, `usr/bin/mw3import` when built, and
  `usr/bin/assets/launcher/` (the launcher's art, where `SDL_GetBasePath`
  finds it);
- `usr/share/doc/mw3/`: README.txt, THIRD-PARTY.txt, LICENSE.txt, the SDL
  libraries' licence files;
- the desktop entry and icon (tools/linux: `mw3.desktop`, `mw3.png`, our
  emblem at 256).

Vulkan, X11, Wayland and the audio servers stay the system's: SDL loads them
at run time, and linuxdeploy's exclude list keeps them out. `APPIMAGE_EXTRACT_AND_RUN=1`
runs the tools (and the AppImage) without FUSE, as in WSL or a container.

## Checking

- `mw3tests` and `mw3sdltests` run headless under WSL (`SDL_VIDEO_DRIVER=dummy`
  where no window is needed).
- The differential tests (tests/differential, Python with Unicorn) need a
  Python with `unicorn` installed; WSL's system Python has none.
- WSLg gives a Wayland and X display (`WAYLAND_DISPLAY=wayland-0`), so a
  window and a shot can be tried there.

## Open

- FFmpeg for the package's `mw3import`: Ubuntu's FFmpeg is built with GPL
  parts; the Windows package uses an LGPL build (docs/windows.md). Which one
  the AppImage bundles is Owen's call.
