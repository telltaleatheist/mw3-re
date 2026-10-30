// The hooks the program leaves unset where the port skips the call (or
// takes 0 / null / false for its answer): each gets a stand-in that tells
// the census, under the exe address it stands for, and returns the same.
// Kept by hand from tools/analysis/hookaudit.py (hooks-audit.md);
// put in at each mission's start and frame (MissionHooks::hooks_check).
// SKIP(accessor, field, key)
SKIP(mech3::collide_calls, ff_jolt, "0x004335d0")  // CollideCalls.ff_jolt
SKIP(mech3::damage_calls, equip_voice, "0x00485390")  // DamageCalls.equip_voice
SKIP(mech3::damage_calls, net_event, "0x0046f160")  // DamageCalls.net_event
SKIP(mech3::damage_calls, stats_hit, "0x00433720")  // DamageCalls.stats_hit
SKIP(mech3::damage_calls, voice, "0x00485370")  // DamageCalls.voice
SKIP(mech3::damage_calls, zaps_hud, "0x0043be70")  // DamageCalls.zaps_hud
SKIP(mech3::dead_calls, net_update, "0x0046ef70")  // DeadCalls.net_update
SKIP(mech3::frame_calls, map_mouse, "FrameCalls.map_mouse")  // FrameCalls.map_mouse
SKIP(mech3::frame_calls, net_entity, "FrameCalls.net_entity")  // FrameCalls.net_entity
SKIP(mech3::frame_calls, net_place, "FrameCalls.net_place")  // FrameCalls.net_place
SKIP(mech3::free_calls, player_object_free, "0x0041e580")  // FreeCalls.player_object_free
SKIP(mech3::heat_calls, heat_haze, "0x0043c1f0")  // HeatCalls.heat_haze
SKIP(mech3::heat_calls, hud_message, "0x0040fe70")  // HeatCalls.hud_message
SKIP(zdec::hooks, register_restore, "Hooks.register_restore")  // Hooks.register_restore
SKIP(mech3::mech_hud_calls, zoom_camera, "MechHudCalls.zoom_camera")  // MechHudCalls.zoom_camera
SKIP(mech3::move_calls, controls_landed, "0x005ac5c0")  // MoveCalls.controls_landed
SKIP(mech3::move_calls, ff_landing, "0x00433800")  // MoveCalls.ff_landing
SKIP(mech3::part_calls, footstep_ff, "0x004337c0")  // PartCalls.footstep_ff
SKIP(mech3::pocket_calls, net_element, "0x00609980")  // PocketCalls.net_element
SKIP(zwep::run_hooks, net_detonated, "0x0072fcb4")  // RunHooks.net_detonated
SKIP(mech3::update_calls, net_update, "0x0046ef70")  // UpdateCalls.net_update
SKIP(mech3::weapon_calls, net_event, "WeaponCalls.net_event")  // WeaponCalls.net_event
SKIP(mech3::weapon_calls, stats_shot, "0x00433630")  // WeaponCalls.stats_shot
SKIP(mech3::weapon_calls, target_projectile, "WeaponCalls.target_projectile")  // WeaponCalls.target_projectile
SKIP(zcls::world_render_hooks, after_scene, "0x005a67c0")  // WorldRenderHooks.after_scene
SKIP(zcls::world_render_hooks, dirty_rect, "0x00576ce0")  // WorldRenderHooks.dirty_rect
SKIP(mech3::damage_calls, force_variant0, "0x00586ce0")  // DamageCalls.force_variant0
SKIP(mech3::damage_calls, player_died_hud, "0x0041b263")  // DamageCalls.player_died_hud
SKIP(mech3::dead_calls, net_fade, "DeadCalls.net_fade")  // DeadCalls.net_fade
SKIP(mech3::frame_calls, map_pick, "FrameCalls.map_pick")  // FrameCalls.map_pick
SKIP(mech3::draw_calls, map_point, "0x004586e0")  // DrawCalls.map_point
SKIP(mech3::pocket_calls, view_pocket, "0x004b5290")  // PocketCalls.view_pocket



