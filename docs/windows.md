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

## Building

`tools\windows\build.cmd` finds Visual Studio (vswhere), configures
`build-win\` once and builds `mw3` (or the targets given). Copy `SDL3.dll`
and `SDL3_ttf.dll` from the zips' `lib\x64` next to `mw3.exe`.

## Running

- The install: `mw3.exe C:\path\to\MechWarrior3`, or an `install=` line in
  `mw3.cfg` in the write dir (`%APPDATA%\MicroProse\MechWarrior 3\`).
- The movies: `mw3import` (needs FFmpeg's libraries) writes them to the
  write dir's `video\` (fmv.md section 6); a set made on another machine
  can be copied there.
- Fullscreen by default (borderless, the desktop's size, the frame fitted);
  `--windowed`, `--window WxH` or `windowed=1` in `mw3.cfg` keep a window.
- It must run in a desktop session: over ssh there is no swapchain
  (`SDL_ClaimWindowForGPUDevice` fails with 0x887A0022).

## The renderer

`sdl_render3d.hlsl` is the Direct3D 12 twin of `sdl_render3d.cpp`'s Metal
shaders; CMake runs `dxc` on it for each entry point and the renderer takes
DXIL when the device offers it. The viewer (`tools/viewer`) is Metal only.

## Not yet built on Windows

The tests (`mw3tests`, `mw3sdltests`; several use POSIX process calls,
guarded), `mw3import` (FFmpeg), the viewer's shaders.
