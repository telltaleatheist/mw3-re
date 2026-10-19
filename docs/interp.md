# zInterp and `interp.zbd` (Mech3.exe 1.2 (researched by a subagent, spot-checked: the header, `Object3DSetScrllAlways` once in the scripts and never in the exe, `LoadGameGen`/`GameGenSetWorld`/`SetModelDirectory` absent from the exe))

Mech3.exe 1.2, 2026-10-19. Research draft, nothing ported. Source files:
`gamezmech3\zinterp\zinterp_parse.cpp` (the big dispatcher 0x591300, named by
the assert path at 0x5f3ce8) and its unnamed neighbours 0x594dc0..0x596c00
(reader, macros, `source`, console); MW3's subclass sits in
`mech3\` between `briefing.cpp` and `mech.cpp` (0x410700..0x4108f0).

**What the file actually is.** `interp.zbd` does not hold mission logic (that
is in the `reader*.zbd` files). It holds the GameZ *world-build* scripts: one
`.gs` per level (`c1 c2 c3 c4 c4b t1`, plus unused `dev1 dev2 lab`) and
35 `Support\*.gw` include files. Each script is a list of commands such as
`FindNode`, `CycleTextureSetMap`, `LightSetColor`. They run once, while a mission
loads. The format matches mech3ax `crates/interp`. A scratch walk that follows
the exe's reads decodes all 44 scripts (2,504 lines) and ends exactly at the
end of the file (99,662 bytes).

## 1. Opening and finding a script

- **Interpreter object.** MW3 creates it at 0x47f9ba (in 0x47f7a0, the
  mission/app setup): `new(0xc0)`, ctor **0x410700**, pointer stored at
  **0x6040b0**. 0x410700 calls the base ctor **0x5952f0(archive =
  [0x5f4248] -> "interp.zbd" (0x5f424c), search path ".;zbd" (0x5e16c8))**,
  sets vtable 0x5bccc8 and +0xbc = 0, and registers the variable `DamageScale`
  (float at 0x5e1f68) through 0x595fa0. Base vtable 0x5cf670.
  (The u32 at 0x5f4248 is the pointer 0x005f424c, which is why string dumps
  show "LB_" there.)
- **Mission load, 0x48faf0 (mission.cpp):**
  - 0x48fb58 defines macro `USEZBD` = "true" (0x595bb0).
  - 0x48fb69 sets **[0x5f4244] = 1**: "use the archive". Nothing else writes
    it, and the interpreter only runs at mission load, so it is always on.
  - 0x48fc41 defines `HARDWARE` = "true" when the hardware renderer is chosen.
  - 0x48fd83 runs `support\<mission>size.gw` (sprintf 0x5e741c). The result is
    ignored. These scripts set the node, model and material array sizes.
  - 0x48fde9 runs `<mission>.gs` (0x5e7404). If it returns 0 (the script could
    not be opened at all), the game stops with the fatal error "Failed to
    Interpret: %s -- Mission Load Aborted" (0x49bfd0, mission.cpp line 0x261).
  - 0x491150 (mission.cpp), cockpit setup: for each of the nodes `cpit_ania`,
    `cpit_anib` and `cpit_anic` found under the cockpit (0x4f9d60), it sets the
    current node (`+0xb8`) and runs `support\cpit_cycleA/B/C.gw`. Those
    scripts have no `FindNode` line; they use the node the game set.
- **0x595990 `source(name)`** (thiscall, ret 4):
  1. If a file is already being read (+0x98), pushes {FILE, ftell, binary flag}
     on the include stack (0x595da0; array +0x9c, count +0xa0).
  2. If [0x5f4244] is set and **0x596620 `open_archive`** succeeds, tries
     **0x596810 `find(name)`**. A hit returns the shared archive FILE, already
     seeked to the script, and sets binary mode.
  3. Otherwise `fopen(name, "r")` on the loose text file (+0xb0 counts these
     opens).
  4. Runs it (0x5954a0) and closes it, unless it is the archive.
  5. Pops the stack (0x595e20) and `fseek`s the parent back. A nested `source`
     inside the archive shares the one FILE, so the saved position is what
     makes includes work.

  It returns 1 if the file was opened, otherwise 0. Errors inside the script
  do not change the result.
- **0x596620 `open_archive`** (thiscall, arg = archive name, +0x78). It returns
  at once if +0x80 (archive FILE) is already set. Otherwise it builds the
  search-path object +0x7c from +0x74 (0x576100) and opens the file with
  0x5763b0(name, "rb") (zutl search-path fopen). It reads `{u32 magic, u32
  version}` and accepts only 0x08971119 and 7, then reads `u32 count` and
  `count` 0x80-byte entries into `realloc(0, (count+1)*0x80)`. **Staleness
  check:** for every entry it calls `_stat(entry.name)` relative to the
  current directory. If a loose file exists and its `st_mtime` differs from
  `entry.timestamp`, it rejects **the whole archive** (fclose, free, returns
  0). Otherwise it keeps magic/version/count at +0x84/+0x88/[+0x8c] (a malloc(4)
  cell) and the directory at +0x90. The FILE stays open for the session.
  Nothing closes +0x80 outside this function; 0x595440 frees only the
  search-path object (likely a leak, harmless).
- **0x596810 `find(name)`**: `_stricmp` over the entry names, so
  `support\c1.gw` matches the stored `Support\c1.gw`. If a loose copy exists
  and `difftime(st_mtime, entry.timestamp) > 0` (the loose copy is newer), it
  returns NULL and the caller falls back to `fopen`. Otherwise it does
  `fseek(entry.offset)` and returns the FILE.

## 2. File layout, as the exe reads it

```
u32 magic   0x08971119          0x5966ba
u32 version 7                   0x5966c4
u32 count   (44)                0x5966db
entry[count], 0x80 bytes each   0x596736
  char name[0x78]               NUL-padded, e.g. "Support\c1.gw", "c1.gs"
  u32  timestamp                time_t, compared with _stat st_mtime
  u32  offset                   absolute file offset of the script
script (at entry.offset), read line by line by 0x5955e0 in binary mode:
  repeat:
    u32 size                    0 = end of script
    u32 argc                    stored as the token count (+0x08)
    u8  text[size]              argc NUL-terminated tokens back to back
```

**0x5955e0 `read_line(FILE, linebuf)`**, binary branch (+0x94 set). It sets
the argument cursor (+0x0c) to 1, then `fread`s `size`. If `size` is 0, it
sets argc = 0 and returns 0. Otherwise it `fread`s argc into +0x08,
`malloc(size)` into +0x1c, `fread`s the text, and points argv[i] (+0x20 +
4i) at each NUL-terminated token in turn. The exe trusts argc. It does not
check it against the NUL count, as mech3ax does, and it does not bound it:
argv has 16 slots (+0x20..+0x5c), and slot 16 would overwrite the macro
table pointer at +0x60. In the shipped data the most tokens on a line is 5
and the longest line is 106 bytes. The scripts are stored one after another,
but the exe reaches each one only through `offset`.

Text branch (loose `.gs`/`.gw` files): `fgetc` into the global line buffer
0x730c20 (cleared as 0x100 dwords, so 1 KB, with no bound check) up to and
including `\n`, a NUL byte or EOF. The tokenizer **0x595850** then cuts
everything from the first `#` on (comment), skips leading whitespace and
splits on `strpbrk(", \t\n")` (0x5f426c). A comma therefore separates
arguments just as a space does.

**0x5954a0 `run(FILE, binary)`**: loops `{ clear linebuf; ok = read_line();
go = exec_line(); } while (ok & go)`. The last line is executed before EOF is
checked, which handles a final line with no newline.

## 3. Executing a line

**0x595510 `exec_line(linebuf)`** clears +0x18 (quit), +0x10 (error) and +0x14
(the miss counter), tokenizes in text mode, then:

1. **0x590e80 preprocessor** (argv[0]). It returns 0 when it consumed the line
   or the line is being skipped.
2. vtable[0] "pre-command hook" (`+0x0c` = 1 first). MW3: **0x4107e0**.
3. **0x591300 dispatcher** (the command table, below).
4. vtable[1] "post-command hook", 0x5912f0 in both vtables. It only counts a
   miss.
5. If misses == 3, logs `"BadCommand (%s): %s"` (0x5f4258).
6. If the error flag is set, calls vtable[2] "on error". MW3: **0x410770**.
7. Frees the line; returns `!quit`.

Each layer that does not recognise the command adds one to +0x14 (0x596090).
Logging goes through **0x4108f0 `log(this, fmt, ...)`** and errors through
**0x5952d0 `error(this, fmt, ...)`**, which also sets +0x10. Both call the
callback at +0x70, and nothing sets that callback unless the debug console
exists (below). In the shipped game, therefore, **every interpreter message,
BadCommand included, goes nowhere**. The MW3 pre-hook counts a miss only when
byte +4 is set, which is again only when the console is open, so without the
console the count never reaches 3.

### Preprocessor (0x590e80, before the dispatcher)

The skip depth is +0xb4. While it is non-zero, every line except `endif` is
skipped.

| Word (match) | Effect |
|---|---|
| `endif` (strncmp 5) | depth-- if depth > 0, else falls through to the dispatcher (where it is unknown) |
| `ifdef` (5) | if depth == 0 and the condition is false: depth++ |
| `ifndef` (6) | if depth == 0 and the condition is true: depth++ |
| `mkdir` (5) | `_mkdir(arg)`; an error other than EEXIST is reported |
| `Quit` (exact) | sets +0x18, which ends the current file only (`source` clears it again for the parent) |
| `set` (3) | `set NAME VALUE` defines or replaces a macro (0x595c60) |
| `source` (6) | runs another script (0x595990) |
| `who` (3) | logs `%d Macros`, each macro, then `%d Variables` with each value (0x596020) |
| `var` (3) | `var NAME = VALUE` or `var NAME set VALUE`: sets a registered variable (types 0 int, 1 float; 2 string refuses with "Can't modify string variables (yet!)") |

**Conditions, 0x590d80**: `ifdef A`, or `ifdef A || B && C ...`, evaluated
strictly left to right with no precedence (`||` 0x5f2e58, `&&` 0x5f2e54,
2-character compare). A token that is neither ends the chain. A name is
"defined" only if the macro exists **and its value is exactly `true`**
(0x595bf0). `ifdef` with no name is false.

**Macros** (+0x60 array of {name, value}, count +0x64): find 0x595a90
(case-sensitive), set 0x595c60, set "true" 0x595bb0, set "false" 0x595bd0.
**Expansion 0x5956e0**: every `%NAME%` in an argument is replaced by its
value, and an unknown name expands to nothing. Expansion happens per argument
when a handler fetches it (**0x595e70 `next_arg`**); argv[0], the command name,
is never expanded. The typed getters are 0x595ef0 (atof), 0x595f10 (atoi) and
0x595ea0 (bool: `on` or `true`, case-insensitive, gives 1; anything else 0;
a missing argument gives the default).

**Variables** (+0x68 array of {name, type, ptr}, count +0x6c): register
0x595fa0, find 0x595f30, print 0x596020. The game registers three, all float:
`DamageScale` (0x5e1f68, the 0.0062 damage factor of damage.md), and in
0x47f2b0 `gravity` (0x5e5d34, 30.0) and `ceiling` (0x5e5d44, 150.0).

### MW3 hooks (vtable 0x5bccc8)

- **0x4107e0, pre**: if argv[0] starts with `W` and is `WorldSetGravity`, it
  reads a float into gravity 0x5e5d34, stores `1/g` at 0x5e5d38, and returns 0,
  so the dispatcher never sees the command. No shipped script uses it. For
  anything else it returns 1 (and counts a miss when the console is open, or
  when the word starts with `W`).
- **0x410770, on error**: if the failing command was `GameZReadZBDFile`, it
  sets `USEZBD` to "false". This is a fallback switch: when `zbd\<m>\gamez.zbd`
  will not load, the following `ifndef USEZBD` block in the `.gs` rebuilds the
  world from the developers' source `.flt` files (but see Discoveries).
- **0x4108a0** (not virtual; called at 0x49141d in the cockpit setup 0x491150): sets
  the current mech (+0xbc) and current node (+0xb8) and logs "current mech = %s".

### What a shipped mission actually runs

Taking `c1.gs` with `USEZBD` true as the example:
`set DB c1`; `source support\tolerances.gw` (BFE, coplanar, colinear and
inverse-Z tolerances); `support\common.gw` (perspective texture, dither,
macros worldName=world1 and so on, model and texture dirs, reader paths,
`LoadSoils soils.zrd`); `support\compile.gw` (RTIME=zbd,
GAMEZ=zbd\c1\gamez.zbd, `AnimSetZBDFile zbd\c1\anim.zbd`, texture dir);
**`GameZReadZBDFile zbd\c1\gamez.zbd`** (0x4ff520, the whole world); the
`ifndef USEZBD` block is skipped; `support\c1.gw` (lens flares, vertex
shading, scrolling water, cycling textures on named nodes);
`commonpostload.gw` (empty); `write.gw` (skipped, `COMPILE` never defined).

### The dispatcher, 0x591300

Before the switch it does `switch (argv0[0] - 'A')`, for 'A'..'m', through the
byte table 0x594d74 and the jump table 0x594d24. Within each letter the names
are tested in a fixed order, mostly with `strncmp(argv0, name, n)` (IAT
0x5bb654; 0x595240 is the same thing as a helper) and a few with an inline
exact `strcmp`. Handlers act on the **current node** at +0xb8 (set by
`FindNode` through 0x4f7fe0 find-by-name, `New*`, `FindSubNode` 0x4f9d60 and
others), passed in ECX. An unmatched name ends at 0x594d11 (miss++), and the
function always returns 1.

Arguments, in the order fetched: s = string (macro-expanded), f = float,
i = int, b = bool. "Handler(s)" lists the calls in the command's block, in
order. Find-by-name 0x4f7fe0 comes first where the argument is a node name;
0x518c60 is likely a texture lookup or registration. The table was generated
from the disassembly (scratch script); a block that jumps to code it shares
with another command shows no call. Uses = lines in interp.zbd.

| Command | Args | Handler(s) | Name check | Uses |
|---|---|---|---|---|
| `AddChild` | s | 0x4f7fe0, 0x4eb9b0 | 0x59134e | 40 |
| `AddClutterTemplates` | s | 0x596db0 | 0x5913df |  |
| `AddEnhancerImage` | s | 0x518c60 | 0x591426 |  |
| `AnimSetZBDFile` | s | 0x5090f0 | 0x591462 | 1 |
| `AnimSetDebugFrame` | - | 0x507dd0 | 0x59149e |  |
| `CameraSetRotate` | fff | 0x4f36b0 | 0x5914db |  |
| `CameraGetTranslate` | - | 0x4f39d0 | 0x59154a |  |
| `CameraSetActive` | b | 0x4f3420 | 0x5915f0 | 1 |
| `CameraSetDynamicLOD` | f | 0x4f32a0 | 0x59163c |  |
| `CameraSetFOV` | ff | 0x4f3d90 | 0x591688 | 1 |
| `CameraSetHorizonXZ` | s | 0x4f7fe0, 0x4f4100 | 0x5916f7 |  |
| `CameraSetHorizon` | s | 0x4f7fe0, 0x4f4090 | 0x591747 |  |
| `CameraSetLODMultiplier` | f | 0x4f3ff0 | 0x591797 | 1 |
| `CameraSetNearFarClip` | ff | 0x4f3a70 | 0x5917e4 | 1 |
| `CameraSetNearClip` | f | 0x4f3b00, 0x4f3a70 | 0x591841 |  |
| `CameraSetFarClip` | f | 0x4f3b00, 0x4f3a70 | 0x5918c7 |  |
| `CameraSetObjectHSETest` | b | 0x4f3550 | 0x59194f |  |
| `CameraSetWindow` | s | 0x4f7fe0, 0x4f3620 | 0x591995 | 1 |
| `CameraSetWorld` | s | 0x4f7fe0, 0x4f3560 | 0x5919e5 | 1 |
| `CameraSetTranslate` | fff | 0x4f3870 | 0x591a35 |  |
| `CountCameraNodes` | - | 0x4f7f80 | 0x591aa6 |  |
| `CountUsedNodes` | - | 0x4f7f80 | 0x591aef |  |
| `CycleTextureSetLooping` | b | 0x53bcd0 | 0x591b38 | 47 |
| `CycleTextureSetMap` | s | 0x518c60, 0x53bd10 | 0x591b84 | 480 |
| `CycleTextureSetOn` | i | 0x4eb420, 0x53bc30 | 0x591bcf | 47 |
| `CycleTextureSetSpeed` | f | 0x53bcf0 | 0x591c9a | 47 |
| `ClearScreenBuffer` | b | 0x57a3c0 | 0x591ce7 |  |
| `DeleteChild` | s | 0x4f9d60, 0x4ebb40 | 0x591d35 | 4 |
| `DeleteFile` | s | inline / shared tail | 0x591daa |  |
| `DeleteTree` | s | 0x4f7fe0, 0x4f9050 | 0x591de8 |  |
| `DirectedLightFluctuate` | b | 0x543910 | 0x591e5f |  |
| `DirectedLightFluctuatePeriod` | f | 0x543920 | 0x591eca |  |
| `DirectedLightFluctuateWavelength` | f | 0x543940 | 0x591f31 |  |
| `DirectedLightFluctuateBaseAndAmp` | ff | 0x543960 | 0x591f98 |  |
| `DirectedLightFluctuateHeading` | f | 0x5439f0 | 0x592016 |  |
| `DisplayOrigin` | ii | 0x4fa600 | 0x592085 | 2 |
| `DisplayResolution` | ii | 0x4fa580 | 0x5920d7 | 2 |
| `DisplaySetClearColor` | fff | 0x4fa680 | 0x592129 |  |
| `echo` | - | inline / shared tail | 0x592198 |  |
| `Echo` | - | 0x595d50 | 0x5921b8 |  |
| `FindNode` | s | 0x4f7fe0 | 0x5921fb | 321 |
| `FindSubNode` | s | 0x4f9d60 | 0x59225b |  |
| `FogLargeObjects` | b | 0x5ac5c0 | 0x5922bc |  |
| `FreeNode` | s | 0x4f7fe0, 0x4f3390, 0x4f1c60 | 0x5922fe |  |
| `GameZReadZBDFile` | s | 0x4ff520 | 0x5923a4 | 9 |
| `GameZWriteZBDFile` | s | 0x4f7f70, 0x4f7bd0, 0x4fe8e0 | 0x5923fa | 1 |
| `GetBFETolerance` | - | 0x53cf00 | 0x592442 |  |
| `GlobalWindRandomMaxSpeed` | f | 0x54b650 | 0x592489 |  |
| `GlobalWindRandomAccel` | f | 0x54b660 | 0x5924d7 |  |
| `GlobalWindRandomAngVel` | f | 0x54b670 | 0x592520 |  |
| `GlobalWindStaticVelocity` | fff | 0x54b630 | 0x59256a |  |
| `LensFlareTexture` | is | 0x518c60, 0x5676d0 | 0x5925d9 | 32 |
| `LightNew` | - | 0x4fa720 | 0x592628 | 7 |
| `LightSetActive` | - | inline / shared tail | 0x592654 |  |
| `LightSetAmbient` | f | 0x4fa940 | 0x59267c | 7 |
| `LightSetColor` | fff | 0x4fb510 | 0x5926c6 | 7 |
| `LightSetDiffuse` | f | 0x4fa9b0 | 0x592732 | 7 |
| `LightSetDirectedSource` | - | 0x4faaa0 | 0x59277c | 7 |
| `LightSetDirectional` | b | 0x4faa20 | 0x5927b5 |  |
| `LightSetOrientation` | fff | 0x4faff0 | 0x5927fe | 7 |
| `LightSetPointSource` | - | 0x4fab20 | 0x59288f |  |
| `LightSetRanges` | ff | 0x4fadf0 | 0x5928c8 | 7 |
| `LightSetSaturated` | b | 0x4faba0 | 0x592923 | 7 |
| `LightSetStatic` | b | 0x4fad30 | 0x59298e | 7 |
| `LightSetSubdivide` | b | 0x4fac70 | 0x5929f9 |  |
| `LightSetTranslate` | fff | 0x4faf80 | 0x592a64 |  |
| `LoadSoils` | s | 0x537cb0 | 0x592ad0 | 1 |
| `LODAddChild` | s | 0x4f7fe0, 0x4fe0a0 | 0x592b0c |  |
| `LODSetRange` | ff | inline / shared tail | 0x592b5a |  |
| `MakeShadows` | - | 0x5487d0 | 0x592bd0 |  |
| `MatlFaceColor` | fff | 0x5793c0 | 0x592c2d |  |
| `MatlNew` | - | 0x536a70 | 0x592ca6 |  |
| `MatlTexture` | s | 0x518220 | 0x592cdf |  |
| `ModelNew` | ss | 0x4fb740, 0x4eb300, 0x538fe0, 0x4eb370 | 0x592d44 |  |
| `Facade` | - | 0x539cc0 | 0x592dac |  |
| `ModelDoShadows` | b | 0x526a70 | 0x592e0a |  |
| `ModelPolygonBegin` | - | inline / shared tail | 0x592e5f |  |
| `ModelPolygonEnd` | - | 0x536ad0, 0x53cc40, 0x53c6c0 | 0x592eae |  |
| `ModelPolygonUV` | ff | inline / shared tail | 0x592f5a |  |
| `ModelPolygonVertex` | fff | inline / shared tail | 0x592fd1 |  |
| `NewCamera` | - | 0x4f32e0 | 0x593061 | 1 |
| `NewDisplay` | - | 0x4fa4c0 | 0x59308f | 2 |
| `NewLOD` | - | 0x4fe030 | 0x5930bd |  |
| `NewNode` | - | 0x4eacd0 | 0x5930eb |  |
| `NewObject3D` | - | 0x4fb740 | 0x593119 | 1 |
| `NewSEQ` | s | 0x4fdc70, 0x4eb300 | 0x593147 |  |
| `NewWindow` | - | 0x4f9e90 | 0x593196 | 1 |
| `NewWorld` | - | 0x4f1bd0 | 0x5931c4 | 1 |
| `NodeDump` | - | 0x5951e0 | 0x5931f0 |  |
| `NodeSetActive` | - | inline / shared tail | 0x593250 |  |
| `NodeSetDescription` | s | 0x4eb300 | 0x593278 |  |
| `NodeSetCanModify` | b | 0x4eb260 | 0x5932bc |  |
| `NodeSetLighting` | b | 0x4f9e50 | 0x593308 |  |
| `NodeSetOverwrite` | b | 0x4eb960 | 0x593354 |  |
| `Object3DAddChild` | s | 0x4f7fe0, 0x4fb7a0 | 0x5933a8 |  |
| `Object3DGetTranslate` | - | 0x4fc020 | 0x59341a |  |
| `Object3DRegisterTexturesToWorld` | b | 0x4eb420 | 0x5934c0 |  |
| `Object3DRotate` | fff | 0x4fbd80 | 0x593544 | 16 |
| `Object3DScale` | fff | 0x4fbc10 | 0x5935da |  |
| `Object3DSetActionPriority` | i | 0x4eb5b0 | 0x59364b |  |
| `Object3DSetActive` | b | 0x4eb170 | 0x593691 |  |
| `Object3DSetColor` | i | 0x4eb420, 0x53bd60 | 0x5936dd |  |
| `Object3DSetFacade` | - | 0x4eb420, 0x539cc0 | 0x59373b |  |
| `Object3DSetOpacityIsSet` | i | 0x4fbb90 | 0x593791 |  |
| `Object3DSetOpacity` | f | 0x4fba20 | 0x5937d7 |  |
| `Object3DSetPoints` | - | 0x4eb420, 0x539cc0 | 0x593824 |  |
| `Object3DSetPriority` | i | 0x4eb420, 0x53bb20 | 0x59387a |  |
| `Object3DSetScrollAlways` | bff | 0x5963f0, 0x596370 | 0x5938da | 170 |
| `Object3DSetScroll` | bff | 0x5963f0, 0x596370 | 0x59399c | 54 |
| `Object3DSetShowBackFace` | b | 0x4eb420, 0x53bb50 | 0x593a6d |  |
| `Object3DSetTextureWorldBaseCoordinates` | ff | 0x526ac0 | 0x593ad3 |  |
| `Object3DSetTextureWorldTexturesPerMeter` | ff | 0x526aa0 | 0x593b2c |  |
| `Object3DSetMorphVertex` | f | 0x4eb420, 0x53b960 | 0x593b85 |  |
| `Object3DTranslate` | fff | 0x4fc0b0 | 0x593bfd | 16 |
| `PerspectiveTexture` | b | inline / shared tail | 0x593c76 | 1 |
| `PrintNodeCount` | s | 0x4f8b80 | 0x593cb9 |  |
| `PrintTree` | - | 0x591290 | 0x593d29 |  |
| `PrintUsedNodes` | - | 0x4f7fa0 | 0x593da3 |  |
| `RdrAddPath` | s | 0x5561e0 | 0x593de5 | 1 |
| `RdrSetPath` | s | 0x5561a0 | 0x593e21 | 1 |
| `SEQ` | - | inline / shared tail | 0x593e61 |  |
| `SEQAddChild` | sif | 0x4f7fe0, 0x4fdcd0 | 0x593e7f |  |
| `SEQNew` | s | 0x4fdc70, 0x4eb300 | 0x593edb |  |
| `SEQSetActive` | i | 0x4fde50 | 0x593f18 |  |
| `SEQSetLoop` | i | 0x4fded0 | 0x593f4b |  |
| `SEQSetPause` | i | 0x4fdf10 | 0x593f7e |  |
| `SEQSetRepeat` | i | 0x4fde90 | 0x593fb1 |  |
| `SetAltitudeSurface` | b | 0x4eb620 | 0x593fe8 | 220 |
| `SetBFETolerance` | f | 0x53cef0 | 0x594020 | 1 |
| `SetCoplanarTolerance` | f | 0x538070 | 0x594067 | 1 |
| `SetColinearTolerance` | f | 0x538080 | 0x59409b | 1 |
| `SetGameZNodeArraySize` | i | 0x5ac5c0 | 0x5940cf | 9 |
| `SetMaterialArraySize` | i | 0x536a60 | 0x5940fc | 9 |
| `SetModel3DArraySize` | i | 0x5269a0 | 0x594129 | 9 |
| `SetIntersectBBOX` | b | 0x4eb750 | 0x594156 |  |
| `SetIntersectSurface` | b | 0x4eb6d0 | 0x59418e | 219 |
| `SetLandmark` | b | 0x4eb7d0 | 0x5941c6 | 2 |
| `SetPaletteName` | s | 0x5975e0 | 0x5941fe |  |
| `SetPaletteShading` | b | 0x5269e0 | 0x59422a |  |
| `SetPerspectiveAdaptiveCorrection` | iif | 0x558120 | 0x59425c |  |
| `SetPerspectiveTextureDeltaX` | i | 0x5580d0 | 0x5942a8 | 1 |
| `SetInverseZTolerance` | f | 0x526df0 | 0x5942d5 | 2 |
| `SetPerspectiveInverseZTolerance` | f | 0x558170 | 0x594309 | 1 |
| `SetPerspectiveTextureFarZ` | f | 0x558140 | 0x59433d | 1 |
| `SetProximity` | b | 0x4eb790 | 0x594371 |  |
| `SetSmallPolygonRejectArea` | f | 0x535ed0 | 0x5943a9 |  |
| `SetTextureDirectory` | s | 0x516f50 | 0x5943dd | 6 |
| `SetVertexShading` | b | 0x526a00 | 0x594409 | 7 |
| `TextureAdd` | s | 0x518c60 | 0x594440 |  |
| `Verbose` | b | inline / shared tail | 0x594470 |  |
| `VideoSetDither` | b | 0x579240 | 0x59449e | 1 |
| `VideoSetWireFrame` | b | 0x579230 | 0x5944cf |  |
| `WindowAddClearPolygonVertex` | fff | 0x4fa2c0 | 0x594506 |  |
| `WindowBuffer` | i | 0x4fa1c0 | 0x594571 |  |
| `WindowCloseClearPolygon` | - | 0x4fa3e0 | 0x5945be |  |
| `WindowOrigin` | ii | 0x4fa0b0 | 0x5945e7 | 1 |
| `WindowResolution` | ii | 0x4f9fa0 | 0x594625 | 1 |
| `WindowSetClearPolygon` | b | 0x4fa230 | 0x594663 |  |
| `WorldAddLight` | s | 0x4f7fe0, 0x4f2d50 | 0x59469b | 7 |
| `WorldExtents` | ff | 0x4f2530 | 0x5946d9 | 8 |
| `WorldOrigin` | ff | 0x4f2500 | 0x594724 | 8 |
| `WorldPartitionInclusionTolerance` | ff | 0x4f2860 | 0x59476f | 8 |
| `WorldPartitionMaxDECFeatureCount` | i | 0x4f2880 | 0x5947ba |  |
| `WorldPartition` | ff | 0x4f2560 | 0x5947ed | 8 |
| `WorldSetFogAltitude` | ff | 0x4f2420 | 0x594838 | 2 |
| `WorldSetFogColor` | fff | 0x4f23f0 | 0x59489d | 2 |
| `WorldGetFogColor` | - | 0x4f24a0 | 0x5948f7 |  |
| `WorldSetFogDensity` | f | 0x4f2460 | 0x59497a |  |
| `WorldSetFogRange` | ff | 0x4f2440 | 0x5949b2 | 2 |
| `WorldSetFogRangeNear` | f | 0x4f24c0, 0x4f2440 | 0x594a15 |  |
| `WorldSetFogRangeFar` | f | 0x4f24c0, 0x4f2440 | 0x594a80 |  |
| `WorldGetFogRange` | - | 0x4f24c0 | 0x594aeb |  |
| `WorldSetFogState` | s | inline / shared tail | 0x594b5f | 8 |
| `WorldSetVirtualPartitionMapSize` | ii | 0x4f1dc0 | 0x594c3d |  |
| `WorldSetVirtualPartition` | b | 0x4f1d90 | 0x594c7b | 6 |
| `WriteTextureSetType` | i | 0x5426a0 | 0x594cb3 |  |
| `WriteTextureSetMap` | s | 0x518c60, 0x5426b0 | 0x594ce0 |  |

Prefix matches (strncmp shorter than the name): `CameraSetRot`,
`CameraSetTransl`, `CountCameraNod`,
`Object3DSetTextureWorldTexturesPerMet`, `SetPerspectiveInvers`, and **`SEQ`
(3)**. Anything starting with `SEQ` enters the SEQ sub-table and returns
"handled" even if no `SEQ*` name matches, so it never counts as a miss.
Longer names are always tested before their shorter prefixes
(`CameraSetHorizonXZ` before `CameraSetHorizon`, `Object3DSetScrollAlways`
before `Object3DSetScroll`, `WorldPartitionInclusionTolerance` before
`WorldPartition`, ...), so none of these prefixes misroutes a real command.
`WorldSetFogState` takes `linear`, `exponential` or `off` (0x594b9e..,
0x4f23e0). `CycleTextureSetOn` is fatal if there is no current node
(0x49bfd0, "No current node to enable cycle textures. Take note of
preceding "FindNode" Error").

## 4. Discoveries

- **Two commands the shipped scripts use do not exist in this exe:**
  `LoadGameGen` (385 lines) and `GameGenSetWorld` (1). Neither string is in
  Mech3.exe. They belong to the developers' world compiler ("GameGen"). Every
  use sits inside the `ifndef USEZBD` blocks, which are dead because the game
  defines `USEZBD`. `SetModelDirectory` (5 lines) is missing too, and those
  lines are **live** (`common.gw`, every mission); they fall through as
  unknown commands, silently.
- **The GameZ fallback cannot work.** If `gamez.zbd` fails to load, the MW3
  error hook (0x410770) turns `USEZBD` off and the `.gs` falls into the
  build-from-source branch. That branch needs `LoadGameGen`, the
  `..\data\...` source tree and the `.flt` files, none of which ship, so the
  mission would come up without a world. The fallback was the developers'
  workflow left in place: `COMPILE` plus `write.gw`'s `GameZWriteZBDFile` is
  how `gamez.zbd` was made.
- **Typo in c1: `Object3DSetScrllAlways`** (`Support\c1.gw`, after
  `FindNode ocean52`). It matches nothing, so the ocean52 water texture in
  mission c1 does not scroll, while ocean50, 51 and 53 do. The original
  silently ignores it.
- **Unused scripts:** `dev1.gs`, `dev2.gs` and `lab.gs` (no `zbd\dev1` or
  `zbd\dev2` folder ships; dev1 is dated 1999-04-28, two weeks before
  release), `dev1size/dev2size/labsize.gw`, `mechs.gw`, `morfutil.gw` (a
  LOD-morph helper driven by macros `%morph_root%` and `%lod_node_1%`), and,
  in effect, `world/weapons/vehicles/effects/cN_mp.gw` (dead branch only).
  `cN_mp.gw` places the mobile field bases (`mfb_stationNN`). The script
  timestamps run from 1998-06-29 to 1999-04-30.
- **Hidden debug console, "GameZ Interpreter".** 0x5960a0 builds a zTwoD text
  window titled "GameZ Interpreter" (0x5f42ac). It sets the log callback
  (+0x70 = 0x596130) and byte +4, and runs each typed line through
  `exec_line` (0x596190; it likely echoes the line into the window first). It is toggled by
  0x4810d0 (global 0x608f74) through 0x480b20, reached from action code
  **0xa3** of the debug-key handler 0x40dba0 (case 25, jump table 0x40de34).
  0x40db60 registers 0x40dba0 only for codes 0x61, 0x70 and 0x6c ('a', 'p',
  'l') and registers 0x4abbc0 for 0x7e, so 0xa3 is **likely unreachable in
  1.2** (not traced further). With the console open, a player could type any
  command above, plus `who`, `var gravity = 10`, `var DamageScale = 0`,
  `WorldSetGravity 5`, `echo`, `PrintTree`, `NodeDump`, `CameraSetFOV` and so
  on. That makes it the natural home for a "GameZ cheat console" in the
  rebuild.
- **Modding the scripts.** Because of the staleness check (0x596620), a loose
  copy of any archived script, at the same relative path (`Support\c1.gw`,
  `c1.gs`) and with a different mtime, makes the game **reject the whole
  archive**. From then on every script is loaded loose. A modder must
  therefore extract all 44 scripts as text (mech3ax does it) next to
  Mech3.exe. The per-script "newer loose file wins" test in 0x596810 only
  fires when a file is edited while the game runs, after the archive has
  already been accepted (the developers' live-edit path). Loose files allow
  `#` comments and commas.
- **Quirks and latent bugs** (none triggered by the shipped data):
  - Nested conditionals inside a skipped block are not counted. While
    depth > 0, `ifdef` and `ifndef` are ignored but their `endif` still
    decrements, so the inner `endif` closes the outer block (0x590e80).
  - **Expansion returns one static buffer** (0x731020). A handler that fetches
    two arguments that both contain `%...%` gets the second expansion for
    both. An argument without `%` is returned in place, so the shipped
    scripts (at most one macro argument per live line) are safe.
  - There is no bound on argc (16 argv slots), the text line (1 KB), the
    macro name (64-byte stack buffer in 0x5956e0) or the expansion (static
    buffer of about 1 KB).
  - `set`, `who` and `var` are matched by their first 3 letters, `ifdef`,
    `endif` and `mkdir` by 5, so `settle x y` is a `set`, and so on.
    Dispatcher names are case-sensitive; macro names are case-sensitive;
    script names are not.
  - A `source` frees the current line (+0x1c) before recursing, which leaves
    the parent's argv dangling (it is not read again).

## 5. Porting order and checking

Leaves first. Everything except the dispatcher is plain C and
self-contained.

1. **Archive directory**: 0x596620 open/validate and 0x596810 find. Port as
   `interp_archive` over a byte buffer, with `stat` behind a callback (the
   rebuild's importer will usually have no loose files).
2. **Line reader** 0x5955e0 (binary and text branches) and **tokenizer**
   0x595850.
3. **Macros**: 0x595a90, 0x595c60, 0x595bb0, 0x595bd0, 0x595bf0, expansion
   0x5956e0, arg getters 0x595e70, 0x595ef0, 0x595f10, 0x595ea0. **Variables**:
   0x595f30, 0x595fa0, 0x596020.
4. **Preprocessor** 0x590e80 and conditions 0x590d80.
5. **exec/run/source**: 0x595510, 0x5954a0, 0x595990, stack 0x595da0 and
   0x595e20; ctor/dtor 0x5952f0, 0x5953d0, 0x595440; log/error 0x4108f0 and
   0x5952d0.
6. **Dispatcher 0x591300** as a table {name, match length, handler}, in the
   original test order, with the first-letter switch kept (it decides
   prefix behaviour). Handlers are thin and call zClass functions (world
   0x4f1bd0.., camera 0x4f32e0.., light 0x4fa720.., window 0x4f9e90.., display
   0x4fa4c0.., object3d 0x4fb740.., cycle textures 0x53bc30..). Port each
   with its module (see gamez.md and zclass-world.md); start with those the
   live scripts use (the "Uses" column, without the dead-branch ones).
7. **MW3 subclass** 0x410700, hooks 0x4107e0 and 0x410770, and 0x4108a0;
   mission calls at 0x48fb58..0x48fde9 and 0x491150. Optionally the console
   0x5960a0 as a dev feature (FIX-free, but it needs a front-end text window).

**Differential tests** (pattern: `tests/differential/test_motion.py`):

- *Reader*: run 0x596620 with `this` in emulated memory and +0x7c pre-set to a
  dummy search-path object. Stub 0x5763b0 (return a fake FILE), `fread`
  (0x5bb594), `realloc` (0x5bb610), `_stat` (0x5bb588, return -1 = no loose
  files) and `fclose`. Then call 0x596810 for each of the 44 names (stubs
  `_stricmp` 0x5bb580, `fseek` 0x5bb59c, `difftime` 0x5bb540). Compare the
  directory and offsets with the port. Run it again with `_stat` returning a
  matching or a different mtime to cover the rejection and live-edit branches.
- *Lines*: call 0x5955e0 repeatedly on each script (binary branch, `malloc`
  0x5bb664) and compare argc and every token with the port; it must end on
  the zero size exactly at the next entry's offset.
- *Tokenizer and expansion*: 0x595850 and 0x5956e0 on synthetic lines
  (comments, commas, tabs, `%A%%B%`, unknown macros, two macro arguments for
  the static-buffer bug), with `strchr`, `strpbrk`, `iswspace`, `strncat`,
  `strncpy` and `_strdup` served from Python (or by running the MSVCRT
  routines as real code if `original.py` can).
- *Whole-script trace* (the strongest check): emulate 0x595990 on each `.gs`
  with the I/O stubs above and macros preset (`USEZBD`, `HARDWARE` on and
  off). Patch the dispatcher entry 0x591300 to a hook that records
  `(argv0, args...)` and returns 1, and point vtable[0] at a stub returning 1.
  Run it a second time with the hook setting the error flag on
  `GameZReadZBDFile` (exercises 0x410770 and the dead branch). The port's
  trace of dispatched commands must be identical, which covers
  ifdef/ifndef/`||`, `set`, nested `source`, `Quit` and the include stack in
  one go.
