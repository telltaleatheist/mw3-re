# The hooks the program leaves unset (2026-10-29)

The ported modules call outside themselves through hooks structs: function
pointers or `std::function` fields such as `zeff::AnimRunHooks`,
`mech3::WeaponCalls` or `zmod::PuffHooks`. The program (`mw3_main`,
`GameEngine`, the `Wired*` hooks and the `app_*.cpp` wiring) sets them. A
field it leaves null is usually skipped silently (`if (h.f) h.f(...)`) or
answers a default, and the census never sees it. zEffect's run hooks
(campaign.md section 14) were the first case found. This is the audit of
the whole class.

## The tools

- `tools/analysis/hookaudit.py` parses the headers for hooks structs: their
  function-pointer and `std::function` fields and their accessor
  (`Type& name();` at namespace scope).
  - It greps for assignments outside `tests/`.
  - `--inc tests/hook_fields.inc` writes the field list the test reads.
- `program_hooks_set_after_a_mission_load_headless` (mw3tests) starts the
  program headless, launches c1m1 and, at Core frame 60, reads every field
  through its accessor (`tests/test_hooks.cpp`). Each null field must be in
  `tests/hook_allow.txt` with a reason starting with one of:
  - `counted:`, `fallback:`, `default:`;
  - `multiplayer:`, `no effect:`, `never set:`;
  - `deferred:`, `test:`, `data:`.
  An allowed field that is set fails too, so the list stays current.
  Break: with the puffers' `set_callback` unwired, the test fails ("null
  without a reason").
- **The census stand-ins** (`src/mech3/hook_census.inc`, installed by
  `app_hook_census.cpp`). Each field there is a pure skip: the port does
  nothing, or takes 0, null or false, when it is null. It gets a stand-in
  that reports its exe address to the census and returns that same
  answer, so behaviour is unchanged.
  - They are put in at each mission's start and each mission frame
    (`MissionHooks::hooks_check`, ours, called from `Missions::begin` and
    `Missions::update`): some wirings reset their structs wholesale.
  - Only fields still null are touched.
  - Not stand-ins: fields whose null answer is not a skip. These are a veto
    that refuses on 0, `can_play`, the network gate, and non-zero defaults.

## The numbers

| | fields |
|---|---|
| hooks structs with an accessor | 78 structs, 866 fields checked (887 found; 8 structs have no accessor) |
| null after start-up and c1m1's load, before | 346 |
| the script engine's (zint::Engine), already counted by the script census | 98 |
| unset runs the port (fallbacks, test hooks) | about 126 |
| silent skips | about 120 |
| wired now | 29 |
| census stand-ins | 52 |
| null after, all on the allow list with a reason | 265 |

## What mattered (wired)

- **The puffers never stepped.** `PuffHooks` (0x004eb5b0, 0x004eb450,
  0x004eaea0) were unset, so `puff_tick` (0x005490e0) was never the tick
  node's action. Smoke, fire and missile trails stood still. Now wired
  (`wire_puff_node`).
- **The arm restriction never applied.** `WeaponCalls.view`, `view_kind`,
  `control_mode`, `torso_aim`, `torso_look` and
  `UpdateCalls.weapons_changed` (0x004b2da0) were unset. In the side views,
  or in free look with the torso more than 35 degrees off, the exe fires
  only one arm's weapons; the port fired them all.
- **The view key did nothing.** `UpdateCalls.key` (0x0051d300) was unset,
  so the view key (CMD 0x17) never switched between torso mode and free
  look.
- **No ammo cooked off in a heat shutdown.** `HeatCalls.ammo_explosion`
  (0x004b0de0(-1, 1) on +0xd4) was unset. Riding out a shutdown, the exe
  explodes a bin 80% of the time.
- **The AI's torso never turned to aim.** `AimCalls.torso_aim` (0x00465ea0,
  `Torso::set_aim`) was unset, so `Mech::aim_at` aimed only the weapons.
- **The AI nets kept the files' heights.** `NetHooks.ground` (0x004edab0
  from 10 m above, then 0x004abd90 with a 10 m step) was unset; the nodes
  are now put on the ground while a world is loaded. The world pointer is
  cleared when the port frees the world (`delete_608c84`, and at
  `missions_reset`), so the next load's waynets see none, as in the exe.
- **The reticle's aim state on the weapons' changes.** 0x004b0d80's calls,
  the reticle's place, `HudTarget::over` and `HudReticle::aim_state`, were
  unset.
- **The rest.**
  - The camera's look for the controls (`ControlsCalls.camera_view`).
  - The MW2 jump jets option (`ControlsCalls.mw2_jets`, 0x0040d1e0).
  - The legs' tree update (`PartCalls.tree_update`, 0x004f7db0).
  - `DamageCalls.networked` (0x00479480).
  - `DamageCalls.message` (0x00576040: the nuke's name for the kill by a
    nuke).
  - The pocket commander's census route (`PocketCalls.unported`, which had
    been silent).
  - `WeaponCalls.part_in_water` (+0x49).
  - The weapons' shutdown node delete (`InitHooks.node_destroy`).
  - `WeaponCalls.owner_x58` and `CollideCalls.remote`: vt+0x58 is
    0x0040bae0 (0) for the mech, the basic vehicle, the field base and the
    turret.
  - A changed texture image reaches its GPU texture again
    (`TextureHooks.reload`, [0x0072f374] = 0x0057baa0, `zvid::reload_texture`).
  - `TextureHooks.release_all`: [0x0072f37c] = 0x005ac5c0, which does
    nothing.

## What the census now shows (reached, in the known table)

The every-mission census, with the stand-ins in, reached 13 keys beyond
the table, and the program's chain runs one more (the fall camera). Each is now listed with its reason:

- **Vehicles got no weapons in the program.** This was the biggest find.
  The vehicle builder 0x004b4660 looks up each weapon's firepoint node
  through `WeaponCalls.node_find` (0x004eb0d0) and `node_by_name`
  (0x004abb60). Both were unset, so every vehicle was built without its
  weapons, silently.
  - Both are ported and wired now (2026-10-29), with the manager's +0x80
    (0 for a vehicle) and the shooter's whole zone record: the vehicles
    fire and hit (weapons.md, The vehicles' weapons in the program). Their
    known-table entries are gone.
  - The Op 1 salvage check had counted the vehicles' empty salvage as the
    coin's "none". The chain now tells the coin's none from an unarmed
    vehicle.
- **A known duplicate.** `app_turret.cpp`'s `find_first` is a second copy
  of 0x004eb0d0, without the null node's report. Fold it into
  `zcls::node_find` the next time test_turret is touched.
- **Also not ported:**
  - the damage decals (0x00414370 / 0x00474d90);
  - the cockpit HUD's power down and up (0x0043bb20 / 0x0043bb50);
  - the mech's zone record (0x00476550);
  - a mech's collisions with other entities' bodies
    (`CollideCalls.ai_reverses`, `entity_dead`, `entity_target`);
  - the static light bake's refresh (`DrawHooks.refresh_static`). This
    one mattered: its stand-in did nothing, so from 160 frames after
    loading, every static model was drawn from an empty light cache. The
    terrain went black about 5 s in (checked in c1m2 and the fly view). Fixed 2026-10-29: the
    draw runs the ported bake when the hook is unset, and the stand-in is
    gone.
  - the fall camera when the viewed mech falls (0x00413af0, its restore
    0x00413b70). The census reaches it once the chain's kills make mechs
    fall.
- **No effect in single player:** the mech's and weapons' network update
  and messages (0x0046ef70, 0x0046f160).

The skips that the census does not reach in its scripted cycle stay as
stand-ins. Examples are the force feedback (only with [0x00608620], never
made), the fall camera (0x00413af0) and the HUD's heat haze (0x0043c1f0).
The census reports them when a run reaches them.
