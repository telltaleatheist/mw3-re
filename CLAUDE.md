# mw3-re

Learning project: understand MechWarrior 3's executable with REA and rebuild its
mechanics. The goal is learning how REA and binary reverse engineering work, not
a byte-identical port.

## Decisions so far (2026-10-18)

- Target: a rebuild that runs on modern systems including macOS from day 1, no
  Wine. It does exactly what the original did, reusing the original's logic as
  closely as possible, but is not byte-identical.
- Language: C++. Game logic is a plain C++ library with no engine types, built
  from Ghidra's decompiled C: give Ghidra the structs first, then clean each
  function into real C++ and keep its original address in a comment
  (`// Mech3.exe 1.2 @ 0x00412340`). A thin Godot GDExtension layer does
  rendering, audio and input; SDL is the fallback if Godot is dropped.
- Checking: differential tests run the original x86 function (in an emulator
  such as Unicorn, on the Mac) and the C++ version on the same inputs.
- Assets: never committed. A first-run importer reads the player's MW3 1.2
  install/disc and converts locally. mech3ax (TerranMechworks/mech3ax, Rust)
  documents the .zbd formats; port its parsers to C++.
- Music: wired up from the start, songs dropped in later. The original asks MCI
  to play CD track numbers, so the rebuild keeps those numbers: the game asks
  for "track N", and the audio layer plays `music/trackNN.ogg` (or .flac/.wav)
  from the player's folder if it exists, else stays silent. A ripped disc maps
  one to one. Which track plays when comes from the exe's MCI calls.
- Reference runtime: the original runs on Windows 11 (Owen's PC) with
  DDrawCompat. dgVoodoo does not work for MW3.
- UnMech3 (Unity) was tried and found weak (campaign barely works, last release
  Sept 2025, source not found). Not a base to build on.

## Guiding rule (Owen, 2026-10-18)

Stay as close to the original code as possible. Change it only to fix what makes
it (A) unable to run on macOS and modern Windows (Win32, DirectX 6, MCI, x86-only
tricks such as the ESP blit) or (B) break on modern Windows without helpers like
DDrawCompat. Each such fix is noted next to the function it touches. The
scaffold is shaped by the real decompiled code, not designed ahead of it.

## Tools on this Mac

- Ghidra 12.1.4 and JDK 21 from Homebrew:
  `GHIDRA_INSTALL_DIR=/opt/homebrew/opt/ghidra/libexec`,
  `JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home`.
- REA 6.7.0 installed globally under nvm Node 24
  (`~/.nvm/versions/node/v24.22.0/bin/rea`); the default Node is
  older than REA allows. The CLI works without the MCP registration:
  `rea <command> game/mw3-1.2/Mech3.exe --provider ghidra --snapshot
  analysis-output/mech3.snapshot.json --json`.

## Game files (git-ignored, under `game/`)

- `game/mw3-1.2/` is a patched v1.2 install assembled from Owen's backups on
  `/Volumes/iO/backups/win98/` (`MechWarrior 3.zip` = the disc ISO, `MW3 patches
  and cracks.zip` = the official 1.2 patch). Rebuild it from there if lost: the
  disc's `DATA1.CAB` is InstallShield (`unshield`, in Homebrew); every component
  merges into one folder without collisions, then the patch's `Patch Files`
  go on top, plus the disc's `VIDEO/`.
- `Mech3.exe` 1.2 (official, not a NoCD build): PE32 i386, sha256
  `95bc2c114c9b2e5c5ada8e40ca78cc79470f862e45313ffad0d1e8e6d4d916bf`.
- Music is CD audio played through MCI (`cdaudio`, `mciSendCommandA`), and the
  backup ISO is data-only, so this copy has no music.
- Cutscenes are Indeo 5 AVIs (`Ir50_32.dll` ships with the game).
- The install has software-texture sets as well as hardware ones: MW3 has a
  software renderer, the likely home of the blit trick.
- Pirate's Moon (expansion, not being built yet; may be remade later):
  `/Volumes/iO/backups/win98/MW3 pirates moon.zip`.
- REA source for reading: `../rea` (a clone, not a
  dependency).

## Known from Owen's own disassembly

- MW3 uses the stack-pointer blit trick (ESP loaded with a surface pointer,
  push/pop runs). In a rewrite it is replaced by engine drawing, not ported.

## Working with REA

- Ghidra sessions in REA are ephemeral: names set through REA are lost on close.
  `docs/symbols.csv` is the project's map of the binary; findings go in `docs/`.
- Each `rea` CLI call re-imports the exe (~80 s). `tools/analysis/readaemon.py`
  keeps one `rea mcp` session open (queries then take under a second); see
  `tools/analysis/README.md`. Not every CLI command takes `--snapshot`.
- REA cannot decompile an address Ghidra never made a function: it treats the
  address as a file offset and refuses. About 1,000 such functions are reachable
  only through vtables and callback tables; `tools/ghidra/CreateAndDecompile.java`
  creates them in a persistent headless project (`analysis-output/ghidra-project`).
- `analysis-output/decomp/<addr>.c` holds Ghidra pseudocode for all ~10,950
  functions; `func2file.json` gives each one's original source file; `mech3.asm`
  is the full objdump disassembly. Grep these before asking REA.

## Code layout and conventions

- `src/gamez/<module>/` and `src/mech3/` mirror the original source tree
  (`docs/module-map.md`); when the original file name is unknown, say so in the
  header and name it after its neighbours.
- Every ported function starts with `// Mech3.exe 1.2 @ 0x00xxxxxx`. Original
  globals become struct fields with their address in a comment.
- Port the logic as written, original quirks included (see zgameapp.cpp). Only
  (A)/(B) problems change, marked `FIX (A)` / `FIX (B)` and listed in
  `docs/fixes.md`.
- Self-contained functions get a differential test (`tests/differential/`) that
  runs the original x86 in Unicorn against the C++ through `tests/probe.cpp`.
- No GPU use while Owen is streaming; the Godot front end is not started yet.

## Next steps

1. Map the init/shutdown sequences (`calls.py 0x480000`, `0x4808a0`) to name
   each subsystem's entry points in `docs/symbols.csv`.
2. Recover the main data structures (mech, weapon, the zClass node) so Ghidra's
   output reads as fields; apply them in the headless project.
3. Port the zReader `.zbd` reader (cross-check with mech3ax) so the rebuild can
   load data, starting with `reader.zbd` (CD_TRACKS is one entry in it).
4. Heat: find HeatMgmt / heat_* users and port the heat model with differential
   tests.
5. Start the Godot GDExtension front end (window, clock, music output) once the
   GPU is free.
