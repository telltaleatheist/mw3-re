# Motion (mech animations)

Mech3.exe 1.2, 2026-10-19. `motion.zbd` is a zArchive of 258 animations
(`<mech>_<move>`: walk, run, fall, ...). The game opens it at mission load
(0x413fd2 -> 0x5a9820, archive at 0x736588) and reads each animation as a
`CZSIScript` (`zSIScript\si_script.cpp`, named in its own error message).
Ported to `src/gamez/zsiscript/si_script.*`;
`tests/differential/test_motion.py` reads every motion with the original and
the port and gets identical results, each read ending exactly at the end of
its archive entry. The layout matches mech3ax (`crates/motion`).

## Format

```
u32 version (4)          checked against 0x5f4bac
f32 loop time            seconds
u32 frame count
u32 part count
f32 -1.0, f32 1.0        kept at script +0x0c and +0x08
part[part count]:
  u32 length, name       (zutl_zar read_string)
  u32 flags              8 translations, 4 rotations, 2 scales (never set)
  vec3[frames + 1]       translations, when flag 8
  vec3[frames + 1]       scales, when flag 2
  quat[frames + 1]       rotations, when flag 4
```

Each part keeps one more key than the frame count; mech3ax notes that the
last always equals the first, so the loop closes on itself.

## Loading

0x5a9ad0 loads by name: it splits the path, keeps the base name at script
+0x11, and asks the archive for the entry (find 0x576a80, seek 0x576ae0) to
read through the script's vtable +4 (0x5a9c70). Failing that it looks for a
loose `<name>.zan` file on the search path (0x5a98d0), the developers'
override, as with the reader's loose `.zrd` files.

The reader calls `SetLoopTime` (0x5aa3b0) with the loop time, which computes the
per-frame time and its inverses, but only when byte +0x10 of the script is set,
and the constructor (0x5a9920) clears it. So at load those derived values stay
0; they must be set again once the script is activated. The mech's
animation classes (MechAnim 0x5bd1f0 and its blends and one-shots,
mech_anim.cpp) set the byte in their constructors (0x5a9990, 0x5a9970,
0x5a99f0) and force the loop time to frames / 30 (frames / 15 for the leg
one-shots) after loading; playback then samples `frac(rate * t / loop)`
(`movement.md`, The animation objects).

## Playback (ported, `test_mechanim.py`)

The phase (0x5abae0; 0x432a80 for looping scripts, +0x70) is `frac(rate t /
loop)` times the key count (frames for a loop, frames - 1 for a one-shot,
which also stops exactly on its last key), split into two keys and a fraction
at +0x7c..+0x84; it is cached by t alone (+0x74). Translations lerp, rotations
slerp (0x525ce0: linear above a cosine of 0.95, the sine's divisor from the
bit-trick square root), velocity keys are `(key - previous) / frame time`
(0x5aac60) times the rate; a part may carry a 3x4 frame (the skis' for the
feet). A blend lends its own timing to both animations' parts and mixes their
samples by its weight; its loop time and stride lie between theirs.

At load each gait is turned so it starts with the left foot furthest forward
(0x41fc40) and resampled to 60 keys (0x5aa780, through the old keys at rate 1).
The footfalls (0x41fd30) are where the feet are equally far fore and aft with
the right foot highest; the step is then turned so its footfall key matches
the walk's (0x431a70). The right limp is the left one mirrored (x negated,
yaw and roll negated, 0x5aa940), the left collapse the right one.

SetLoopTime resets the cached t to 0 but not the keys, so after the resample
a first sample at exactly t = 0 reuses the resample's last keys: the wrong
pose for that sample, and for motions longer than 60 frames keys past the
arrays (FIX (bug): recomputed when out of range; `fixes.md`).
