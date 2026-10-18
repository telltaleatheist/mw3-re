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

## Next steps

1. `npx rea-agents setup` (needs Owen's approval; restart Claude Code after).
   Provider: Ghidra + JDK, or Hopper.
2. Validate REA against known ground truth: find code that loads ESP from a
   non-stack value and compare with Owen's notes.
3. First open question: heat dissipation and heat sinks.
4. Rebuild that one mechanic as a small C++ unit with tests.

Analysis output goes in `analysis-output/` (git-ignored), never in the repo.
