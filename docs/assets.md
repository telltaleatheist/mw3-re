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
| `motion.zbd` | zArchive | mech animations (`<mech>_<move>`) | format not traced (mech3ax `motion`) |
| `mechlib.zbd` | zArchive | `version`, `format`, then `.flt` models | format not traced (mech3ax `mechlib`) |
| `interp.zbd` | own header (magic 0x08971119) | mission scripts (mech3ax `interp`) | not traced |
| `c1`..`c4`, `t1`: `gamez.zbd` | own | the mission world: nodes, textures directory, materials (`textures.md` covers its texture directory reader) | not traced |
| `c*/anim.zbd` | own | mission animations (mech3ax `anim`) | not traced |

The loose `.wav` files in `zbd/` (`mech_engine_np.wav`, soil footsteps) are
plain RIFF WAV too.
