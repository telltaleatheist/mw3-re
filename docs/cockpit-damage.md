# The cockpit's cracks and screens

Mech3.exe 1.2, playtest #19 (mac 2, 2026-10-21). The cockpit model a mech
type has (`mech_<name>_hud.flt` in mechlib, made by 0x00430ad0's second half,
mech-build.md) carries two things that change while you play: cracks and
dings on the glass, and small animated screens.

## Cracks and dings

The cockpit tree, Mad Cat and Owens alike:

```
mech_<name>_hud.flt
  grill
    dbase            (frame, panels; the Owens' cpit_ania / cpit_anib under it)
    g<n>             (the window frame and glass)
      crack01..03    textured cpit_crack01, one quad each
      hcrack01..03   pieces of the whole pane (cpit_window) the cracks replace
      ding01..03     textured cpit_ding01
```

In the file every one of them is active (node flags 0x0308031c). What hides
the marks is the mission's animation `cpit_effects_explosion` (anim.zbd,
node `grill`, ON_CALL, AUTO_RESET). Its reset state sets damage01..03,
crack01..03 and ding01..03 inactive. Its sequence `frequency_check` calls
`damage`, then `effects`. `damage` is a chain of IF RANDOM_WEIGHT tests:
0.16, 0.2, 0.25, 0.33, 0.5, then an ELSE. Each test draws a new number from
the animation's random table, so each of the six marks comes up about one
time in six (0.16, 0.84 x 0.2, ...). A crack shows itself, hides its whole
pane (hcrack0n) and plays `mech_cockpit_crack` at its node. A ding does the
same without touching the pane. `cpit_effects_reset` is the same reset on
its own. No code in the exe names it, and nothing seen calls it.

At load the animation binds to the world's own copy, `dummy_hud.flt` in
gamez.zbd, which has a `grill` with the same children. Two calls move it
onto the player's cockpit:

- **The cockpit is made** (0x00430bf2): `0x00508460(ECX = MechData +0x200,
  EDX = the cockpit's node)`. 0x00508460 hands EDX through 0x00508490 to the
  reset 0x00508310, which binds the def to that node first (0x00509eb0:
  the def's node becomes it, the objects are found again under it). The
  reset sequence then hides this cockpit's marks.
- **The head is hit** (0x00460bef, in TakeDamage 0x00460970, the player's
  part with node id 6): `0x00508da0(ECX = the explosion, EDX = the
  cockpit's node; 0, 0, 0)` starts it there, and one mark appears.

The same restart with a node is used by the respawn (0x0046ded3, still a
hook), two AI calls (0x004a6a60, 0x004a7b12) and the anim log's replay
(0x0050cb8a). The rest pass EDX = 0: the wait tick (0x005085d8), the
reset event (0x00506e86), the weapons (0x005830ef, 0x00584f59, 0x00584f6e)
and the reset of every animation (0x0050ad60, at 0x0050adac).

**The port's mistake (playtest #19).** `zeff::anim_restart` and
`anim_reset_logged` took only the def, so the node in EDX was lost. The
cockpit's marks kept their file state (all shown from the start), and the
reset ran on the world's dummy instead. The program also left the head hit's
call unwired. Both now take the node.

## The screens (cycled textures)

Most cockpits (all but the Mad Cat's among those checked: Annihilator,
Blackhawk, Bushwacker, Cauldron, Champion, Firefly, Orion, Owens, Puma,
Shadow Cat, Strider, Sunder, Thor, Vulture) have nodes `cpit_ania`,
`cpit_anib` and/or `cpit_anic`. At the launch (0x00491150) each one found
runs `support\cpit_cycle<A..C>.gw` with the node as the script's current
node, and then the textures load (0x005190d0). The scripts:

- `CycleTextureSetOn n` (0x0053bc30): the model's first polygon's material
  becomes cycled with n slots of the default texture (0x005375d0).
- `CycleTextureSetMap name` (0x00518c60, then 0x0053bd10): the texture is
  added to the table and put in the next slot (0x00537680).
- `CycleTextureSetLooping` (0x0053bcd0, 0x005377a0) and
  `CycleTextureSetSpeed` (0x0053bcf0, 0x005377e0).

The draw steps a cycled material once a frame, when a polygon using it is
drawn (0x005376c0): the slot is the index truncated, modulo the count. The
index grows by speed x dt; without looping it is held at the last slot, and
below 0 it is lifted by whole counts.

**The port's gap.** The interp's engine had no `get_model` (0x004eb420),
`texture_add` or model-cycle calls, so every `CycleTextureSetOn` found no
model ("ERROR no GFX data for cycled texture"). The world's own cycles
(c1's cycwave01, gullfly, cityfount) were silent the same way. The launch's
texture load was also unwired. Now:

- `zmod::cycle_set_on` / `_map` / `_looping` / `_speed` (gmod_const) are the
  four wrappers, and the interp's defaults use them;
- `get_model` reads node +0x3c;
- the program's LaunchHooks load group 0 after the scripts. Without that,
  the textures the scripts add have no surface, and the draw crashes on
  them.

## Quirks

- **A cycle made again keeps its frame.** 0x005375d0 reallocs the cycle and
  sets every field but +0x04 (the frame last stepped). A cycle made again
  keeps it; a new one starts with realloc's leftovers (0 in the port). The
  port had zeroed it; it keeps it now (test_cycle caught 104 of 400
  sequences).
- **A cycle of 0 slots** divides by zero when it is stepped or set to a
  frame (0x005376c0, 0x0053bc90). No shipped script makes one.
- **Material flag 4 without a cycle** crashes the map (0x00537680 reads the
  null cycle). No file has it (mechlib's materials are 0x10 and 0x11).
- **0x0053bc30 with no material** on the first polygon clears flag 0x400 at
  address 0, a crash. FIX (bug): nothing is done.
- **0x004eb420 with a null out pointer** (the interp's scroll-off 0x00596370)
  writes to address 0. FIX (bug): the write is skipped.

## Checks

- `tests/differential/test_cycle.py`: 400 random sequences of the four
  wrappers, the frame step, the reset 0x0053bc10 and the frame set
  0x0053bc90 on one model, the x87 at 24 bits (a null model, the default
  material, looping and held cycles among them). The return values, the
  material and its cycle match after every call. Breaks caught: the map a
  no-op, one slot more, the frame zeroed again, the speed wrapper setting
  the loop.
- `tests/differential/test_anim_run.py`: the restart now passes a node in
  EDX, on both sides, for 30% of the random resets. On c1 it also moves
  `cpit_effects_explosion` onto the world's `dummy_hud.flt` and compares the
  whole state. Break caught: the node dropped (the first difference at
  def 905, `cpit_effects_explosion`).
- `tests/differential/test_damage.py`: the head hit's 0x00508da0 now logs
  ECX and EDX, with distinct values for the explosion and the cockpit's
  node.
- `mission_c1m1_cockpit_cracks_on_a_head_hit_headless`: in c1m1's Mad Cat,
  no mark is shown at frame 20 and both panes are; a hit on the head shows
  one. Breaks: the restart without the node (6 shown at the start), the
  head hit unwired (none after).
- `mission_c1m1_owens_cockpit_screens_cycle_headless`: in an Owens, both
  screens are cycled, all 23 slots hold loaded `cpit_ani_*` images, and a
  drawn one steps. `cpit_ania` (one polygon) was not drawn in the first 60
  frames, so its cycle did not step: a cycle steps only when drawn. Breaks:
  no `get_model` (none cycled), no launch texture load (crash).
