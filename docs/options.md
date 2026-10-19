# The game's options

Mech3.exe 1.2, the functions at 0x0040c3b0..0x0040dae0 (original file unknown,
between mech3\briefing.cpp and mech3\mech.cpp; ported as `mech3/options`).

## Where they live

Each option is an entry of the settings store (0x00597d90, a linked list of
`{data, ?, type, size, name, scope}` at `[0x00731910]`; types 0/1 hold a dword,
2 eight bytes, 3..7 a `calloc`'d buffer), saved in the registry under
`SOFTWARE\MicroProse\MechWarrior 3\1.0` (0x00598210, read back by 0x005977d0,
scope 1 = HKCU, 0 = HKLM). The detail.zrd loader 0x0040c420 makes each entry,
stores its pointer at 0x005f4cc8.., and gives it a value from detail.zrd
(0x0040c3b0: the first item of the named list whose condition holds: DEFAULT,
or CPU_CLASS / CPU_MHZ / VIDEO_KB / RAM_KB / HW_ACCEL compared by 0x0040bf50
with the machine's) or the default below. The loader, the store and the
registry are not ported yet: the rebuild keeps each option as a field of
`mech3::Options`, starting from these defaults (the GfxFlags without the
machine bit).

| Pointer | Registry value | Default | Getter / setter |
|---|---|---|---|
| (0x005f4cc8) | which _SW/_HW entry applies | | set with HWCardFlag by 0x0040d4c0 |
| 0x005f4ccc | HWCardFlag | InstallOptions & 0xff | 0x0040d4d0 / 0x0040d4c0 |
| 0x005f4cd0 | HWCardDev | 999 or -1 | 0x0040cc10 / 0x0040cbf0 (also picks HW when >= 0) |
| 0x005f4cd4 / d8 | TextureMemory_SW / _HW | 0 | 0x0040d410 / 0x0040d3f0 |
| 0x005f4cdc / e0 | GfxFlags_SW / _HW | made of entries: 0x02 Transparency (1), 0x01 Lighting (1), 0x08 Perspective (1), 0x10 GlobalLight (SW 0, HW 1), 0x10000 AllVideoBuffer (0), 0x04 from the machine (`[0x005f4cb0]` bit 0) | 0x0040d480 / 0x0040d430 (0x10 lights the world's "sunlight" node) |
| 0x005f4ce4 / e8 | ObjectLOD_SW / _HW | 0 | 0x0040d3b0 / 0x0040d310 |
| 0x005f4cec / f0 | EffectsLevel_SW / _HW | 1 / 0 | 0x0040d2f0 / 0x0040d2a0 (effects get 2 - level) |
| 0x005f4cf4 / f8 | ShadowParts_SW / _HW | 0 | 0x0040d220 / 0x0040d1f0 |
| 0x005f4cfc | Shadow (hardware only) | 1 | 0x0040d280 / 0x0040d240 |
| 0x005f4d00 | SoundLOD | 0 | 0x0040d3e0 / 0x0040d3d0 |
| 0x005f4d04 | GameOptions | bits below | 0x0040ce00 sets the word |
| 0x005f4d08 | GameIntensity | 1 (0 low, 1 medium, 2 high) | 0x0040cdf0 / 0x0040cc20 |
| 0x005f4d0c | VMode | 5 | 0x0040d9d0 / 0x0040d810 |
| 0x005f4d10 | InGameVMode | 5 | read by the mission state |
| 0x005f4d14 | VStride | 1 | 0x0040d7a0 |
| 0x005f4d18 | Camera (12 bytes, +8 the camera node) | | 0x0040d5e0 / 0x0040d590 |
| 0x005f4d1c / 20 / 24 | Render / Display / Window (0x28-byte rectangles) | | 0x0040d6a0.. |
| 0x005f4d2c / 30 | MouseAccelX / Y | 1.3 | |
| 0x005f4d34 / 38 | JoyAccelX / Y | 1.3 | |
| 0x005f4d3c | SoundVolume | 0.5 | 0x0040daa0 / 0x0040da80 |
| 0x005f4d40 | ActiveCtlCfg (0x104 bytes) | a message string | |
| 0x005f4d44 | MouseMode | 2 | |
| 0x005f4d48 | CDVolume | 0.5 | 0x0040dae0 / 0x0040dab0 (x 65535 to both channels, truncated) |
| (0x005f4d4c) | VMode 3's doubling | | 0x0040d4b0 / 0x0040d4a0 |

**GameOptions bits**: 0x001 CameraMode 1 (0x100 is mode 2; 0x0040ce70 reads
1, 2 or 0), 0x002 Invincible, 0x004 InfiniteAmmo, 0x008 HeatMgmt (heat on),
0x010 TorsoAutoRtn, 0x020 ThrottleConst, 0x040 UseJoystick, 0x080 MW2JJets,
0x200 FastMenu. Defaults: CameraMode 1, HeatMgmt and UseJoystick on, the rest
off. **FastMenu is what `-seasick` sets**: the menu screens read it
(0x0040cec0) and store its inverse at their +0x138, likely turning the menus'
motion off (seasick players).

The setters have side effects on the pilot's mech (`[[0x0060861c]+0x14]`):
invincibility its +0x630, unlimited ammo 0x004b0220 on its weapons (+0xd4),
TorsoAutoRtn `[+0x39c]+0xa0`, ThrottleConst the inverse at `[+0x37c]+0x3f`,
UseJoystick its control mode (0x0041cf30: 6 joystick, 3 mouse) and the
control axis table at 0x006040d0 (four 16-byte entries, rewritten only while
its +0x40 byte is set). In a network game (the app's +0x1bd, `0x00608e45`)
invincibility and unlimited ammo leave the mech alone. Turning invincibility
or unlimited ammo on, or heat off, sets the cheated flag 0x00609994
(protection.md).

**Screen modes** (0x0040d810): all three rectangles to the origin, 16 bpp,
then VMode 3: render 320x240, window and display 640x480, doubled; 5..10:
640x480, 800x600, 1024x768, 1152x864, 1280x1024, 1600x1200; 4 and others
keep the sizes they had. The input gets the display size (0x0051b590).

**GameIntensity** (0x0040cc20): stores the level as given and picks a preset by
the level clamped to 0..2: low (0.5, 1.2, 0.7, flags 1 1), medium (1, 1, 1, 0 0),
high (1.2, 0.6, 1.3, 0 0); a complete `(name f f f b b)` entry in
intensity.zrd replaces it. The three floats and two flags are the app's
+0x1cc..+0x1d4 and +0x1c4/+0x1c5 (game-loop.md); who reads them is not traced.

**The camera** (0x0040d590): stores the node and sets its horizontal field of
view to the vertical one times the render width over height.

**Apply all** (0x0040cb30, at the end of InitInstance and on entering a
mission) sets each option to itself in this order: camera mode, invincible,
infinite ammo, torso return, throttle, shadow parts, effects, object LOD,
graphics flags, screen mode, joystick, intensity, shadow, sound volume, CD
volume. HeatMgmt, MW2JJets and FastMenu are not re-applied.

## Tests

`tests/differential/test_app_options.py`: 80 random option states and random
sequences of every getter and setter above (and apply-all), with and without a
pilot's mech, against the original: the call traces (getter results
included), all 85 option words and the bytes written into the mech. The
camera's field of view is allowed one unit in the last place (the x87
divides in 64-bit precision, the port in double).
