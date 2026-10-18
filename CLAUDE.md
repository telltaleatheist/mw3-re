# mw3-re

Learning project: understand MechWarrior 3's executable with REA and rebuild its
mechanics. The goal is learning how REA and binary reverse engineering work, not
a byte-identical port.

## Decisions so far (2026-10-18)

- Target: reimplementation that runs on modern systems including macOS, no Wine.
  Match behaviour and feel, not bytes.
- Language: C++. Game logic in plain C++ with no engine types, so the front end
  can change. Thin Godot (GDExtension) layer for rendering/input first; SDL is
  the fallback if Godot is dropped.
- Assets: never committed. A first-run importer reads the player's MW3 1.2
  install/disc and converts locally. mech3ax (TerranMechworks/mech3ax, Rust)
  documents the .zbd formats; port its parsers to C++. Music may be CD audio
  tracks rather than files: check the disc.
- UnMech3 (Unity) was tried and found weak (campaign barely works, last release
  Sept 2025, source not found). Not a base to build on.

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
