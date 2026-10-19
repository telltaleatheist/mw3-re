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
| `c*/anim.zbd` | own | mission animations (mech3ax `anim`) | not traced |

The loose `.wav` files in `zbd/` (`mech_engine_np.wav`, soil footsteps) are
plain RIFF WAV too.
