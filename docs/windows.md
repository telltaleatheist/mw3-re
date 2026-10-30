# Building and running on Windows

The same code as on the Mac, built with clang-cl (the Mac's compiler family,
so the floats round alike: `-ffp-contract=off` under both) and Ninja against
Visual Studio's headers and libraries.

## What it needs

- Visual Studio (2022 or later) with the C++ tools; its Windows SDK brings
  `dxc`, which compiles the Direct3D 12 shaders.
- LLVM's clang-cl on the PATH (`scoop install llvm`, or Visual Studio's
  "C++ Clang tools" component).
- SDL3 and SDL3_ttf: their `-VC.zip` development releases unpacked into
  `deps\` (git-ignored), e.g. `deps\SDL3-3.4.12`, `deps\SDL3_ttf-3.2.2`.
- For the movies' importer only: FFmpeg's shared libraries, BtbN's LGPL
  build `ffmpeg-n7.1-latest-win64-lgpl-shared-7.1.zip` (github.com/BtbN/
  FFmpeg-Builds, releases, "latest") unpacked into `deps\` as
  `deps\ffmpeg-n7.1-latest-win64-lgpl-shared-7.1` (its `include\`,
  `lib\*.lib` and `bin\*.dll`). Any shared build with those three folders
  does (CMake takes the first `deps\ffmpeg-*shared*`, or `-DFFMPEG_DIR`).
  It has FFmpeg's own Indeo 5 decoder and MPEG-1 encoder, which is all the
  importer uses. Without it `mw3import` is not built; the game is.

## Building

`tools\windows\build.cmd` finds Visual Studio (vswhere), configures
`build-win\` once and builds `mw3` (or the targets given). Copy `SDL3.dll`
and `SDL3_ttf.dll` from the zips' `lib\x64` next to `mw3.exe`.
`tools\windows\build.cmd mw3 mw3import` also builds the importer, and its
build copies FFmpeg's DLLs next to `mw3import.exe`; give a player the two
exes and all the DLLs in one folder.

## Running

- The install: `mw3.exe C:\path\to\MechWarrior3`, or an `install=` line in
  `mw3.cfg` in the write dir (`%APPDATA%\MicroProse\MechWarrior 3\`).
- Any version's data runs (assets.md, Data versions); `mw3` prints which at
  start ("data version 1.2", "1.0", "1.0 with 1.2 files" with the counts, or
  the files that are neither). An install with the 1.2 `Mech3.exe` copied
  over older data (seen on one PC) is older data: 1.0's `reader.zbd` lacks
  hud.zrd's 1152x864, 1280x1024 and 1600x1200 blocks (it has 800x600 and
  1024x768), and its `interp.zbd` and campaign 3's missions differ.
- The movies: on a player's first plain start, when the write dir's
  `video\` lacks the install's movies and `mw3import.exe` is beside
  `mw3.exe`, mw3 runs it for the missing ones with a small progress window
  (about a minute for the 19; fmv.md section 6). Without the importer the
  game plays without movies, as before; `--import` runs it again after a
  failed try, `--no-import` skips it. A set made on another machine can be
  copied into `video\` instead.
- The music: `mw3import --cue "D:\path\disc.cue" --music-only` writes a
  disc image's audio tracks as `music\trackNN.wav` in the write dir
  (fmv.md section 6, Music).
- Fullscreen by default (borderless, the desktop's size, the frame fitted);
  `--windowed`, `--window WxH` or `windowed=1` in `mw3.cfg` keep a window.
- It must run in a desktop session: over ssh there is no swapchain
  (`SDL_ClaimWindowForGPUDevice` fails with 0x887A0022).

## A crash

Windows' event log (Application, event 1000) gives the exception code and
the fault offset in mw3.exe. The build keeps symbols (mw3.pdb beside the
exe), so `llvm-symbolizer --obj=build-win\mw3.exe --inlining
--relative-address 0x<offset>` names the function and line.

## The renderer

`sdl_render3d.hlsl` is the Direct3D 12 twin of `sdl_render3d.cpp`'s Metal
shaders; CMake runs `dxc` on it for each entry point and the renderer takes
DXIL when the device offers it. The viewer (`tools/viewer`) is Metal only.

## Not yet built on Windows

The tests (`mw3tests`, `mw3sdltests`; several use POSIX process calls,
guarded), `mw3import` (FFmpeg), the viewer's shaders.
