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
| (new) `test_pm_mechsave.py` | PM | | | new, part of `test_mechsave.py` | a saved loadout's save and load, both formats |
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

`test_pm_debrief_chain` and `test_pm_messages` are new, not moves: `mw3pm` through three chains to its debriefings and the campaign's end, and the port's message ids against PM's table. `test_debriefing` itself (below, unclassified) stays to move, with the twod bench on PM's exe (`campaign.md`).

## The rest that cannot load on this branch

The table above went by the file naming `libmw3probe`. Importing every `test_*.py` once (2026-11-05): 144 of 259 fail at load, every `test_pm_*` loads. These 83 reach MW3's probe through another test's module and are **not classified yet**. `test_mechdef` among them is covered by `test_pm_mechdef`.

`test_actionscreen` `test_anim` `test_anim_binder` `test_anim_reset` `test_anim_run` `test_anim_save` `test_briefing` `test_config_unpack` `test_convexify` `test_crater` `test_ctlscreen` `test_cycle` `test_debriefing` `test_decal` `test_destroy` `test_field_cut` `test_fmv` `test_gmod_add` `test_hit_globals` `test_hud` `test_inout` `test_inout_query` `test_lab_list` `test_lab_screen` `test_lab_view` `test_loadsave` `test_mainmenu` `test_map_pick` `test_mech_health` `test_mech_net` `test_mech_net_apply` `test_mech_net_create` `test_mech_net_end` `test_mech_net_events` `test_mech_net_launch` `test_mech_net_respawn` `test_mech_net_send` `test_mech_net_world` `test_mechdata` `test_mechsave` `test_menus` `test_model_sweep` `test_node_find` `test_objective_check` `test_pilot` `test_pilotscreen` `test_place` `test_pocket_frame` `test_pocket_keys` `test_pocket_panel` `test_pose` `test_prim2d` `test_project_point` `test_pull_in` `test_ripple` `test_salvage` `test_salvage_screen` `test_script_setters` `test_shadows` `test_shotq` `test_ski_polygon` `test_skins` `test_store_fill` `test_tag` `test_training_lessons` `test_tree_copy` `test_twod` `test_twod_blit` `test_twod_line` `test_twod_msgbox` `test_twod_prims` `test_weapon_planner` `test_weapons_fly` `test_weapons_hit` `test_weapons_run` `test_world_params` `test_znet_create` `test_znet_dispatch` `test_znet_players` `test_znet_session` `test_znet_teams` `test_zsave`
