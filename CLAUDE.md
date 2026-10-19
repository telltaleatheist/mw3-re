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
  (`// Mech3.exe 1.2 @ 0x00412340`). A thin SDL3 layer does
  the window, rendering (SDL's GPU API: Metal, D3D12, Vulkan), audio and input,
  standing in for the DirectX, MCI and Win32 calls one to one, and the
  original's own main loop stays as written (decided with Owen 2026-10-19:
  closest to the original and most portable). Godot (GDExtension) is the
  escape hatch if SDL gives too much trouble; the game library does not change
  either way.
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
- mech3ax source for checking formats: `../mech3ax`
  (a clone, EUPL-1.2). Ports come from the exe; mech3ax is the cross-check.

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
- Comments (Owen, 2026-10-19): short ones where decompiled logic is not obvious
  (a quirk, a magic number, why a branch exists), one or two lines, pointing to
  `docs/<topic>.md` for more. How a system works, the evidence and open
  questions go in `docs/`. No comments that restate the code, no long blocks.
- The target is play-accurate, not byte- or bit-identical (Owen, 2026-10-19).
  Differential tests compare floats within a tolerance; no chasing x87 precision.
  A real bug that breaks play can be fixed, documented in `docs/fixes.md`.
- Port the logic as written, original quirks included (see zgameapp.cpp). Only
  (A)/(B) problems change, marked `FIX (A)` / `FIX (B)` and listed in
  `docs/fixes.md`.
- Self-contained functions get a differential test (`tests/differential/`) that
  runs the original x86 in Unicorn against the C++ through `tests/probe.cpp`.
- No GPU use while Owen is streaming; the SDL front end is not started yet.
- Every quirk, bug or hidden mechanic found goes in `docs/discoveries.md`, in
  plain words for players (Owen wants to share these with the MW3 community).

## Next steps

Order agreed with Owen on 2026-10-18: zReader first, because it stands alone,
can be checked two ways and unblocks loading data.

1. zReader: done for the archive and binary data (`docs/zreader.md`),
   emulator-checked on all of `reader.zbd`; `zsnd_cd::read_cues` reads
   CD_TRACKS from it. Still open: the loose-text tokenizer and the archive
   writer (neither is used by the shipped game), and the other `.zbd` kinds
   (motion, sounds, textures, mechlib), which have their own formats.
2. Data structures: the mech hierarchy, its parts, the mech definition and the
   scene node are in `docs/types.json` and applied to the headless project
   (`docs/structs.md`). Still open: weapons and ammo, the pilot/HUD object
   (+0x684), and naming more of the mech's ~170 placeholder fields.
3. Heat: ported (`src/mech3/mech.cpp`, `docs/heat.md`), emulator-checked.
   Open: the lava-per-frame decision, mission mode 3, the controls value.
4. Init/shutdown: mapped (`docs/startup.md`), with the game states and the
   tamper check. Open: the 0x200 pool, the options setters at 0x40cb30.
5. Start the SDL3 front end (window, clock, music output) once the GPU is
   free. Present with vsync, as the original did (`docs/heat.md`, Frame rate).

## Work queue (small pieces, 2026-10-19 night)

Each item is one sitting; findings go in `docs/`, quirks also in
`docs/discoveries.md`.

- [x] Textures: traced and ported (`docs/textures.md`), identical on all
      10,612 textures.
- [x] Textures: `tools/analysis/tex2png.py` exports any library as PNG.
- [x] Game types: mapped (`docs/gametypes.md`); 3 is multiplayer.
- [x] Network: message types mapped (`docs/network-messages.md`); state sync.
- [x] Damage: mapped (`docs/damage.md`). Open: the front/rear edge order
      (check in the emulator), torso ApplyImpact 0x464af0, the `xl` flag.
- [x] Weapons at run time: mapped (`docs/weapons.md`). Open: damage, falloff,
      missiles in the engine's weapon code 0x581xxx-0x585xxx.
- [x] Controls: mapped (`docs/controls.md`); +0x1c is GetThrottle.
- [x] Power: ported and emulator-checked (`docs/heat.md`).
- [ ] Mech definition (0x46ba40): too big for one sitting; it calls ~30
      functions. Pieces, each with an emulator check over every `dfn_*.zrd`:
      skins (ported, `mech3/mech_skin`, `docs/mechdef-skins.md`), firepoints and
      parts (ported, `mech3/mech_reader`, `docs/mechdef-parts.md`), configurations and loadout (mapped, `docs/mechdef-configs.md`; 0x424b70, 0x484e40, 0x49ced0, 0x49da10;
      `docs/weapons.md` maps the loadout), then the reader itself.
- [x] Motion: ported and emulator-checked (`docs/motion.md`).
- [x] Mechlib: mapped (`docs/mechlib.md`), with a porting order.
- [x] Mechlib: materials and models ported (`src/gamez/zclass/cls_mechlib`),
      identical on all 442 materials and 770 models.
- [x] Mechlib: the node tree ported (`cls_mechlib` read_tree, `object3d`,
      `cls_node`), identical on all 57 trees. Debt: mechlib models are their
      own `zcls::Model` (Node::lib_model), apart from the world's `zmod::Model`;
      merge the two model ports. Open: zClass removal and destroy (step 7).
- [x] Mission worlds: mapped (`docs/gamez.md`), walk scripts in
      `tests/differential/explore/gamez_*.py`.
- [x] Mission worlds: `gamez.zbd` ported end to end (`zclass/cls_zbd`
      0x4ff520 and the section readers), identical on the six worlds and on
      broken files. Left for later: the re-read path (0x4ff730, 0x4ff7a0,
      0x538640), `docs/gamez.md` porting order step 7.
- [x] Mission scripts: mapped (`docs/interp.md`), decoder in
      `tests/differential/explore/interp_*.py`.
- [ ] Mission animations: the reader 0x50a0e0 ported and emulator-checked
      (`zeffect/zeff_anim_init`), its scene calls hooked. Next the lookups
      0x509640 and friends on the ported node pool, then the creators
      (`docs/anim.md`, porting order steps 2-3).
- [ ] Mission scripts: port the archive, line reader, macros, preprocessor and
      dispatcher table (order in `docs/interp.md`), checked by a command trace.
- [ ] Assets: interp scripts, gamez.zbd and anim.zbd traced (`interp.md`,
      `gamez.md`, `anim.md`); trace what remains in `docs/assets.md`, one per sitting,
      cross-checked against mech3ax.
