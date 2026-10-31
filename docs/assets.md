# Asset files

What each file under `zbd/` is, and where the rebuild stands. "zArchive" is the
container in `zreader.md` (files back to back, table at the end), ported in
`src/gamez/zutil/zutl_zar.*`.

| File | Container | Contents | Status |
|---|---|---|---|
| `reader.zbd`, `c*/reader*.zbd` | zArchive | reader data (`.zrd`): mechs, weapons, sounds, HUD, missions | ported (`zreader.md`) |
| `rimage.zbd`, `rmechtex*.zbd` | texture library | 16-bit textures, palettes, alpha | ported (`textures.md`); `tex2png.py` exports them |
| `soundsL.zbd` | zArchive | 999 RIFF WAV, PCM 8-bit mono 11,025 Hz | container ported; WAV is standard |
| `soundsH.zbd` | zArchive | the same 999 sounds at 22,050 Hz | container ported; WAV is standard |
| `motion.zbd` | zArchive | 258 mech animations (`<mech>_<move>`) | ported (`motion.md`) |
| `mechlib.zbd` | zArchive | `version` 27, `format` 1, `materials`, 57 `.flt` model trees (770 models) | materials and models ported; node tree next (`mechlib.md`) |
| `interp.zbd` | own header (magic 0x08971119, version 7) | 44 world-build scripts, run once at mission load | mapped (`interp.md`), not ported |
| `c1`..`c4`, `c4b`, `t1`: `gamez.zbd` | own (signature 0x02971222, version 27) | the mission world: texture directory, materials, models, nodes, dumped as in-memory pools | mapped (`gamez.md`), not ported |
| `c*/anim.zbd` | own | mission animations (mech3ax `anim`) | traced (`anim.md`) |

The loose `.wav` files in `zbd/` (`mech_engine_np.wav`, soil footsteps) are
plain RIFF WAV too.

## Data versions

The port needs no exe, only `zbd\`, `video\` and the message table
`Mech3Msg.dll`, and runs on any version's data. At start `mw3` prints which
data the mounts hold (`mech3::detect_data_version`, ours): each of the 26
data files the official 1.2 patch replaces or adds, `Mech3Msg.dll` and
`force_eff.ifr`, is matched by size and CRC (`zutl_zar::crc32` from 0)
against 1.0's and 1.2's. The result is one of:

- `1.2`: all 28 are 1.2's.
- `1.0`: the 16 that 1.0 has are 1.0's, and the 12 that 1.2 adds are absent.
- `1.0 with 1.2 files`: some files patched and some not, with the counts.
- `unknown`: a file that is neither version's (a 1.1 install, whose data
  we don't have yet, or a mod), named.
- `incomplete`: a file 1.0 already had is missing, named.

How the two installs were made:
- `game/mw3-1.0` is the disc's `DATA1.CAB` (with `DATA1.HDR`): `unshield -d
  out x`, every file group merged into one folder, with no collisions. Its
  `video` links to `game/mw3-1.2/video`; the disc's `VIDEO\` is the same in
  both.
- `game/mw3-1.2` is that plus the patch's `Patch Files` (CLAUDE.md). The
  two differ in exactly these 28 files and the exe.

Everything else (mechlib, motion, the textures, the sounds and the other
missions) is the same in both.

**On 1.0 data** the port runs as the 1.2 exe would there, checked with every
headless test (`build-10`: 222 pass) and the kills chain (c1m1..c1m4 won,
c1m3 on the same frame as on 1.2):
- hud.zrd has no V1152x864, V1280x1024 or V1600x1200 block: those three
  modes (registry-only) keep the 640x480 places, as the exe does.
- sounds.zrd has no PLAYBACK_FORMAT: the primary buffer takes the exe's
  default (mono, 22050 Hz, 16 bits); the mech sounds are 1.0's own
  (`mech_footfall.wav` and the rest, in the sound archives).
- Mech3Msg.dll lacks 38 messages; they read as "" (a FIX (bug): the
  original's null reached unchecked copies, fixes.md). The settings' default
  controls name (0xfc0, "DefaultCtlConfig") is then empty and the controls
  start from their defaults.
- The tests that pin exact outcomes (Dominic's three, the HUD anchors' block
  count) check them on 1.2's data only and the behaviour on any. The 1.0 exe's SHA-256 is `1c6419fcdbb0503d2dee006861477fc7354039e6ce95c431b5ac5e4995b465bc`;
1.2's is CLAUDE.md's. To run the tests on another install, configure a
build with `-DMW3_GAME_DIR=<install>` (for example `build-10` on
`game/mw3-1.0`).

| File (zbd\\) | 1.0: size, CRC, SHA-256 | 1.2: size, CRC, SHA-256 |
|---|---|---|
| reader.zbd | 2143369, `ea3cca83`, `fde31f93949aec52c0b22f51c57659b66ef667337f40f1ea42bc54204fb811bd` | 2146707, `19ef6c8c`, `5e17037f3634480f74265b3e378e4513b84cae518e20bc55cd5c6e9c6683851a` |
| interp.zbd | 99662, `41a8aabd`, `e4c66e6f217e952a1ba0e9fe6c8b2f6b47a3d83ef881548f2b40fb8263f5361f` | 99662, `36ebd755`, `f4ae013c419984cd1139dcbe06695efddd1875e88baa55bf0997adf36907b171` |
| c1/reader.zbd | 771601, `948e1eff`, `3bff6190a04ca0c3f585c0e13677ccb53a7e30bd18638edfaafa60cb3a8b4a7c` | 774352, `b01a0321`, `482501ffbb210ccf516b8655ece2df27e0558d24cc7c9977ef9edf9ea21c4046` |
| c2/reader.zbd | 758993, `b3a52dcc`, `e8020d7bffaf6ba5bdaf96e1bbbaa0bc649c3407f450a5c6bbe30081c8977151` | 759298, `989527dc`, `9852cbcca859adcc377a23bca19193daec918f6c9d495056ec6a29c1e74946f8` |
| c2/readerm3.zbd | 68026, `47909bb0`, `8f98cad8d776ec7152a12f74e1e29b55e1c55b5e29cca3594b7dc82cea107e33` | 73391, `f87ae650`, `e263d938c9caa7feae09936b1fd6affce9909a2c1d10a86027db872e698e940f` |
| c3/readerm2.zbd | 64538, `d335bad6`, `1d559b48944f9b4338fa38e71435c7adf091b310cb6de63164d21a4e4d9c1069` | 64542, `eafeea59`, `019b13a3e384b5506328fab6e408fc63423d4a81f9e73ccdff3a4af066b60d42` |
| c3/readerm3.zbd | 177365, `c87581b4`, `2a7ed79091830515888dd3af6644d0fc9e24bcf4bf2f65fe1fa0d84ca19cd788` | 177365, `164a18e1`, `54fe45f8b378d5287a148210d2fb87f04485d96c547a9e1d3314edde719ff3fd` |
| c3/readerm4.zbd | 116679, `fff8b52a`, `9a11120c2f391dc33f6d6196f3645d5d7722dfce72ab28068562568d69c8a1e5` | 116699, `007f3be3`, `10c837259ef8eccd686f84195e6359d381e2abd8f04345f0e010208c0edb82ee` |
| c3/readerm5.zbd | 107888, `f9dd200d`, `374ebeb150ea887ed96d743410b0628843270aa349d9f343c91c5c59ba7ae2bf` | 107912, `7d30e8d3`, `89043c04714b2f38bf806190d2d418cd25b1f02a124096e2aefe1f4151cd0ea6` |
| c3/readerm6.zbd | 125891, `55bb9635`, `ced433d4d5bf1a1dd43d99a829a65bd5ef98ab0c130e9b471705648141d5685b` | 125863, `15f9b23c`, `3e73aa429aa8461387322e245ef41337830c4dcd7def9afd47d0384a426c1ef1` |
| c3/readermp3.zbd | not in 1.0 | 7262, `fd4b0956`, `2ba33f7279334ebee6d2ba466fba0223054119ee4dd30bfbc8a83e1962e8d251` |
| c3/readermp4.zbd | not in 1.0 | 6662, `f1bfc250`, `ff567a76c1aea1417d6062b5f8445120fe427be3a9dc451d6d9c538d4002d569` |
| c3/gamez.zbd | 12999820, `9798b442`, `a38a401b605596cf809a88db65f12d0af1e26d5dc4d93158854b4fc83fbb3546` | 12994852, `41d99b82`, `3110b96b7ff30489da339038770a5fb08b8dc7f91b1aab89342c35bc97a0204e` |
| c3/anim.zbd | 3879480, `9769911d`, `7244cb74108ff778a36b3277b388c6776d0a4ec2588055de67a0485b9101a503` | 3879480, `43014a87`, `69eb5a140e34c82cd9675c76789ca00b7c5cabb6c56c919a95d2659a9ee6fc09` |
| c4b/readerm4.zbd | 34205, `3462d142`, `183101bdaf846fac8dfb1995acdeccb2996e120f1b44977fb75c1dce527cb323` | 41618, `48c8f206`, `a2d04807664e874e2582563f5c0fbc8945ee036532207d43b43d822a52f7f2c2` |
| c4b/readerm5.zbd | 26025, `c1b07763`, `500aa4a843e640aa0a29b1d026afec649d7b61e72ec69b3d4495abfbffc6cd54` | 50984, `1c5e96dc`, `10d57108b2385fbb504e057aef82c4d2e5cc7ef124db8a3282e8f1c6089e4d98` |
| mech_engine_np.wav | not in 1.0 | 107850, `15b317cc`, `099ef778bf1e992ba2026d22cb9c653753261cc74005cebf8322878a263cbd2a` |
| mech_footfall_2s.wav | not in 1.0 | 124048, `26a9df29`, `492e86565e68c0d80666c809bd8522b3511cb61ddd99a059176e395d18a39093` |
| soil_concrete_2s.wav | not in 1.0 | 124048, `e4e6f0e4`, `324ba7300161a12838f96b0ce8ef0a2a41a21e95d5573facda0494c06a4d32a2` |
| soil_dirt_2s.wav | not in 1.0 | 124200, `2d9d975b`, `92d568ff620453fa4d268b1acad2d4aeeaa4122c4e6930f8669fc026ae3d9f05` |
| soil_grass_2s.wav | not in 1.0 | 124048, `984d002d`, `11cb30d5fed7be0e0a51b6905e1bc13ae7315ed1edf04606c325ff04a83a4f35` |
| soil_in_water_2s.wav | not in 1.0 | 123912, `e7c3ac31`, `76f8b06395beb4c31425dc87c59d1709347c00e006c15f85b0f8ecb7702d70e2` |
| soil_metal_2s.wav | not in 1.0 | 124200, `2b7aa8ed`, `01ea56319e77539e79e81471ab03ed5f1d366085238adb309a0d141e0c8f3b95` |
| soil_mud_2s.wav | not in 1.0 | 110474, `d3a4e7ae`, `d933c94053f5bb7fbcdb91111284f23664007228753623b8cb38d55177e053ef` |
| soil_snow_2s.wav | not in 1.0 | 124200, `3326788d`, `d647280939dbfd44a027f88412bc98b6010e1d6f0d1c34dd62007719b11586d2` |
| soil_water_2s.wav | not in 1.0 | 124048, `a04c1e75`, `33320d09f4ea21889cd8d03d8dbc8ef82144670e41e7942df25c0bef05190ff4` |
| ../Mech3Msg.dll | 120320, `04a9d33f`, `ceb29fa9b871fcf15fc94030e46b26d2df9ca02203859b36fbf057713bdbee01` | 123392, `e519c565`, `8899fabb7e57383a9a483cb048636bcc027334b48941aa49835cdcc1c4715b8c` |
| ../force_eff.ifr | 2936, `d5f07541`, `9902299387d7c8767a62dced03d09bad85a68bc30ff714dffb572c80205add4c` | 4113, `4957166c`, `5d021f79378493302509ef3fad31282961e1f3c037367ae4f00faa154d3d2a26` |

## Pirate's Moon

Not ported yet (`pirates-moon.md`). Its data is in the same formats at
newer versions, which the port's readers refuse: zArchives version 2 (a
CRC after the count), `gamez.zbd` 41, `anim.zbd` 50 and `mechlib.zbd`'s
`version` 41 (MW3: 1, 27, 39, 27); `interp.zbd`, the texture libraries and
the motions keep MW3's (`pirates-moon.md`, PM's data against MW3 1.2's).

Its disc is laid out as MW3's (DATA1.HDR/CAB, InstallShield 5), but its
`Mech3.exe` is a loose `MECH3.EXE` on the disc beside the cabinet, not in
it. The disc install (`mw3 --install-from`, Installing from the disc)
reads PM's Redump image and installs its 102 cabinet files, identical to
`game/pm`, and its CD audio, but not that exe; when PM is ported, its
install copies the loose `MECH3.EXE` too (the port itself never runs it).

## The 1.2 patch

The official patch ("MechWarrior3 Patch", DATA.TAG's Version=1.1, which
installs 1.2) is an InstallShield 5 setup: `Setup.exe`, `setup.ins`,
`_inst32i.ex_`, the `_sys1`/`_user1` cabinets and `data1.hdr` +
`data1.cab`. Its data is `data1.cab`'s one file group, "Patch Files": 31
whole files (not binary deltas): `Mech3.exe`, `force_eff.ifr`,
`Mech3Msg.dll`, `readme.txt`, `readme.doc` and the 26 zbd\ files. All 31
are the 1.2 install's byte for byte (unshield 1.6.2 extracts the same).

`mw3 --patch PATH` (ours, `mech3/patch_import`) reads it without running
it and puts the 28 files the port reads (the table above; not the exe or
the readmes) into `<write dir>/patch12/`, which every later start mounts in
front of the install. The install is never written. PATH may be the ZIP the
patch came in (or a self-extracting exe with a ZIP appended), the unpacked
folder or any folder above it, its `data1.hdr`, or loose 1.2 files (the
backups' NoCD folder holds them too). Every candidate is kept only when its
size, CRC and SHA-256 are 1.2's; nothing is written unless all 28 are
found (into `patch12.new/`, then renamed), so a damaged download or 1.0's
own files leave the data as it was.

The cabinet, as unshield (github.com/twogood/unshield, `lib/`) reads
version 5 (version word 0x01005201): the common header ("ISc(", the
version, the cab descriptor's offset at +0x0c); the descriptor's file table
(+0x0c), directory count (+0x1c) and file count (+0x28); the table's
offsets, directories first, then file descriptors of 0x3a bytes (name,
directory, flags at +8, expanded size at +0x0a, compressed at +0x0e, the
data's offset in data1.cab at +0x26, an MD5); a compressed file is chunks
of a 16-bit length and raw deflate. Inflate is miniz's tinfl
(`third_party/miniz`). The test, `patch_import_upgrades_10_data_from_the_official_patch`,
reads the patch from `game/patch-1.2/` (the unpacked folder and a ZIP of
it, made from the backups) and, with `MW3_PATCH_ZIP`, the backups' ZIP.

## Installing from the disc

`mw3 --install-from PATH` (ours, `mech3/disc_install`) installs the game
without running the disc's Windows installer, so Mac and Linux players can
install it, and on Windows it saves a step. PATH is the CD drive's root or
any folder holding the disc's files, an ISO 9660 image (`.iso`: 2048-byte
sectors, or a raw image of 2352-byte ones), or a cue sheet (`.cue` with its
`.bin`s, as Redump dumps the disc). From a cue the first data track is read
(MODE1/2352: each sector's 2048 user bytes after its 16-byte sync and
header; MODE2/2352 form 1 after 24; MODE1/2048 as is), and its AUDIO
tracks become the music, `<write dir>/music/trackNN.wav`, as `mw3import
--cue` writes them (`platform/cue_audio`).

The image's file system (ECMA-119): the primary volume descriptor at sector
16 ("CD001"), its root directory's record at +156, each directory's records
(a zero length byte ends a sector's), ";1" dropped from the names. The MW3
disc's root holds the InstallShield setup (`SETUP.EXE`, `DATA1.HDR`,
`DATA1.CAB` of 189 MB, `_SYS1`/`_USER1`), `VIDEO\` (the 19 movies),
`EXTRAS\` and `REDIST\`.

`DATA1.CAB` is read by the patch's reader (The 1.2 patch, above), now from
where it lies, a file at a time (`each_installshield`), and every file
checked against the MD5 its descriptor carries (+0x2a): a damaged disc
writes nothing. Every file group goes into one folder, as unshield's
extraction made `game/mw3-1.0` (a path twice must be the same bytes; MW3's
disc has none twice). `VIDEO\` is copied as `video/`, its file names as on
the disc. The 16 data files of the table above are checked against the
1.0 disc's size, CRC and SHA-256, and `Mech3.exe` against 1.0's; anything
else is reported as a warning (another release, a mod) and the version
"unknown". With `--patch` too, the patch's 28 files (each checked by the
import) go over them in the same step, and the install is 1.2 (the exe
stays 1.0's: the port never runs it).

Everything is written to `<folder>.new` and renamed when whole; the folder
gets `mw3-install.txt` (the source, the version, the counts). A later
install may replace a folder holding that file; any other non-empty folder
is refused. The folder is `--install-to DIR`, else `<write dir>/install`,
and it is saved as `mw3.cfg`'s `install=`.

**For the setup screen** (`mech3/game_setup.h`, `mech3::setup`): the
same operations as plain calls for a UI, each with a progress callback
(stage, fraction of the whole, the current file; returning false cancels)
and a result in plain words (a summary, warnings, the technical detail
apart): `install_from_disc`, `apply_patch` (the overlay, as `--patch`),
`add_music` (a .cue's audio), `check_install` ("Found MechWarrior 3 1.2
at ..."), `probe_source` (a quick look at a picked drive, folder, .iso or
.cue before the copy: version, movies, music tracks, or why it is not the
disc, Pirate's Moon's named) and `find_discs` (the drives holding the disc:
Windows' CD drives, `/Volumes`, `/media/$USER`, `/run/media/$USER`). The
fraction counts the cabinet's bytes read, the movies' copied and the
music's written; a cancel at any point, the music included (written to
`music.new` and moved in last), leaves the folder and the music as they
were. `--install-from` and `--patch` are wrappers over them. Test:
`setup_api_probes_installs_cancels_and_explains` (break: the movies' copy
ignoring a cancel, 2 checks).

On Owen's disc image (`MechWarrior 3.zip`'s `Mechwarrior 3.iso`, sha256
`08c26c0d71375d6130df7716abc2c282147494d869bd48ce13bd5f972f763e49`):
126 files from the cabinet (404 MB) and 19 movies, all 145 identical to
`game/mw3-1.0`, all 16 data files 1.0's; with the patch all 28 are
`game/mw3-1.2`'s, and `mw3` starts c1 on it. Pirate's Moon's Redump image
(`game/pm-disc`, its .cue) reads through the raw sectors the same way:
its 102 cabinet files are `game/pm`'s and its track 2 the music (PM's exe
is a loose `MECH3.EXE` beside the cabinet, not in it).

Tests: `disc_install_from_an_image_a_cue_and_a_folder` makes a disc (the
patch's real cabinet and a VIDEO folder) as an image, as a raw MODE1/2352
.bin with a cue and an audio track, and as a folder; each installs the same
files, the cue's audio is the music, a foreign folder is refused, and of 20
one-bit damages to the cabinet 7 are caught only by the MD5 (breaks: no
MD5 check, 20 checks; the raw sectors read without their 16-byte header, the
cue fails). `disc_install_from_the_mw3_disc` (with `MW3_DISC` set to the
disc's .iso, .cue or drive) compares the install with `game/mw3-1.0` and,
patched, with `game/mw3-1.2`.
