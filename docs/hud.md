# The HUD

The in-mission display: radar, compass, reticle and zoom window, gauges
(speed, jump fuel, heat, coolant), the two damage displays (likely the MFD
for your mech, TARGET for what you have selected), the weapons list, objectives and
scores, the mission timer and the equipment list, laid out per resolution by
hud.zrd. Original file unknown: the code between mech3\mech_data.cpp and
mech3\mech_map.cpp, from about 0x0043a7c0 to 0x00448880 (the options
screens come before it, the mech lab's Lab State after). Mapped here; the
porting order is section 7. Status: mapped, nothing ported yet.

## 1. Where it lives and when it runs

- **One HUD per mech type**, owned by the mech data (Pilot +0x224), made on
  first use by 0x00430ad0 (`new 0x22e40`, constructor 0x0043b190) and
  returned by 0x00430aa0 (0 when the data is not loaded). The same call makes
  the **cockpit model** at Pilot +0x228: an 8-byte object (vtable 0x005bd2f4)
  holding the mech's cockpit node from mechlib (0x004847e0), or a new
  object3d (0x004fb740) when it has none; with a `cockpit` reader node it is
  placed at the origin, made non-intersecting and flagged (0x004eb6d0,
  0x004eb620). The Core frame draws the cockpit part through the view (camera
  id 1; game-loop.md, the Core frame).
- **Set-up for a mech** (0x0043bb80): from the mech's view attach (0x0041ce30)
  and 0x0041cdc0; reads hud.zrd (section 3).
- **Each frame**, three entries:
  - 0x0043c390 from the world tick's camera update (0x0040f8d0): the game
    data into the pieces (radar contacts, compass heading, the gauges' speed,
    jump fuel and heat, the target's text, the timer, objectives, the
    equipment's damage).
  - The HUD's own +0x00 (0x0043c820) through the target mech's vt+0xe0
    (0x0041cd40, from the Core frame's HUD update 0x0040fac0, only for the
    view's target): per-frame bookkeeping, the debug overlays (`%s [%s]: %d
    %d %d`, the gait names), the ammo gauge 0x00443d20, then 0x0041bd80 on
    +0x1d48 (the gauges) unless the mech's +0x638.
  - Drawing: the pieces are zTwoD objects in the HUD's Layer (0x0059fba0);
    zTwoD's per-frame update (0x0059fd20) draws them inside the Core frame's
    locked back buffer (render-frame.md). The 25 message lines at
    [0x005fc848] (0x4b0 each, y = 0x5f + 12 i) are drawn and hit-tested by
    0x0040e4b0.
- **Other entries**: the camera's look (0x00446240 on +0x6520, from
  0x00421c00), the ZAPS static and hold 0x0043be70 / 0x0043d07a (weapons.md
  ZAPS_HUD), the reset 0x0043d920 (from the mission's 0x00491696), the weapon
  HUD update 0x00446520 (from the weapons: 0x00415a48, 0x00460e83.., and the
  list itself), the ammo gauge 0x00443d20.

## 2. The HUD object (0x22e40 bytes; vtable 0x005c01d8)

A zTwoD Layer (0x0059fba0) with two virtuals of its own: +0x00 the frame
update (0x0043c820) and +0x04 shown or not (0x0043baf0; the set-up passes
whether the mech is alive, vt+0xa0). The pieces live inside it:

| Offset | Piece | Constructor (args) | Final vtable | Update (+0x24) | Added to the Layer |
|---|---|---|---|---|---|
| +0x00010 | the mech it shows | | | | |
| +0x00014 | radar | 0x0043d260 (60.0) | 0x005c01e0 (FilledBox) | 0x0043dd00 | no (drawn by the Core frame's radar call) |
| +0x00dd4 | compass | 0x0043f3e0 | 0x005c02e0 | 0x0043f620 | yes |
| +0x0144c | reticle, with the zoom window (0x00442ac0 at 10, 370, 90 x 90) | 0x00443b60 ("ret_on", "ret_off") | 0x005c0688 (Image) | 0x004441a0 | no |
| +0x01da8 | gauges | 0x00444760 | 0x005c0708 | 0x004452c0 | yes |
| +0x03434 | MFD (your mech's damage) | 0x00442060 (530, 370, 75, 100, 1) | 0x005c0470 | 0x00442540 | yes |
| +0x0410c | TARGET (the target's damage and text) | 0x0043fcb0 (10, 370, 75, 100) | 0x005c03e8 | 0x00441490 | yes |
| +0x06520 | weapons list | 0x00445f80 | 0x005c0898 | 0x00446680 | yes |
| +0x17964 | objectives and multiplayer scores (a TextBox) | 0x00446700 | 0x005c0918 | 0x00447410 | no |
| +0x18410 | mission timer (a FontText, "00:00:00.00", font 0x005e14e4) | 0x005a00c0 | 0x005bc598 | | no (shown for good) |
| +0x188c0 | equipment list | 0x00448070 | 0x005c0b90 | 0x00448450 | yes |
| +0x22e08..+0x22e34 | cleared by the constructor | | | | |
| +0x22e38 | hud.zrd's root while the set-up runs | | | | |
| +0x22e3c | the resolution's block while the set-up runs | | | | |

The damage silhouettes are loaded once for every HUD (0x0043ae50 per part,
flag [0x00605990]): `dmg_<part>_blue/grn/yel/red/blk` for head, ltorso,
rtorso, ctorso, larm, rarm, lleg, rleg with each part's offset in the
silhouette (head 30, 2; ltorso 46, 6; rtorso 19, 6; ctorso 31, 15; larm 58,
8; rarm 2, 8; lleg 39, 42; rleg 7, 42), plus three background images whose
size goes to [0x00605998] / [0x0060599c].

### What each piece draws

- **Radar** (0xdc0 bytes): a FilledBox with three FontTexts (mech_string),
  the contact images (`hud_light_green/yellow/red` and the like, 0x50 a
  list), the range text (`%.1fkm`), large and small sizes (0x0043da00),
  centred at (0x37, 0x37) of a 0x140 x 0xf0 area by default.
- **Compass** (0x678): the `hud_compass` tape (245 x 19) scrolled under
  `hud_compass_frame` (245 x 11) by the heading (0x0043f590).
- **Reticle** (0x95c): `ret_on`, `ret_off`, `ret_over`, `ret_cant` (aimed,
  idle, over a target, out of range); the zoom window: the `zoomcam` camera
  (0x004436c0 places it), `hud_zoomon` / `hud_zoomoff`.
- **Gauges** (0x168c): `hud_speed_frame` (575, 256), `hud_coolant_frame` (8,
  290), `hud_fuel_frame` (527, 296), `hud_heat_bar` (18, 163),
  `hud_heat_meter` (16, 163), the heat arrow (`lab_toggler_green`, 10, 150),
  the heat text (Arial 8), and three bars (key-list pieces 0x005a6bd0).
- **MFD and TARGET** (0xcd8, 0x2414): the damage silhouette in five colours
  per part; TARGET adds the enemy arrows (`hud_enarrow_left/right`) and the
  target's text (`%s: %s`, `%.2fkm`, ` (%dm)`, 0x00441850).
- **Weapons list** (0x11444): one entry per weapon (0x00447c40:
  `wepstring_red/green`, `hud_light_*`), `hud_weapon_select`,
  `hud_weapon_chain`, the ammo (`%3d`, 0x00445dc0).
- **Objectives** (0xaac): `rtobjective_title/incomplete/complete`,
  `rt_mp_scores`.
- **Timer**: the mission clock (`%02d:%02d:%02d`; the multiplayer clock
  [0x00609920] in a network game, else the game time [0x0060991c]).
- **Equipment** (0x22e40 - 0x188c0): the equipment list with `dmg_bg_grn`.

## 3. hud.zrd and the set-up (0x0043bb80)

hud.zrd holds one block per resolution, `V640x480`, `V800x600`,
`V1024x768`, `V1152x864`, `V1280x1024`, `V1600x1200`, each with:

| Key | Fields | Read by |
|---|---|---|
| MFD | position (x y), size (w h) | 0x00442230 on +0x3434 |
| TARGET | position, size | 0x00440110 on +0x410c |
| WEAPONS | position, width | 0x00446120 |
| EQUIPMENT | position, width | 0x00448200 |
| COMPASS | tape (x y), frame (x y) | 0x0043f500 |
| TIMER | position | the timer's set_pos (vt+0x0c) |
| GAUGES | speed_frame, fuel_frame, heat_bar, heat_meter, heat_text, heat_arrow, coolant_frame (x y each) | 0x00444dc0 / 0x00444e55 |

The set-up keeps the mech (+0x10), opens hud.zrd (0x00556740), picks the
block by the video mode (0x0040d9d0: 3 and 5 V640x480, 6 V800x600, 7
V1024x768, 8 V1152x864, 9 V1280x1024, 10 V1600x1200; any other mode keeps
+0x22e3c as it is: 0 after an earlier set-up, which clears it at its end,
but on the first set-up whatever `new` left there, as the constructor does
not clear it; a bug candidate), sets MFD, TARGET
and TIMER, then gives each piece the mech (0x0043be10, 0x00444250,
0x004423c0, 0x00441760, 0x00441800 with the mech's +0x6c, 0x0043d6d0) and
the readers their blocks (EQUIPMENT, WEAPONS, COMPASS, GAUGES), shows the
HUD when the mech is alive, sets the zoom's state (0x00442df0 when the
mech's 0x0041df10 is 2), and releases the reader.

## 4. The primitives it uses

- zTwoD (twod.md, ported): Object2D groups (0x0059fda0), Image (0x0059b970,
  named 0x0059b830), FontText and TextBase (0x005a00c0 with a fonts.zrd font
  0x00517680, 0x005a0660), Text (0x005a15a0), TextBox (0x005a18f0), Slider
  (0x0059e0f0), the Layer's add 0x0059fbe0.
- zTwoD's HUD side, not ported (twod.md, Left): FilledBox and Frame
  (0x005a3b60, 0x005a39f0), PolyLine and Shape (0x005a3830, 0x005a3170;
  0x005a38a0, 0x005a3580 its transformed points), zRender's 2D line list
  0x00562cb0 and boxes 0x005630c0 / 0x00563440, the line clip helpers
  0x0059b2f0..0x0059b7e0, HUD lines 0x005a67e0..0x005a6b30, the key-list
  bars 0x005a6bd0..0x005a6d10.
- 3D: the zoom window's camera (`zoomcam`: 0x004f3a70, 0x004f3d90,
  0x004f3ff0, 0x004b5150, 0x004b5440, a second view drawn through
  0x004f9fa0 / 0x004fa0b0); the cockpit model's node.

## 5. 640x480 assumptions (for the any-resolution work)

- Every piece's default position in the constructors is a 640x480 one (the
  MFD at 530, 370; TARGET at 10, 370; the gauges' images; the zoom window at
  10, 370; the compass); hud.zrd then moves the pieces for the six listed
  modes. A mode not in that list keeps the 640x480 layout.
- The radar's area is 0x140 x 0xf0 (half of 640x480) with its centre at
  (0x37, 0x37); the message lines step 12 pixels from y 0x5f.
- hud.zrd's larger layouts keep some 640x480 values: the GAUGES heat bar,
  meter, text and arrow and the coolant frame are the same at every size (on
  the left edge, as at 640x480).
- Fonts are fixed sizes (Arial 8 for the heat text): text does not scale.

For any resolution: lay the pieces out by anchor (the left and right edges,
the bottom) from the 640x480 block rather than by a table per mode
(fixes.md, Candidates).

## 6. Per-frame data flow (0x0043c390)

With a mech: its controls' update (vt+0x1c on the controls' sub-object),
0x004453a0; the radar's positions (0x0041cdf0 or, with +0x70, 0x00522170 and
the normalise), 0x0043f130 / 0x0043f2d0; the compass heading from the
mech's forward (atan2 of +0x24, -+0x2c) into 0x0043f590; the controls'
vt+0x2c; the gauges: 0x00445450, 0x004455e0 (jump fuel: +0x554 over 1.5, and
the ratio +0x558 / dt - +0x55c), 0x004456f0, 0x00445520 (+0x3d8 > 0, the
mech's vt+0x148: heat); the target's text 0x00441850; the equipment
0x00448320 when shown; the MFD 0x004424b0. Always: the timer's text,
0x00446900 (objectives), 0x00440b00 (TARGET), and in a network game
0x00479ef0.

## 7. Porting order

Small items, each with a differential test:

1. **Construction and data**: the HUD object (0x0043b190), the pieces'
   constructors with their defaults, the damage images (0x0043ae50), and the
   set-up 0x0043bb80 with every hud.zrd reader (0x00442230, 0x00440110,
   0x0043f500, 0x00444dc0 / 0x00444e55, 0x00446120, 0x00448200, the timer).
   Test: the fields on the shipped hud.zrd at every video mode and on
   crafted variants (missing keys, missing blocks, odd values).
2. **The HUD primitives** zTwoD has left: FilledBox / Frame, PolyLine and
   Shape with the transformed points and zRender's line list, the line clip
   helpers, the key-list bars (twod.md, Left).
3. **Compass** (0x0043f590, 0x0043f620) and the **timer**.
4. **Gauges** (0x00444250, 0x00445450..0x004456f0, 0x004452c0): speed, jump
   fuel, heat, coolant.
5. **Weapons list** (0x00445f80.., 0x00446520, 0x00446680, 0x00447c40) and
   the ammo gauge 0x00443d20; the ZAPS static and hold 0x0043be70, 0x0043d07a.
6. **MFD and TARGET** damage displays (0x004423c0, 0x00442540, 0x00441760,
   0x00441490, 0x00441850, 0x00440b00).
7. **Radar** (0x0043d6d0, 0x0043dd00, 0x0043f130, 0x0043f2d0, 0x0043da00).
8. **Reticle and the zoom window** (0x004441a0, 0x00444230, 0x00442ac0..,
   0x004436c0, 0x00442df0).
9. **Objectives, scores, equipment** (0x00446900, 0x00447410, 0x00448320,
   0x00448450).
10. **The frame entries** (0x0043c390, 0x0043c820, 0x0040fac0, 0x0040e4b0,
    the message lines), the reset 0x0043d920, the look 0x00446240, the
    cockpit model (0x00430ad0's second half), and the program's wiring.

## 8. Open

- Which buttons and options show the large radar, the zoom window and the
  equipment list; what +0x70 of the HUD selects for the radar.
- 0x0043be10, 0x00441800 (with the mech's +0x6c) and 0x0041bd80 are not
  read yet.
