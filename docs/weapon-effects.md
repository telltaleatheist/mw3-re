# Weapon effects: what firing shows and plays

What a shot looks like and sounds like, from the trigger to the impact, and
how the program wires it (`mech3/app_weapons.cpp`, `app_mission.cpp`). The
weapons' logic is `weapons.md`; this is their sight and sound.

## 1. Where each effect comes from

weapons.zrd names every effect by name; zWeapon's reader (0x583880)
resolves the names at load:

| Key | Resolved by | What it is |
|---|---|---|
| `ANIMATION`, `ANIMATION_ATTACHED`, `MODEL_ANIMATION`, `SURFACE_ANIMATION`, `ANIMATION_MIN_RANGE`, `DAMAGE_ANIMATION`, `DESTROY_ANIMATION`, `KILL_ANIMATION` | 0x50b450: the mission's animation defs (anim.zbd), by name, skipping def 0 and defs whose status (+0x98) is 5 | an animation (zEffect) |
| `MODEL` | 0x4f7fe0 on node list 6 | a node tree under each shot's node |
| `SOUND`, the bounce sounds | 0x56f6f0 (zSound) | a sound or group |
| `EFFECT` | 0x50d9e0: zEffect's effect table | a sprite effect |

The animations are the mission's own (every mission's anim.zbd carries the
weapon animations: `mac_muz.flt`, `ric_yellow.flt`, `impact_mgun_mech`,
`laser_red_muz.flt`, `rocket_puff_m20`, the pock marks `sm_pock.flt` ..),
so weapons.zrd is read after the animations (0x490ab0's order).

**The effect table is empty.** effects.zrd in both MW3 1.2 and Pirate's Moon
is `EFFECTS ()`: 0x50d2c0 reads no record, every `EFFECT` name resolves to
-1 and 0x50d650 never spawns anything. The port reads the list's header and
looks names up (`zeffect/zeff_init`); an entry's reading (its node, gfx data
and texture cycles) and the spawn are recorded hooks that no shipped file
reaches.

## 2. Firing

zWeapon's spawn (0x585010) plays, for the def's FIRE effect: the sound at
the muzzle (0x56e7e0), the animation at the firepoint
(`ANIMATION_ATTACHED`: 0x508da0) or at the muzzle turned along the shot
(`ANIMATION`: 0x508aa0 with the shot's pitch and yaw, `RANDOM_ROTATE` a
random roll). The machine gun's `mac_muz.flt`, the lasers' `*_muz.flt`, the
missiles' `rocket_puff_*`.

The beams (zwep_beam) start the FIRE sound and the looping FLYOUT sound,
whose pitch follows the beam's reach (0x56f280, zSound's frequency: the
hook was misnamed "volume" until now).

## 3. In flight

Each shot is a node in the world (zwep_shot 0x584df0, 0x585010) with the
FLYOUT `MODEL` under it, or a pooled copy of it when the model has a
`MODEL_ANIMATION` (0x5830b0), which runs on that copy (0x508c10) and clears
the shot's pointer when it ends (0x508f00 with 0x583120). `ANIMATION`
follows the node (0x508ee0), `ANIMATION_ATTACHED` hangs on it (0x508da0).
A beam is its segments' nodes, each with the FLYOUT model scaled to the
segment's length (0x582f00).

**The machine gun's tracer is a point.** `mac_trc.flt` holds one node,
`tracer`, whose model has no polygons and one model light (model +0x20).
Model lights are drawn by 0x541df0: the light's vertex to camera space, past
the near plane, projected and inside the point rect, its depth biased by the
light's priority (+0x18) like a polygon's, then one Direct3D point
(0x57dba0 through [0x72f3e0]) faded out between +0x30 and +0x34 (rate
+0x38). A light with +0x3c also goes on the lens-flare list (0x567420),
drawn later (0x5674e0 -> 0x562d90 plots single pixels; 0x5675e0 picks the
flares with a texture); not ported, and no weapon light has +0x3c. In the
software renderer every model light goes through that list as a single
pixel blended toward the fog colour. Ported: `gmod_draw` `model_light`,
`zvid_ddd3d` `draw_point`, checked by `test_modeldraw.py` with the original
0x541df0 running.

## 4. The impact

The impact (0x5826b0) by the surface's soil (material +0x20) takes the def's
`IMPACT` entry for that soil, else `default`: its `SOUND` (0x582f60, a
random one of up to four), its `ANIMATION` at the point, its
`SURFACE_ANIMATION` turned to the surface (0x525a50 from (0, 1, 0) to the
normal, then 0x525f80: quaternion to matrix to angles; `zmth::quat_angles`,
`test_matrix.py`), a crater or quicksand (0x5825b0 / 0x5824b0 into the
crater module, zdeclient), the surface mark (0x542060, the decal module).
A hit on something with a damage handler plays `DAMAGE_ANIMATION` /
`KILL_ANIMATION` (0x583480). Impact lights come from a pool of eight
(0x586970, made with the pool's nodes, freed with them 0x5869f0).

An animation started while it already runs gets an instance: its node tree
copied (0x4f9b50(node, 1, 1); `AnimHooks::copy_tree`).

## 5. Not ported (recorded hooks)

- Craters and quicksand: the crater module (zdeclient: 0x4ff920 defaults,
  0x4ffaa0 make, 0x5020f0 / 0x502110 quicksand), ~1,700 lines; 16 weapons
  have `CRATER`.
- Surface marks: the painting 0x542060. Its state is ported (`gmod_hit`,
  0x5426a0..0x542710): a weapon's `IMPACT_TYPE` (0x5426a0) picks one of
  three mark images that the script command `WriteTextureSetMap` (0x5426b0)
  gives, which also turns the marks on ([0x74a2ac]; 0x542700 is never
  called); the ray queries keep the hit's texture coordinates while they
  are on (0x5426e0), and a mark image stays a software image when loaded
  (0x542710). **No shipped script (MW3 or Pirate's Moon) uses the command**,
  so the marks are never on and 0x542060 returns at once in the game. When
  on, it would blend the image (an alpha byte per pixel: 565 or 555 by the
  pixel format [0x74a738], or a colour-keyed copy) into the hit material's
  texture centred on the coordinates (wrapped to 0..1), locking the
  texture through [0x72f368] and reloading it ([0x72f36c], [0x72f374](0)).
  The port logs `0x00542060` if a modded script turns them on.
- The lens-flare list (0x567420, 0x5674e0, 0x5675e0) and the software
  renderer's pixel plot 0x562d90.
- zEffect's table entries and spawn (0x50d2c0's records, 0x50d650): never
  reached by the shipped data.

## 6. In the program

`wire_weapons` (app_weapons.cpp) sets zWeapon's RunHooks and HitHooks to
the ports: zEffect's animations (`anim_start*`, `anim_set_callback`,
`anim_restart` for 0x508460, `anim_stop` for 0x506ca0), zSound (`play_at`,
`stop`, `set_frequency`), cls_di's ground below a guided shot (0x4eda10,
`ground_below`, `test_place.py`), zWeapon's own impact sound and craters
(0x582f60, 0x5825b0), the surface angles. `weapons_load` sets InitHooks'
animation and effect lookups and the impact lights. The mission's cleanup
stops every animation (0x50ace0, `anim_stop_all`).

The weapons' own sounds (`U_LOOPED`, `U_RECHARGE_SND`: the machine gun's
rattle is `wep_mgun_fire`) are found by name in zSound's loaded sets, so they
need the sets the Load state's 0x490720 loads before the mission: "common",
"mech", the mission directory's and "<dir><block>" (unless the game is of
kind 6 or the briefing voices are off, app +0x1c3); `GameEngine::
missions_reset`. The game side plays them through WeaponCalls (`play_loop`
0x56e3d0, `play_at` 0x56e7e0, `stop_sound`); the cockpit's table (0x41e770)
and Betty (0x4e6dc0, 0x4853d0) stay hooks.

Tests: `test_modeldraw.py` runs the original 0x541df0 (with the lights'
priority, fade and flare flag varied) against `model_light` and the point
entry; `test_place.py` checks 0x4eda10 on the worlds and on 4,000 made-up
hit lists (ties, zone bytes, NaN) with 0x4edab0 stubbed; `test_matrix.py`
the surface angles; `mission_c1m1_player_fires_on_the_owens_headless`
asserts the machine guns' muzzle animation and fire sound, their impact
animation on the Owens and ricochet sound, and the laser's muzzle and
impact (`weapon_fx_log`, ours). Breaks caught: the priority bias, the fade,
the tie rule, the NaN compare, the surface angles, the impact animations
unset, the common set not loaded.

**A second mission in one run** gets its own animations: the mission
unload (0x00490480, `mission-load.md` section 6) frees them (0x0050b380),
with the effect table, zWeapon, zClass and the textures.

`mw3 --mission c1m1 --fire N [--fire-group G]` (ours, for checking) holds
the fire command from the N-th frame shown, with group G (else the first
that heats) selected, and prints the shots in flight every five frames.
