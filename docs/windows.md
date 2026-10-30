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
- Optional, for .ogg, .flac and .mp3 music (without it the music plays
  `.wav` tracks only): SDL3_sound's `SDL3_sound-devel-3.2.0-VC.zip`
  (github.com/icculus/SDL_sound, releases) unpacked into `deps\` as
  `deps\SDL3_sound-3.2.0`; build.cmd hands it to CMake as
  `SDL3_sound_ROOT`, which an existing `build-win\` also picks up on its
  next configure. Its decoders are built in (stb_vorbis, dr_flac, dr_mp3).

## Building

`tools\windows\build.cmd` finds Visual Studio (vswhere), configures
`build-win\` once and builds `mw3` (or the targets given). The build copies
the DLLs mw3.exe links (`SDL3.dll`, `SDL3_ttf.dll`, `SDL3_sound.dll`) next
to it. `mw3soundcheck --music FILE` plays a music file through the game's
music player for 3 s and says whether this build decodes it.

## Running

- The install: `mw3.exe C:\path\to\MechWarrior3`, or an `install=` line in
  `mw3.cfg` in the write dir (`%APPDATA%\MicroProse\MechWarrior 3\`).
- Any version's data runs (assets.md, Data versions); `mw3` prints which at
  start ("data version 1.2", "1.0", "1.0 with 1.2 files" with the counts, or
  the files that are neither). An install with the 1.2 `Mech3.exe` copied
  over older data (seen on one PC) is older data: 1.0's `reader.zbd` lacks
  hud.zrd's 1152x864, 1280x1024 and 1600x1200 blocks (it has 800x600 and
  1024x768), and its `interp.zbd` and campaign 3's missions differ.
- The movies: `mw3import` (needs FFmpeg's libraries) writes them to the
  write dir's `video\` (fmv.md section 6); a set made on another machine
  can be copied there.
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
