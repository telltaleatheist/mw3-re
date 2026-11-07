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
- **Worth knowing:** `test_pm_ai_boundary_real` screens 594,560 queries with the port and asks the exe about 7,618: every rare case the port finds (up to 100 an area) and a random sample of the rest. A rare case the port wrongly misses is only compared if the sample lands on it. Asking the exe about all of them would take about 20 minutes.

**The recount of what's left** (`tools/analysis/pmrecount.py --trace DIR`) counts a function as done when the port's source names its address outside the hook classes. Cited isn't landed: a comment can name what isn't ported, and only a test shows a function is. It's an estimate of the size, nothing more.

**On the switch's rerun, added:** the 36 of `0x4d9d00`'s breaks not rerun on the widened test. The 5 twins, 875 instructions, are done (2026-11-07): `pm-map.md`, the link pick and the manoeuvre.

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

## The cloud's death band

`test_pm_window.py` runs 0x4b2b40 and 0x4b2c00 on the exe against `cloud_kill` and
the vehicle's kill: 448 cases at the band's edges, on and off, active, dead, the
pilot's mech and another. `pm_canyon_death_band` (`mw3pmtests`, always on, forked,
nothing on Windows) walks the player off the rim of the real c1m2: dead at y -30.43,
Mission Failed, one AI mech dead the same way. 14 of 14 breaks caught. The
camera 3 call and the fall sound rest on the port of the death prep and on reading:
the mission test sees the failure, not the picture (vehicles.md).

**Settled by a tighter search (2026-11-07), reading, PM's exe:** the weapon definition's `ammo_limit` (+0x18) and `beam_energy` (+0x58) are stored and never read. Definitions are 0x204 bytes in PM, from the table [0xb0daac] and `find_def` 0x5a3550. Followed from there: every read at those offsets in zWeapon, which takes the definition as an argument, is on a shot, a node or a pointer list; the weapon module (0x4b2000 to 0x4be000) holds definitions at the weapon's +0x30, +0x58 and +0xa0, the landmines at [0x628b64], and no read goes through any of them; the +0x18 in 0x4b4cd0, 0x4b5230 and 0x4b5f80 is the weapon's own, not the definition's. PM's weapons.zrd sets `AMMO_LIMIT` on its weapons, and no weapon sets `ENERGY`, so beams would hold their default of 1. MW3's same functions (0x4b0de0, 0x4b1370) read the same way, but MW3 wasn't followed from its table. No test can show an unread field; this rests on reading.

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
