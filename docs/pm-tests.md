# Tests that load libmw3probe, for `pm` (2026-11-05)

Every `test_pm_*` ends through `pm_floors.floors`: what it loaded or reached against a floor, because an empty pass is worse than no test (the lead, 2026-11-05). Data counts are exact; what depends on random cases is about half of what a run reaches. With arguments only zero fails. `test_pm_mechlib` and `test_pm_zar` also load MW3's data on purpose, to compare: some of their counts are not PM's.

`mw3probe` is not built on `pm`, so none of these test this branch. Each row: what becomes of it. *Fns* is how many mapped
functions it names, *in PM tests* how many of those a `test_pm_*` also names. That is a count of names, not proof a case is
covered: "covered" below was judged per test from those names and the test's subject. *Class* is `pm_survey`'s: whether PM
changed or removed one of its functions.

To move, ranked by how much of the moved roster ids and mech code it proves (1 first). The rest of "to move" is unranked. The order of the work is `docs/pm-mech-step.md`: moving a test and porting its code are one piece. 

| Test | Class | Fns | In PM tests | Becomes | Note |
|---|---|---:|---:|---|---|
| `calltrace.py` | - | 0 | 0 | not applicable | a helper: goes with whichever test uses it |
| `pose_world.py` | - | 7 | 0 | not applicable | a helper for the pose tests: goes with them |
| `test_action_lists.py` | changed | 42 | 14 | to move |  |
| `test_actions.py` | gone | 19 | 3 | moved to `test_pm_actions.py` | the falls and getting up |
| `test_ai_nets.py` | changed | 33 | 0 | moved to `test_pm_ai_nets.py` | the compile |
| `test_ai_paths.py` | gone | 20 | 0 | moved to `test_pm_ai_paths.py` | at 53 bits, nets of up to 30 nodes for ways |
| `test_ai_readers.py` | changed | 13 | 1 | covered | `test_pm_ai_nodes` (the header, every node), `test_pm_ai_nets`, `test_pm_ai_regions` (the cache and lookups, through the build) |
| `test_ai_regions.py` | gone | 19 | 0 | moved to `test_pm_ai_regions.py` | at 53 bits |
| `test_ai_valves.py` | gone | 31 | 6 | covered | `test_pm_ai_valves` reaches the same record, builder, actions, conditions, sets and timers, judged by what it runs; the mission's end 0x4e4400, 0x4e4380 isn't run on the exe |
| `test_bake.py` | changed | 3 | 2 | covered | test_pm_bake |
| `test_clip2d.py` | gone | 1 | 0 | covered | test_pm_clip2d |
| `test_clip3d.py` | gone | 0 | 0 | covered | test_pm_clip3d |
| `test_collide.py` | changed | 24 | 7 | moved to `test_pm_collide.py` | mech collision |
| `test_controls.py` | changed | 3 | 0 | moved: `test_pm_controls` | the controls |
| `test_d3d_frame.py` | gone | 10 | 1 | covered | test_pm_device (its frame; the tint unchecked) |
| `test_damage.py` | gone | 50 | 12 | moved to `test_pm_damage.py` | mech_damage and mech_parts Elemental sites, roster 4 and 16 |
| `test_deaths.py` | gone | 65 | 14 | moved to `test_pm_deaths.py` | landing, salvage, kill credit, part destroyed, roster 4 and 16; the dead step rewritten; the mech's own LOD stays in test_pm_lod |
| `test_di.py` | gone | 20 | 6 | covered | test_pm_di |
| `test_drawtable.py` | changed | 8 | 4 | covered | test_pm_device, test_pm_walk, test_pm_submit |
| `test_entity_walk.py` | unchanged | 3 | 0 | to move |  |
| `test_fogsort.py` | gone | 3 | 1 | covered | test_pm_device (fog) ; the queue in test_pm_walk |
| `test_gait.py` | changed | 8 | 1 | moved to `test_pm_gait.py` | crouch and the Elemental in 0x41c9f0 |
| `test_gamez.py` | changed | 9 | 8 | covered | test_pm_gamez |
| `test_heat.py` | changed | 10 | 6 | moved to `test_pm_heat.py` | heat |
| `test_interp.py` | gone | 157 | 36 | to move |  |
| `test_jets.py` | gone | 20 | 6 | to move, rank 1 | jets, landing: three Elemental sites in mech_move |
| `test_lance_dispatch.py` | gone | 10 | 3 | moved to `test_pm_lance_dispatch.py` | at 53 bits, with the order's object 0x46a750 and the own marker |
| `test_light_setters.py` | changed | 5 | 5 | covered | test_pm_light_setters |
| `test_lighting.py` | changed | 18 | 5 | covered | test_pm_lighting |
| `test_matrix.py` | gone | 37 | 4 | covered | test_pm_modeldraw, test_pm_device |
| `test_mechanim.py` | changed | 22 | 2 | moved to `test_pm_mechanim.py` | mech animation, on PM's own motions; no port change needed |
| `test_mechlib.py` | changed | 4 | 2 | covered | test_pm_mechlib |
| `test_modeldraw.py` | gone | 24 | 14 | covered | test_pm_modeldraw |
| `test_motion.py` | unchanged | 2 | 0 | to move |  |
| `test_nav_waynet.py` | changed | 8 | 0 | moved to `test_pm_ai_nets.py` | the waynet builder and its points, 6 of 7 breaks caught; the seventh is a dock point's flags, reached only by the arrangenodes point, stubbed. A link past the nodes isn't run: the exe reads out of range |
| `test_objectives.py` | gone | 24 | 8 | to move |  |
| (new) `test_pm_standalone.py` | PM | | | new | the second campaign's game object, list, salvage and lineup |
| (new) `test_pm_net_shot.py` | PM | | | new | the sender's shot record and PM's Total Shots |
| (new) `test_pm_lab_pick.py` | PM | | | new | the lab's firepoint picker |
| (new) `test_pm_lab_data.py` | PM | | | new | the lab's two data panels' numbers, on every loadout |
| (new) `test_pm_puton.py` | PM | | | new | the lab's put-on 0x455a80, the chassis list's empty row 0x44a660 |
| (new) `test_pm_archives.py` | PM | | | new, part of `test_mechsave.py` | the saved archives' writer and loader |
| (new) `test_pm_lab_message.py` | PM | | | new | the lab's message: 3 seconds, the red flash |
| (new) `test_pm_briefing_buttons.py` | PM | | | new | the briefing's buttons by game type |
| (new) `test_pm_mech_vt.py` | PM | | | new | the aim 0x41c900, the placement's block |
| (new) `test_pm_settings_key.py` | PM | | | new | mw3pm's settings key, an older file's values kept |
| (new) `test_pm_standalone_chain.py` | PM | | | new | the second campaign from the menu; not played: PM's AI isn't running |
| (new) `test_pm_ams.py` | PM | | | new | the AMS's frame 0x41a020 with its find and fire |
| (new) `test_pm_mechsave.py` | PM | | | new, part of `test_mechsave.py` | a saved loadout's save and load, both formats, with byte damage |
| `test_mech_build.py` | changed | | | moved to `test_pm_mech_build.py` | launch and mech construction, every block; the weapons loop stubbed |
| (new) `test_pm_firepoints.py` | PM | | | new | the firepoint fix-up 0x46ea40 and its aim cone |
| `test_parts.py` | gone | 75 | 16 | moved to `test_pm_parts.py` | setups, updates, the arm's clamp, the torso's mode, the part aim; floats within 1e-5 |
| `test_player.py` | gone | 17 | 3 | moved: `test_pm_player` | the player controls |
| `test_pocket.py` | changed | 18 | 3 | moved to `test_pm_pocket.py` | PM's keys, the pushed key map, the real 0x4291a0 under the mode |
| `test_pocket_orders.py` | gone | 9 | 1 | moved to `test_pm_pocket_orders.py` | at 53 bits, each unit's own marker |
| `test_power.py` | changed | 8 | 5 | moved to `test_pm_power.py` | power; the headlight off and the network target dropped |
| `test_puffers.py` | gone | 44 | 12 | covered | test_pm_puffers |
| `test_scenewalk.py` | changed | 33 | 13 | covered | test_pm_walk (10 of its 33 functions; the rest by test_pm_nodebox and test_pm_world_box) |
| `test_sensors.py` | changed | 35 | 5 | to move | the sensors, the entity sort and the allies' turn; added 2026-11-07, it was on neither list. The sort 0x4e6140 keeps about an eighth of MW3's lines, the turn 0x4e6240 a quarter; `test_pm_lance_flag` has 0x4e5f90 alone |
| `test_shade.py` | gone | 23 | 5 | covered | test_pm_lighting, test_pm_modeldraw |
| `test_shadow_image.py` | changed | 29 | 16 | covered | test_pm_shadows |
| `test_skis.py` | changed | 18 | 3 | to move, rank 12 | ground following |
| `test_sound.py` | gone | 47 | 46 | covered | test_pm_sound (imports it for its harness) |
| `test_sound_extra.py` | gone | 15 | 2 | the wake moved: `test_pm_wake`; the sounds to move |
| `test_spans.py` | unchanged | 2 | 1 | to move |  |
| `test_texload.py` | changed | 8 | 3 | covered | test_pm_mission_load, test_pm_gamez |
| `test_textures.py` | changed | 3 | 0 | to move |  |
| `test_throttle_keys.py` | unchanged | 1 | 0 | to move, rank 15 | throttle keys |
| `test_update.py` | gone | 39 | 11 | to move, rank 5 | Mech::Update, whole mech step |
| `test_weapons_zrd.py` | changed | 29 | 8 | to move |  |
| `test_weiler.py` | changed | 34 | 2 | to move |  |
| `test_zclass.py` | changed | 2 | 0 | to move |  |
| `test_zdec_init.py` | changed | 16 | 9 | to move |  |
| `test_zgeo_model.py` | changed | 37 | 2 | to move |  |
| `test_zinput.py` | gone | 53 | 7 | to move |  |
| `test_zreader.py` | gone | 4 | 4 | covered | test_pm_zreader |
| `test_zreader_text.py` | gone | 7 | 6 | covered | test_pm_zreader_text (imports its file makers) |
| `test_ztimer.py` | unchanged | 2 | 0 | to move |  |

39 to move, 21 covered, 2 not applicable.

`test_pm_debrief_chain`, `test_pm_messages` and `test_pm_pilot` are new, not moves: `mw3pm` through three chains to its debriefings and the campaign's end, and the port's message ids against PM's table. `test_debriefing` itself (below, unclassified) stays to move, with the twod bench on PM's exe (`campaign.md`). `test_pm_ai_nodes` is new too: PM's AI readers on every shipped net, targets, aiv and valves file, exe against port, `test_pm_ai_valves`, the valve builder and actions, `test_pm_ai_boundary`, the boundary and its clamp, and `test_pm_ai_boundary_real`, the same on every shipped polygon; `test_ai_readers` stays to move with the valves and the rest of the reader's harness.

## MW3's tests rewritten for PM (`pm_moved.py`)

`pm_moved.py test_x` imports a MW3 test with every literal in the image's range replaced by its twin in PM, and runs it on PM's exe through `libpmprobe`. No address map at run time, so the pointers the exe holds are read as they are. Run today: `test_twod`, `test_twod_msgbox`, `test_lab_list`, `test_lab_screen`.

Field offsets go by a rule for the 2D world (`field`) and a member table for the lab screen (`LAB_MEMBERS`, `LAB_TEXT`). Vtables go by `tools/analysis/pmvtables.py` (run it after regenerating the map): `addrmap.json`'s data section pairs them by contents, and it disagreed with the constructors on 244 of 502. A stub's stack pop comes from PM's own function, as some became cdecl.

`MW3_TWOD_HW=1` runs them with the exe and the port in hardware mode ([0x72f348], PM's 0x8c4b70), the only mode `mw3pm` plays: `test_twod` 90/90, `test_twod_msgbox` 90/90, `test_lab_list` 40/40, `test_lab_screen` 40/40. Breaks caught in that mode: the static parts always updated at a dialog's load (`test_twod`), and the message box's two mode tests (`test_twod_msgbox`). What it compares is the logic and the calls out, not the picture: PM's textured quads 0x5c0510 aren't ported (rendering rule), so that drawing rests on the eye checks.

## The rest that cannot load on this branch

The table above went by the file naming `libmw3probe`. Importing every `test_*.py` once (2026-11-05): 144 of 259 fail at load, every `test_pm_*` loads. These 83 reach MW3's probe through another test's module and are **not classified yet**. `test_mechdef` among them is covered by `test_pm_mechdef`. `test_mechsave` is unmoved: `test_pm_mechsave` has a loadout's save, load, version and MW3's format with byte damage, not the whole archives (0x4267f0, 0x426bd0, 0x427e80). It stays as the source for those and for six tests that import its helpers: `test_config_unpack`, `test_field_cut`, `test_lab_screen`, `test_mech_net_send`, `test_salvage`, `test_salvage_screen`.

`test_actionscreen` `test_anim` `test_anim_binder` `test_anim_reset` `test_anim_run` `test_anim_save` `test_briefing` `test_config_unpack` `test_convexify` `test_crater` `test_ctlscreen` `test_cycle` `test_debriefing` `test_decal` `test_destroy` `test_field_cut` `test_fmv` `test_gmod_add` `test_hit_globals` `test_hud` `test_inout` `test_inout_query` `test_lab_list` `test_lab_screen` `test_lab_view` `test_loadsave` `test_mainmenu` `test_map_pick` `test_mech_health` `test_mech_net` `test_mech_net_apply` `test_mech_net_create` `test_mech_net_end` `test_mech_net_events` `test_mech_net_launch` `test_mech_net_respawn` `test_mech_net_send` `test_mech_net_world` `test_mechdata` `test_mechsave` `test_menus` `test_model_sweep` `test_node_find` `test_objective_check` `test_pilot` `test_pilotscreen` `test_place` `test_pocket_frame` `test_pocket_keys` `test_pocket_panel` `test_pose` `test_prim2d` `test_project_point` `test_pull_in` `test_ripple` `test_salvage` `test_salvage_screen` `test_script_setters` `test_shadows` `test_shotq` `test_ski_polygon` `test_skins` `test_store_fill` `test_tag` `test_training_lessons` `test_tree_copy` `test_twod` `test_twod_blit` `test_twod_line` `test_twod_msgbox` `test_twod_prims` `test_weapon_planner` `test_weapons_fly` `test_weapons_hit` `test_weapons_run` `test_world_params` `test_znet_create` `test_znet_dispatch` `test_znet_players` `test_znet_session` `test_znet_teams` `test_zsave`

## AI hooks with an output: does a test see it dropped? (2026-11-06)

Why: walk 0x4d9680 passes the carrot to the boundary as both place and answer. The first port's hook took it const and dropped the answer, and its stub never wrote one, so "0 differ" didn't cover it. Each hook below has a pointer or reference output. The check: make the probe's stub drop that output, run the test, see if it fails. The exe-side stubs write theirs, or the test couldn't catch this.

`mw3probe` doesn't compile on `pm`, so the MW3-side rows ran on a scratch library linked from what compiles (`probe.cpp` with one call patched, `probe_world` and `probe_debriefing` left out), against MW3's exe. It's since removed and proves only these drops.

| Hook | Test that caught the drop |
|---|---|
| `SteerPmHooks` position, forward | `test_pm_ai_steer` |
| `WalkPmHooks` position, forward, pull, way_from | `test_pm_ai_walk` |
| `BrainHooks` position, forward | `test_ai_steer` |
| `BrainHooks` weapon_node_position, entity_lock_position | `test_ai_aim` |
| `BrainHooks` velocity, vehicle_matrix | `test_ai_plan` |
| `BrainHooks` zone_list, node_world_cached, segment_first's count | `test_ai_clearline` |
| `BrainHooks` lead_point | `test_ai_manoeuvre` |
| `BrainHooks` dock_point | `test_ai_repair` |
| `BrainHooks` ammo_value | `test_ai_frame` |
| `BrainHooks` inout_clamp | `test_ai_node` |
| `BrainHooks` pilot_of | `test_ai_node` |
| `BrainHooks` player_allies | `test_ai_brain` |
| `TargetHooks` record's name, node_box | `test_ai_targets`, part 2 only |
| `AnimOwnerHooks` node_translation, node_rotation | `test_anim_owner` |

**Not caught, or not checked:**
- `bearer_aim_clamp`: missed by `test_ai_aim`, `test_ai_plan` and `test_ai_firing`. Expected by reading: the caller ignores the answer (zai_firing.cpp, "feeds a dot the exe drops"). That the exe drops it isn't compared.
- `InOutHooks::ground`: no test on `pm` runs it. `test_inout` can't, because MW3's boundary is gone from this branch. PM's exe path (0x4ce8a0) is not compared. It only moves a corner's height, and the queries are in x and z, so by reading the height has no effect on an answer. Rests on reading.
- `NetHooks::ground`: `test_pm_ai_nets` (20 of 20 breaks caught) compares it (2026-11-07): the pick 0x4af770 runs on the exe, `net_ground_height` on the port, over one stand-in for the ground query. It found that PM counts water as ground here and the port didn't (`pm-map.md`, PM's net compile). The query itself, `ground_point` on a real world, isn't in that test; the hook's few lines around it rest on reading.
- `zai_nav`'s `World::ground`, `parked`: ours, the way finder, with nothing in the exe to compare against.

**Const check, by reading:** of the hooks taking a const pointer, the boundary is the only one where the exe's call passes one pointer as both place and answer. The ones whose bodies were read only read theirs: 0x4b0bc0, 0x56e7e0, 0x4fc1b0, 0x4d51d0, 0x4eed90, 0x41dbd0, 0x41dca0. 0x41dc70 passes its argument on to 0x465c70, which wasn't read. 0x418f30 (vt+0xdc) passes its arguments on to callees that weren't read. `region_inside`, `wall_between` (ours) and `node_offset`'s callee beyond its signature weren't read either. MW3's call sites that pass the same pointer (`inout_clamp(v, to, to)`) are written through non-const `out`. PM's 17 calls of the pull aren't all ported yet; each is checked as it lands.

**Limits:** the list comes from the hook classes in `zai*`, `zainet*` and `zrdr*`, by signature. A hook's return value isn't covered. Hooks that write a field through a call (`brain_write`, `target_masks`, `set_local_velocity`) weren't given drop breaks.

**That scratch build, and what it means for the rows above.** Its first form left out `probe.cpp`, and with it the line that turns the zfix fixes off for the whole probe library, so `bugs` stayed on and `test_ai_attack` differed at seed 281, round 5, step 33, on the port's own fixed path. With `probe.cpp` linked it passes, 300 cases, the same numbers as `main` at 355a1b09 and the merge base d6c63bf2, so it was the build, not `pm`. All rows above except `record`'s name and `node_box` ran on that first form, fixes on: a drop caught there is still a drop the test sees, but they weren't rerun with fixes off. `test_ai_targets` part 1 fails with fixes off as well, at fn 10: PM's `save_target` against MW3's exe, by design.

**On the switch's list:** the real hooks in `app_mission.cpp`.

**Closed from that list (2026-11-07).** `brain_write` and `target_masks`: dropped at the caller, `test_pm_ai_valves` differs (1 and 15 files). `InOutHooks::ground`: dropped, no test differs, and none can: `app_mission.cpp` wires it, but a corner's height is in no answer, since every measure is in x and z and `inout_pull` writes the height back from `in`. Rests on reading. `set_local_velocity`: only MW3's `JumpState` calls it; PM's `zai_jump_pm` doesn't.

## Scaled tolerances and non-finite values (2026-11-06)

A float compare whose allowance scales with the value, `abs(x - y) <= k * max(1, |x|, |y|)`, passes `inf` against anything: the allowance is infinite too. The `>` form also passes a NaN. Found in `test_pm_ai_jump`'s compare, where `inf` matched `FLT_MAX`. The search looked for the shape in every test and helper, not for a name; a compare spread over several lines or with its scale in a variable could still escape it.

- **Fixed** (a non-finite value now only matches itself): `test_pm_ai_jump`, `_ai_steer`, `_ai_walk`, `_clutter`, `_di`, `_health`, `_heat`, `_mech_build`, `_puffers`, `_shadows`, `_submit`, `_world_box`. Rerun after the fix with the importers `test_pm_skis_world` and `test_pm_clutter`: every past result held, none changed. Proved on one test, `test_pm_health`, by planting `inf` as the port's answer: with the old compare all 20,000 trials pass, with the new one the first trial stops it. The others share the shape, so they rest on that and on reading.
- **Already guarded:** `modeldraw_common.same_float` (the only such helper PM tests share with MW3's), `test_pm_firepoints`, `test_pm_parts`.
- **MW3's tests with the same shape, unchanged from `pm`** (they aren't built here, and `main` owns them): `test_action_lists`, `test_bake`, `test_clip2d`, `test_clip3d`, `test_config_unpack`, `test_lighting`, `test_matrix`, `test_mech_net_apply`, `test_mech_net_send`, `test_mechsave`, `test_project_point`, `test_puffers`, `test_scenewalk`, `test_veh_aim`, `test_veh_damage`, `test_veh_frame`, `test_veh_move`, `test_veh_suspension`, `test_weapons_fly`. Whether each can really pass one wasn't checked; some guard earlier.

**On the switch's list, added:** once the AI runs, watch the mech's +0x588 and the fuel in a real jump against the trace; only then is PM's full tank a finding. Also: every field of each struct the AI port owns is compared at least once, or has a line saying why not (the steering's `x40` had a wrong constant that nothing read, so "0 differ" didn't cover it).

## Early exits and skips (2026-11-06)

A skip that tests some fields and doesn't prove the others agree hides what it doesn't test. `test_pm_ai_walk` had one, now `tie_split`: with two planted breaks on such lines, a changed flag passed the old exit outright (44 plans dropped, under its ceiling of 45) and a changed field was stopped only by that ceiling. The new exit stops both at once, and still ends the 1 real tie.

The search read each `if ...: continue/break/return True` over a value in `test_pm_*` and `modeldraw_common`, by shape on one or two lines, so one written otherwise could escape it. None of the others has this shape:
- **Tests the very value it skips:** `test_pm_clutter:158`, `test_pm_skis:278`, `modeldraw_common.same_float`.
- **Compared just before:** `test_pm_modeldraw:133` (the centre, bit for bit, and the count).
- **Choosing inputs, not results:** `test_pm_nodebox:79`, `test_pm_submit:205`, `test_pm_strips:83`, `test_pm_gamez:275,343`, `test_pm_lance:136`, `test_pm_ai_boundary_real:46,49`, `test_pm_zar:344`, `test_pm_projected:92`.
- **Fixed:** `test_pm_ai_boundary`'s hunt picked its cases from the port's own answer; every query in it now goes to both sides. The same 57 rare cases turn up.
- **Worth knowing:** `test_pm_ai_boundary_real` screens 594,560 queries with the port and asks the exe about 7,618: every rare case the port finds (up to 100 an area) and a random sample of the rest. A rare case the port wrongly misses is only compared if the sample lands on it. Asking the exe about all of them was done per world (2026-11-07, below): about 4 hours, 0 differ.

**The recount of what's left** (`tools/analysis/pmrecount.py --trace DIR`) counts a function as done when the port's source names its address outside the hook classes. Cited isn't landed: a comment can name what isn't ported, and only a test shows a function is. It's an estimate of the size, nothing more.

**The rerun, done (2026-11-07):** all 409 breaks on the walk, jump, steer, drive and boundary pieces, `0x4d9d00`'s 36 among them, on the 53-bit tests. 66 had text the 53-bit work changed and were rewritten by hand as the same mutation on today's code, so those are not the identical breaks. 399 caught, by a compare or a fault: the floors count on the exe's side, so a floor line only says the run stopped early on a compare. `test_pm_ai_jump` now puts the mech in the glide's band, which it had stopped reaching, and two breaks came back. **Two are caught only at 5 times the runs, so the switch's final run must use these settings:** `test_pm_ai_walk.py 1500` for `second dir` (the way cut short after a pull at the stop), and `test_pm_ai_steer.py 1500` for `keep float`. The default runs miss both. `dot limit` wouldn't build, and is the same as a rewritten one that's caught. Nine missed at 5 times the runs, by reading:

- Can't change an answer: `push none` (the caller stops on no entity first); `by vehicle first` (with both in reach the two sizes are equal, and the sign is the vehicle's); `apart dist` (0.01 and 0 are both under the middle range + 200, and nothing else reads it); `circle near zero` (`range_near > |l[1]|` right after needs it above 0); `10 wake`, `10 until` (the plan just after writes both on every path); `update compare stored` (a float `along_min` can't fall between a value and its rounding).
- Only on input the tests don't give: the bounds' `lt` against `<` differ on a NaN corner.
- Already recorded as resting on reading (`pm-map.md`, Compared): the condition `from_in && to_in` in 0x4cbbb0.

`near float` missed at 5 times the runs too, and is caught now by a tie made by hand (`near_ties` in `test_pm_ai_steer`, 6 cases on the exe and the port). 0x4dc210 past the way's end keeps the sum of along - length and the side in the register. The tie can arise: on a way short enough that its length has bits a float of the sum can't hold (under about 1 unit, with the sum near 10), the exact sum is just over `near_dist` and the rounded one equal to it. The exe answers far, a rounded sum near. On ways of ordinary length along - length fits a float and there's no such case.

**The boundary per world, every query on the exe (2026-11-07):** c1 8 areas, 228,520 queries; c2 6 areas, 157,480; c3 6 areas, 109,360; c4 5 areas, 99,200: all 25 areas and all 594,560 screened queries, 0 differ, so the port picked nothing. `MW3_BOUNDARY_ALL=1 MW3_BOUNDARY_WORLD=c1` and so on, c1 about 1 hour 50 minutes.

## PM's AI runs at 53 bits only (2026-11-06)

`test_pm_ai_steer`, `test_pm_ai_walk`, `test_pm_ai_jump`, `test_pm_ai_boundary` and `test_pm_ai_boundary_real` set `MW3_X87=53` (control word 0x027f, a value added to `original.py`) and compare to the bit, with no tolerance. This differs from CLAUDE.md's "tests still run both": these ports follow the exe's evaluation at the reference's precision, measured 2026-11-04, so at 24 bits they don't match, by design, and that isn't run. Every new PM AI piece is written and tested this way. `pm-map.md`, "PM's AI at 53 bits", has the evaluation, the ties and the numbers.

The compares were 1e-3 wide, which is thousands of last-place units; 53-bit runs with them exact found what that hid. The other PM AI tests' stubs were read for the split that made seed 447 look like a tie, Python doubles on the exe's side against floats on the port's: only that one stub computed a float, and it's fixed. The rest return handles, ints and names.

**Rerun at the switch, added:** every break for these pieces on this evaluation.

## The boundary's small helpers, compared through their callers (2026-11-07)

PM keeps as functions what MW3 inlined: 0x4c7e10, 0x4c7e50, 0x4c7f30, 0x4c9220, 0x4c9260, 0x4caa60, 0x4caca0, 0x4cab20, 0x4cad50,
0x4cade0, 0x4cb730, 0x4cae10, 0x4cc970, 0x4ccac0. The port has them in line. Counted on the exe, `test_pm_ai_boundary` runs the first 13
(600 to 12.8 million times; 24,000 queries, 0 differ) and `test_pm_ai_walk` the last. 6 of 8 breaks on the port's forms caught.
The two missed need an exact tie the 200 areas don't reach: a point exactly on a side's line (0x4c9260) and a side exactly at the limit
(0x4cab20); those two comparisons rest on reading.

## Reading only, for the switch's review (2026-11-07)

No test can show these; each rests on reading the exe.

- 0x4c9260: a point exactly on a side's line. The tie isn't reached in 200 areas.
- 0x4cab20: a side exactly at the limit. The same.
- `InOutHooks::ground`: a corner's height is in no answer.
- The plan 0x4da1c0's second pull through vt+0x24, and its [0x62fb20] branch: not ported, as all three contexts answer 0 at vt+0x24 and nothing writes the flag.
- The arrangenodes point (0x4c5002): 0x4e57e0 is stubbed on both sides of `test_pm_ai_nets`, so no such point is built, and with it a dock point's flags in 0x47c5f0 (`WaynetPoint`'s `xe, mfb, xd, xf`). The break on those flags is the waynets' one miss, 6 of 7 caught. Unlike the rest of this list a test could show it, by leaving 0x4e57e0 running.

## The lance orders and the commander (2026-11-07)

0x46a750 (the order's object) and the brain's +0x2cc (the own marker, `cmd_target`) are ported, with five tests: `test_pm_lance_dispatch`, `test_pm_pocket_orders`, `test_pm_pocket`, `test_pm_lance_flag` (0x4e5f90: the bits 30 and 29 and the marker made or freed) and `test_pm_pocket_lines` (0x469a70 and 0x469bf0, the panel's new lines). 33 breaks: 30 caught, one of them after the follow path's test gained a case for it. The three left: `own != target` needs the marker to be the lancemate's own, which the game can't make; the saved target's test on +0xbc has a second check on the same field just after it, so the break changes nothing; `key_map = nullptr` is written just before the mode is deleted.

Moving the test found what the MW3 classification hid: PM's commander isn't MW3's with new addresses. Ported with it: picks are commands 0x5d..0x60, keys 1..4 bound in a copy of the key map pushed for the mode, so the throttle's other keys work there; the panel's lines show those keys; the orders' keys are read at each panel from a renumbered table (the port still held MW3's numbers); the player's key handler 0x4291a0 ignores keys in the animation states 12 and 13.

Reading only: the panel's pick by +0x70 (the mates, not the list); `program_own_marker` (what the port builds for the marker, the tests stand in for it); 0x46c920 answers 0 where MW3's answers 1, which the port's FIX for the double delete already amounts to. `test_pocket_panel`, `test_pocket_frame` and `test_pocket_keys` are not moved: the frame 0x46b980 keeps about a quarter of MW3's lines and the keys 0x46cc50 two thirds.

## The switch's plan (2026-11-07, for Owen and the lead: nothing flipped)

`mw3pm` runs MW3's AI as a stand-in. Nothing outside the `zai_*_pm` files and their tests calls PM's AI: the six hook classes (`AttackPmHooks`, `PendingPmHooks`, `JumpPmHooks`, `LinkPickPmHooks`, `SteerPmHooks`, `WalkPmHooks`, 105 virtuals) have no implementation in the program.

### 1. What the first campaign can reach and isn't ported

From `tools/analysis/pmrecount.py --list`, every function listed in `docs/pm-switch-list.md`. It's static reach, direct calls and the vtables a function installs, inside 0x4b8000..0x500000, so it overstates a mission (network code is in it) and understates the AI's whole cost (the mech and weapon code it calls lies outside the range). "Named" means the port or a test names the address: it doesn't show a function is right, and a function under a tested root isn't named.

| From the brain's tick 0x4fab90 | functions | instructions | ran in the trace |
|---|---:|---:|---:|
| ported for PM | 163 | 17,437 | 80 |
| ported from MW3, PM's the same | 39 | 673 | 1 |
| ported from MW3, **PM's differs** | 220 | 13,586 | 23 |
| **not ported** | 387, and 63 vtable slots with no code | 17,453 | 89 |
| all | 872 | 49,149 | 193 |

The 112 that ran without a PM port are 9,105 instructions, the first to do.

The attack state's own 334, the recount's set, by the same classes:

| | functions | instructions |
|---|---:|---:|
| ran | 72 PM, 2 MW3 differs, 24 none | |
| **listed, didn't run: the 58** | 14 PM, 7 MW3 differs, 37 none | 1,678 / 889 / 2,120 (4,687) |
| **unlisted: the 178** | 34 PM, 14 MW3 same, 52 MW3 differs, 78 none | 1,834 / 155 / 1,980 / 1,254 (5,223) |

Not ported, by the source files they lie between: zainetmechinout..zaitarget 152 functions, 5,881 instructions; weapon..zainetedit 56, 4,469; zrdrvalve..class.c 49, 2,174; zaitarget..zrdrainet 45, 1,419; zrdrainet..zrdraiv 13, 1,401; zrdraiv..zrdrvalve 10, 762; the rest 62 by file, 1,347.

The 387 are a known size. The 220 are the risk: they run today as MW3's port with PM's changes missing, and nothing fails: that is how the commander looked until its test moved. `test_sensors` isn't on this file's table either. Of the 220, 19 are named by a PM test. Each needs its PM side read or tested before the switch, or a line saying why not.

### Single player against network, and the remainder (2026-11-07)

Split of the two counts above by what's reached only behind a network check. The AI calls out of its range (0x4b8000..0x500000) to no network source file at all (znet_dplay, znet_session, the lobby or connect screens, mech_net): its only network checks are the game type (the game object [0x62c40c]'s +0x6c, vt+0x28: 3 is network) and the multiplayer player (0x47b710). Within the tick's reach those are 0x4e7170 (a vehicle's sensor range: a network server takes [0x60bd9c]), 0x4e2820 and 0x4e2f50 (a target's death: 0x472b40, a multiplayer player's mode 6, when the target's +0xe8 isn't negative). None gates a function of its own on the list; only a few instructions are network-only inside functions that single player runs too.

| From the brain's tick | single player | network only |
|---|---|---|
| ported from MW3, PM's differs | 220 functions, 13,586 instructions | none as functions; inside 0x4e2f50, the 6 instructions of its multiplayer line (0x4e2fe2..0x4e2ff2) |
| not ported | 387 functions, 17,453 instructions | none |

By reading the call graph and the checks named; a check that reaches the network by another object (a vtable slot that only a network class fills) wouldn't show here.

**The remainder (Owen, 2026-11-07: "for posterity, i would like to convert as many functions as we can").** After the first runs, everything left of PM's differing and unported functions is ported and tested too, network included, in the order the runs set: what affects play first, then the rest. As a final group on this list it is, from the tick: the 220 differing and the 387 not ported less what the runs' groups take, today 607 functions and 31,039 instructions before them; and beyond the tick, PM's multiplayer (docs/pm-map.md, PM's multiplayer against MW3's) and the rest of the program, counted by `tools/analysis/pmmap.py` per file.

### 2. A first run

On a throwaway branch from `pm`, never merged: the tick and states wired to the PM classes, an environment switch `PM_AI=1`, the hooks real or logged as unported. The size, counted 2026-11-07 once the brain was in: 18 hook classes in `src/mech3/zai_*_pm.h`, 307 virtuals, not the six and 105 this plan first said, with some counted twice (the same call in several classes, Jump and Walk inheriting Steer). What's still to build before it runs: every hook body (the only ones today are the test stand-ins in `tests/probe_pm_ai_*.cpp`); a mapping from the plain structs (`BrainTickPm`, `OrdersPm`, `TargetsPm`) to `Brain` and `AiTarget`; the code that reads `PM_AI`, which is nowhere yet; and the first-hit log, which isn't built. Dev 2 has the wiring, on a `pm`-based branch, filling in these interfaces from its own files; the estimate in sittings comes after it has seen its first class. One mission, `c1m1` idle, the trace's own run, forked and headless like `pm_starbase_refit`, 255 s of mission time at 53 bits, the stand-in's run the same way as the control.

| Compare | Good | Bad |
|---|---|---|
| The trace's 193 AI functions that ran in the missions | the port hits those it has, by a first-hit log of the PM addresses; a score against c1m1-idle | a function the exe ran that the port never reaches; one the exe never ran hit often |
| Unported hooks | each fires only where the list expects | a hook fired that the trace says matters, its count |
| Faults | none, and no NaN or infinity in an AI mech's position, heading or speed | any |
| What the AI does | the fight near the base by the trace's clock, 4:32 at the end, in the same place | mechs standing still, leaving the ground, state transitions stuck |
| Against the control | differences each explained | a mission that can't end, an objective changed |

The first run only needs to reach the end, and each miss becomes an item. It can't show play identical. The trace records first hits, not positions, so no moment-by-moment compare exists. That needs a run of the exe logging each mech's place and state once a second (the PC, the lead's to ask), and then a compare by event, because the two sides' random numbers differ.

### 3. What that run closes

| Item | Closed by the first run? |
|---|---|
| The real hooks in `app_mission.cpp` | Partly. It writes them and shows which fire; `set_behaviour`, `rand_int` and `reach` among them. Each needs its compare, and the branch doesn't land. |
| +0x588 and the fuel | Half. The jump state 0x4db0e0 ran in c1m1-idle's mission, so the run shows the port's values in a real jump, but the trace holds no field values: the same watch on the exe closes it. |
| Mission 5's bases | No. Another mission: `PM_MFB=4700,-4200`, `PM_KILL=0,1,5`, with PM_AI=1, and the exe's flags to compare. A second run after the first. |

Also left at the switch, from this file: every field of each struct the AI port owns compared once or a line why not; every break for these pieces rerun on the switch's evaluation; the three commander tests above; the 53-bit precision kept on.

### The first steps, decided (2026-11-07)

The lead's call: before the first run, port and test the 112 that ran, biggest risk first, at today's bar. The other 197 differing functions wait for what the run shows; the 387 less those stay listed, not ported speculatively. `test_sensors`, `test_pocket_panel`, `test_pocket_frame` and `test_pocket_keys` move as their code is reached.

| Group | Functions | Instructions | MW3's test to start from |
|---|---:|---:|---|
| Firing, 0x4b8ce0..0x4bd920, all under the step 0x4bb700 from 0x4fb3d0 (aim, pick, shot and step done, the rest named under Firing's step) | 46 | 5,039 | `test_ai_aim`, `test_ai_plan`, `test_ai_firing` |
| The brain and the tick, 0x4f5030..0x4fc1c0 | 19 | 1,055 | `test_ai_brain` |
| The states, 0x4d4010..0x4dfe90 | 26 | 1,518 | by state |
| Nets, 0x4c6ab0..0x4cb730 | 13 | 873 | |
| Targets, 0x4e2f50..0x4ea5f0 | 8 | 620 | `test_sensors` near it |

Firing is first and more than half. It isn't MW3's changed: PM's step keeps about a fifth of MW3's 0x4b5b80's lines and 0x4bd920, 412 instructions, is `std::sort` on the shot's records, not game logic. Its parts, bottom-up: the aim (0x4badc0, 0x4bb040, 0x4bb290, 0x4bb580, 0x4b9f00), the plan's pick (0x4bc2e0, with 0x4bbfa0, 0x4bbe30, 0x4ba030, 0x4ba190, 0x4ba340 and their helpers), the shot (0x4bcd40, with 0x4bd920, 0x4bcad0, 0x4b8f80, 0x4bc930), then the step and the plan's virtuals (0x4ba9e0, 0x4baae0).

### Firing's aim (2026-11-07)

`test_pm_ai_aim`: 0x4bb040, 0x4bb580, 0x4badc0, 0x4baf40, 0x4bb290, 0x4013d0 and 0x4f8dc0 real on the exe, `zai_firing_pm`'s `AimPm`, 20,000 cases at 53 bits, exact from the first run, every re-pick branch reached (the offset carried exactly onto the target by a directed case). 27 breaks, 25 caught. The dot's sum order decides branches (`along > 0`, `limit2 < o2`), so it's forced: an offset whose square sits on a float's rounding edge across a power of 2, against a limit on that edge, and a line that cancels so the last term alone sets the side. Caught then. The two left, in `unit3`, reach only stored values: the root's sum order moves a double by a last bit, and the one comparison on the range, the hold at 100, gives the same 100 either way. They rest on reading the asm. Not tested: 0x4b7800 stands in (a blocked weapon line leaves the exe on stack garbage), the hooks that read the brain and vehicle, and the constructor, which comes with the step. **Deferred, still one of the 112:** 0x4b9f00, the fifth in this group, waits for the step: its numbers [0x62f8d0..0x62f8e8] are written by a loader not yet traced. 0x4b7800's blocked line, when wired, can't match the exe's stack garbage: it takes a safe answer as FIX (bug) with a fixes.md row.

### Firing's plan's pick (2026-11-07)

`test_pm_ai_pick`: 0x4bc2e0 with 0x4bbfa0, 0x4bbe30, 0x4bbf20, 0x4ba190, 0x4ba340, 0x4ba030, 0x4bc230, 0x4bc260, 0x4bc2a0, 0x4ba970, 0x4ba990, 0x4baaa0, 0x4bd2e0, 0x4bd680, 0x4bd6c0, 0x4bd6f0, 0x4bd710 and the sort 0x4bd760 real, `plan_pick`, 6,000 cases at 53 bits, exact, every part mode. 53 breaks, 52 caught by a compare (one after adding the empty list with a wish kept); the other was dead code, a flag on the last subtraction, and is gone. Stand-ins: 0x4e5d00 (line of sight), 0x5cb260, the threat lists 0x4b2820 and 0x4b2890, and the entities' virtuals. The sort's heap is only run at one place; the port keeps its effect, not its code, so the branches of 0x4be050 aren't reached in either. Ties are pinned: contacts alike with the vehicle refusing some, so the whole order after the sort shows in the targets asked for, 807 cases; a later equal doesn't displace the first, and each better one swaps with the front. Breaks of that rule (`<=`, a rotate for the swap) are caught. The start-up's 0x4ba110 and 0x4ba140 run in the test, not in the port, which has them as constants. The plan's virtuals come with the step.

### Firing's shot (2026-11-07)

`test_pm_ai_shot`: 0x4bcd40 with 0x4bc930, 0x4bcad0, 0x4bca70, 0x4bd170, the removes, `std::sort` 0x4bd920 (`crt::msvc_sort`), 0x4b8d30 real, `shot_pick`, 20,000 cases at 53 bits, exact, volleys of one to many. 34 breaks, 28 caught by a compare. First pass 22; the misses that were gaps got directed cases: ammo exactly at the keep level, the room exactly on a weapon's number, equal numbers, the multiplier 0 so the sort's ties show, and a sum that decides as a double but not as the float it's stored in. The 6 left are equivalent: a division of equal numbers is 1, two forms of the "some but not all" test give the same list, the cheapest of one, the single fit through the other branch, and `std::stable_sort` matches at 16 weapons or fewer, where the exe's sort is an insertion sort. The port doesn't lean on that: it keeps `crt::msvc_sort`, the exe's own sort at any size, though the test stops at 16. Past 16, read: the shipped game can't get there (loadouts of 4 to 12; the lab stops at 16, TAG counted). Three ways in, all outside it: a mod's mech file with 16 weapons and TAG (the reader clamps weapons to 16, TAG goes in by 0x4b0460 unchecked), a hand-edited saved loadout, a network peer's config. Then the add writes the 17th over the manager's next field; weapons.md, Latent overflow. That's the limit to fix for mods, not this sort. Stand-ins: 0x4b7bd0, 0x4b8f80, 0x4b7b40, 0x4b7b90, the group's calls. The test leaves out a vehicle with no brain: the exe faults on null there (`fld [ebp + 0x38]`), the port reads zeros; a firing's shooter always has one.

### Firing's step (2026-11-07)

`test_pm_ai_step`: 0x4bb700 on the exe, `FiringPm::step`, every state, 3,000 cases of 40 frames at 53 bits, exact, with all it calls stood in on one script of answers. 40 breaks, 40 caught, 7 of them after adding ties (no time between frames, heat on its limit), start states (busy, already fired, seen, a far time) and +0x40 to the log. The step's `fire_now` is 0x4bbdc0, inlined; 0x4bb6b0 is its head. MW3's `volley_pick` is still in PM's exe as 0x4b56d0, but only the plan's vt+0 0x4bacc0 leads to it, nothing in the step calls that slot, and no trace run hit it.

### Firing's plan and weapon checks (2026-11-07)

`test_pm_ai_plan`: 0x4baae0, 0x4ba9e0, 0x4ba0d0, 0x4baac0, 0x4b9de0 with 0x4b9d80, 0x4b8f80, 0x4bd1c0 and 0x4b9f00 on the exe, 16,000 cases at 53 bits, exact. 37 breaks, 36 caught by a compare; the last drops 0x4b9f00's "the target itself" test, which a contact's dot with itself passes anyway. 0x4b9f00 isn't a loader's: its numbers are the start-up's 0x4b9e40(10, 30, 10, 200). The test runs that start-up on the exe, with `_hypot` as `sqrt(x*x + y*y)` on the x87 (it lives in the C library, so this stands in), and compares the table bit for bit. That found a bug in the port: the exe takes the rise from the top unrounded, the port from its float, 4 in the last bits of +0x20, now fixed. Moving `_hypot`'s answer 8 doubles either way changes neither stored number, so the table doesn't depend on which library the player has. Firing's group of 46 is done, stand-ins aside; the pieces meet only through hooks, so no test runs them together yet. Not ported: 0x4bacc0 and 0x4b9fe0, the plan's vt+0, which nothing in the step calls and no trace hit.

### Tests that agree with themselves: a pass (2026-11-07)

After the cone's table. Two looks at the `test_pm_ai_*`, lance and pocket tests. Every write to a fixed address in the exe is a pointer, a clock or a world value, not a constant worked out in Python; the cone's table and the aim's scale were the only two, both now from the exe. The start-up table (0x602174, 152 entries): of its entries in the AI's range, every one that sets a global a ported function reads is run by a PM test (0x4ba100, 0x4ba130, 0x4bad30, 0x4d2d30, 0x4daef0, 0x4dbf70, 0x4dbfb0, 0x4dc010, with 0x4b9ed0). The union across tests, not per test: a test that doesn't run one reads zero on the exe's side against the port's constant and would differ, so that case shows itself. A start-up's write through a pointer isn't seen by the scan. Nothing else to fix.

### PM's follow, found (2026-11-07)

The lancemate's gap in the 112. MW3's getter 0x4c0de0 maps to PM's 0x4c7920, slot 0 of the vehicle's net; PM's 0x4c7030 makes it first (vtable 0x5ed38c), and its make_state vt+0x8c is 0x4d91e0, which builds 0x4daaf0, vtable 0x5eecd4. That's a fight state on `FightStatePm` (0x4d9b10), run by 0x4dabf0: MW3's straight-line `FollowState` is gone from this slot, and PM's follow walks the walk states, which ask the link pick. So on PM a following lancemate can reach a jump link, as Owen remembers; shown by the class it's made from, not run. Size: 0x4d91e0 30, 0x4daaf0 48, 0x4dab80 13, 0x4dabf0 34, 125 instructions. 0x4daaf0 and 0x4dabf0 are ported, `follow_start` and `follow_run`, and tested in `test_pm_ai_walk` on the exe, 0 differ; 6 breaks, 5 caught, one of them after a case with the target gone. The sixth drops the start's target check, on a context that's thrown away. Left for the wiring: the thin factory 0x4d91e0 and the delete 0x4dab80.

### The states (2026-11-07)

In `zai_walk_pm`, compared in `test_pm_ai_walk` at 53 bits, exact. None of it runs until the switch.

- **Behind the target** (`BehindPm`, 0x4d8580, 0x4d8690, 0x4d89d0, vtable 0x5eebc0): a place 0.9 of the brain's +0x44 back from the order's target, by the way it faces, driven to while it stays within 20 m of the place picked.
- **Circling** (`CirclePm`, 0x4d8ca0, 0x4d8d30, 0x4d8dc0, vtable 0x5eebe4): round the target at the brain's +0x44. The way is the tangent with a share of the way in by how far off the circle it is, the sign of +0x20 the side and its size the time of the last change. The boundary is asked about a point three stopping distances ahead (10 m at least) and only says yes or no: on a yes the throttle is 0 and the side changes, and two changes within a second clear the brain's +0x3c. The exe works out the point on the circle as well and never reads it, and the context's +0x18 (0x4c7950) isn't read by this state, so both are left out.
- **Guarding** (`GuardStatePm`, 0x4dfd00, 0x4dfe40, 0x4dffb0, vtables 0x5ef2c4, 0x5ef2e8): keeps a way to a picked point while the way's end stays between the brain's +0x2c and 1.3 of its +0x44 from the target. The way out to a side state (0x4dbe00, step 1) needs the state's +0x2c, which only the constructor writes, with 0: by reading it can't be taken, the trace never ran it, and it isn't ported. The guard's destructors only free that side state.
- **The attack's constructor** 0x4decb0: `AttackStatePm::start`, its context only lending the vehicle to 0x4d9be0.

Compared through other tests: the jump context 0x4dae80 is called inside the exe's 0x4daf40 and 0x4db0e0 in `test_pm_ai_jump`. Wiring with no logic: the factories, the destructors, the STL deque helpers 0x4d7750 and 0x4de9d0. 0x4d5a10 goes with the brain, being MW3's `ScanState::run` on PM's brain.

0x4dffb0 was counted as done for a while: the address was in a comment on its context. A count of what's ported wants `@ addr` or a declaration, not any mention.

### The brain and the tick (2026-11-07)

`zai_brain_pm`, compared in `test_pm_ai_brain` at 53 bits, exact. Three parts, none running until the switch.

- **The frame** 0x4fab90 against `tick_pm`, 6,000 random frames with everything it calls standing in, the calls in order and the brain's fields compared. It keeps MW3's shape. The changes from MW3's 0x4e9710: the hurt flag in the params' +0xcc is gone; a hit that sets off a reaction also hands the worst attacker (0x4b2820) to the brain's vt+0x28; MW3's `own_order_run` and the attack's bookkeeping left the frame (0x4fb260, 0x4fa150). 0x4b8f50's ammo value stays a double in the compare. The FIX for no vehicle and a NaN range is MW3's row, carried over. The net's vt+0x18 after 0x4fbf30 is a getter whose answer is dropped: left out.
- **The smaller pieces**, each real on the exe with its neighbours standing in: `may_engage_pm` 0x4fa5a0 (mode must be exactly 1, where MW3 takes any non-zero; the health floor is asked each time, not once a frame), `manoeuvre_pm` 0x4fab30, `on_hit_pm` 0x4fbc20, `died_pm` 0x4fbdd0, `way_reset_pm` 0x4fbe90, `scan_run_pm` 0x4d5a10.
- **The orders**: the exe's own deque and pending list (0x4fafe0, 0x4faff0, 0x4fbf30, 0x4ea220, 0x4f9e70) run real against `std::deque` and `std::list`, past a block of 31 and back, with what they run standing in: `may_fire_pm` 0x4fa6d0, `orders_idle_pm` 0x4fa470, `order_parts_pm`, `pop_order_pm`, `replace_orders_pm` 0x4fa860, `run_orders_pm` 0x4fb260. The first run of the test found the pop takes up the order under it (vt+0x10), which reading the frame hadn't shown.

`on_hit_pm` differs from MW3's 0x4ea520 twice: the base's hit record comes last, so what's read is the record before this hit; and allies turn on the player only for hits lasting over 3 s, the last within 7 s, more than 10 in all, against MW3's 3 s alone.

The clock in that test, 0x8c1b50, is another clock from 0x62da90: what sets it hasn't been read. One-liners with no logic of their own, wiring when the switch comes: 0x4f5030, 0x4f5f00, 0x4f5f10, 0x4f9d80, 0x4f9f60, 0x4fa150, 0x4fa170, 0x4fbe60, 0x4fbe70, 0x4fc130, 0x4fc1c0.

### Nets and targets (2026-11-07)

`test_pm_ai_brain` again, the same bar.

- **The vehicle net** 0x4c6f90 runs real and its ten slots' vtables must match `kVehicleSlotsPm`; `kind_behaviour_pm` 0x4c7800 and the getters 0x4c7920, 0x4c7950, 0x4c7980 on random live slots. The base 0x4c6ab0 runs inside that, its vector growth STL. What the slots' behaviours are is `test_pm_ai_nets`.
- **The way's pieces** 0x4c9220, 0x4c9260, 0x4cab20, 0x4caca0, 0x4cad50, 0x4cae10, 0x4cb730 are inlined in `zainet_inout`'s ports of their callers and run real on the exe in that module's tests; constructors 0x4c7e10, 0x4c7f30, 0x4caa60, 0x4cade0 likewise. Checked by call sites, not by a test of their own.
- **The lead point** `lead_point_pm` 0x4e7210: MW3's three ways to the time, at the register's width, on 6,000 random cases, every way reached. Two changes from MW3's: a mech shooting leads by its smoothed view of the target's velocity (its +0x100, aim_part), not the velocity itself, and with nothing to lead (no weapon, instant, no speed, no muzzle) the point moves by that view's error, where MW3's stays.
- **The target change** `set_target_pm` 0x4e6330: a new slot +0x78 keeps the last ally with bit 4 of its +0xbc, while the target's owner hasn't bit 5.
- **The stop, a new order and the pending list**: `stop_orders_pm` 0x4ea5f0 tells the last order's state (vt+0x0c) and pushes an empty order; `give_order_pm` 0x4ea740 and `push_behaviour_pm` 0x4ea7f0 build on it (0x4fa150 and 0x4fa170 are their doors from the brain); 0x4ea0f0 and 0x4ea180 are `std::list` push_back and push_front, on the exe's own list in the orders test. A strict pass (`@ addr`, not any mention) found 0x4ea740 and 0x4ea7f0 only named in hook comments, as 0x4dffb0 had been.

Not ported, by reading: 0x4e2f50's one addition to MW3's kill, 0x472b40 on the target's +0xe8 when that isn't negative. 0x4e30d0 takes damage only while it's negative, so a target that can be hurt never gets there; what 0x472b40 does is a mode for the player object of multiplayer (0x47b710). No writer of +0xe8 was found in 0x4e0000..0x4e4fff. 0x4e9c80 is `AiParams`' copy, its size already PM's (`target_words`). 0x4e45c0 and 0x4e4b00 are in `zai_valve` (`test_pm_ai_valves`). 0x4ea070..0x4ea0e0 are the list's small STL pieces.

Breaks over the states, the brain, the nets and the targets: 168, of which 166 caught, on `test_pm_ai_walk` and `test_pm_ai_brain`. The first run missed ten that showed real gaps, each closed and caught on the rerun: the behind state's plans had reach 0 and sent nothing to compare, the circling state's probe point wasn't compared, the ammo's compare wasn't between 0.1 and 0.1f, the engage range had no case near it, and the case with no time at all wasn't reached. The two left: `bh21`, a step past 3, which nothing can set; `l8`, 0.001 as a double or float, which no stored float falls between. That run also showed the port held 0.001 as the float's value where the exe's constant is the double: fixed, though no float can tell.

`test_sensors` doesn't move with this group. Its PM twins 0x4e6140 (the sort) and 0x4e6240 (the allies' turn) didn't run in the trace, so they aren't among the 112; `allies_turn` stays a hook and they stay listed. `test_pocket_panel`, `_frame` and `_keys` weren't reached either.

### A bug the wiring found (2026-11-07)

`can_hit` 0x4b8f80 kept the muzzle in the part's frame: 0x4ff3c0 turns its point into the world's in place, and the port's hook took it as const. `test_pm_ai_plan` passed because its stand-in didn't move the point on either side; both sides now move it, so the test sees the difference. It shows the limit of a stand-in: it can't catch what it doesn't do. `firepoint_reaches` 0x4a7000 and `torso_reaches` 0x467490 are compared real in `test_pm_ai_brain`, 4,000 cases of each, `_CIasin` replaced in the emulator by the same x87 formula.

### The weapon's line (2026-11-07)

`weapon_line` 0x4b7800 in `zai_firing_pm`, compared real in `test_pm_ai_brain`: 3,000 weapons with or without a part, node and firepoint, a salvo or not, a point or none, the debug byte [0x62f86c] on or off, every tenth with the tolerance met exactly; the node's placing 0x4ff3c0 stands in alike on both sides. 13 breaks, 12 caught: the order of the three products in the dot (`wl12`) isn't, since the sum is stored as a float and the doubles' difference doesn't reach it on these cases. The blocked line's FIX row is in fixes.md. The wiring's `weapon_cos` and the aim's `weapon_line` take it next, in place of `Weapon::aim`.

### Named checks for the first run, from Owen's play of PM mission 2 on the stand-in (2026-11-07)

Both on c1 mission 2 with `PM_AI=1`, next to c1m1, and both by reading so far, nothing run.

**The chase: do the enemies leave Op Able and reach the canyon crossing?** Owen: "in the original, they met me at the canyon crossing and killed me before i could cross. now theyre staying by op able and doing nothing." What starts it, from `readerm2.zbd`: no order and no script. Clint (`clint_01`, 2433 -4347) and Puma (`puma_01`, 2043 -4247) have no net in their records and vlists that only fire on damage; the valves touch Clint once, `jump_made` setting attack strategy 2 when the player reaches a `reg_canyon_*`, after the crossing; no text for them turned up in `interp.zbd`. That leaves the attack state's own pursuit: sight ranges 1050 and 2000 reach Able (3304 -4659), and `net_15` runs through there, with a jump link and `via gorge_01` nodes beside it (nodes 00, 01). So it should need PM's attack, but a stand-in fault isn't ruled out: their strategies are 1 and 5, and MW3's switch on the brain's +0x40 (0x4c0ba0, `VehiclePart`) has cases 1..4 only, so 5 gets no behaviour. PM's reader of +0x40 wasn't found by searching, so whether 5 means something there is open.

**The lancemate's jump: does Epona cross with the player when fitted with jets?** Owen: "my lancemate was outfitted with jump jets but never crossed the canyon with me. shes supposed to do that." What makes her jump, by reading: a net's jump link, the same `net_15` node 01 (`jump 1 25.0`), taken by the way finder only with jets; then the jump state. On the stand-in that's `ValueStep::passable`, on PM the link pick 0x4d60d0 and `zai_jump_pm`. The real loadout is read: `Pilot::jump_jets` is `mech_jump_jets`, `equipment[11]`, the built mech's count, and PM's jump state `has_jets` is +0x3e8, the same field; to confirm in the run that a lancemate's mech is built from the player's loadout, not the stock one. Her strategy is 5 too. The stand-in has the link, the jets and a jump state, so reading can't say it must wait for the switch; which step fails there is unknown: a short stand-in run with her ordered to follow would show it. No valve orders her across: `jumpingspeak` is only her line.

**Run on the stand-in (2026-11-07):** `PM_M2_WATCH=3360,-4640 PM_KILL=0,1,2 PM_M2_FOLLOW=epona_rhi` on `pm_enemies_die_under_real_fire`, 100 s of mission, with `PM_M2_ACROSS=3090,-4646,900 PM_M2_JETS=4` for the crossing. The player is dropped by Able and stands still, or is moved across. Epona's jets are set by hand on the launched mech, since a test launch has no campaign lance. No save of Owen's and nothing on the exe.

| Who | What happened | Where it fails |
|---|---|---|
| Clint | Takes a target by 951 m, walks to the canyon's edge in `CircleState`, circles the player at 330..480 m. In one run he walks off the edge and dies at y -57 | He comes. The stand-in doesn't keep him out of the canyon |
| Puma | `NodeState` and manoeuvres, wandering around 1800 -4450, no target at 1,490..1,726 m though her sight is 2,000 | Never engages; why isn't found |
| Epona | `FollowState`: straight at the player. With the player across and jets 4, she walks off the edge at 3256 -4657 and dies at y -57. Never a jump | MW3's follow is a straight line, and only the walking states ask the way finder |

**Strategy 5** (on the exe logger's list: a watch on the brain's +0x40). Every integer access of `[reg + 0x40]` in 0x4b8000..0x500000, 89 functions by the asm, and the three `lea`s checked: 0x4f5570 writes the brain's +0x40, and nothing found reads it. MW3's one reader, `VehiclePart` 0x4c0ba0, has no twin in the address map. So strategy 5, and any value, looks unused in PM, which would rule it out for Puma and Epona. That rests on reading: the base register of each access wasn't traced, and a watch needs the running exe (the logger's route, or the PC).

**The lancemate's loadout.** Read: `mech_set_config` calls `mech_configure`, which clears `equipment` and rebuilds it from the config given, so a lancemate has the config the launch hands her, if `roster` matches her mech; otherwise it's refused. Not run through a campaign save.

**Verdict: both wait for the switch.** On MW3's code the jump is a step on a net, and the way finder's link pick is called only by the walking states (MW3 0x4cf140, 0x4cfad0, 0x4d00a0; PM 0x4dcfe0, 0x4ddab0, 0x4de0e0), so a following mech can't take it. Teaching the stand-in's follow to use the link would be a stand-in for what PM's own state does (PM's follow, above). Keep the chase and the jump as named checks. Added to them: nobody walks off the canyon's edge, and Puma takes a target.

## A logger for the exe (2026-11-07, design only: nothing built, nothing on the PC)

What it's for: PM's places and states once a second, to compare by event against the port, and the jump's +0x588 and fuel. The smallest thing that does it is a logging form of `tools/weapon-patch/proxy`.

- **Where:** a copy of PM in its own folder, with `IFORCE2.dll` renamed and the stand-in in its place, as `install.cmd` does. PM imports IFORCE2 as MW3 does. The disc's exe stays untouched, and deleting the folder undoes it.
- **Which exe:** by sha256, the trace's no-CD b28ecb70 alone, anything else left alone with a line in its log, as the proxy refuses unknown exes.
- **The hook:** the brain's tick has two callers, 0x4f8f4b and the thunk at 0x4fab7b, both `call 0x4fab90`. Change those two rel32s in memory to a wrapper that calls the tick and then logs. Nothing in the tick moves, no entry bytes are copied, and the two writes are the only change. `0x4fab90` is thiscall; the wrapper keeps ECX and the stack and returns EAX untouched. It must save the x87 state too, as the trace's first version showed (floor(NaN) after a tracer that didn't).
- **What it logs:** one line per brain, per mission second of [0x62da90]: the clock, the brain, its mech, the place, the heading if it's cheap, the state (+0x59c), the fuel and +0x588, the health, the brain's current state and order, the target. Read straight from memory, with no calls into the game, under `__try`, so a bad pointer turns the logger off rather than the game. The place has to come from the mech's node; the exe's 0x5cb260 may rebuild a matrix, so it isn't safe to call. Which fields hold the heading, state and order is to be read from the map first.
- **The file:** a text file in the copy's folder, written through a buffer of its own and flushed each second, so a crash loses at most a second. A size cap stops it.
- **What it can't do:** anything about the random numbers. The two sides' `rand` differ, so the compare is by event (who fought whom, when the first shot, who died, where).
- **Also to watch:** reads of the brain's +0x40, to settle what strategy values do in PM (a data breakpoint, or the logger reading it beside the state; pm-tests, Strategy 5).
- **Checks on the Mac:** `test_proxy.py`'s way: the built DLL in the emulator with the exe's pages really protected, DllMain run, the two rel32s checked, the exe's tick run through the wrapper with registers, flags and x87 compared to a run without it, breaks of the source, refusal of another exe.

Left to work out before building: the fields above, and whether 0x4fab90 runs for every AI mech in a mission at a rate that's fine for this (`port_hits` 621,773 over the port's runs says the wrapper must be cheap between seconds: one float compare per mech).

## The cloud's death band

`test_pm_window.py` runs 0x4b2b40 and 0x4b2c00 on the exe against `cloud_kill` and
the vehicle's kill: 448 cases at the band's edges, on and off, active, dead, the
pilot's mech and another. `pm_canyon_death_band` (`mw3pmtests`, always on, forked,
nothing on Windows) walks the player off the rim of the real c1m2: dead at y -30.43,
Mission Failed, one AI mech dead the same way. 14 of 14 breaks caught. The
camera 3 call and the fall sound rest on the port of the death prep and on reading:
the mission test sees the failure, not the picture (vehicles.md).

**Settled by a tighter search (2026-11-07), reading, PM's exe:** the weapon definition's `ammo_limit` (+0x18) and `beam_energy` (+0x58) are stored and never read. Definitions are 0x204 bytes in PM, from the table [0xb0daac] and `find_def` 0x5a3550. Followed from there: every read at those offsets in zWeapon, which takes the definition as an argument, is on a shot, a node or a pointer list; the weapon module (0x4b2000 to 0x4be000) holds definitions at the weapon's +0x30, +0x58 and +0xa0, the landmines at [0x628b64], and no read goes through any of them; the +0x18 in 0x4b4cd0, 0x4b5230 and 0x4b5f80 is the weapon's own, not the definition's. PM's weapons.zrd sets `AMMO_LIMIT` on its weapons, and no weapon sets `ENERGY`, so beams would hold their default of 1. MW3's same functions (0x4b0de0, 0x4b1370) read the same way, but MW3 wasn't followed from its table. No test can show an unread field; this rests on reading.

## The way along a net (2026-11-07)

`test_pm_ai_paths`, moved from `test_ai_paths`, on `test_pm_ai_nets`' nets: the nearest step, the finder, the way followed and the steps' tests of the pilot, at 53 bits to the bit, each way's length too; about 12,300 answers on 50 archives, and hand-made nets for what a random plan doesn't reach: two ways of one length, a way that shuts and is found again, a valve union, two distances that round alike, asking from off the way. 19 of 24 breaks caught. PM's distances 0x537a00 and 0x537a50 sum in the register and store once; the port now does, and the 10 m test at a search's start reads the target's vt+0x1c. The five missed can't change an answer: cutting a branch or not (2), as a cut only saves time; `still_open`'s previous step and the target (2), as no step's test reads who came from where and a node always passes; and leaving a distance unrounded in `dist_sq`, as its caller stores it. Ways in nets over 30 nodes aren't asked, as the search is depth first and very slow in the emulator. Not compared: the pilot's place (0x5cb260 stands in).

## Regions (2026-11-07)

`test_pm_ai_regions`, moved from `test_ai_regions`: every `reg_*.zrd` asked twice, net files and a missing name asked as regions, made-up files, points round each region's corners and sides, at 53 bits, where each test stops too. 238 regions, 5,712 points, 0 differ; 12 of 13 breaks caught. The miss, the loop's version test, can't change an answer: the builder only calls it at that version. The inside test's sum is held in the register and stored once, as 0x4d0e50 does; a float sum fails this test. The net's file reader's stand-ins are `test_pm_ai_nodes`', whose region stub is now switched off here.

## The calls on the pilot (2026-11-07)

`test_pm_pilot_games`, moved from `test_pilot`: the pilots' list, new pilots, every game's fields, the stores' add and take, saved campaigns, the save after a mission, renames and damaged files, on the exe's calls and files against the port's, 150 plans by default, 800 identical on a longer run, 13 of 15 breaks caught: of the two missed, a clamp at 10 against one above 9 is the same number, and a negative strings count needs damage the plans don't aim there. It found, all matching the exe now: the save after a mission asks with flags 5 and matches a saved campaign by its mission alone; the salvage is merged at the campaign's end too; the starting store goes by the mission alone; deleting the other pilots also deletes DefaultPlayer; the lab's first slot is mech 5; multiplayer's score to win is saved no higher than 10; negative counts in the standalone lineups and the strings run on as the exe does, to the file's end.

- **Multiplayer's store:** PM doesn't fill it in the game's constructor but at the lobby's lock, 0x49eb20, which is `LobbyScreen::lock_mech` (MW3's 0x49b360), tonnage limit and all. The fill is there now, at each lock, as the exe has it, so a store is empty until the lock, as in the exe. `pm_lobby_store_ready` compares nothing with the exe; it says what the port's line does. `net_lobby_*` aren't built on this branch. What PM added to that function and the port hasn't: a gate at its start, 0x49ea90 and [0x62cc38], which skips it on a world item above 1. For the multiplayer work.
- **Three damaged-file differences, explained and matched (2026-11-07):** all in a campaign's strings, 0x435820 with 0x597280, and each needs a misaligned read, so garbage as strings. The exe holds a string as a C string, so it ends at the first zero; the port kept the raw length. A length of -1 mallocs nothing, reads nothing and goes on; the port stopped. A length of 1.5 MB is allocated and read short; the port stopped at 0x10000, and now takes what the file has. A good file's strings have no zero, no -1 and no such length, so good files are unchanged. One difference was the plan's: a rename onto a name another rename had just taken.
- **Not run:** a string's damaged length past 15 MB or below -1 (the exe clears it whole; those plans say so and aren't compared, 1 in 800); a rename onto a taken name in the damage half; a line-up count above 10, past which a removal copies past the array and the exe faults.

Left for the multiplayer work, from the sweep of stored fields: `resends` [0x605fd0] and `ping_counter`
[0x5e5690]; the exe reads both, nothing in the port does yet.

## The frame's box window

`pm_player_tick_box_window` (`mw3pmtests`) holds the tick's order against
0x4a4490 and 0x4a4440 as read; the two orders are not compared against the
running exe. `pm_enemies_die_under_real_fire` is gated by `PM_KILL=index,type,number`
(c1m1 is `0,1,1`) and takes minutes: every AI mech dies under real fire, the real
bit counted after the walk each frame (on during it, off after it). `PM_LEG=<weapon index>` aims that one weapon's ready shots at
a leg through the real trigger and prints each hit: the node, part or spread, the
damage, the segment's real end, every section's health. On the ER Large
(index 5): every mech dies, the Thor in 11 to 14 shots, every hit of the player's
on a leg. The slower weapons (`PM_LEG=3`, `0`, `8`) print and aren't held to a
kill: the time or the ammo runs out. `pm_leg_down` is the short one, always on:
the Thor, the reload and, between beams, the recharge skipped, the same checks.
Open: it runs in a forked child, because a second mission in one process crashes
(pm_c2m4's note: what a mission leaves in zcls), so on Windows, with no fork, it
does nothing. `test_pm_window.py` compares a
mech's two slots with the exe, pilot's vehicle or not, with a headlight or not.

Breaks, all caught: the box never set (the reticle alone boxes 3 roots, against 13), never
restored, restored on, the walk outside the window, the player's update outside it,
the leg's scale 1.
`pm_leg_down` catches the box never set, never restored, restored on and the leg's scale 1
(the restore gone brings the fault back: 42 hits, all spread). `test_pm_window.py` catches the clear
on every mech, the gauge on every mech, no frame time, the aim after the gauge, no attach, no
detach.

## The starbase refit

`test_pm_refit.py` compares the state 0x5e4c80 with the exe on 3,000 random set-ups, every call out and the flags;
`pm_starbase_refit` (`mw3pmtests`, always on, forked, so nothing on Windows) calls the MFB from the start of mission
1: no base moves, the mech at the point is repaired and the dock ends. `PM_MFB=x,z` runs the same on any mission
(`PM_KILL=0,1,2` for mission 2); `PM_MFB_NO_CALL` is its control. Not compared: the animation itself and a campaign
store's armour and ammo through the refit; that the port's point takes `static_mfb` is checked by the mission, not
against the exe's record offset.

## On the switch's list from the starbase refit

The full boundary run (`MW3_BOUNDARY_ALL=1`, `test_pm_ai_boundary_real`) runs per world: one pass at 53 bits stopped at its 3,000 s cap with 109,880 queries on the exe, 0 differ, through c1's campaign areas and into standalone.

Mission 5's field bases stand still until a static MFB is called, then drive about 211 m (`PM_MFB=4700,-4200`,
`PM_KILL=0,1,5`, control `PM_MFB_NO_CALL`). The exe's static branch sets "may drive" too, and these bases have `vlist`
orders, so the port follows the exe's flags, but on the stand-in's AI: compare once PM's AI runs.

## For later: MW3's AI at 53 bits (after PM, on Owen's list of MW3 fixes)

`main`'s AI is ported at 24 bits; the reference runs at 53. Measured 2026-11-06 on the differential tests at 53 bits,
past each first difference, a real sign flip being a steering value that comes out the other way (e.g. -1.0 against
+0.93): `ai_lance` 479 in 5,491 runs, `ai_repair` 257 in 4,581, `ai_aim` 388 in about 7,000 tracks, `ai_attack` 42 in
8,768; `ai_steer`, `ai_plan` none. Those are upper bounds on random states: after the first flip a chain runs on diverged
state, so it counts again. In play it would be a frame or more of a wrong turn, or an order picked differently.
The fix is known: PM's `unit_flat` and `wide_sum` (`zai_steer_pm`) for the same expressions, what's stored in a float
and what stays in the register by function (pm-map.md, PM's AI at 53 bits), with the tests' compares exact at 53.
`ai_nets` and `ai_regions` differ only at 24, which isn't the reference; `ai_repair`'s crash is the test's teardown.
