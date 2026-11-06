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
| `test_ai_nets.py` | changed | 33 | 0 | to move | with PM AI |
| `test_ai_paths.py` | gone | 20 | 0 | to move | with PM AI |
| `test_ai_readers.py` | changed | 13 | 1 | to move | with PM AI |
| `test_ai_regions.py` | gone | 19 | 0 | to move | with PM AI |
| `test_ai_valves.py` | gone | 31 | 6 | to move | with PM AI |
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
| `test_lance_dispatch.py` | gone | 10 | 3 | to move | with PM AI |
| `test_light_setters.py` | changed | 5 | 5 | covered | test_pm_light_setters |
| `test_lighting.py` | changed | 18 | 5 | covered | test_pm_lighting |
| `test_matrix.py` | gone | 37 | 4 | covered | test_pm_modeldraw, test_pm_device |
| `test_mechanim.py` | changed | 22 | 2 | moved to `test_pm_mechanim.py` | mech animation, on PM's own motions; no port change needed |
| `test_mechlib.py` | changed | 4 | 2 | covered | test_pm_mechlib |
| `test_modeldraw.py` | gone | 24 | 14 | covered | test_pm_modeldraw |
| `test_motion.py` | unchanged | 2 | 0 | to move |  |
| `test_nav_waynet.py` | changed | 8 | 0 | to move |  |
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
| `test_pocket.py` | changed | 18 | 3 | to move | with PM AI |
| `test_pocket_orders.py` | gone | 9 | 1 | to move | with PM AI |
| `test_power.py` | changed | 8 | 5 | moved to `test_pm_power.py` | power; the headlight off and the network target dropped |
| `test_puffers.py` | gone | 44 | 12 | covered | test_pm_puffers |
| `test_scenewalk.py` | changed | 33 | 13 | covered | test_pm_walk (10 of its 33 functions; the rest by test_pm_nodebox and test_pm_world_box) |
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
- `NetHooks::ground`: no probe on this branch or `main` sets it, so nothing compares it. It changes a net node's height, which can matter. Owed with the moved `test_ai_nets`.
- `zai_nav`'s `World::ground`, `parked`: ours, the way finder, with nothing in the exe to compare against.

**Const check, by reading:** of the hooks taking a const pointer, the boundary is the only one where the exe's call passes one pointer as both place and answer. The ones whose bodies were read only read theirs: 0x4b0bc0, 0x56e7e0, 0x4fc1b0, 0x4d51d0, 0x4eed90, 0x41dbd0, 0x41dca0. 0x41dc70 passes its argument on to 0x465c70, which wasn't read. 0x418f30 (vt+0xdc) passes its arguments on to callees that weren't read. `region_inside`, `wall_between` (ours) and `node_offset`'s callee beyond its signature weren't read either. MW3's call sites that pass the same pointer (`inout_clamp(v, to, to)`) are written through non-const `out`. PM's 17 calls of the pull aren't all ported yet; each is checked as it lands.

**Limits:** the list comes from the hook classes in `zai*`, `zainet*` and `zrdr*`, by signature. A hook's return value isn't covered. Hooks that write a field through a call (`brain_write`, `target_masks`, `set_local_velocity`) weren't given drop breaks.

**On that scratch build, `test_ai_attack` and `test_ai_targets` part 1 fail before any break.** `test_ai_targets`' first failure is fn 10, PM's `save_target` against MW3's exe, as expected. `test_ai_attack` differs at seed 281, round 5, step 33, at the boundary's place; `zai_attack.cpp` is identical to `main`'s, so the cause is elsewhere and not found. The stand-in AI in `mw3pm` runs this code, so it's an open question, not a pass.
