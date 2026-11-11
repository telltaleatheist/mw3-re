# The Upgrades sort (for Owen, 2026-11-10, re-sorted to his rule)

Owen's rule (2026-11-10): missing content restored (voice lines, wingmen's
calls), corrected graphics, and the sound, HUD and text fixes stay on for
good, with no switch. Gameplay or physics changes that aren't game-breaking
go behind a switch: the big, noticeable ones each get their own, the small
ones all go under one **Small fixes** switch whose list a player can read.
"Some people want to play the game exactly as it was 30 years ago. We should
be able to give them that experience."

This sorts every row of `fixes.md`, MW3's (main) and Pirate's Moon's own
(pm), into:

- **(a) always on, no switch**: game-breaking (crashes, hangs, stray memory
  and lost data), the FIX (A) / FIX (B) port fixes (running on modern systems,
  the frame-rate timings), the copy-protection traps, fixes only our own tools
  reach; and restored content, pictures, sound, HUD and text;
- **(b) own switches**: the four big ones, all on by default;
- **(c) Small fixes**: every other gameplay change, one switch, on by default;
- and rows of `fixes.md` that change nothing (candidates, ported as written).

Rows that could sit on either side are listed under **For Owen** with the
side this proposes. No code changes with this; Owen approves (or moves rows)
before anything moves.

478 rows: fixes.md's 473 table rows, its four data typos still to fix (Original bugs
still to fix) and the Small Pulse Laser's flash. 436 always on, 5 in our own
tools, 8 under the four own switches, 20 under Small fixes, 4 left as the original, 5 not changes.
Rows are MW3's unless marked PM (a row on both games is listed once, as MW3's).

## The switches

Only gameplay switches are left, so the host's settings carry to everyone in
a multiplayer game (release.md); the lobby would send them as it sends the
mods' check. With every switch off the game plays as it shipped, but for the
always-on rows: no crashes or lost data, and the content, pictures, sound, HUD
and text put back as the data had them.

| Switch | What it changes | Rows |
|---|---|---|
| MFB regroup after repair | The field bases come back to you as soon as a repair is done, and a lancemate can use them too. | 2 |
| Improved AI | Lancemates find their way round walls, rivers and corners, and keep their floor in another zone. | 4 |
| Water movement | Deep water slows a mech smoothly instead of stopping it. | 1 |
| Artemis tracking | Artemis FCS missiles get their extra tracking. | 1 |
| Small fixes | Everything in the list below: weapons, turrets and vehicles, mission scripts, the mechs' data typos. | 20 |

The turrets' pick (their count ran on across missions, so each later mission's
turrets acted less often) was the one other row noticeable in play; Owen made
it a permanent fix (always on, 2026-11-10). The reticle's pick past 800 m (a lock at the long weapons' range) is the next
most visible; Small fixes.

The bug-fix switch goes away: `bugfixes=` in mw3.cfg is read once, as all
switches on or off, then dropped; `--original` turns every switch off for that
run. The UPGRADES page (which today holds BUG FIXES and the wide menus) gets a
line per switch, and Small fixes a button that shows its list.

### Small fixes, as the in-game list would show them

The page's list (`upgrades/settings/upgrades.cpp`) is MechWarrior 3's: these
lines without the three Pirate's Moon ones (6 to 8, on Pirate's Moon's own
list), in shorter words to fit the page. The list shows exactly what the
switch turns off: `small_fixes_list_matches_its_gates` (tests/test_fixes.cpp)
holds each line's gates by file and fails for a line with no gate or a gate
with no line. Checked line by line on 2026-11-10: LOCK_ON_LEAD's code had no
gate (off the list, always on); the reticle's lock past 800 m read the
always-on flag by a mapping slip and reads `small_fixes` now (hud.cpp); TAG's
beam hold at any frame rate (weapon.cpp) read `small_fixes` with no line and
is always on, as its twin in zwep_beam.cpp (frame rate to time).

On this branch the page's list is Pirate's Moon's, nine lines: 1, 3, 4, 5,
13, 14, 16, 17 and 18. Left off: 2, 19 and 20 (MW3's data: the shared code
finds nothing to change in Pirate's Moon's files), 12 (Pirate's Moon's table
has the plain PPC), 15 (Pirate's Moon's exe moves the selection, 0x004b6f70;
the fix is not on this branch), 10 (this branch's HUD has no such change),
6 and 11 (made without a switch in the code, here and on main), and 7 to 9
(the lead, 2026-11-10: the three guards stay always on and unlisted, a crash
or an unset value having no original to give back). The page has four
switches: no Water movement, which has nothing to act on here (fixes.md, On
Pirate's Moon).

1. A turret's charge or glow goes out when the turret dies.
2. Two c4 enemies (readeria 1 and 2) follow their scripted orders.
3. Tripod turrets aim and turn when facing straight up or down.
4. A vehicle's gun stops at its true aiming limit.
5. A vehicle file with pitch and no roll keeps its pitch settings (no shipped vehicle).
6. Pirate's Moon: the backup node fix (MW3's is always on: its code has no original way to give back; Pirate's Moon's the same if its code is unconditional too).
7. Pirate's Moon: a deploy node's valves are read (no shipped file uses them).
8. Pirate's Moon: range actions on a valve (no shipped file uses them).
9. The reticle locks on a mech past 800 m when the selected weapons reach it.
10. The mech lab offers the plain PPC the data defines.
11. TAG's air strike lands where the target is, not where it stood 7 s before.
12. A beam held after its button is let go stays on a moving mech.
13. A destroyed weapon's selection moves on to one that still stands.
14. A mech re-placed where it stands no longer drops under a bridge or onto a roof.
15. A mission's last animation can be waited for.
16. An animation whose object is missing waits for its conditions.
17. Leaving c4b mission 5's or 6's area fails the mission.
18. c2 mission 3's secondary objective waits for the mission's two Fireflies (it named two that aren't there, so it completed at once).
19. The Sunder Alternate B carries its LRM 20's extra 240 missiles.
20. The Strider's recoil slide is read as a number, as on the Cauldron, Bushwacker and Champion.

## Owen's answers (2026-11-10)

The borderline rows, as Owen settled them:

- **Always on, no switch**: the stick lever's reverse, the slider and the axis
  popup (hardware support); the typed-key leak, the mouse sensitivity kept and
  the fall's force feedback; the HUD timer and the debriefing time; the
  lancemates' career kills and the saved-game list after No; nav points marked
  reached; all four multiplayer fixes; the Small Pulse Laser's muzzle flash (a
  corrected graphic: made, the Small Laser's blue flash in its place).
  Also always on as proposed: Alan's voice, the heat-sink callout, delayed
  updates, the aim past 4:3, the gait's blend-in.
- **Left as the original** (the fix removed, or never made): Pirate's Moon's
  firepoint aim cone (0x4a6ee0) and aim re-pick (0x4bb290); vehicles passing
  through walls, and not slowed by quicksand or lava.
- **Small fixes**: c2 mission 3's secondary objective (the Fireflies), made
  already under the bug fixes (fixes.md's row said otherwise); the plain PPC; the turrets and vehicles (Improved AI stays the
  lancemates' pathing).
- **The backup node's aim** (0x4cfad0): always on. Its code is unconditional (the exe's aim is an unset
  stack vector, so there is no original to give back), so it is not on the Small fixes list (the lead, 2026-11-10).
- **The turrets' pick** (0x004a70d0, the count across missions): a permanent
  fix, always on (Owen, later the same day).

## Where the `bugs` switch's users go

Every `zfix::options().bugs` in the code today, by file (main / pm), and the
switch it would read instead. Many of them guard fixes this sort puts under
(a); those would lose their switch and stay on.

| File | Uses (MW3 / PM) | Goes to |
|---|---|---|
| `frontend/pm_debug_world.cpp` | - / 1 | always on (PM's debug view) |
| `frontend/sdl/mw3_main.cpp` | 2 / 2 | the switch itself (`bugfixes=` and `--original`): replaced by the Upgrades settings and an "all off" for `--original` |
| `gamez/zeffect/zeff_anim_init.cpp` | 2 / 2 | Small fixes (the animations' last def, missing prerequisites) |
| `gamez/zfmv/fmv_script.cpp` | 1 / 1 | always on (movies) |
| `gamez/zfmv/fmv_stream.cpp` | 1 / 1 | always on (movies) |
| `gamez/zinput/zin_kbd.cpp` | 1 / 1 | always on (a typed key) |
| `gamez/zmodel/gmod_const.cpp` | 1 / 1 | always on |
| `gamez/zmodel/gmod_puff.cpp` | 1 / 1 | always on (particle fade-in) |
| `gamez/zsound/zsnd_3d.cpp` | 1 / 1 | always on (fades) |
| `gamez/zsound/zsnd_grp.cpp` | 1 / 1 | always on (wingmen's ammo calls) |
| `gamez/zsound/zsnd_init.cpp` | 3 / 1 | always on (stereo, the medium format) |
| `gamez/zsound/zsnd_play.cpp` | 1 / - | always on (unpause) |
| `gamez/zweapon/zwep_beam.cpp` | 1 / 1 | always on (TAG's hold at 60 and 144 fps: frame-rate timing, as `per_second`) |
| `gamez/zweapon/zwep_shot.cpp` | 1 / 1 | Artemis tracking |
| `mech3/app_engine.cpp` | 1 / 1 | always on (the frame's fill) |
| `mech3/app_menus.cpp` | 1 / 1 | always on (lancemates' career kills) |
| `mech3/app_mission.cpp` | 1 / 1 | Improved AI (the nav restart) |
| `mech3/controls_menu.cpp` | 3 / 3 | always on (the lever, a slider) |
| `mech3/credits.cpp` | 1 / 1 | always on |
| `mech3/debriefing.cpp` | 1 / 1 | always on (the mission time) |
| `mech3/force_feedback.cpp` | 1 / 1 | always on (the fall's force feedback) |
| `mech3/hud.cpp` | 5 / 5 | always on (the HUD) |
| `mech3/lab_screen.cpp` | 1 / 1 | always on (sort by name) |
| `mech3/mech_build.cpp` | - / 1 | none: the exe's way always (PM's firepoint aim cone, Owen 2026-11-10) |
| `mech3/mech_collide.cpp` | 1 / 1 | always on (a mech's collision zones outside the player's: the lancemate's lost floor is a stray-state fix) |
| `mech3/mech_control.cpp` | 2 / 2 | always on (the lever's reverse, no stick) |
| `mech3/mech_fieldbase_dock.cpp` | 4 / 5 | MFB regroup after repair |
| `mech3/mech_move.cpp` | 1 / - | Water movement |
| `mech3/mech_names.cpp` | 2 / - | Small fixes (the plain PPC) |
| `mech3/mech_net.cpp` | 3 / 3 | always on (remote aim, the push's credit, the latency) |
| `mech3/mech_objective.cpp` | 1 / 1 | Small fixes (objective data) |
| `mech3/mech_parts.cpp` | 1 / 1 | always on (the heat-sink callout) |
| `mech3/mech_place.cpp` | 1 / 1 | Small fixes (placing a mech) |
| `mech3/mech_pose.cpp` | 2 / 2 | always on (the gait's blend: the feet's pose) |
| `mech3/mech_skin.cpp` | 1 / 1 | always on (damage textures) |
| `mech3/mech_skis.cpp` | 2 / 1 | Improved AI (an AI mech's floor) and always on (the foot probes) |
| `mech3/mech_waynet.cpp` | 2 / 2 | always on (nav points reached) |
| `mech3/mission_launch.cpp` | 1 / 1 | always on (a crash with the view on another vehicle at the launch) |
| `mech3/mp_lobby.cpp` | 1 / 1 | always on (the lock's stock loadout) |
| `mech3/options_menu.cpp` | 4 / 4 | the UPGRADES page's BUG FIXES line: replaced by one line per switch; the mouse sensitivity's use: always on |
| `mech3/pilot.cpp` | 3 / 3 | always on (pilot names, saving before the question) |
| `mech3/player.cpp` | 2 / 2 | always on (a vehicle's whole names, its controller's crash) |
| `mech3/pocket_panel.cpp` | 1 / 1 | always on (the pocket commander's use after free) |
| `mech3/quit_box.cpp` | 4 / 4 | always on (closing with the box up left the mouse unable to fire) |
| `mech3/turret.cpp` | 3 / 3 | always on (the turrets' pick, Owen 2026-11-10) and Small fixes (a turret shut down, its yaw) |
| `mech3/veh_controls.cpp` | 2 / 2 | Small fixes (vehicles) |
| `mech3/weapon.cpp` | 6 / 6 | Small fixes (TAG, the beam, the selection); always on (the aim past 4:3, ZAPS_HUD's hold) |
| `mech3/zai_attack.cpp` | 5 / 5 | Improved AI (the detour and trail) |
| `mech3/zai_nav.cpp` | 1 / 1 | Improved AI |
| `mech3/zai_repair.cpp` | 1 / 1 | MFB regroup after repair (the drive to the dock) |
| `mech3/zai_valve.cpp` | 1 / 1 | always on (delayed updates) |
| `mech3/zrdraiv.cpp` | 3 / 3 | always on (Alan's voice); Small fixes (loose orders, c4b 5 and 6's boundaries) |
| `pm/gamez/zeffect/zeff_anim_init.cpp` | - / 2 | Small fixes (PM's copy) |
| `pm/gamez/zmodel/gmod_clip3d.cpp` | - / 1 | always on |
| `pm/gamez/zmodel/gmod_const.cpp` | - / 1 | always on |
| `pm/gamez/zmodel/gmod_projected.cpp` | - / 1 | always on |

## (a) Always on, no switch

### Crashes, hangs, memory and lost data (229)

| Fix | Game | Why |
|---|---|---|
| A mission without its world, the mission load 0x0048faf0 (lead,… | MW3 | a crash on starting any mission of an operation whose world file is gone |
| CycleTextureSetOn with no current node, the script dispatcher 0x00591300… | MW3 | a mission whose nodes are missing (its world unread), loaded after another mission, writes into that… |
| The script's model with no current node, and the three model pointers at a mission's end, 0x00591300 / 0x004eb420 / 0x005964c0 | MW3 | after a failed FindNode in a mission loaded after another, the Object3D model commands write a freed mission's model |
| A mission load that fails, GameEngine::missions_start after 0x0048faf0 | MW3 | the player is put back at the briefing with no idea why |
| zImage's release, 0x00517b10 (the SDL host's `image_release`) | MW3 | a second release of a freed image would free it twice |
| The Action screen's wave, 0x0048a810 (menus.md section 13) | MW3 | the reset asks for the pilot's best wave, 20 once all are won: the row past the table, strings read as mechs… |
| ActionGame's texts, 0x004368e0 / 0x00436950 / 0x004369c0 | MW3 | the old buffer may be shorter than 4 bytes (a short text before) |
| Training's view_object and weapon_group_select, 0x0045f400 / 0x0045e850… | MW3 | a check with no player vehicle (or no weapon manager) reads through null |
| A message the table lacks, 0x00576040 (Mech3Msg.dll) | MW3 | 1.0's Mech3Msg.dll lacks 38 of 1.2's messages (assets.md, Data versions): the 1.2 exe over 1.0's table would… |
| fmv.zrd's entries, 0x00598d80 | MW3 | a short entry reads past its list; no shipped one is short |
| Mech definition dtor, 0x46b9b0 | MW3 | a mech file with a bad or missing `version` stops the read before the loadouts are made, and the save then… |
| Model cycle on, 0x0053bc30 (the interp's `CycleTextureSetOn`) | MW3 | a crash; no shipped model reaches it |
| Canned effects' node copies, 0x431800 | MW3 | a removal the node refuses would loop for ever (none does in the shipped data) |
| Surface marks' image, 0x5426b0 | MW3 | a script's type past 2 writes over the hit's texture coordinates and the globals after them (no shipped… |
| The mission boundary's read and selection, 0x004c3930 / 0x004c3b80 (ai.md… | MW3 | a write and later reads past the heap block (an operation outside 1..4 has no areas, so every child would);… |
| The mission boundary's drawing, the vehicle editor's Show InOut,… | MW3 | a read past the areas, a hang, and taking down freed nodes at the next mission's Show InOut; editor only,… |
| The vehicle editor's record cache, 0x004defc0 / 0x004df5d0 (editor.md,… | MW3 | the next mission's vehicle editor loads the old records, their orders pointing at freed regions and valves… |
| The mission boundary's nearest outline point, 0x004c2470 (ai.md section 11) | MW3 | a read of whatever the stack held; only with NaN places |
| The mission boundary at the mission's end, 0x004c3dd0 | MW3 | a mission without a boundary (multiplayer, a world without `inout`) after one with it would pull the AI's… |
| A field base lost, 0x004590b0 (vehicles.md, A mobile field base lost) | MW3 | an item that crosses 64 writes past the array, and the draw at 64 reads and zeroes the saved exception frame… |
| The mission waynet's selected element, 0x00479c50 / 0x00479ef0 / the… | MW3 | the default behaviour (the mcbwaynet's element 0, which the MFB picking's N / Shift+N wraps onto) or a null… |
| The nav waynet's build, 0x004be690 | MW3 | a link to a node that is not there reads the map at -1 or past its end, the buffer and the elements are… |
| The waypoint icons' free, 0x00479850 | MW3 | the up and down arrows' images are never released (zImage keeps one reference more each mission) |
| The waypoint arrow's size, 0x00479ef0 after 0x00479850 | MW3 | the next mission's first such arrow reads a released image (AddressSanitizer: heap-use-after-free; the kills… |
| A turret's setup, 0x004a6270 (vehicles.md, The turrets) | MW3 | reads through null: a crash at load; no shipped turret has both (tur_ams, the one without a weapon, gives a… |
| The turrets' file, 0x004a76b0 / 0x004a6270 | MW3 | a value that is not a list, or a list shorter than read, reads past it; no shipped file has one |
| A manager's weapons, 0x004b0400, 0x004b0460 (weapons.md, Sixteen weapons) | MW3 | A 17th goes over the owner field: a mod's mech with 16 and TAG, or a network peer's config, which… |
| A turret's frame, 0x004a6be0 | MW3 | a turret without one (a definition without PARTS) reads through null: a crash; no shipped turret |
| A turret's aim, 0x004a71c0 | MW3 | a turret without a gun (PARTS of one name, or a gun node not found) reads through null: a crash; no shipped… |
| A wreck deleted, 0x0041e140 / 0x004aa6b0 (damage.md, After death; ai.md… | MW3 | a mech the wreck hit that dies later (a fall, its own ammo) credits the kill to it: its vt+0x80 on freed… |
| A vehicle's forget, 0x004aa4d0 | MW3 | a leak each time an entity goes (a wreck deleted, for every vehicle that saw it) |
| The animations' message 0x11, 0x004d4980 (ai.md section 18) | MW3 | with no such node (no shipped world has one; no shipped animation sends 0x11) the translation and rotation… |
| The animations' binder, 0x004d4c80 (ai.md section 18) | MW3 | on a def without a node (none shipped) the parent count of nothing is read: a crash |
| Saved loadouts, item list 0x471540 | MW3 | a corrupted `configs\<mech>.mw3` can hang the game at load for up to 4 billion reads |
| Length-prefixed strings, 0x576460 | MW3 | a damaged length makes it allocate up to 4 GB and crash when that fails |
| Archive tables, 0x5767b0 (zutl_zar's read_table, every .zbd and saved game) | MW3 | a damaged saved game's names are then read from that buffer: zSave crashes (save.md section 4) |
| zSave's load, 0x5a8e40 (an entry's name) | MW3 | a damaged saved game crashes the mission load |
| zSave's load, 0x5a8e40 (an entry's length) | MW3 | a damaged length makes MFC's new fail and throw: a crash |
| The anim registrants' loads, 0x50b970 / 0x50c250 / 0x50c890 (save.md… | MW3 | a damaged saved game makes the loads index past the defs, read past the entry, overrun a def's sequences or… |
| Matrix multiply mode 2, 0x51fee0 | MW3 | undefined values in a node's matrix |
| Model draw, 0x53f2e0 | MW3 | a longer polygon overruns the buffers; only the inout areas (up to 242, c4) have them, and those are deleted… |
| Equipment critical, 0x461211 | MW3 | a crash |
| Lights reaching a model, 0x543d90 (and the colour mix 0x53daa0) | MW3 | the 64 records end at the shadow sets (0x7489a8): with more than 64 lights (impacts and explosions bring… |
| Projected-light subdivision, 0x547160 (from 0x547740) | MW3 | a large polygon in a light's falloff band overruns them into the light count and the vertex records |
| Shadowed draws, 0x57eeb0.. | MW3 | a crash on a textured polygon whose material lost its texture |
| Mech data refresh, 0x4319f0 | MW3 | a crash at the second mission's load after a broken mech file |
| Shadow image size, 0x542d30 | MW3 | a crash; unreached in 1.2 (only a new caster gets a size) |
| Polygon filler, 0x55d010 | MW3 | uninitialised memory; a hang |
| The HUD's mode block, Hud ctor 0x43b190 / setup | MW3 | the heap's old word is read as a node: a crash at launch when it is garbage |
| The zoom lock key, 0x41be80 case 0x15 | MW3 | a crash on keypad 0 when the view's mech has no HUD |
| The lead point, 0x41d940 (TARGET's lead circle) | MW3 | a crash: a mech with a targeting computer whose groups are all gone, with a target on screen |
| Cockpit camera update, 0x421e40 | MW3 | uninitialised memory; unreached: every shipped `attach` block has `dist (0.0)` |
| Software tint rows 0x5582c0 / 0x558320 | MW3 | on the buffer's last row the extra pair is written past its end |
| 2D blit, 0x55b3c0 | MW3 | an image whose bottom lands exactly one row past the buffer writes that row beyond it |
| 2D blit, 0x55b3c0, palettised paths | MW3 | a zero-width source rectangle on a palettised image counts from 0 past 2^32 and faults |
| Credits background, 0x004289a0 | MW3 | a null read without a main menu (unreachable: the credits open only from it) |
| Credits text, 0x00429200 | MW3 | a longer line overruns the stack (no shipped line comes near) |
| The briefing's SOUND, inline in 0x00405230 | MW3 | a crash: a briefing's SOUND without MARKERS (none ships) |
| The debriefing's parts, 0x0047b430 | MW3 | a crash or garbage with an edited dlgWin.zrd (none ships), a `%` in a message |
| ARMOR_TITLES' refresh, 0x00450e00 / 0x00450bd0 (mechlab.md, LOADOUT_LIST… | MW3 | a crash with an edited dlgMechLabII.zrd (none ships) missing them |
| UNASSIGNED_LIST's refill and filters, 0x00453f20 / 0x00453770 /… | MW3 | a crash with an edited dlgMechLabII.zrd (none ships) missing ALIST_TITLES or its labels, or the list used… |
| Salvage of a mech's weapons, 0x00472da0 | MW3 | a null read for a weapon without a def (none is built without one) |
| The briefing's debug lines, 0x00405120 | MW3 | a `%` in a line reads arguments that are not there |
| Video device names, 0x004394d0 | MW3 | a `%` in a driver's name reads the stack; a long one overruns |
| Cycle buttons, 0x0048b550 / 0x0048b6b0 | MW3 | a null read in a dialog without a CURSOR (every shipped one has one) |
| Text box, 0x5a2320 / 0x5a1e80 / 0x5a2280 | MW3 | the next write reads the flags of a line that is not there, and an append (0x5a22f0) writes through it |
| Script lines, zinterp.cpp (0x5954a0 / 0x5955e0, 0x5956e0, 0x595850) | MW3 | a long line, name or expansion, or too many tokens, corrupts memory |
| Freed line, exec_line 0x595510 / `source` 0x590e80 | MW3 | use after free |
| Script commands, 0x591300 | MW3 | none reached by the shipped scripts |
| Puffer event key counts, 0x5035b0 | MW3 | a damaged anim.zbd reads the following data as keys |
| Sound duplicates, 0x56e2d0 | MW3 | its position and has-position flag are whatever the heap held, which can place a later volume change in 3D |
| Weapon effect blocks, 0x584bb0 | MW3 | a fifth sound writes over the next fields |
| Damage animations, 0x58459c | MW3 | more overwrite the destroy and kill animations |
| Projectile pool size, 0x58468e | MW3 | a def without RANGE uses the previous def's range, the first one stack garbage |
| Weapon manager with no group, 0x4b0c30 / 0x4b0790 / 0x4b07e0 / 0x4b08c0 | MW3 | group -1 is a null call (crash); the manager starts at group 0, so only a weapon moved to group -1 gets there |
| Gravity shots that accelerate, 0x585d61 | MW3 | a crash; no shipped weapon has gravity |
| Air burst record, 0x582320 | MW3 | the surface animation's orientation is whatever the stack held |
| Animation phase cache, 0x5abae0 / 0x432a80 (and every sampler inlining it) | MW3 | for motions longer than 60 frames those keys are past the new arrays: the exe reads the heap beyond them for… |
| Foot ground probes, mech skis +0x2f4 (0x475de0, 0x475ea0) | MW3 | a foot's ground and water come from the front or back probe, or from any probe by chance |
| Toe poses, 0x417fde.. / 0x4178a8.. / 0x417177.. | MW3 | a right foot with fewer toes than the left reads and writes one quaternion and one toe node past its arrays |
| Missing stand or step motion, 0x431350 | MW3 | a crash; never with the shipped motions |
| The HUD's static on TARGET, 0x0043cd20 | MW3 | a crash |
| The rain and snow's first frame, 0x005a6110 / 0x005a5b40 | MW3 | streaks projected with stack garbage for a frame |
| ActiveCtlCfg, the loader 0x40c420 to 0x4120a0 | MW3 | a saved value without its 0 runs past the buffer |
| AI reader files, zrdraiv.cpp / zrdrainet.cpp (0x4decf0, 0x4ded20,… | MW3 | a malformed file (a number where a list or a name belongs, a short key) crashes the game or reads past a… |
| aiv.zrd old tags, 0x4df730 | MW3 | the result depends on what was on the stack |
| AI regions, 0x4c5b00 (from 0x4bf3f0) | MW3 | a crash at mission load (emulator: the `no name` case) |
| AI nets, arrangenodes 0x4c7340, node links 0x4c71f0 | MW3 | a link past the file's nodes reads past the vector |
| AI net compile, 0x4bde20 / 0x4bd790 | MW3 | a write past the vector; a node whose links all go nowhere crashes the load (emulator: 0x4bd790..0x4bdd70) |
| AI net compile, 0x4bde20 | MW3 | heap garbage as the link's target; c2 mission 3's `net_13` does it |
| AI net compile, 0x4bd790 | MW3 | writes past the compiled net's vector |
| AI net steps, 0x4c9e60 | MW3 | followed later, a crash |
| Vehicle collisions, 0x4a09e0 (vehicles.md, The collisions) | MW3 | a tracked vehicle driving over fire-soil ground could crash the game |
| Field bases' registry, 0x45a030 (vehicles.md, The mobile field base) | MW3 | a mission with four field bases would lose the first base's valve (the field-base objective's) to a pointer;… |
| Repair dock's drives, 0x45b150 / 0x45b660 (vehicles.md, The repair dock) | MW3 | reading freed memory; harmless on the original heap |
| Repair dock, 0x45af00 / 0x45b460 / 0x45ab70 | MW3 | a mission with fewer than three field bases would crash once a mech asks for a repair; every shipped mission… |
| Field base damage, 0x458f50 | MW3 | a field base hit off multiplayer with no player's vehicle would crash; the mission always has one |
| Vehicle motion block, 0x49ef40 (vehicles.md) | MW3 | a modded vehicle.zrd without them crashes the mission start (every shipped vehicle has both) |
| Vehicle weapon slots, 0x4af2a0 (vehicles.md) | MW3 | a modded `vehicle.zrd` naming an unknown weapon crashes the mission start (no shipped vehicle does: AC2,… |
| AI way finder, valve unions 0x4d4490 | MW3 | a union entry without a valve (no name) crashes the search |
| Numbered objects, 0x58b060 | MW3 | reads past an empty vector |
| Keyboard read, 0x51a5d0 / 0x51aa00 / 0x51a300 | MW3 | a DIK code from 0xde up (Power, Sleep, Wake, the web and media keys) with all three modifiers held writes… |
| Mouse start, 0x51b5f0 | MW3 | a crash at start on a machine without a mouse |
| Text tokens, 0x556510 | MW3 | the reader returns into the middle of an instruction; its "too long" message (after 257) is never reached… |
| Text strings and directives, 0x556510 / 0x557550 | MW3 | a stack overrun, or a hang at load |
| Controls file axes, 0x4120a0 (controls.md, Setup) | MW3 | it overwrites 0x604110..0x60411f: the table's flags and the groups' vector, so the game crashes at exit or… |
| Controls file entries, 0x4120a0 | MW3 | a hand-edited file with a number or an empty list there crashes the game |
| Controls screen rows, 0x0042c4e0 / 0x0042cca0 / 0x0042ce90 (menus.md… | MW3 | a group with no listed command (no name, or empty; not in the shipped setup) reads through a null vector: a… |
| Controls screen file path, 0x0042d0b0 | MW3 | a long ActiveCtlCfg overruns the stack buffer |
| Controls name map, 0x410fe0 | MW3 | a dangling key, read by later lookups and the writer |
| Mission start names, 0x49e810 (mission-start.md section 9) | MW3 | a `%` in a name is read as a format; a name of 20+ characters loses its start, one of 28+ overwrites the… |
| Vehicle controller, 0x49f800 | MW3 | its animation slot becomes a stack address; a failed build deletes the record as a controller (a crash).… |
| Lancemate slot, 0x49e810 | MW3 | a crash if it failed |
| Launch lancemates' names, 0x49e5e0 (mission-start.md section 12) | MW3 | a `%` in a name is read as a format; one of 256+ overruns the frame |
| Assuming a lancemate's duties, 0x49e540 / 0x49e4e0 (mission-start.md… | MW3 | a crash; neither happens in play |
| A glowing vehicle's end, 0x4ac630 (vehicles.md, The glow) | MW3 | the light is never freed: zClass refuses to delete it with the world ("ERROR deleting light; Light attached… |
| The segment queries' cell walk, 0x4eefe0 (view.md section 7) | MW3 | the game freezes in that query (the AI's line of sight is one); found when a test placed the player at NaN |
| An animation's instance, 0x5097c0 (anim-run.md, How an animation runs;… | MW3 | the mission's end (0x50b2f0) or any free of it (0x50b0e0) destroys the borrowed node's tree: the node an… |
| Instant Action's line-up, 0x49dc80 (mission-start.md section 13) | MW3 | a crash or the wrong record; none happens in play (the records read, waves start at 1, names are short) |
| AI radio names, 0x4e7dc0 (ai.md section 12) | MW3 | a radio voice (aiv.zrd `sound_tag`) of 15 or more characters overruns the frame; the shipped ones are 3 to 5 |
| valves.zrd records, 0x4e2330 (ai.md section 13) | MW3 | a hand-edited file with a number or a string where a list or name belongs crashes the mission start |
| objectives.zrd, 0x45c920 / 0x4711a0 / 0x45c810 / 0x45c340 (objectives.md,… | MW3 | a hand-edited file with a number or list in those places, an empty list or a missing `list` crashes the… |
| Instant Action's enemies group, 0x45c4a0 (objectives.md, piece 3) | MW3 | another mode leaves the pointer null and the exe reads through it |
| reach_area's check, 0x45e890 (objectives.md, piece 4) | MW3 | a check with no vehicle (before the launch) reads through a null |
| An objective's further lines, 0x45e110 -> 0x40fe20 | MW3 | a `%` in a mission's objective text reads arguments that are not there |
| Valve scans, 0x4e3200 | MW3 | a scan naming a vehicle that is missing or not AI-driven writes through null (a crash); no shipped mission… |
| Valve teleport, 0x4e3ba0 | MW3 | a pilot with no order (a net that is not compiled) reads before the deque: a crash |
| The pocket commander's release, 0x46ad80 (the mech going, 0x41520b) | MW3 | a double free when the player's mech goes (the mission's end) while the lance command mode is up |
| The pocket commander at the mission's end, 0x46ab10 (0x46aa40) | MW3 | a use after free when a mission ends in the lance command mode (t1m4's CMD_DIRECT_POCKET lesson crashed the… |
| The pocket commander's frame, 0x469d00 | MW3 | a crash in the command mode whenever the view is not on the player's mech |
| The lance's "only an enemy" / "only an ally" orders, 0x4ea390 / 0x4ea3f0 | MW3 | a crash when the contact on the player's target sees nothing any more |
| The player's vlist, 0x4d8990 (controls made once, 0x427280) | MW3 | a read of freed memory at every mission start after the first (a crash in the port, likely silent in the exe) |
| Valve reactions, 0x4d3bf0 | MW3 | the stack runs out and the game crashes; no shipped file has a loop (Owen 2026-10-20: he may make missions) |
| AI vehicle controls' destructor, 0x4ac800 | MW3 | a vehicle that fails to build deletes its controller before giving it an owner (mission-start.md section 5):… |
| Firepoints, 0x46cb80 (mech-build.md section 12) | MW3 | a crash (it reads through a null part) for a model without one of its definition's firepoints; none ship |
| Launch, 0x491150 (mech-build.md section 12) | MW3 | crashes: a definition without skins (none ship), the view elsewhere (cannot happen: the launch attaches it… |
| HUD constructor, 0x43b190 (hud.md section 3) | MW3 | the first set-up in such a mode reads hud.zrd through whatever `new` left there: a crash or garbage… |
| zRender's 2D primitives, 0x563c50, 0x5630c0, 0x563440, 0x562970 (twod.md) | MW3 | a shape partly off screen writes into the next rows or past the surface |
| HUD line draw, 0x5a68b0 | MW3 | a clipped line without a rect restores the box from its uninitialised stack |
| Foot, 0x462590 | MW3 | a crash for a model without the foot's node; none ship |
| Sequence step, 0x4fdf50 | MW3 | undefined reads, possibly a hang |
| zTwoD's 2D line, 0x563b80 (behind 0x562940; the edit bar's caret) | MW3 | a point off the screen writes outside the buffer |
| Line editor resize, 0x5a4740 | MW3 | a text longer than the new size runs past the buffer (the popup resizes its empty line, so nothing shipped… |
| Edit bar text, 0x5a52a0 (from the file list's select, 0x42f1c0) | MW3 | a crash when the list's select is given -1 or an index past its items (nothing in the popup does) |
| The net view's link colour, 0x58b550 (editor.md, The net view) | MW3 | the colour hangs on stack garbage: a link with a clear line may draw red |
| AI net text writer, 0x4db8d0 (editor.md, The save) | MW3 | a crash on a record with one of those set and its valve missing (nothing in the release calls the writer; a… |
| File list layout, 0x48cda0 | MW3 | the empty list's hit area is garbage: a click anywhere below could count as on the list |
| File list rows, 0x48cda0 | MW3 | a `%` in a file name reads arguments that are not there (garbage, or a crash on `%s`) |
| File list row font, 0x48d1c0 | MW3 | an item below the shown rows indexes past the rows (no caller in the ported screens) |
| Pilot campaigns and lancemates, 0x4790d0 / 0x434040 | MW3 | a corrupted pilot file hangs the game for up to 4 billion reads |
| Pilot name, 0x477b20 / 0x478560 | MW3 | a pilots\ file name of 64 characters or more overruns the record into the games' vector: a crash at the… |
| Current pilot, 0x477f50 | MW3 | a dangling pointer (the pilot screen's DELETE on the pilot chosen before; a lobby launch then took it for a… |
| Pilot names, 0x478be0 / 0x477fe0 (pilot.md) | MW3 | "BOB" made over "bob", or a rename onto another pilot's name, shares one file: both are listed and whichever… |
| Loadout free slots, 0x425bd0 (mechdef-configs.md) | MW3 | an item placed in a part outside 0..8 (a damaged saved loadout) writes past the table into the loadout |
| Loadout copy, 0x423e00 | MW3 | copying a loadout with an engine item (a hand-made saved file) writes through null: a crash |
| Pilot screen DELETE, 0x487e30 | MW3 | a crash: DELETE pressed before any pilot exists |
| Sensors' C3 merge, 0x4a9fd0 | MW3 | targets.zrd objects count as contacts too, so a big enough union writes past the heap block |
| Sensors' contact, 0x4a9530 | MW3 | a vehicle-less entity in sight would crash the game |
| Entity sort, 0x4d7650 | MW3 | a lancemate without a `pilot` in aiv.zrd crashes the mission start |
| Soil table, 0x47f400 (mission-start.md section 11) | MW3 | a write outside the table; no shipped soils.zrd lacks a name |
| Selected group's ammo, 0x4b1cd0 (the HUD's) | MW3 | the answer is whatever the heap address holds, usually "has ammo" or not by chance |
| AI order stack, 0x4e9100 | MW3 | reads past an 8-byte heap block: a crash or garbage when no order is on the net and the orders fill a block… |
| AI link weight, 0x4cd430 (from a step chain, 0x4cd470) | MW3 | a crash on a broken net (a step whose next names nothing); no shipped net has one |
| AI jump state, 0x4cb4b0 | MW3 | a jump step whose next is not a node (no place) crashes the game; the shipped nets' jumps land on nodes |
| AI deploy node's valves, 0x4d00a0; the mcbwait node's, 0x4ccda0;… | MW3 | a deploy, mcbwait or valve_assign record without a valve crashes the game; no shipped record has one |
| AI firing's drop, 0x4b6690 / 0x4b6700 | MW3 | a vehicle without weapons that is hit (the brain's stand-down) crashes the game |
| AI firing's shot, 0x4b5b80 | MW3 | a weapon without a definition crashes the shot |
| AI volley plan, 0x4b6fa0 / 0x4b6e10 | MW3 | a plan whose target was cleared (the contact gone) crashes the next firing pass |
| AI station keeping's run, 0x4cc3e0 | MW3 | a pilot whose +0x78 is below the 0.9 of it its state took (a negative +0x78; none shipped) hangs the game… |
| AI station keeping's maker, 0x4cc150 | MW3 | a lancemate whose own order is on a vehicle with no bearer (vt+0x2c / vt+0x40 none) crashes the game; no… |
| AI home points (the leash's), 0x4c0ba0 / 0x4c0dc0, 0x4c0a90 / 0x4c0b50 | MW3 | a part going before one stacked on it (the own order's part, made during a fight, outliving the fight's)… |
| AI attack's way home, 0x4cae10 | MW3 | a pilot whose net has no step near (none found) crashes the game when a fight ends |
| AI vehicle net's part, 0x4c0d60 | MW3 | a crash (0x5a85e0 on null) |
| AI volley pick, 0x4b1860 | MW3 | a vehicle without a pilot chain, or an AI mech whose picked weapon has no group, crashes the game |
| AI volley plan's track, 0x4b6e30 | MW3 | an AI mech with no group selected (or no weapons) crashes the game when it tracks a target |
| AI brain's attack, 0x4e9710 | MW3 | a crash on broken aiv.zrd data; no shipped record has one |
| Quicksand texture, declient.zrd read 0x501ab0 | MW3 | a crash or a random texture at mission load |
| Crater clip buffers, Weiler 0x512940 | MW3 | past 128 entries (a large clip) the clip reads and writes freed memory |
| Crater clip side tables, Weiler 0x513750 | MW3 | for large polygon pairs the first table runs into the second and the second past the Weiler's end |
| Crater clip walk, Weiler 0x510da0 | MW3 | the game freezes in an endless loop that keeps allocating, when a crater's clip meets that case |
| Crater clip's merge, Weiler 0x5129f0, 0x5129a0 | MW3 | a crash in a crater, seen once in `test_crater.py` at 53 bits |
| Crater clip contour count, Weiler 0x513390 (and the output 0x5135a0) | MW3 | the game freezes in an endless loop counting the contour |
| Crater ring triangles, convexify 0x515be0 (0x515aa0) | MW3 | a polygon corner seeing more than 30 points of a crater ring writes past the list on the stack |
| Crater record, zdec 0x4ff940 / 0x4ffad0 (and quicksand's 0x502110) | MW3 | the 5 m overlap test reads freed memory: whatever the heap left there decides it |
| Untextured crater or quicksand, zdec 0x4fff80 / 0x4ff940 / 0x5026c0 | MW3 | a crater ordered without its texture flags (0x1008) crashes the game; every shipped crater has them |
| Strip expansion, zModel 0x553160 | MW3 | garbage flags; no shipped model has strips |
| Crater clip node list, zgeo 0x50ec10 | MW3 | a heap overrun when they do |
| TAG's hold, 0x5814b0 | MW3 | 60 x (1/60) and 144 x (1/144) fall just short, so the hold takes a frame more at those rates |
| TAG's beam, 0x4b4230 | MW3 | the beam lasts 32 frames at 30 fps (1.067 s), 62 at 60 (1.033), 146 at 144 (1.014): the slack over the… |
| The objectives box's lines, 0x00446970 / 0x00446a20 (hud.md section 24) | MW3 | a '%' in a text would read the stack (no shipped text has one) |
| zNetwork's send, 0x005888a0 (multiplayer-port.md piece 1) | MW3 | through null when there is no local player (a send after the session ended) |
| zNetwork's handlers, 0x00587b20 and 0x00587a00 during the dispatch… | MW3 | the dispatch's walk then reads the freed node for the next one (a crash or a skipped handler, by what the… |
| Another player's loadout, 0x004259c0 (multiplayer-port.md piece 5b) | MW3 | a record shorter than its size word (another player's game, or a damaged one) is read past its end |
| Another player's mech from their record, 0x0046d450 (multiplayer-port.md… | MW3 | a short record is read past its end; the controls leak |
| A remote mech's state applied, 0x0046e4d0 (multiplayer-port.md piece 6b) | MW3 | a short or lying message is read past its end |
| The network clock heard, 0x00470740 (multiplayer-port.md piece 7b) | MW3 | a 0x11 with a reason outside a network game reads through null |
| The pilot's respawn, 0x0046dde0 (multiplayer-port.md piece 8d) | MW3 | through null outside a network game, or for a type with an explosion and no cockpit (the dead list asks only… |
| zNetwork's translation 0x00588a70, a player destroyed… | MW3 | the 0xec-byte record itself is never freed: a leak per player who leaves |
| The lobby's world list, 0x00497720 (multiplayer-port.md piece 11a) | MW3 | a small leak each time the lobby opens |
| The saved command map, the settings' writes (the app's destructor… | MW3 | the box's unbind is meant for while it shows: closing the program with it up left the mouse unable to fire… |
| The saved map's reverse tables at the load (the options' loader… | MW3 | the quit box's mouse_command(1) (and any lookup) named the defaults' command, not the one bound |
| A saved map with that damage, at its load (the options' loader… | MW3 | older builds of the port saved maps without the mouse's fire (above) |
| A network kill's score, 0x00470170 (hud.md 24b) | MW3 | a read at 0x17970: a crash when the view is on another mech (a dead pilot's camera on another player, say)… |
| The objectives box's network scores, 0x00446ad0 at 0x00447237 (hud.md 24) | MW3 | a player's name with a '%' read garbage (and names come from the network) |
| FIND GAME without a connection, 0x004957c0 (piece 9b) | MW3 | a press before CONNECT reads through null |
| The matrix stack, 0x0051e9d0 / 0x0051e930 / 0x0051ea00 and its users… | MW3 | Owen's v1.0 crash (late c4b, two Daishis and Elementals left): `zmth::load` wrote through a bad slot… |
| The player's axes with no stick, `read_axes` 0x00427d60 (controls.md,… | MW3 | with a saved slider or a changed deadzone on the throttle and no stick, the mech walks by itself; a slider… |
| A light's place, 0x004fb0c0 through 0x004ed280 | MW3 | a light under an object3d sat at its parent's origin |
| Saved loadouts, item list 0x471540 (PM 0x473930, which stops on a count… | PM | a corrupted `configs\<mech>.mw3` can hang the game at load for up to 4 billion reads |
| Pirate's Moon's zArchive table, 0x00597610 (pm/gamez/zutil/zutl_zar) | PM | as MW3's |
| Pirate's Moon's points, 0x00542a70 (pm/gamez/zmodel/gmod_draw) | PM | a points model with no vertices loops 2^32 times, with no polygons reads through null; no shipped world has… |
| Pirate's Moon's near and side clippers, 0x00549440.. with 0x0054fea0,… | PM | PM's polygons allow 1,023 vertices. The largest drawn has 64, and one near cut makes 65: on the exe in the… |
| Pirate's Moon's model draw, 0x00543b10 (pm/gamez/zmodel/gmod_draw) | PM | Past them the exe overwrites what follows |
| Pirate's Moon's polygon lighting, 0x005603f0 (pm/gamez/zmodel/gmod_draw) | PM | A crash where a strip is lit with vertex shading off |
| Pirate's Moon's overlay polygons, 0x0059d450 and its siblings… | PM | A shipped polygon can have 313 corners |
| Pirate's Moon's lobby lines, 0x0049bb70 (mech3/pilot_games `lobby_lines`) | PM | A team with no skin shows bytes of memory in its tag |
| Pirate's Moon's player record from its data, 0x0049c650… | PM | A short record (another version's, or damaged) reads past its buffer |
| Pirate's Moon's clutter settings, 0x005116b0 (pm/gamez/zclass/cls_clutter) | PM | Reads as 0. Nothing shipped does this; the test compares well-formed files only |
| A saved game on a fresh world (Pirate's Moon), 0x435d30 with 0x435e40 | PM | record_mission's fix above wrote the world whatever the mission, so builds before 2026-11-08 left the last… |
| Controls file's first axis with no source and no KEYS, PM 0x412a26… | PM | a garbage axis source |
| The current pilot deleted, Pirate's Moon 0x47a8f0 / 0x47a280 | PM | the next use of the pilot reads freed memory, after a debug start only |
| AI brain's attack, 0x4e9710 (and PM's 0x4fab90, `tick_pm`) | PM | a crash on broken aiv.zrd data; no shipped record has one |
| Pirate's Moon's objectives box, 0x004486e0 (hud.md section 24) | PM | %2 reads what follows on the stack: a random mission number |
| Pirate's Moon's Progression waves: the slot accessors 0x0040c2c0 /… | PM | a mod's wave asked past its slots, a short pair, or more than six slots read and write past the end |
| Pirate's Moon's pilot file: a campaign's strings 0x597280, the standalone… | PM | a damaged pilot file hangs or runs out of memory |
| The pilot's header, 0x47af50 (`Pilot::open`) | PM | a player's pilot lost with no word |
| Pirate's Moon's shot, 0x4bcd40's record 0x4bc930 (`shot_pick`) | PM | the port reads zeros. Unreachable: a firing's shooter always has a brain. |
| Pirate's Moon's plan's turn, 0x4baae0 (`plan_turn`) | PM | the port aims as without a weapon. Not seen in play: a shot sets the group first. |
| Pirate's Moon's weapon line, 0x4b7800 (`weapon_line`, wiring) | PM | the port writes the zero way and cosines of 1; the aim's frame (0x4badc0, `AimPm::frame`) takes the… |
| Pirate's Moon's attack state, its hold (0x4dedf0, wiring) | PM | the record keeps a copy of the point and +0x104 points at it: the last hold, what the exe usually reads… |

### Port fixes, FIX (A) / (B) (152)

| Fix | Game | Why |
|---|---|---|
| The saves' temporary files, zSave 0x005a8ab0 and 0x005a8af0 (Owen's PC:… | MW3 | a save would fail with a very deep %TMP% |
| The screen grab, 0x00579500 (the pause screen's backdrop, menus.md… | MW3 | the program's 3D frame is on the GPU and its 2D buffer holds only the layer over its key colour: the grab… |
| A movie not found, 0x00599c00 (fmv.md, Movies) | MW3 | no CD drives (protection.md) |
| The movie player's files, 0x0059a650 / 0x0059a900 / 0x0059aaf0 /… | MW3 | no Video for Windows and no Indeo 5 decoder off Windows |
| Force feedback, 0x00433090..0x00433c80 (input.md section 7) | MW3 | no IFORCE2 or DirectInput force feedback off Windows 9x; today's pads rumble |
| A gamepad's axes and buttons, SdlInput (input.md section 7) | MW3 | SDL's raw joystick order differs by platform and its triggers rest at one end: the defaults' throttle (Z)… |
| A raw stick's axes, SdlInput (input.md section 9; W54, Reddit v1.0, a… | MW3 | SDL packs a stick's axes in that order without the gaps: the T.16000M's twist (Rz) arrived as Z, the… |
| The joystick opened and an unplugged one, SdlInput (input.md section 9;… | MW3 | odd HID devices (headsets, RGB keyboards, virtual pads) come first as joysticks today; a stick taken out… |
| The silent sound output's play cursor (platform/silent_sound.h) | MW3 | the stand-in for running without a device (`--nosound`) kept it at 0, and the movie player takes its frame… |
| The movie scripts' screen copies, [0x0072f358] / [0x0072f35c] /… | MW3 | the program has one 16-bit 2D buffer and Render3D |
| The Action screen's loadout choices, 0x00489c10 / 0x0048a140 | MW3 | an address does not fit the cycle's 32-bit value on 64-bit machines |
| The message box's loop, 0x005a7280 (twod.md, The message box) | MW3 | the program's input comes as window events: unpumped, no click would reach the box |
| The mech lab's Ctrl+X / Ctrl+I, 0x00426180 / 0x00426300 (mechlab.md… | MW3 | no Windows dialogs |
| Span routines, 0x564080, 0x56cb40 | MW3 | x86-only; fragile anywhere, since anything using the stack meanwhile writes into the picture |
| Frame clock, 0x575a00 | MW3 | after ~10 days of uptime a float can only step 64 ms, so frames quantise and the game stutters |
| CD music, zsnd_cd.cpp | MW3 | no MCI, and machines have no CD drive |
| zReader lists, zreader.cpp | MW3 | reads past a heap block: whatever the CRT heap holds in the original, out of bounds in C++ |
| Archives, zutl_zar.cpp; every file open | MW3 | Win32 only, `\\` separators |
| Pools: models, nodes, list items (`gmod_const`, `cls_node`, `list`) | MW3 | pointers are 64-bit on modern systems, so the file's slot layout cannot hold them |
| Engine abort, 0x4eada0 | MW3 | Win32 |
| The program's handles (GameEngine, ours: the exe passes raw pointers) | MW3 | once a wreck is freed in play, its old handle answered the freed object, and a new object at that address… |
| Saved loadouts, item load 0x472b20 | MW3 | a heap address |
| Engine table, 0x432f40 / 0x432fa0 | MW3 | memory past the table |
| The software renderer's offers: VIDEO DEVICE's cycle 0x4394d0, the saved… | MW3 | the software renderer is not ported (the GPU draws everything; render.md, Owen 2026-10-25), so the entry led… |
| Finding the install (ours, before WinMain; program.md, The install) | MW3 | the port is a separate program, not in the install |
| Disc check, 0x482f90 / 0x575d40 | MW3 | and (A): no CD drives |
| Main loop, 0x58fd70 | MW3 | Win32 |
| App start and window, 0x47f7a0 / 0x480000 / 0x480570 / 0x482d10 | MW3 | Win32 / DirectX |
| Missions, 0x480c70 / 0x481fc0 | MW3 | as the row says |
| `rand()` | MW3 | every C library's `rand()` differs |
| Projected shadows and lights, zvid_ddd3d.c (0x57eeb0, 0x57f2a0, 0x57f860,… | MW3 | Direct3D 6 |
| World materials, 0x5363d0 / 0x536ad0 / 0x537830 (`gmod_matl`) | MW3 | 32-bit pointers in the slots; the polygons hold the materials' addresses |
| mechlib materials, 0x4fc520 (`cls_mechlib`) | MW3 | 32-bit pointer fields in a file layout |
| Mech data part effects, 0x4320b0 (`MechData`, mech.h) | MW3 | one C++ array per kind |
| The fade's stills, 0x579500 / 0x55b1f0 / 0x517cc0 (the fade state 0x48f0b0) | MW3 | DirectDraw |
| The coolant gauge's easing, 0x004456f0 | MW3 | it settles faster the higher the frame rate |
| The message panel's slide, 0x0040f8d0 | MW3 | above 60 fps that is 0: the panel never comes up, the messages are never seen (1 from 30 to 60 fps: 30 to 60… |
| The menus' ripple, 0x00485650 | MW3 | at 120 fps and over that is 0: the main menu's ring never spreads |
| The briefing's loading bar, 0x00406440 | MW3 | it fills faster the higher the frame rate (a per-frame step, Owen's rule; the lead: no candidate) |
| The back buffer's lock in Direct3D, 0x577590 / 0x577600 under 0x576d00 /… | MW3 | DirectDraw; a GPU frame cannot be locked cheaply |
| FOGCOLOR in the GPU layer, 0x0057bcf0 (render state 34) | MW3 | the GPU layer applied the frame's last fog colour to every fogged draw: a saturated light (the jump jets'… |
| The screen grab in Direct3D, 0x579500 (the fade's still) | MW3 | the GPU's frame is not in memory: the grab copied the empty 16-bit layer, and the mission-start fade came in… |
| Shadow textures, 0x57baa0 / 0x57bb00 ([0x72f374] / [0x72f378]) | MW3 | Direct3D 6 |
| Device caps, 0x579b70 | MW3 | Direct3D 6 |
| Direct3D draw table, zvid_ddd3d.c (0x57bf80, 0x57c700, 0x57ce20,… | MW3 | Direct3D 6 does not exist on macOS and is legacy on Windows |
| 2D blit, 0x55b1f0 (zTwoD's every draw) | MW3 | DirectDraw |
| 2D blit, 0x55b3c0, any source rectangle | MW3 | the menu screens draw their still background (a mission's Options, FastMenu) with the screen's size as the… |
| Screen modes, 0x0040d810, 0x004ab860, 0x0043a470 | MW3 | today's screens are wider and larger; the estimate means nothing on a modern card |
| Camera's horizontal angle, 0x0040d590 | MW3 | at 16:9 that is 106.7 degrees, squeezing the picture to 77% of the original's shape |
| The menu screens' frame on a wide canvas (`GameEngine::construct`, our… | MW3 | Core makes the menu screens a mission can reach while in the mission's 16:9 mode, so they took the 853x480… |
| The credits' names at 16:9 (`GameEngine::widen_screen`; CREDITS_SCREEN… | MW3 | ours: the 16:9 menus moved the control half the extra width but not its RECT, so the names scrolled 106 left… |
| The pause screen at 16:9 (`GameEngine::construct`'s frame,… | MW3 | at 16:9 the pause kept the 640 frame's place in the middle, and back from the options (mode 5) it showed at… |
| The menus' whole-screen layer and the 3D's viewport (platform layer:… | MW3 | our 2D layer draws through the 3D's GPU pass with its own viewport; on a 16:9 canvas the 853x480 stayed… |
| The 2D target across a mode switch (`GameEngine::screen_size`, the… | MW3 | our one 16-bit buffer is remade by the switch while the 2D target kept the lock's old size and memory; the… |
| The first start's mission size: InGameVMode's default in 0x0040c420 | MW3 | today's screens are wide and large; Owen, 2026-10-30: 16:9 by default |
| The window and the mouse (platform layer: DirectDraw's full screen,… | MW3 | in a window of another shape the frame is stretched, and the mouse's scale assumed a 640-wide frame |
| The mouse's speed (platform layer: DirectInput's mickeys,… | MW3 | ours took SDL's raw count as a whole 640-wide pixel: at 3840x2160 a count crossed six screen pixels, the… |
| The held mouse's relative mode and cursor (platform layer:… | MW3 | SDL's relative mode can be refused or dropped (a fullscreen switch, the focus) without SdlInput's flag… |
| The mouse buttons at a mission's start (platform layer:… | MW3 | ours loads at once, so the LAUNCH click (latched, or still held) was read as the mission's first fire… |
| The message box over a GPU frame (`GameEngine::hold_shown_frame`,… | MW3 | our 3D frame is larger than the 16-bit buffer (the menus' 640x480 in the lab, 16:9 in a mission), so the… |
| Fullscreen and the focus (platform layer: DirectDraw's exclusive mode,… | MW3 | a borderless window at the desktop's size |
| The screen mode switch, 0x004ab6b0 / 0x00576c80 / 0x0057a1a0, Shift+V… | MW3 | no DirectDraw; (B) the modes past 10 |
| The HUD at larger sizes: hud.zrd's blocks 0x0043bb80, the display rect… | MW3 | the modes past 10 have no block, and 640x480 art is tiny at 1080p and 4K |
| Dialogs over a mission at a canvas mode: Dialog 0x0059ec40, MenuScreen… | MW3 | past mode 10 the pause screen and quit box sat in the canvas's top-left corner |
| Main menu ripple, 0x00558dd0 | MW3 | past 1024x768 a ripple low on the screen writes beyond it; the original's menus always ran at 640x480, the… |
| The briefing map's circles, 0x00407ea0 | MW3 | a pixel a frame above 20 fps: faster the higher the frame rate |
| A loadout copied out of a store, 0x00424010 | MW3 | a memory leak each time (the port's loadouts own their items) |
| Menu text, twod_wintext.cpp (0x5a00c0..0x5a0790, 0x5a0f00) | MW3 | GDI is Win32 only |
| Script files, zinterp.cpp (0x595990, 0x596620, 0x596810, 0x5763b0) | MW3 | Windows paths and MSVCRT text mode |
| Puffer sprite, 0x567720 | MW3 | the default texture's record is made at startup by 0x5184e0, not ported |
| Sound, zSound (zsnd_*.cpp, sound.md) | MW3 | DirectSound does not exist on macOS and is legacy on Windows |
| Sound output format, 0x5702a0 | MW3 | a modern mixer has no primary buffer |
| Sound cue points, 0x56dfb0 | MW3 | out of bounds in C++ |
| Lava heat, 0x4192f0 | MW3 | frame-rate dependent: about 1.5/s at 30 fps, 7/s at 144 fps |
| Ground probes landing, 0x00477940 (Skis::integrate) | MW3 | above 30 fps the catch-up lifts it only part way, so walking up even a gentle slope leaves every probe under… |
| Movement steps per frame: a fall's carried momentum (x0.6) 0x416860, the… | MW3 | at 144 fps a fall's momentum dies in a fifth of the time, slopes and ledges shove nearly five times as hard,… |
| Throttle keys, 0x427d60 (and the lance command mode's own, 0x46b0a0) | MW3 | at 144 fps the throttle opens almost five times as fast (full in 0.28 s instead of 1.3 s) |
| The throttle's snap, 0x00426960 | MW3 | with the keys' step per 1/30 s, that step falls inside 0.009 below 12 ms: above about 83 fps the throttle… |
| Beam recharge, 0x4b4230 | MW3 | the fraction is lost every frame, so the faster the game runs the slower beams recharge: TAG (166.7 a… |
| The turrets' turns, 0x004a70d0 (vehicles.md, The turrets in the program) | MW3 | how often turrets act follows the frame rate: at 60 fps twice as often as at 30 |
| The HUD's damage static, 0x0043cd20 (hud.md section 21) | MW3 | the static builds faster the higher the frame rate |
| The static's noise table, 0x005581c0 / 0x00558c10 | MW3 | a read past the heap block |
| The zoom's corner mask, 0x0057bd50 (hud.md section 23) | MW3 | the port makes no such texture |
| zTwoD's message texts, 0x00575ff0 (FormatMessageA) | MW3 | no FormatMessage off Windows |
| The rain's slant and the snow's fall, 0x005a6110 / 0x005a5b40 (weather.md… | MW3 | the faster the frame rate, the steeper the rain looks while moving and the faster the snow falls |
| The rain and snow's ramp, 0x005a5470 | MW3 | the other rows are whatever memory held |
| zMath's e^-x, 0x00523f70 (W50) | MW3 | every per-second easing (rate * dt) stops on fast frames: the turrets' aim (3, 8 per s, 0x004a71c0), the… |
| Smoke drag and wind, 0x5490e0 / 0x523f70 | MW3 | at 60 fps and above muzzle smoke and trails get no drag at all, and gusts change faster the faster the game… |
| Menu motion: the background's scroll 0x40a210, MECH3TEXT's slide… | MW3 | the main menu crawled and the ripple died faster the faster the machine drew (at 144 fps a ripple lasts 0.13… |
| zWeapon kill_all, 0x584fe0 | MW3 | a firing beam (0x150 bytes) is taken for a projectile (0x5d8): the kill writes its speed past the beam's end… |
| Mine bounce length, 0x586740 | MW3 | stack garbage |
| Projectile pool and beams, zwep_init 0x583880, 0x584a70, 0x5832f0 | MW3 | 32-bit layouts holding pointers |
| The program, `mw3` (program.md) | MW3 | Win32 / MFC |
| Main window, 0x482d10 (`CWnd::CreateEx`, `SetWindowTextA`,… | MW3 | Win32 |
| Message queue, 0x58fd70 / OnActivate 0x590c00 / MM_MCINOTIFY 0x590740 | MW3 | Win32 |
| Mech3Msg.dll, 0x575f30 / ZLocGetID / 0x576040 / 0x575fd0 | MW3 | a Windows DLL cannot be loaded on macOS |
| Message boxes, `MessageBoxA` | MW3 | Win32 |
| Clock, `GetTickCount` / `Sleep`, and the clock start 0x5759a0 | MW3 | as the row says |
| Input, DirectInput (0x51b0a0: `DirectInputCreateA`, the keyboard buffered… | MW3 | DirectInput does not exist on macOS |
| Video, 0x579c60 / 0x576de0 / 0x576ea0 / [0x72f38c] | MW3 | DirectDraw |
| A 3D lock inside a lock, 0x576d00 / 0x576d60 (render-frame.md section 1) | MW3 | the port's layer goes over the frame at the unlock, so the 2D drawn after the inner unlock was lost (the… |
| The back buffer's picture, 0x576de0 / 0x576d00 (render-frame.md) | MW3 | the 16-bit buffer holds only the 2D layer after a frame through Render3D |
| Settings store, 0x5977d0 / 0x597a60 / 0x597fc0 (settings.md) | MW3 | the registry is Windows' |
| Machine probe, 0x597fc0 (CPUID, a timing loop for the MHz,… | MW3 | x86 and DirectDraw only |
| Mission load thread, 0x490880 / 0x490900 (mission-load.md) | MW3 | the world's textures go to the GPU, which wants the main thread |
| Menu text, GDI (twod_wintext.cpp, through `ztwod::Host`) | MW3 | GDI |
| AI values of the wrong type | MW3 | a 32-bit heap address |
| Input, zInput (0x51a170..0x51ddc0, input.md) | MW3 | DirectInput does not exist on macOS |
| The mouse in a window (platform layer: DirectInput's DISCL_EXCLUSIVE… | MW3 | in a window (Alt+Enter) the player could not take the mouse out of it |
| zReader text files, 0x556240 / 0x5563a0 (zreader.md, Text files) | MW3 | Win32 |
| Controls file location, 0x42d0b0 / 0x411e60 / 0x4120a0 | MW3 | the install is read-only data in the rebuild |
| Node bounds, 0x4ecc00 | MW3 | the Windows game has no console; on macOS it would print to the terminal every frame the node moves |
| Controls file list, 0x42eff0 (menus.md section 6) | MW3 | Win32; the files are in the write dir |
| The AI Network Editor dialog, 0x0040e120 -> 0x004a4c20 (editor.md, The… | MW3 | no MFC; a modal Windows dialog over the game's window |
| Pilot files, 0x478ce0 / 0x478560 / 0x4781e0 / 0x477fe0 (pilot.md) | MW3 | Win32; the files are in the write dir |
| Game paths, `zutl::host` / `zutl::list` | MW3 | the host's names are UTF-8 |
| Names on Windows, `tools/windows/utf8.manifest`, `zutl_winnames.cpp` | MW3 | the rebuild's names are UTF-8 (SDL's, the host's) |
| Pilot list item, 0x488770 | MW3 | a 64-bit pointer |
| Shot queries' model test, 0x54cad0 from 0x4ef570 / 0x4ef980 / 0x4efaa0 /… | MW3 | memory safety; a shot through more than 32 nodes |
| Point query's model test, 0x54be40 from 0x4eddb0/0x4ee140/0x4ee220… | MW3 | memory safety; no shipped place has 32 nodes' polygons under one point |
| Crater history and zEntity records, zdec 0x500850 / 0x500b50 / 0x500c20 | MW3 | a 64-bit port cannot hold them, and a pointer means nothing on the machine that restores it |
| A state's or shot's target player, 0x0046f160 (multiplayer-port.md piece… | MW3 | a target that is not a network mech (a turret, a vehicle, the map's AI) is read past its end: whatever the… |
| A peer's packets (multiplayer-port.md piece 8c): the crater 0x0046fde0… | MW3 | a peer's packet is untrusted over the internet; the exe trusted DirectPlay's peers: any of these reads or… |
| zNetwork's DirectPlay 4 (IDirectPlay4A, IDirectPlayLobby3A;… | MW3 | DirectPlay is gone from macOS and Linux and a legacy feature on Windows 11 |
| The connection screen's providers, 0x00493260 / 0x004949f0 / 0x004957c0… | MW3 | only our transport exists |
| The lobby's rules message 0x20, 0x00499cf0 (multiplayer-port.md piece 11b) | MW3 | a peer's packet is untrusted over the internet: any player could set the rules or lock the game, and a short… |
| The lobby's start message 0x1e, 0x0049bcb0, and its scores 0x14,… | MW3 | a peer's packet is untrusted over the internet: any player could start the game for the others, and a short… |
| The lobby's chat 0x1f, 0x00499610 (multiplayer-port.md piece 12) | MW3 | a peer's packet is untrusted over the internet: a short message or one without its 0 was read past its end |
| The in-game chat 0x1f, 0x00470490 (multiplayer-port.md piece 14a) | MW3 | a peer's packet is untrusted over the internet: read past its end; control characters on the screen |
| The CD music under a movie (the movie hooks, 0x005996d0 and the zFMV… | MW3 | the original read its movies off the disc, and the drive reading them stopped its audio: the menu music… |
| The mission's quit box in a network game, 0x0048e2d0 / Core's frame… | MW3 | the port's hardware path drew nothing after the frame: the box stayed up unseen and the mouse never fired… |
| The controls screen's key capture, 0x0042be10 with 0x0051aa00 (Reddit,… | MW3 | DirectInput filled its buffer in real time, so a key that came while the screen drew was still there for the… |
| A player leaving a network game, ENet's close (ours; DirectPlay's Close) | MW3 | the service never came: the others dropped the player only at ENet's timeout (about 30 s), a frozen mech… |
| The app's inactive wait, zGameApp::Run 0x0058fd70 (playtest W45) | MW3 | in a network game the others fight a mech that neither moves nor hears them, then the catch-up comes at once… |
| The keypad in a text field on a Mac, zInput's 0x0051ab90 through the SDL… | MW3 | a Mac has no Num Lock and its keypad always types digits; SDL reports the lock off there, so the keypad… |
| A locked game's door (ours, piece 11b; the exe's connection screen… | MW3 | over the internet anyone can try to join |
| The lobby's scores, 0x00498550 (multiplayer-port.md piece 11a) | MW3 | the service is gone, and so is the lobby launch |
| An arm's hand freed, 0x00466c50 (damage.md section 4) | MW3 | a read of freed memory: Win32's heap kept the bytes, a modern allocator or checker need not (found by ASan… |
| A vehicle's support on the ground, 0x004a2820 (vehicles.md, High frame… | MW3 | the same rise launches a vehicle harder the shorter the frame: at 5 to 8 ms an APC rolls up to 82 degrees… |
| Pirate's Moon's motion.zbd lengths (ours; pm/gamez/zutil/zutl_zar span) | PM | the port's bounded motion reader stops at an entry's length |
| Pirate's Moon's light map, 0x005a03b0, 0x0040b270… | PM | No fixed-function stages on a modern API; the caps mean nothing there |
| Pirate's Moon's CULLMODE CW on a strip, 0x0059b870… | PM | Each backend winds its own way |
| Pirate's Moon's light maps' gate, 0x00543b10 (pm/gamez/zmodel/gmod_draw) | PM | One shader does them on any device |
| Pirate's Moon's text files, 0x00578410 / 0x00579440 / 0x00578750… | PM | as MW3's; (A) Win32 |
| Drowning (Pirate's Moon 0x41a100) | PM | at 144 fps a mech drowns in about a fifth of the time |
| A wreck thinning out (Pirate's Moon 0x41d730) | PM | at 144 fps it thins in about a fifth of the time |
| Pirate's Moon's static quad, 0x0043dfa0 (hud.md section 21) | PM | on a canvas smaller than the frame (16:9: 853x480 on 1920x1080) the static sat at canvas pixels on the… |
| A push against the throttle, Pirate's Moon 0x41ae00 | PM | the same fall per second at any frame rate, 0.6 per 1/30 s, behind `per_second` |
| Pirate's Moon's HUD static, 0x0043e1f0 (hud.md section 21) | PM | more static the higher the frame rate |
| Pirate's Moon's boundary, 0x4cbbb0 (`InOutArea::resolve`) | PM | no side, no move; only NaN gets here. |
| Pirate's Moon's via cost, 0x4cf730 (`way_through`) | PM | the port doesn't read it. |
| Pirate's Moon's saved worlds' animation states, 0x524e10 / 0x525d70 and… | PM | the saves our earlier builds wrote are still read: an entry exactly MW3's length for its count is read in… |

### Restored content, pictures, sound, HUD and text (42)

Owen's rule: on for good; the borderline ones he confirmed (Owen's answers, above).

| Fix | Game | Why |
|---|---|---|
| The Small Pulse Laser's muzzle flash (weapons.zrd's `plaser_blue_muz.flt`; fixes.md, Candidates) | MW3 | a graphic missing: no world has the flash it names, so it fired with none. Made: the Small Laser's `laser_blue_muz.flt` in its place (Owen 2026-11-10) |
| Alan's voice, aiv.zrd 0x4df730 | MW3 | misplaced data: his voice lines likely do not play there |
| Heat-sink critical, 0x460d50 | MW3 | losing a sink is silent (`heat.md`, `damage.md`) |
| A weapon's aim, 0x004b36e0 (weapons.md, Aim) | MW3 | wider than 4:3 the reticle reaches past the cone, so at full twist the arm beams snapped to the arm's line… |
| Gait blend-in, 0x417a80 (0x417d85) | MW3 | the right foot snaps to the walk's angle and the left lurches toward the right's old one for the first 2/3 s… |
| The nav waynet's frame, 0x00479c50 (vehicles.md, The nav waynet) | MW3 | once past the selected point the rest of the list is skipped: those nav points are not marked reached (their… |
| The debriefing's mission time, 0x0047b750 | MW3 | past an hour 01:01:3640 |
| Saved game replaced before the question, 0x478f10 / 0x434e00 | MW3 | No keeps the old entry in the list, but its file already holds the new world |
| Lancemates' career kills, 0x0047ac20 (the Win State's copy of the… | MW3 | a lancemate not on that card (or not matched by name) loses their career mech kills on every win |
| Delayed updates, 0x4d3d30 | MW3 | the new timers are lost: c3 mission 2's `ambush_powerup_down_a` can lose three radio lines |
| The radar's contacts, 0x0043dd00 (from 0x0043e445; hud.md section 15) | MW3 | a targets.zrd building's node is at the world's origin, its mesh in world co-ordinates, so a targeted… |
| The HUD's timer, 0x43c390 | MW3 | past an hour the seconds run into the thousands: 3700 s shows 01:01:3640 |
| TARGET's distance past 1 km, 0x00441850 | MW3 | a cosmetic text slip |
| The credits' "Other Voices" line, read by 0x00429200 | MW3 | a typo in dlgCredits.zrd: the heading sits indented like a name with no gap |
| A world point on the screen, 0x53cf10 (TARGET's tracking) | MW3 | the caller (TARGET's tracking 0x440b00) tests those bits, so which way the arrow goes for a target behind… |
| SORT_CHASSISNAME, 0x004496c0 | MW3 | "sort by name" sorts by the game's own mech order |
| Particle fade-in, 0x5489a0 | MW3 | a typo: inside a puffer's fade-in band the alpha is negative and the particle is not drawn, instead of… |
| Sound fades, 0x571940 | MW3 | fades to a partial volume never end: the voice wobbles a step around the target and the fade stays in the… |
| Unpause, 0x56f130 | MW3 | sounds that had ended start over and doubles pile up after a pause |
| Wingmen's ammo calls, group reader 0x573560 | MW3 | the low-ammo and out-of-ammo lines never play |
| Stereo, the PLAYBACK_FORMAT reader (zsnd_init `read_playback`) | MW3 | on Windows 9x the mix was mono, so the 3D sounds' left/right pan was lost |
| Medium sound format, 0x570ccf and the per-sound formats | MW3 | a typo: the medium playback format is never read |
| Mad Cat and Supernova damage textures, skin reader (`mech3::read_skins`) | MW3 | misplaced data: those mechs show the wrong or no battle damage |
| ZAPS_HUD, WeaponType reader 0x4af71b | MW3 | the HUD static (0x43be70 adds the first value to a channel's level and keeps the largest second value as a… |
| Polygon add, colinear points, zModel 0x53a530 / 0x53c6c0 | MW3 | the texture and shading of such a polygon slide by one corner from the dropped point on; about 1 in 200… |
| A movie's step, 0x00599de0 with the stream's frame 0x0059aaf0 (fmv.md… | MW3 | the two clocks start together, so each ask falls on the sound's frame change: with a play cursor in 10 ms… |
| A movie's sound frame at a wrap, 0x0059ad70 (fmv.md section 5) | MW3 | Video for Windows makes each half an exact second, so a device's 10 ms cursor steps land on 0 at every wrap:… |
| The frame's fill in a mission, 0x00576cf0 and 0x005777f0 (Owen,… | MW3 | from height, a band between the far ground and the sky |
| Pirate's Moon's near clip of a polygon with sets, 0x0054a6f0… | PM | The sibling shows the intent; the leftover is another polygon's fog, or nothing |
| Pirate's Moon's projected lights over a light map, 0x00562840, 0x00562120… | PM | The colours are plainly meant to be averaged like the other sets'; the first is simply missed |
| Pirate's Moon's projected lights' splits, 0x00562840… | PM | As MW3's row |
| Medium sound format, 0x570ccf and the per-sound formats (Pirate's Moon… | PM | a typo: the medium playback format is never read |
| Pirate's Moon's HUD static, 0x0043dfa0 (hud.md section 21) | PM | the level is worked out and then ignored, where MW3's static thickened with damage |
| A key typed into a text field, zInput's read 0x0051a5d0 (input.md;… | MW3 | a key let go after the Enter that closes a line (the cheat box, the chat) read as released to the game: L,… |
| A stick's throttle lever, PlayerControls' axes 0x00427870 (W56; Reddit… | MW3 | a lever had no reverse of its own: a player had to find Backspace |
| The reverse flag on a stick's lever, PlayerControls' axes 0x00427870 (a… | MW3 | the lever has its own reverse now, and the flag stuck: throttle-down tapped with the lever behind stop, or… |
| The axis popup's sources and OK, 0x00430180 and 0x0042e5b0 (W56) | MW3 | a lever on a slider (a T.16000M's) could not be picked; a throttle bound with the lever at an end gave stop… |
| The fall's force feedback, 0x00433090 (input.md section 7) | MW3 | the effect patch 1.2 added for falling over never plays |
| The game options' invert box, ACCEPT 0x00438100 | MW3 | the player's mouse sensitivity (the registry's MouseAccelY, any size) is lost each time the options are… |
| Key names, 0x51c480 | MW3 | 0xde..0xff read the joystick's names, then numbers, as names |
| The saved mouse settings, the same writes (MouseAccelX/Y at… | MW3 | a value set for a moment would reach the disk; with our MOUSE SENSITIVITY the player's own value must… |
| A peer's latency, 0x00470a00 (multiplayer-port.md piece 14c, the 1.2… | MW3 | the clamp means a negative trip to give 0; read unsigned, a delay past the round trip (a stall, a peer's… |

### By Owen's call (4), and two whose code has no switch (2)

Owen, 2026-11-10: all four multiplayer fixes always on (the latency's is in the restored rows' table above), and the turrets' pick.

| Fix | Game | Why |
|---|---|---|
| The turrets' pick, 0x004a70d0 (vehicles.md, The turrets' frame) | MW3 | the count runs on across missions: each later mission's turrets act once every as many frames as turrets… |
| A remote mech's aim carried on by the lag, 0x0046e4d0… | MW3 | the correction is about 0.1%: the remote aim shown lags behind |
| A collision's push from a remote mech, 0x004705b0 (multiplayer-port.md… | MW3 | a pilot killed by the push credits no one |
| The lock's stock loadout, 0x0049b360 (multiplayer-port.md piece 11b) | MW3 | every player is put back on the mech's first stock loadout, even one who chose another stock one |
| AI backup node's state, 0x4cfad0 | MW3 | the exe's aim is a stack vector it never sets, so there is no original way to give back: the code is unconditional, always on (the lead, 2026-11-10: the list shows exactly what the switch turns off). No shipped net has a backup node |
| LOCK_ON_LEAD, 0x583fb9 | MW3 | a typo: `[eax+0xc]` for `[eax+4]`; its code is unconditional (zwep_init.cpp, the count read as meant), so always on and off the Small fixes list (the lead, 2026-11-10) |

### Copy-protection traps, FIX (trap) (5)

| Fix | Game | Why |
|---|---|---|
| The mech lab's import, 0x004260c0 | MW3 | hand-edited loadouts refused, as the saved ones (protection.md) |
| Saved loadouts, Load 0x424600 | MW3 | anti-tamper, `protection.md` |
| Exe self-check, 0x4854c0 | MW3 | as the row says |
| Multiplayer rules check, 0x46fc08 | MW3 | as the row says |
| Archive lines, 0x5955e0 / 0x596620 | MW3 | a damaged interp.zbd crashes or overruns |

### Only in our own tools (the editor, mods) (5)

No original play to restore: the original never reaches these.

| Fix | Game | Why |
|---|---|---|
| The developers' teleport 0x004a37a0 and dialog 132 (editor.md, Teleport;… | MW3 | crashes, a lost backup, a Cancel that does not cancel; a world a mod adds may have no location.zrd |
| The AI net record compare, 0x4c6680 (editor.md, The record cache) | MW3 | an edit of only those reads as the file's own, and the record cache drops it: lost at the save |
| The AI net record cache's copy, 0x4dc2b0 (0x4da790 then 0x4c63a0) | MW3 | every edited node saved without its auxname |
| The editor's reload, 0x4bed90 (editor.md, The reload) | MW3 | an edit to a running net shows only at the next start |
| Pirate's Moon's teleport, a typed place 0x004a8d70 (editor.md, Pirate's… | PM | MW3's took the yaw field; a typed place saved with a stray facing |

### fixes.md's data typos (4), made 2026-11-10

fixes.md's bulleted list (Original bugs still to fix) holds data typos for the
importer besides the tables' rows; c2m3's Fireflies there is the Missions row
below. The list's other entries (the CPU speed, the precision notes, the
fonts) are port fixes, FIX (A) / (B), or the exe's own precision: always on,
or no change. Two typos are pictures and text (always on), two play (Small
fixes, items 19 and 20). Made in the readers (mac4/upgrades-typos), the data untouched:

| Fix | Game | Why |
|---|---|---|
| Firefly skin 5's label `MSG_SKIN_VARIANTD` (should be `VARIANTE`, SLATE) | MW3 | text: always on (`read_skins`, the label of the `_e` skin) |
| c1's script `Object3DSetScrllAlways` on `ocean52` (should be `Object3DSetScrollAlways`) | MW3 | a picture: the water does not scroll. Always on (the interp's dispatcher reads the misspelt word as the command) |
| Sunder Alternate B's `extra (120 120)` beside the LRM 20's `ammo` instead of inside it | MW3 | play: 240 missiles lost. Small fixes |
| Strider's `recoil_slide ("1.5.0")` as text (should be `1.5`) | MW3 | play: its recoil read as none. Small fixes |

## (b) Own switches

### MFB regroup after repair (2)

| Fix | Game | Why |
|---|---|---|
| The dock's places step, 0x0045ab70 with the request 0x0045a780… | MW3 | called to the point they last docked at, the bases already stand on their slots (the close drives them back,… |
| The repair dock after a repair, 0x0045b6c0 / 0x0045b6f0 (vehicles.md,… | MW3 | every repair needs a new M-M at the same place, and a lancemate cannot use the bases after the player |

### Improved AI (4)

| Fix | Game | Why |
|---|---|---|
| An ordered lancemate's follow, 0x004d1980 (ai.md section 17.6, W17) | MW3 | a lancemate told to follow or attack stands behind a wall, short of the target, for good |
| An ordered lancemate's follow round corners, 0x004d1980 (ai.md section… | MW3 | round a corner or along a canyon he is left behind, against a wall or stood still |
| An AI mech's ground and collision queries, 0x00476a30 and 0x00422f5c… | MW3 | a lancemate in a zone the player is not in loses his floor; the skis take the land over him (the lowest of… |
| A lancemate's walk, the follow's 0x004d1980, the fight's 0x004d1d10 and… | MW3 | Owen: "the pathing is a ridiculous joke": a lancemate stands at a river, a wall or a shape's edge for good,… |

### Water movement (1)

| Fix | Game | Why |
|---|---|---|
| Deep water, gait 0x4181c0 | MW3 | the factor falls from 0.63 to a standstill as the water nears the mech's height, then jumps back to 0.7 |

### Artemis tracking (1)

| Fix | Game | Why |
|---|---|---|
| Missile homing, 0x00585900 (weapons.md, Homing) | MW3 | the FCS "double tracking" never applies (the terrain following, +0x5c8, does) |

Deep water: its row once said "game-breaking per Owen" (2026-10-30, when it was folded into `bugs`); release.md names water movement as an upgrade, so it is listed there.

## (c) Small fixes (20)

One switch; the in-game list above names each in the same order.

### Turrets, vehicles and the AI's data (8)

| Fix | Game | Why |
|---|---|---|
| A turret shut down, 0x004a7620 | MW3 | the flag test is the wrong way round: an effect lit as the turret dies (a charge before a shot) stays lit;… |
| Loose orders, aiv.zrd 0x4df730 | MW3 | misplaced data: the reader skips them, so those enemies follow no scripted orders |
| A turret's yaw, 0x004a71c0 (W50) | MW3 | a facing straight up or down (the tripod turrets are made so) zeroes the yaw matrix's axes: the turret and… |
| Vehicle gun limits, 0x4a41b0 (vehicles.md, The gun's aim) | MW3 | a vehicle's clamped aim misses its limit (24.6 degrees for a target dead ahead 40 degrees up with the… |
| Vehicle motion block, 0x49ef40 (vehicles.md) | MW3 | a vehicle with chas_pitch and no chas_roll loses its pitch settings (no shipped vehicle has either key) |
| Pirate's Moon's backup node's state, 0x4ddab0 (zai_drive_pm `run_backup`) | PM | as MW3's |
| Pirate's Moon's deploy node's valves, 0x4de0e0 (zai_drive_pm… | PM | as MW3's |
| Pirate's Moon's range actions on a valve, 0x4f5050 (`RangeAction::fire`) | PM | no brain, no write, as the scan switches. No shipped file uses these keys. |

### Weapons (5)

| Fix | Game | Why |
|---|---|---|
| The reticle's pick, 0x00444350 (hud.md section 10) | MW3 | six weapons reach past it (ER large laser and NARC 1000 m, ER PPC 920, Gauss 880, LB 2-X 870, Ultra AC/2… |
| Plain PPC, weapon table 0x5e64f0 | MW3 | the mech lab cannot offer a weapon the data defines |
| TAG's strike, 0x4afed0 | MW3 | they arrive 7 s later where the target stood: a mech walking at 30 kph or more is not touched |
| A beam after its button is let go, 0x004b4230 (weapons.md, Beams) | MW3 | the beam trails a moving mech by a metre or more, and its ray starts there (playtest #24) |
| Weapon::destroy, 0x004b3020 (pm-tests item 21) | MW3 | the selection stays on a destroyed weapon when the one after it is gone too |

### Mech movement (1)

| Fix | Game | Why |
|---|---|---|
| Placing a mech, 0x4161c0 | MW3 | a mech re-placed where it stands (a mech swap, 0x4783c0) can drop under a bridge or land on a roof; the… |

### Missions (4)

| Fix | Game | Why |
|---|---|---|
| Animation prerequisites, 0x50aaa3 | MW3 | the mission's last animation can never be waited for |
| Missing animation object, 0x50a0e0 | MW3 | the animation then fires without waiting for any of its conditions |
| c4b missions 5 and 6's boundaries, the aiv.zrd vlist reader 0x4dfb00 | MW3 | leaving either mission's area never fails it |
| c2m3's secondary objective (objectives.zrd c2 `m3`, "Destroy Smoke Jaguar… | MW3 | the objective completes with both Fireflies alive (playtest W24). Made already: `fix_objective_data` points them at firefly_03 / 04 (Owen 2026-10-25; fixes.md, Original bugs still to fix, whose row here was stale); Small fixes (Owen 2026-11-10) (the… |

## Left as the original (Owen, 2026-11-10) (4)

Owen's call: these play as they shipped, the fix removed (Pirate's Moon's firepoint cone, behind `bugs` today, takes the exe's way always) or never made.

| Fix | Game | Why |
|---|---|---|
| A firepoint's aim cone (Pirate's Moon 0x4a6ee0) | PM | a Pirate's Moon typo that changes a firepoint's aim limit: a balance change as much as a bug |
| Pirate's Moon's aim re-pick, 0x4bb290 (`AimPm::repick`) | PM | Pirate's Moon's aim re-pick may be a slip or meant: making it MW3's changes how often PM's AI hits |
| Vehicle motion block, 0x49ef40 / 0x49ecd0 (vehicles.md, In the program) | MW3 | Candidate for Owen: vehicles never place their collision points and pass through walls and buildings except… |
| Vehicle suspension, 0x4a1760 / 0x4a23f0 (vehicles.md, The suspension) | MW3 | Candidate for Owen (balance): vehicles are never slowed in quicksand or lava as their quicksand_slowdown /… |

## Not changes

Rows of fixes.md that change nothing (candidates for Owen, ported as written, a row naming the other branch's fix).

| Fix | Game | Why |
|---|---|---|
| The mech lab's Ctrl+X / Ctrl+I on Pirate's Moon | PM | none: ported as written |
| Pirate's Moon's `MipBias -1` (common.gw), 0x00598230 | PM | Not reproduced yet: the words are read, the value goes nowhere. Candidate for Owen with the renderer's… |
| Deep water, gait (MW3 0x4181c0; Pirate's Moon 0x418fc0) | PM | Pirate's Moon's own WATER switch since 2026-11-10 (Owen): its flat 60% eased in from full speed, the exe's way with it off (fixes.md, On Pirate's Moon) |
| Plain PPC, on `pm` | PM | Not a fix on this branch: the row above is MW3's |
| String variables, 0x596020 | MW3 | no effect (was under unsure): nothing registers a string variable, so the fix never runs |
