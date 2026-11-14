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
libraries (libavformat, libavcodec, libswscale, libavutil); without them it
is not built and the game is. The package's come from build_deps.sh (FFmpeg,
below). On Linux the
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

### What the platform layer needed

- A hidden window's frames paced: with no swapchain image to wait on
  (`--hidden`, every test), nothing made the program wait for the GPU, and
  on WSL's software Vulkan the frames fell behind by hundreds, each holding
  its cycled staging buffers: a mission's run grew 200 MB a second (3 GB in
  14 s; 0.2 GB on the Mac's Metal), and the tests that start several games
  at once ran WSL out of memory (2026-10-31, the VM's 13 GB cap; WSL's
  service then needed a `wsl --shutdown`). `SdlRender3D::present` now keeps
  one such frame in flight (a fence waited on before the next submit): the
  same run peaks at 0.28 GB.
- A window's new size synced (`SDL_SyncWindow` in `SdlWindow::create`): X11
  and Wayland apply it later, and the next frame read back at the old size
  (`sdl_layer_scales_sharp` at 1.5x).

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
- The launcher's font: `assets/launcher/font.ttf` (Saira Condensed Bold,
  in the package), else a narrow system face at its known places, else a
  walk of the font folders fontconfig reads (`font_search`: the user's,
  `XDG_DATA_DIRS`', `/usr/share/fonts`), wanted names first, then any
  upright text face. The log says which (`mw3: launcher: font`). The game's
  own fonts come from the install.
- The MechWarrior (1989) entry looks for `mechwarrior` (no `.exe`) beside
  `mw3` (docs/launcher.md).

## Packaging

`tools/linux/make-appimage.sh [PREFIX] [TOOLS]` builds
`build-linux-pkg/dist/MW3-Rebuild-<version>-x86_64.AppImage`: its own build
tree with `MW3_PACKAGE`, then linuxdeploy (the libraries `mw3` and
`mw3import` load, SDL's from PREFIX) and appimagetool, both from their
GitHub releases (continuous). In the AppDir:

- `usr/bin/mw3launcher` (the entry: mw3.desktop's Exec), `usr/bin/mw3`,
  `usr/bin/mw3import` when built, and
  `usr/bin/assets/launcher/` (the launcher's art, where `SDL_GetBasePath`
  finds it);
- `usr/share/doc/mw3/`: README.txt, THIRD-PARTY.txt, LICENSE.txt, the SDL
  libraries' licence files;
- the desktop entry and icon (tools/linux: `mw3.desktop`, `mw3.png`, our
  emblem at 256). The entry's Name is "MechWarrior 3" (the launcher
  installed under the game's name, Owen 2026-11-01); the AppImage's file
  keeps MW3-Rebuild-... (release.md).

Vulkan, X11, Wayland and the audio servers stay the system's: SDL loads them
at run time, and linuxdeploy's exclude list keeps them out. `APPIMAGE_EXTRACT_AND_RUN=1`
runs the tools (and the AppImage) without FUSE, as in WSL or a container.

**The launcher stays while the game runs, in an AppImage** (FIX (A),
`Launcher::watch_game`; a player's report, reproduced 2026-11-04). Mounted
through FUSE, the image is unmounted when its first process ends. The launcher
used to close at the game's first frame, with `mw3` and its libraries still
paged in from the mount: the game went on from what was already read, then died
of SIGBUS at the first page it had not read, the end of the intro (200 s), with
nothing in the log. It now hides its window and waits (`APPIMAGE` in the
environment), and shows the box if the game ended by a signal. Checks must run
the AppImage mounted, without `APPIMAGE_EXTRACT_AND_RUN`: unpacked, this never
shows.

## Checking

- Under WSL2 (2026-10-31): `mw3tests` 262 tests, 0 failed; `mw3sdltests`
  each test on its own under `run_capped.py --mem-gb 5` (7 for
  `sdl_program_screens_are_not_blank`, which starts ten games at once;
  3 GB peak). Run them one at a time with a cap there: WSL's VM is shared
  and capped, and a run that outgrows it takes the whole distro down.
- WSL stops a distro once no wsl.exe client is attached, so a long job is
  run attached (in the foreground of the session), not left with nohup.
- The differential tests (tests/differential, Python with Unicorn) need a
  Python with `unicorn` installed; WSL's system Python has none.
- WSLg gives a Wayland and X display (`WAYLAND_DISPLAY=wayland-0`), so a
  window and a shot can be tried there.

## FFmpeg (mw3import)

The package's FFmpeg is a minimal LGPL 2.1 build from its pinned release
source (Owen, 2026-10-31): `tools/deps/build_deps.sh linux` (docs/deps.md)
builds it with SDL3, SDL3_ttf (FreeType inside) and SDL3_sound into
`build-deps/linux/prefix`, which `make-appimage.sh` takes as PREFIX
(pkg-config finds FFmpeg there; the licence files come from the sources).
The script stops before building anything when PREFIX has no FFmpeg (its
four `.pc` files): until 2026-11-11 it went on and made a package without
`mw3import`, and v1.0.5's AppImage went out that way, built against a prefix
that held only SDL (found by the 1.1.0 dry run; the published file has no
`usr/bin/mw3import` and no FFmpeg library). `MW3_PKG_NO_IMPORT=1` builds
without the importer on purpose, never for a release.
Only what mw3import uses is in it: the AVI demuxer, the Indeo 5 decoder, the
MPEG-1 video encoder and muxer, swscale, the file protocol. For WSL the
script leaves out SDL's XTest (no libXtst headers) and, where nasm is
missing, FFmpeg's x86 assembly.

Checked under WSL (2026-10-31): the AppImage's `--import --movie INTRO`
converted the 19 movies in 35 s (0.22 GB) and played the intro.

## The movies' conversion on a player's machine (2026-11-14, for 1.1.2)

A player on Arch with 1.1.1: "converting movies kinda gets stuck, but I don't think it really changes
anything". SDL's process pipes are non-blocking (SDL_PROP_PROCESS_STDOUT_POINTER), and the launcher's
reader (`run_importer`, movie_import.cpp) and mw3's first-run reader (first_run_import.cpp) took the
first empty read, SDL_IO_STATUS_NOT_READY, for the end: then they sat in SDL_WaitProcess until
mw3import was done. The bar stayed where it was (often at the start), CANCEL did nothing, and every
movie was still converted. `read_child` now waits on NOT_READY and stops only at the end
(`sdl_importer_progress_is_followed_while_it_pauses`: a stand-in pausing between its lines, every
line heard and a stop at the first movie kills it; 1.1.1's read heard nothing and could not stop it).
The importer itself was fine there: the 1.1.1 AppImage's mw3import converted the 19 movies in 36 s
under WSL (ext4), progress lines and all.

Second, on a case-sensitive disk an install's VIDEO and the converted movies' video can sit in one
folder (the install and the data in one place). `zutl::any_case` takes an exact "video", so with one
there both the launcher's count and mw3import saw no AVIs: 1.1.1's importer converted 0 of 2 in such a
folder. Both now read every folder named video in any case (`video_dirs`; mw3import inline), a movie
in two counted once (`sdl_movies_found_in_every_case_of_the_video_folder`, skipped on a disk that
folds case; run under WSL, and the fixed importer converted both).

## Vulkan

The game draws through SDL_GPU, which on Linux is Vulkan. Without it mw3's window fails ("No
supported SDL_GPU backend found!"); a player fixed it by installing lib32-vulkan-intel, which brought
the 64-bit driver with it. The launcher draws with SDL's renderer and needs no Vulkan, so it checks at
its start (`vulkan_check`, vulkan_check.cpp): libvulkan.so.1 not loading is the loader missing,
SDL_GPUSupportsShaderFormats(SPIRV, "vulkan") false is no usable driver. Either way it shows what to
install (64-bit, not lib32: Arch vulkan-icd-loader with vulkan-intel, vulkan-radeon or nvidia-utils;
Debian or Ubuntu libvulkan1 and mesa-vulkan-drivers; Fedora vulkan-loader and mesa-vulkan-drivers)
with TRY AGAIN and BACK, and PLAY checks again before starting the game. mw3's fatal box adds the same
text. Nothing is bundled (no lavapipe). MW3_VULKAN_CHECK=noloader|nodriver|ok answers for the checks
(`sdl_vulkan_help_names_the_packages`, `sdl_program_launcher_says_what_vulkan_needs_and_holds_play`).
Under WSL the real check found Vulkan, and with VK_ICD_FILENAMES pointing at nothing said "no Vulkan
driver".
