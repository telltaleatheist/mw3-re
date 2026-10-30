# Settings: the store, the registry and detail.zrd

Mech3.exe 1.2. The game keeps its options, its key bindings and a few video
choices in a **settings store** (0x005977d0..0x00598270, original file
unknown, between `zinterp_parse.cpp` and `zfmv\fmv_script.cpp`; ported as
`gamez/zutil/zutl_reg`), saved in the registry. At start-up the **settings
loader** 0x0040c420 (between `briefing.cpp` and `mech.cpp`; ported in
`mech3/options.cpp`) makes one entry per option, gives each a value picked
from `detail.zrd` for the machine, then lets the registry's saved values
overwrite them. The options themselves (getters, setters, side effects) are
options.md. Checked against the exe by `tests/differential/test_settings.py`
(section 6).

## 1. The store

A list of entries at `[0x00731910]`, newest first (each made one is put at
the head). An entry is 0x20 bytes, `malloc`'d by 0x00597d90:

| Offset | Field |
|---|---|
| +0x00 | the value: types 0..2 in the entry itself (4 or 8 bytes), types 3..7 a pointer to a `calloc`'d buffer |
| +0x08 | type: 0 a dword (saved REG_DWORD), 1 four bytes (floats; REG_BINARY), 2 eight bytes, 3..7 a buffer of `size` bytes (REG_BINARY) |
| +0x0c | size: 4, 8 or the buffer's |
| +0x10 | name (`_strdup`) |
| +0x14 | scope: 1 HKCU, 0 HKLM, anything else not saved |
| +0x18 | next |

- 0x00598210(company, product, version), InitInstance step 4 with
  "MicroProse", message 7 ("MechWarrior 3") and "1.0": the key's names
  (0x00731918..20), the list emptied (an old one is dropped, not freed), the
  flag 0x00731914, then the machine probe 0x00597fc0 (section 4).
- 0x00597d10(name): the entry by exact name (`strcmp`), or 0.
- 0x00597d90(name, type, size, scope) (ECX, EDX, two stack arguments): the
  existing entry of that name whatever the other arguments, else a new one.
  A buffer type with size 0 makes nothing and returns 0.
- 0x00597d80(mask): HKLM's `InstallOptions` (read by the probe) `& mask`.
- 0x00597fa0(out): copies the machine's 12 dwords.
- 0x005977d0, **read**: opens `HKCU\SOFTWARE\MicroProse\MechWarrior 3\1.0`
  and then the same under HKLM, both KEY_READ; if either fails nothing is read
  (returns 0). For each entry of scope 0 or 1 it asks for the value's size
  and reads it only when that is the entry's size (4, 4, 8 or the buffer's);
  the registry type is not checked. The bytes go straight into the entry:
  no setter runs.
- 0x00597a60, **write**: creates both keys (KEY_WRITE, HKCU first); a
  failure returns 0 having written nothing (and closes a handle it never got).
  Each entry of scope 0 or 1 is written, type 0 as REG_DWORD, 1..7 as
  REG_BINARY; the first failing write returns 0 at once, leaving the keys
  open.
- 0x00598270: frees every entry and the names.

Who makes entries: the loader (section 2) and zInput's command map
(0x0051bd70: the bindings, `0xa7 * 4` bytes, type 7, HKCU, named
"DefaultCmdMap" by InitInstance step 5, before the loader). Others only look
entries up: zSound's `SoundLOD`, `SoundVolume` and `MuteSound` (no such
entry), the CD's `CDAudio` (none), `GfxFlags_SW` / `_HW` from 0x00526718
(before gmod_init), 0x00557aab (before zrndr_draw) and zvid_dd.c,
`TextureMemory_*` from 0x00516ed1 (between zgeometry and zimage), `VMode`
from 0x0059ed61 (between fmv_main and twod_dialog).

## 2. The loader, 0x0040c420

Called by InitInstance (startup.md step 7) with "detail.zrd" (ECX). It
clears its 0x2d dwords at 0x005f4c98 (the machine's copy, the `_SW`/`_HW`
selector 0x005f4cc8, every entry pointer), loads the file (0x00556740;
failing, returns 0: InitInstance then opens `zbd\reader.zbd` and tries
once more), copies the machine (0x00597fa0), then makes each entry and sets
it through its setter, in this order:

| Entry | Type, scope | Value |
|---|---|---|
| HWCardFlag | 0, user | `select_hw(InstallOptions & 0xff)` |
| HWCardDev | 0, user | 999 with a 3D card, else -1 |
| TextureMemory_SW, _HW | 0, user | detail, default 0 (selector 0, then 1) |
| GfxFlags_SW | 0, user | 0x04 if MMX, Transparency (1) 0x02, Lighting (1) 0x01, Perspective (1) 0x08, GlobalLight_SW (0) 0x10, AllVideoBuffer (0) 0x10000 |
| GfxFlags_HW | 0, user | the same with GlobalLight_HW (1) |
| SoundLOD | 0, user | detail, 0 |
| ObjectLOD_SW, _HW | 0, user | detail, 0 |
| EffectsLevel_SW, _HW | 0, user | detail, 1 and 0 |
| ShadowParts_SW, _HW | 0, user | detail, 0 |
| Shadow | 0, user | detail, 1 |
| GameOptions | 0, user | 0, then CameraMode (1), Invincible (0), InfiniteAmmo (0), HeatMgmt (1), TorsoAutoRtn (0), ThrottleConst (0), UseJoystick (1), MW2JJets (0), FastMenu (0) through their setters |
| MouseAccelX, Y, JoyAccelX, Y | 1, user | 1.3 |
| GameIntensity | 0, user | `set_game_intensity(1)` (no detail lookup) |
| MouseMode | 0, user | 2 |
| SoundVolume, CDVolume | 1, user | 0.5 through their setters |
| VStride | 0, **not saved** | 1 |
| Camera (0xc), Render, Display, Window (0x28 each) | 7, **not saved** | zeros |
| VMode | 0, **not saved** | `set_vmode(detail, 5)` |
| InGameVMode | 0, user | detail "InGameVMode" (not in the file), 5 |
| ActiveCtlCfg (0x104) | 3, user | message 0xfc0, "DefaultCtlConfig" |

The names in parentheses are detail.zrd's keys and the defaults used when the
file has no entry for them. Then: mech3's default key bindings 0x00411150
(controls.md, Setup), `[0x005f4d28]` = the "DefaultCmdMap" entry, **the
registry read** 0x005977d0, the controls file named by ActiveCtlCfg
(0x004120a0), the camera's node and the three rectangles' targets cleared
(handles of a run that is gone), the file freed; returns 1. The saved
values are applied by InitInstance's last step, apply-all 0x0040cb30
(options.md).

**What is not saved.** VMode, the mode the menus run in, has scope 2: every
start begins at detail.zrd's choice (640x480 on anything from a Pentium Pro).
InGameVMode, the mode a mission starts in, is saved and read as any number:
5..10 are 640x480, 800x600, 1024x768 and the three the menu does not offer,
1152x864, 1280x1024 and 1600x1200 (0x0040d810, options.md).

## 3. detail.zrd

`reader.zbd`'s `detail.zrd` is one list of `NAME (ENTRY ...)` pairs; each
entry is `(CONDITION VALUE)`. 0x0040c3b0(root, name, default) finds the name
(0x005569e0, depth first) and returns the value of the first entry whose
condition holds, else the default (also without a file). An entry is taken
to be a list of at least two items (the port skips anything else, which the
shipped file never has).

- **Condition** (0x0040c1a0): the string `DEFAULT` (exact case) holds; a
  list of exactly three items `(NAME OP VALUE)` compares the machine's value
  for NAME with VALUE: `CPU_CLASS`, `CPU_MHZ`, `VIDEO_KB`, `RAM_KB`,
  `HW_ACCEL` (the probe's +0x10, +0x14, **+0x20**, +0x1c, bit 6 of +0x18);
  any other name compares 0. Anything else fails.
- **Operators** (0x0040bf50): `==`, `<`, `>`, `<=`, `>=`, `!=` (signed), and
  `~=`: |machine - value| < machine * 0.02 (within 2%); any other fails.
- **Values** (0x0040bf20): an int; a float truncated (`_ftol`); a name from
  the table at 0x005e0710 (`TRUE` 1, `FALSE` 0, `HIGH` 0, `MEDIUM` 1, `LOW`
  2, `CPU_CLASS_8086` 0 .. `_80286` 2 .. `_PENTIUM_NEWER` 7, `TEXMEM_MAX` 0
  .. `TEXMEM_2MB` 6, `ZVID_320x200x16` 2 .. `ZVID_1024x768x16` 7,
  `SOUND_API_DSOUND` 0 / `_A3D` 1, `camAttach` 1, `camTether` 0,
  `camTetherZ` 2), unknown names 0.

`VIDEO_KB` reads the probe's +0x20, which nothing writes; the video memory
is at +0x24. It is always 0. The shipped file never asks for it. `SoundAPI`
is in the file but no code reads it.

## 4. The machine (0x00597fc0)

12 dwords at 0x00731924: +0x00 CPUID's vendor string, +0x10 the CPU class
(0x00598360), +0x14 its MHz (0x005986c0, a timing loop), +0x18 flags (bit 0
MMX from CPUID 1's EDX bit 23, bits 1 and 2 always 0, bit 6 a 3D card:
devices found and `InstallOptions` says so), +0x1c `GlobalMemoryStatus`'s
total physical memory in KB, +0x24 DirectDraw's total video memory in KB,
+0x28 0. `InstallOptions` (HKLM, from the installer) is at 0x00731954.

On a machine of today the shipped detail.zrd gives: VMode 640x480,
TextureMemory MAX for both, Lighting and Perspective on, SoundLOD,
ObjectLOD and hardware effects HIGH, software effects MEDIUM, FastMenu off.

## 5. Saving

The app's destructor 0x0047fab0, at process exit, after ExitInstance, runs
0x0040cb20: the store's write 0x00597a60, then 0x00410a80 (the controls'
groups freed), then frees the store (0x00598270). One other caller saves:
0x00437f60 (between mech_data.cpp and mech_map.cpp, a screen's handler;
likely the options screen, not traced). Nothing is saved when the game
crashes.

## 6. The port

- `zreg` is the store; an entry points at the field that holds its value
  (the options are `mech3::Options` fields), or owns its bytes (zInput's
  bindings: `SdlZinHost::settings_block`). The order, the lookups, the size
  checks and the failures are the original's.
- **FIX (A)**: the registry is `platform::Settings`, call for call
  (RegOpenKeyExA, RegCreateKeyExA, RegQueryValueExA, RegSetValueExA,
  RegCloseKey). `platform::RegFile` keeps both keys in `settings.reg` in the
  write dir, as regedit's text: `"Name"=dword:...` and `"Name"=hex:...`
  under `[HKEY_CURRENT_USER\SOFTWARE\MicroProse\MechWarrior 3\1.0]`. A
  Windows export of the game's keys can be dropped in (UTF-16 or plain;
  `WOW6432Node` paths count); values the game does not know are kept. The
  file stands for both keys, so a missing HKLM key (no installer) does not
  stop the read.
- **FIX (A)**: the machine is described by the front end (`GameEngine::
  settings_init`): CPU class 6, 2000 MHz, MMX, a 3D card, the real memory
  (capped at 4 GB as GlobalMemoryStatus caps it), 256 MB of video memory;
  InstallOptions 0x070707, a full install (below; until 2026-10-29 0x101,
  which loaded the reduced mech textures and no bullet holes).

**InstallOptions** (HKLM, written by the installer, its components in
`SETUP.INS`): one bit per installed component.

| Bit | Component | Read by |
|---|---|---|
| 0x1 | HW Files\2MB Card | the card's texture choices (0x0043a310: 2 MB, with 2 MB or more); 0x004804f0: a card under 4 MB needs it |
| 0x2 | HW Files\4MB Card | 0x0043a310 (4 MB, with 4 MB or more); the mission load 0x0048faf0 |
| 0x4 | HW Files\8MB Card | 0x0043a310 (8 MB, with 8 MB or more); 0x0048faf0 |
| 0x100 | SW Files\Low Detail | the software texture choices (0x0043a310: 2 MB) |
| 0x200 | SW Files\Medium Detail | 0x0043a310 (4 MB) |
| 0x400 | SW Files\High Detail | 0x0043a310 (MAX) |
| 0x10000 | Sound\Low Fidelity | the sound quality list (0x00439570: Low) |
| 0x20000 | Sound\Medium Fidelity | nothing (soundsM.zbd is not shipped) |
| 0x40000 | Sound\High Fidelity | 0x00439570 (High) |
| 0xff, 0xff00 | any card set, any software set | the device list (0x004394d0), HWCardFlag (0x0040c420), 0x0047fe10, 0x00480970, 0x005981c0 |

In the mission load, neither 0x2 nor 0x4 (only the 2 MB card set) loads
the mech textures as variant 0x11 (`rmechtex16.zbd` in place of
`rmechtex.zbd`) and turns the RGB textures off, and with them HOLES
(`mission-load.md` step 3, `damage-decals.md`).
- **FIX (bug)**: a saved ActiveCtlCfg without its terminating 0 made the
  controls file's name run past the 0x104-byte buffer; the port stops at
  the buffer's end.
- `mw3` runs the destructor's steps (`Mech3App::destroy`) when the app ends,
  so the settings are saved on exit as the original saves them.

## 7. Testing

`test_settings.py`: 300 cases on both sides with the registry faked
identically (its values, its call trace, failures from the script): the
shipped detail.zrd and random ones (every condition name and operator, `~=`
at its edges, unknown names, short lists, value names, ints and floats), 
random machines and InstallOptions, registry values of the right size, the
wrong size and missing, either key failing to open, with and without
zInput's entry, a file that cannot be read, no file at all. Compared: the
loader's calls (the setters' calls out included), the whole store (every
entry's name, type, size, scope and bytes, in order), the machine copy and
the globals the setters write; then a save with values changed, with the
key creation and value writes failing. The x87 at 53 bits (load time).
Breaking the port showed the test catches it, 14 of 14: `~=` inclusive,
VIDEO_KB reading the video memory, floats rounded, unknown names as 1, the
software effects default, the hardware global light default, InGameVMode
from VMode, the MMX bit ignored, the size check loosened, the HKLM failure
ignored, floats saved as dwords, the save going on after a failure, entries
appended instead of prepended, `DEFAULT` matched in any case.

`mw3tests` (`settings_file_reads_an_export_and_keeps_what_the_game_saved`):
a UTF-16 regedit export with a WOW6432Node HKLM key read back, an unknown
value kept through a save, InGameVMode 10 read and saved.

## Open

- mech3's default bindings 0x00411150 and the controls file 0x004120a0 are
  ported (controls.md, Setup). After the registry read the map's bindings hold the
  saved keys while its reverse tables still hold the defaults until the
  next rebuild (0x0051bca0); not traced whether the original rebuilds them
  before play.
- The other save, 0x00437f60.
