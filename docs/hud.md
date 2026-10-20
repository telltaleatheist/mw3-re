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
    view's target): the HUD's frame (section 11: the debug overlays, the
    pieces' updates and draws, the reticle's aim state 0x00443d20), then
    0x0041bd80 aims at the reticle's point +0x1d48 unless the mech's +0x638.
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

- **Radar** (0xdc0 bytes): a FilledBox (green) with the view cone's two
  edges and a red line (HUD lines 0x005a6ae0), two texts (mech_string; the
  range `%.1fkm`), a light (`hud_light_green/yellow/red`, at 11, 11) and two
  polylines. Two sizes, moved to in steps of 10 pixels (0x0043da50): small,
  radius 50 at (55, 55); zoomed, radius 180 at (320, 240). The cone is the
  view's 60 degrees (0x0043f0c0), turned round on a flipped map (0x0043f130).
- **Compass** (0x678): the `hud_compass` tape (245 x 19) scrolled under
  `hud_compass_frame` (245 x 11) by the heading (0x0043f590).
- **Reticle** (0x95c): `ret_on`, `ret_off`, `ret_over`, `ret_cant` (aimed,
  idle, over a target, out of range; `ret_on` is fetched twice, +0x914 and
  +0x920); the zoom window (0x00442ac0): the `zoomcam` camera (5 degrees,
  zoom 10, the player camera's clip), its light `hud_zoomoff/on/over/cant`,
  a text, and a box (or a circle when +0x16c), sized by its scale +0x170 and
  kept 2 pixels inside the display (0x004438f0, 0x0043fa40).
- **Gauges** (0x168c): `hud_speed_frame` (575, 256), `hud_coolant_frame` (8,
  290), `hud_fuel_frame` (527, 296), `hud_heat_bar` (18, 163),
  `hud_heat_meter` (16, 163), the heat arrow (`lab_toggler_green`, 10, 150),
  the coolant and heat texts (Arial 8, messages 0x1d5 / 0x1d6, green with a
  dark green shadow), the speed text, and three bars (four-point polygons,
  0x005a6bd0; coolant blue, fuel green) inside the frames.
- **MFD and TARGET** (0xcd8, 0x2414; one base, 0x00442060 and 0x00441da0):
  the damage silhouette in five colours per part (the player's own, or a
  target's mirrored as seen from the front, 0x00447680 / 0x00447820), a 3D
  view of the mech (0x00442620: a zoom view on a see-through panel, boxed),
  the nav list (0x0047e840) and a title (message 0xdbe, 0xfbb in a
  multiplayer game). TARGET adds two Shapes, three lines, a Frame, twelve
  arrows (`hud_enarrow`, `hud_allyarrow`, `hud_othererrow` left, right, up,
  down; "othererrow" as spelled), a marker and three texts above it
  (message 0x22a; `%s: %s`, `%.2fkm`, ` (%dm)`, 0x00441850). Their modes
  (+0x38) survive in [0x00605988] / [0x0060598c]; at first 1 in Direct3D, 3
  in software (0x00441d90).
- **Weapons list** (0x11444): sixteen lines (0x00445760: a Slider, the
  recycle bar, with the name and two texts, `wepstring_red/green/yellow/grey`,
  a light `hud_light_*` and a mark), `hud_weapon_select`, `hud_weapon_chain`,
  the ammo (`%3d`, 0x00445dc0), over a see-through panel.
- **Objectives** (0xaac): a TextBox of 25 lines over a panel and the scores
  box 260 pixels to its right (0x00446870), fonts
  `rtobjective_title/incomplete/complete`, `rt_mp_scores`; hidden at first.
- **Timer**: the mission clock (`%02d:%02d:%02d`; the multiplayer clock
  [0x00609920] in a network game, else the game time [0x0060991c]).
- **Equipment** (0xa548): sixteen lines (0x00447c40: the name, a value
  right-aligned at column 110, a light `hud_light_red/yellow/green`; fonts
  `wepstring_*`), 10 pixels apart under (530, 30), over a panel.

The see-through panels (0x0043a7e0, 0x108 bytes) are an image of one colour,
green (0, 64, 0), at 60% opacity in Direct3D and opaque in software
(0x0043a7c0), with the four corners Direct3D draws it by (depth 0.9).

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

The set-up keeps the mech (+0x10), loads hud.zrd (0x00556740) and picks the
block by the video mode (0x0040d9d0: 3 and 5 V640x480, 6 V800x600, 7
V1024x768, 8 V1152x864, 9 V1280x1024, 10 V1600x1200; any other mode keeps
+0x22e3c as it is: 0 after an earlier set-up, which clears it at its end, but
on the first set-up whatever `new` left there, as the constructor does not
clear it; FIX (bug), section 9). With a block: MFD and TARGET (both `size`
and `position` needed; 0x00442230 moves the silhouette, the 3D view, its
panel, box and nav list, and the title; 0x00440110 also TARGET's three
texts), TIMER's `position`. Then, block or not:

- 0x0043be10: +0x22e08..+0x22e34 cleared, the radar's fade 0 (0x0043dc70:
  the box's and lines' +0x54 / +0x4c).
- The reticle, MFD and TARGET get the HUD (0x00444250 with the lock tone's
  channels 0 and 12 stopped, 0x0041e7a0; 0x004423c0 gives the silhouette the
  mech and the nav list the mech, 0x0047eba0; 0x00441760 also clears
  TARGET's texts), TARGET the mech's target slot (+0x6c, 0x00441800).
- The radar (0x0043d6d0): amber (255, 226, 4) instead of green when the
  mech's +0xbc bit 2 is set (0x0043d630); the zoomed size from the display:
  centred, radius 180 at 480 lines (h x 180 / 480); the view cone from the
  player camera's field of view.
- EQUIPMENT (0x00448200, with a mech): `width`, then `position`; the lines
  hang below the point unless the weapons (manager +0x24) and the listed
  parts' equipment (mech +0x3ac by index 0, 12, 3, 8, 14, 6, 7, 4; only the
  positive counts) come to 17 or more.
- WEAPONS (0x00446120, with a mech): `width`, `position`, the weapon manager
  (+0xd4), every line emptied (0x00445d10), the lines filled from the mech
  (0x00446520).
- COMPASS (0x0043f500): `tape` and `frame`.
- GAUGES (0x00444dc0): `speed_frame`, `fuel_frame`, `heat_bar`,
  `heat_meter`, `heat_text` (the text of message 0x1d6), `heat_arrow` (the
  toggler), `coolant_frame`; then the texts, bars and heat marks laid out
  again from the frames, as the constructor did.

Last: the HUD shown when the mech lives (mech vt+0xa0; 0x0043baf0 shows or
hides the radar, 0x0043db70 / 0x0043dc00), the zoom window round when the
mech's 0x0041df10 is 2 (0x00442df0), hud.zrd released (+0x22e38 gets
0x00556820's answer, +0x22e3c 0).

## 4. The primitives it uses

- zTwoD (twod.md, ported): Object2D groups (0x0059fda0), Image (0x0059b970,
  named 0x0059b830), FontText and TextBase (0x005a00c0 with a fonts.zrd font
  0x00517680, 0x005a0660; text by message id 0x00409bd0), Text (0x005a15a0),
  TextBox (0x005a18f0), Slider (0x0059e0f0), PolyLine (0x005a3830), the
  Layer's add 0x0059fbe0.
- zTwoD's HUD side, built and set up but not drawn yet (twod.md): Frame and
  FilledBox (0x005a39f0, 0x005a3a60, 0x005a3b60), Shape (0x005a3170,
  0x005a34c0..0x005a3530), the HUD lines (0x005a67e0, 0x005a6880, 0x005a6ae0),
  the polygon bars (0x005a6bd0, 0x005a6c30). Not ported: their draws,
  Shape's transformed points 0x005a3580, zRender's 2D line list 0x00562cb0
  and boxes 0x005630c0 / 0x00563440, the line clip helpers
  0x0059b2f0..0x0059b7e0.
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

1. **Construction and data**:
   - 1b, done: the HUD object (0x0043b190) and every piece's constructor
     with their defaults and helpers, on the ported zTwoD (`mech3/hud`,
     section 9).
   - 1a, done: the set-up 0x0043bb80 with every hud.zrd reader and the
     pieces' attach calls (section 3), on the shipped hud.zrd at every video
     mode and on crafted variants.
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

- What the mech's +0xbc bit 2 is (the radar turns amber with it,
  0x0043d630) and what 0x0041df10 answers (2 makes the zoom window round).

- Which buttons and options show the large radar, the zoom window and the
  equipment list; what +0x70 of the HUD selects for the radar.
- 0x0043be10, 0x00441800 (with the mech's +0x6c) and 0x0041bd80 are not
  read yet.

## 9. As ported (`mech3/hud`)

`mech3::Hud` is the 0x22e40-byte object, its pieces members in the
original's order (constructed in it, so the recorded calls come in the same
order): `HudRadar`, `HudCompass`, `HudReticle` (with `HudZoomWindow`),
`HudGauges` (`HudBar`), `HudMfd` (`HudDamageView`, `HudMfdView`),
`HudTarget`, `HudWeapons` (`HudWeaponLine`), `HudObjectives`, the timer and
`HudEquipment` (`HudEquipLine`), with the shared `HudPanel`, `HudText`
(a FontText in mech_string, vtable 0x005bc598) and `HudLight`. The lights,
fonts and damage silhouettes every HUD shares are `weapon_set()`,
`equipment_set()` and `damage_set()`. The constructors and the set-up
(`Hud::setup`, the pieces' `read_layout`, `place` and `attach`) are ported;
the frame and the pieces' updates too (section 11).

The engine sits behind `HudHooks`: hud.zrd's load, the video mode, the
display's size, the mech's fields the set-up reads (vt+0xa0, 0x0041df10,
+0xbc, +0xd4 and its +0x24, +0x3ac, the target slot +0x6c), the HUD's
sound channels, the nav list's attach and place, the weapon list's refresh
0x00446520, the mission entry's flip, zRender's nodes for the zoom views (window and camera nodes,
names, size, place, zoom, field of view, clip, active), the player camera's
clip, the options' display and window rects, the nav list 0x0047e840, the
pilot's game, Mech3Msg.dll's messages and the saved MFD modes.

The zoom views (0x0043f660: the MFD's and TARGET's mech, the reticle's zoom)
are built as structs with the original's fields (`HudZView`: the camera node,
its window, the world, the render path; `HudViewWindow`: the window node,
its area and target and their ratio) and their engine calls recorded.

FIX (bug), behind `zfix::options().bugs`: the constructor clears +0x22e3c,
which the original leaves as `new` left it and the set-up reads for a video
mode without a hud.zrd block (section 3; `fixes.md`).

Test: `test_hud.py` (`tests/probe_hud.cpp`) builds the HUD in the emulator
and in the port (the constructor runs the set-up), with random render paths,
display and window rects, saved modes, message tables, mission flips, video
modes (listed and not), hud.zrd shipped, crafted (blocks, pieces and keys
dropped or renamed, odd values, MFD and TARGET resized) or missing, and a
fake mech or none, and compares the call traces and every field the
constructor and set-up set, on 100 runs. `test_fixes.cpp`
fix_hud_block_cleared checks the fix. Breaking the set-up: 22 of 25
caught (the misses: the MFD's two keys read in the other order, an amber that
packs to the same 565 pixel, steps computed after the move that makes them
0, and TARGET's clear that set_target repeats). Breaking the constructors: 33
of 41 mutations caught; the eight misses change nothing a constructor does (a
wrap constant that rounds to the same float, flags already clear, the red
look never taken, red 1 and 0 packing to the same 565 pixel, steps already
positive, a text already empty, points set in order, and +0x22e3c, which only
the set-up reads: item 1a's test).

### Left

- The set-up with a target in the mech's slot (TARGET's 0x00441800 path).
- The zoom views' drawing and camera work, with the render work: the zView
  and window creation (0x004b5150, 0x004b5440) and the camera setters stay
  recorded hooks until then (lead, 2026-10-20).
- The nav list 0x0047e840 (its own 0x2fb8-byte object, mech3's mission
  side) stays a recorded hook.
- The paths with a target or a mech: TARGET's 0x00441800 / 0x00441780 /
  0x00441660 with a target, the radar's zoom sound (0x0043da50 with a
  source, 0x0043da00), the weapon line's refresh 0x00445dc0 with a weapon,
  the equipment's and weapons' readers 0x00448200 / 0x00446120 with a HUD.
- The show_for overrides 0x0043db70 (radar), 0x004438d0 (zoom window),
  0x00441560 (TARGET), and the objectives' and weapons' parts of theirs
  (0x00446ad0, 0x00446520).
- TextBox's own message-id setters 0x005a1cd0 / 0x005a1cf0 (FontText's run).
- The weapon group calls of the lock (section 12): 0x004b1bd0, 0x004b1c70,
  0x004b1cd0 stay recorded hooks: mac 4, the planner (they sit in the AI
  weapon planner's range 0x004b1170..0x004b1d7f).

## 10. The aim (the reticle's world point)

Every weapon aims at one world point the HUD works out each frame.

1. **Placing** (the mech's 0x0041ce60, from Mech::update for the view's
   target): the controls' vtable +0x14 gives where the player looks, (x, y)
   in -1..1 of the display's half size (PlayerControls 0x00428800: its
   +0x140; every other controls class answers none, and in view 0 the
   controls keep it at 0, so the reticle stays centred). 0x004445f0 puts the
   reticle there (display centre + offset x half size), then:
2. **The ray** (0x00444350): the reticle's point, through 0x0040d9e0 (halved
   into the render target when pixels are doubled, VMode 3), unprojected
   into the world at the player camera's near and far planes (0x004b51f0 to
   set the camera's matrices, 0x00523930 with 1/z = 1/near and 1/far). The
   world's segment query (0x004eed90) runs between them with the mech's own
   node left out (0x004eb6d0 off, then on). One hit counts unless its
   material's game soil is 1 (water); with several, the nearest by squared
   distance from the near point among the non-water ones. A failed query or
   no hit: the far point.
3. **Kept** at reticle +0x8fc (the aim point, = HUD +0x1d48), +0x924 its
   distance from the near point (the far plane's when nothing was hit), and
   +0x928 the hit's record (0x00444710; 0x004446a0 builds one facing up at
   the far point). The zoom window follows the reticle when shown or scaled
   (0x00443660).
4. **Handed over** (the mech's 0x0041cd40, its vtable +0xe0, from the Core
   frame's 0x0040fac0): the HUD's frame (+0x00, 0x0043c820, not ported), then
   0x0041bd80 aims the mech at the point (0x0041bc20) unless +0x638.

In the program (`mech3/app_hud`, `mech_hud`): the player's HUD is made on
its mech data when first asked (0x00430aa0) and set up at the view attach
(0x0041ce30); `update_calls().player_frame` runs 0x0041ce60; the Core frame's
HUD update runs 0x0041cd40 before the rest of 0x0040fac0 (still unported);
the hooks use zClass's segment query, the camera's clip and field of view,
zModel's camera set-up and zMath's unproject (0x00523930, ported). Tests:
`test_hud.py` places the reticle on precomputed rays and hit lists
(missing, failing, water and several hits) and compares every field;
mw3tests `mission_c1m1_player_aims_with_the_reticle_headless` pitches the
torso until the reticle's ray ends on the Owens 80 m away and checks the
weapons, with no other aim, take its armor down. Breaking the aim: 10 of 10
caught.


## 11. The frame (0x0043c820) and the pieces' updates

The HUD's vtable +0x00, run by 0x0041cd40 each frame for the view's target,
only with a mech:

1. The **debug overlays**, while the debug text 0x005fc378 shows (toggled by
   0x0040dc34 and [0x005f4d6c]; the test reads its flags [0x005fc384]): the
   mech's gait, speeds and the like into five global texts
   (0x005fe918..0x005ffbd8, 0x4b0 apart). Hooked (`hud_debug`).
2. zTwoD's **Layer update** (0x0059fd20, only while the layer is active):
   each piece the constructor added (compass, gauges, TARGET, MFD, weapons,
   equipment) runs its +0x24.
3. The **timer**, the **objectives** and the **radar** (0x0043dd00, hooked
   until item 7), which are not in the layer's list.
4. While the reticle's state is still 5 (nothing set it this frame):
   whether its point lies over TARGET's shape or circle with a live target
   (0x00440a30) into the **aim state** (0x00443d20: the weapons' range and
   lock, their sounds, the reticle's image by state 0 on, 1 off, 2 over,
   3 can't). Both hooked; item 5's ammo gauge in the porting order.
5. The **reticle** (0x004441a0), then its state back to 5; the network's
   part (0x00479ed0) in a network game ([0x00609980]).

Each piece's update draws only while shown (kHidden clear), except the MFD
(0x00442540) and the damage silhouette (0x00447b50), which leave it to their
children:

| Piece | Update | Draws |
|---|---|---|
| Compass | 0x0043f620 | the frame, then the tape |
| Gauges | 0x004452c0 | the speed, coolant and heat texts; the speed bar and frame, the fuel bar and frame, the coolant bar and frame; the heat bar, meter and toggler |
| MFD | 0x00442540 | by its mode (vt+0x80): 1 its 3D view's box (0x00442880), 2 the damage silhouette, 3 the nav list; the title only with [0x00609994], or in a multiplayer game while [0x00605f88] is clear |
| TARGET | 0x00441490 | the three lines, the mode's piece as the MFD, three of the four texts, the marker, the circle; no title. Its mode (0x004413a0) is none without a live target and only the 3D view for a target that is not a mech |
| Damage silhouette | 0x00447b50 | the background, the eight parts (each a flashing image, 0x005a6d10) |
| Weapons | 0x00446680 | with a weapon manager: the lines' looks (0x00446540, hooked) while shown, the panel, the 16 lines (0x00445c10), the bracket |
| Weapon line | 0x00445c10 | when changed, the second text's and the name's rendered images become the bar's fill and empty; the bar, the count (unless -1), the light, the mark |
| Equipment | 0x00448450 | the panel, the lines up to the first hidden one (0x00448030: name, light, value) |
| Objectives | 0x00447410 | the panel, the text, the scores |
| Panel | 0x0043ad00 | in software or when opaque; a see-through panel in Direct3D draws in the 3D pass |
| Light | 0x004486e0 | with an interval, the image steps each interval |
| Zoom window | 0x00442e30 | its scale grows at 4.8 a second while shown and shrinks at 7 while hidden; at full size its light shows the reticle's state, then its text; then the circle or the box |
| Reticle | 0x004441a0 | the zoom window; nothing more once the HUD is off with its mech dead; itself only while the zoom window is hidden; +0x954 counts down to 0 |

A flashing image (zTwoD's update 0x005a6d10): mode 1 blinks
(drawn on phase 1), mode 2 swaps its image with a second one each half
period (+0xbc, 0.35 s). Mode 0 falls through into mode 1's code (a missing
`break`): it draws, then draws again on phase 1. Ported as written.

**The Direct3D pass** (0x0043c6b0, the HUD's own, from the mech's vt+0xe4
0x0041cd90 for the view's target, in the Core frame before the 2D layer's
lock), with a mech and in Direct3D:

1. With the HUD on: TARGET's and the MFD's 3D view rects (in mode 1; for
   TARGET through 0x004425d0) given to the dirty rects (0x00576ce0).
2. One scene (0x00579af0 / 0x00579b10) of see-through quads (0x0043ad70: the
   panel's four corners into the TL buffer from the last one back, then the
   flat transparent draw 0x0057bf80 with the panel's colour and alpha; an
   opaque panel draws its image instead): with the HUD on, TARGET's and the
   MFD's view panels in mode 1, the weapons' while shown, the equipment's
   while it and the weapons show; always the message panel (0x0040fb30 on
   [0x00603d7c]) and the objectives' while shown; then the transparent
   flush ([0x0072f3e4]).
3. With the HUD on, TARGET's and the MFD's +0x28: in mode 1 the 3D view
   (TARGET: the target's vt+0x68(1), 0x004428b0 on its view and the
   target's node, vt+0x68(0); the MFD: its view's zView +4, 0x00442870);
   in mode 2 the damage view's +0x28, which is empty.

A panel's own +0x28 (0x0043ad20) is the same quad in a scene of its own;
nothing in the HUD calls it. Ported (`Hud::draw3d`, `HudPanel::draw3d` and
`draw_quad`, the MFD's and TARGET's `draw3d`); the program runs it at the
view target's vt+0xe4 with the scene, the quad and the flush on the zVideo
ports. Hooked: the dirty rects, the message panel and the 3D views'
rendering (0x004428b0, 0x00442870: the render work). Test: test_hud.py's op
17 runs the pass between frames with the render calls logged (100/100).
Breaking it: 9 of 10 caught; the miss (the quad's opaque test at 0xfe) is
equivalent, set_alpha (0x0043a9d0) making 0xfe and up opaque.

In the program, `hud_handoff` runs the frame (`Hud::update`) inside the Core
frame's 2D layer, and the Direct3D pass (`Hud::draw3d`) at the view
target's vt+0xe4; the hooks the program does not implement answer 0 (no
debug overlay, no radar, the aim state left alone, no nav list).

Test: `test_hud.py` runs, after the build and the aim, a random sequence of
frames (dt 0 to 2.5 s) with changes between them (the HUD on or off, the
zoom window shown or hidden and round, the reticle's state, the zoom light's
interval, a damage part's flash, the MFD's mode, any piece shown or hidden),
the unported pieces stubbed (the aim state sets a state 0..3, as in play),
zRender's 2D draws logged, and compares every draw and the HUD's fields
after, on 100 runs (126,437 calls). Ops 13 to 16 (every child dirty or
shown, texts and images set) are there because most children are managed
(drawn only when dirty) and empty until their unported setters run. Breaking
it: 21 of 23 caught; the misses are equivalent here: the scores box has no
lines (its setter 0x00446ad0 is not ported) and the MFD view's box, a managed
PolyLine, is never made dirty in the test.

## 12. The missile lock and the reticle's image (0x00443d20)

Run from the HUD's frame while the reticle's state is still 5 (once a
frame), with whether the reticle's point lies over TARGET's shape or circle
with a live target in range (0x00440a30, hooked). Reads:

- the weapon manager's selected group (mech +0xd4): its longest range
  (0x004b1bd0), whether any weapon in it locks (0x004b1c70) and 0x004b1cd0
  (both needed); not ported, mac 4 ports them with the AI weapon planner;
- TARGET's slot's contact: its mech (0x004e6ff0; kind 7 at +0x684 +0xc never
  locks) and its distance (0x004e7030, zai_target);
- the aim's distance (+0x924) against the same range.

"Near": the contact or the aim point within the group's range. A good frame:
over the target, a group that locks, near, a contact, not kind 7.

| +0x950 | Next | On entering |
|---|---|---|
| 0 none, 4 lost | 1 on a good frame, else 0 | 4: both lock tones stopped (HUD sounds 0 and 0xc) |
| 1 acquiring | 2 on a good frame, else 4 | tone 0xc unless playing; +0x954 = 0.6 s, 1.2 s when the target mech's +0x3cc is above 0 |
| 2 waiting | 3 once +0x954 reaches 0, 4 on a bad frame | |
| 3 locked | stays on good frames or while +0x954 is above 0, else 4 | on a good frame +0x954 = 3.5 s; tone 0xc stopped, tone 0 unless playing |

Not near and not locked: 4. The reticle's update counts +0x954 down. While
locked the weapon manager's +0x88 is set and the reticle shows ret_on
(state 1); otherwise ret_off (0), then ret_over (2) while waiting, or by the
aimed hit's node (+0x94c): its object (0x0040e920, asked twice) with its +8
byte and the aim near: over; no object and the node's +0xbc above 1: over;
nothing near: ret_cant (3). With the image kept (+0x90c) the state still
changes but the image does not.

Ported (`HudReticle::aim_state`); the outside through HudHooks. In the
program (`app_hud`) the hooks are the player's mech, its WeaponManager with
mac 4's group getters (`weapon_planner.h`), its target contact (TARGET's
slot is the mech's `target`, +0x6c) and the contact's mech and distance
(zai_target); "kind 7" is the target Pilot's roster index, the Elemental,
and +0x3cc its ECM count. "Over TARGET's box" (0x00440a30) waits for
TARGET's tracking (0x00440b00), so the program never locks yet; the aimed
node's object (0x0040e920) is not ported either (no "over" image from it).
mw3tests `mission_c1m1_hud_weapon_lines_and_lock_headless` forces "over"
(`program_hud_force_over`) with the Owens as the target and the LRM group:
acquiring, waiting, locked 0.6 s later.
Test: test_hud.py runs the original 0x00443d20 in the frames, its outside
stubbed (the range and distance on the x87 stack), the hits' nodes real
blocks with +0xbc; the weapon manager's +0x88 compared after. Breaking it:
12 of 12 caught.

## 13. The weapon lines (0x00446520, 0x00446240, 0x00446540, 0x00445dc0)

Refreshed (0x00446520: layout, then looks) when the list is shown
(0x00446200), when its reader gives it the mech (0x00446120), and by the
weapons and the camera's look from outside (hud.md section 1).

- **Layout** (0x00446240): the manager's five groups in order fill the
  sixteen lines (0x00445d10 per weapon); the current group's lines get the
  bracket (the PolyLine: 11 pixels either side of the list's x, from the
  group's first line to its last, 10 pixels a line); unused lines hidden;
  the panel moved to the list and sized to the column by 10 per line; with a
  HUD, the equipment list placed 6 pixels below, and the reticle reset when
  the group cannot lock.
- **A weapon in a line** (0x00445d10): the weapon's name into the name and
  the second text; both look flags flipped and set back (0x00445a60), so the
  fonts are set again; then the refresh.
- **Looks** (0x00446540), each shown line: the select mark for the current
  weapon's line in the current group (single fire), every line of the
  current group (group fire), or select for the current one and chain for
  the rest (chain fire); none outside the current group. Then the refresh;
  the list keeps refreshing every frame while any line recycles. +0x1143c:
  the weapon after the current one (else the first line's).
- **Refresh** (0x00445dc0): the weapon's vt+8, then the bar at its recycle
  fraction (vt+0x34, clamped to 0..1; +0x111b when it drops from full), the
  light by damage (0x004b3010: green, yellow over 0.5, red over 0.8), the
  count its ammo (vt+0x14, "%3d"), the look red when it has ammo and is
  enabled (+0x74) and not destroyed, grey when destroyed (damage 1).

Ported (`HudWeapons::refresh_all/layout/looks`, `HudWeaponLine::set_weapon/
refresh_line`); the manager and weapons read through HudHooks (weapon.h's
WeaponManager and Weapon, read-only). The HUD's handles are pointers
(`void*`): the program passes its own WeaponManager*, Weapon* and Mech*; the
probe's are the emulator's 32-bit addresses, mapped back by `id()`. In c1m1
the Mad Cat's ten weapons show, each named by its definition's DESC, the
current group bracketed (mw3tests above).
Test: test_hud.py gives the fake mech a manager with up to five groups of
fake weapons (16 lines at most) and runs the original's refresh: 100/100.
Breaking it: 11 of 13 caught; the misses are equivalent here (the unused
lines are already hidden by the reader's set_weapon(nullptr), and the bar's
set_value clamps a negative fraction to 0 itself).

## 14. TARGET's tracking: the target on the screen

The pieces, ported in order (the lead's plan, 2026-10-20):

1. **A world point on the screen** (0x0053cf10, `zmod::project_point`,
   zmodel/gmod_screen): through the view (on zMath's stack: push, load_view,
   pop), projected (0x005234f0) and clamped to the point rect (0x0074031c);
   the outcode: 1 left, 2 right, 4 above, 8 below. A point behind the near
   plane (0x00740304) gets 8, y on the bottom edge and x spread across the
   screen (-5000..5000 over the clip's width from its left; the clip's right
   0x00740314). FIX (bug): there the original ORs the low bits of the input
   point's address (an unset local when the top is the identity) into the
   outcode; the port returns 8. And its way back with pixels doubled
   (0x0040da30, `render_to_screen` in options: x doubled, y from the
   render's top doubled onto the window's, clamped to the window). Test:
   `test_project_point.py`, 3000/3000 each (586 behind, 575 at the near
   plane); breaks 9/9.
2. **The tracking** (0x00440b00, `HudTarget::track`, each frame with a live
   target, from the HUD's data update 0x0043c390): TARGET's camera put
   behind the target (the target's node position less 1.2 x the contact's
   size, 0x004e7070, along the contact's direction, 0x004f3870) and turned
   from the player toward it (`zmth::direction_angles` 0x00523ac0,
   0x004f36b0). The target's position projected (part 1), through the zoom
   window's view first when it shows: on the zoom window it is used there;
   off it, through the player's view, kept on screen only when it lies past
   the zoom window's side its outcode names (else nothing). The lines,
   marker, circle and shape hidden, then by the outcode:
   - **On screen**: with a targeting computer (equipment kind 14, mech
     +0x3e4) the **lead circle** (radius 6, the Frame) at the lead point
     (0x0041d940, `lead_point` in mech_hud: where to aim the selected
     group's current weapon so its shot, at the definition's speed or
     instantly for beams and instant weapons, meets the target moving at
     its velocity relative to the mech; from the mech's own aim point when
     it has one), clamped 7 pixels inside the display; green, or with ECM
     on the target yellow (255, 226, 4) and thrown up to 4 pixels each way
     by rand. Then the **triangle** (0x00440510): three lines round the
     target's projected position, its height the target's on screen (its
     centre against its centre raised by its size; a new target's pulse
     +0x240c adds 3.33 x that of the screen's height and runs down), red for
     an enemy, blue for an ally (the mech's +0xc8 / +0xc4 masks against the
     target's +0xc0 team), else green; in a round zoom window each side cut
     to its circle (0x00440290, zTwoD's 0x005a3580 in floats), else clipped
     to its rect or the window's ([0x005e39d8]); TARGET's shape at its
     centre scaled 1.155 x the height (Shape's 0x005a3530 is a float scale:
     zTwoD's `set_scale`).
   - **Off an edge** (1 left, 2 right, 4 above, 8 below), projected through
     the player's view: the team's arrow (enemy, ally, other: left, right,
     up, down) on that edge, kept on screen along it. Corners (3, 5, 6, 7)
     and the rest show nothing.
   - Then the MFD mode's tail for the target: 2 the damage view (0x00447a50),
     3 the nav list (0x0047ef10), both hooked.
   FIX (bug): the lead point with no group selected (the original reads
   through a null group, a crash; `fix_lead_point_without_a_group`). Test:
   test_hud.py's ops 18 (the tracking; projections answered from a table,
   rand an MSVCRT LCG on both sides) and 19 (the lead point, on a second
   fake mech with a contact and an aim point): 100/100. Breaking it: 15 of
   21 caught; the six misses (the targeting computer's kind, the lead
   circle's ECM colour, jitter and clamp, the new target's pulse, the round
   clip's end test at equality) need a target in the player mech's own slot,
   which the set-up only takes once set_target (0x00441800) is ported:
   part 3 reruns them.
3. **A target given to TARGET** (0x00441800, `set_target`, with the slot the
   set-up hands it: the mech's own +0x6c): with a live contact TARGET shows
   with its three lines and texts (0x00441560) and turns on (0x00441780: the
   triangle's 0.3 s pulse, the reticle reset); its texts (0x00441660): the
   target's mech into the damage view and the nav list, the mode the saved
   one (else 1 in Direct3D, 3 in software), the first text the target's
   name (vt+0xa4); a target whose vt+0xa8 answers (dead) gets mode 0 and no
   mech. **Over the target** (0x00440a30, `over`, the lock's "over"): a
   NARCed target (its mech's +0x570 above 0) always; else the reticle's
   point inside TARGET's triangle shape (zTwoD 0x005a3440: below its apex,
   above its base, within 0.577 of the depth either side, after the shape's
   points are worked out, 0x005a3270, also on its update 0x005a3550) or
   inside the lead circle (0x005a3b30). Test: test_hud.py with a target in
   the fake mech's own slot half the time, the original 0x00440a30 in the
   frames, contact mechs with NARC timers: 100/100. Breaking parts 2 and 3
   together: of the 17 left from part 2 and new here, 11 caught; the misses:
   the ECM colour's blue 4 (the same 565 pixel as 0), TARGET's shape never
   drawn by TARGET's update (its transform matters only to the hit test),
   the circle's and the shape's edges only at equality, the round clip's
   end at equality, and the dead target's mode (rarely reached).
