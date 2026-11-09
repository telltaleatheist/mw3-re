# src/upgrades: the plan (proposal, 2026-11-09)

Owen's rule: a reader of the port should see exactly what the original does.
Fixes stay inline (`FIX (A|B|bug|trap)`, listed in fixes.md). The SDL wiring
stays where it is (`frontend/`, `platform/`, `crt/`: the stand-ins for
DirectX, MCI, Win32 and the CRT, FIX (A)). Everything we add that the
original does not have goes under `src/upgrades/`: gameplay upgrades of the
Upgrades page's kind (behind an upgrade switch, not the bug-fix one) and new
features. Nothing is moved yet. Owen (2026-11-09): the move waits for the final sweep; until then new feature and upgrade code is written straight into src/upgrades/.

## 1. Inventory (main, ee5fb3c3; 581 files)

How a file was classed: a port file holds `Mech3.exe 1.2 @` (or Pirate's
Moon) addresses; a header goes with its .cpp; "mixed" is a port file with
three or more "ours" blocks. Counts: port 332, mixed 82, ours 93, stand-in
74. The ours files, by where they would go:

| upgrades/ folder | files (src/...) |
|---|---|
| `mods/` | mech3/mods.*, mods_menu.*, mods_tables.h, mod_install.*, mod_campaign.*, mod_rows_file.* |
| `json/` | gamez/zreader/zreader_json.cpp, gamez/zutil/zutl_json.*, zutl_zip.*, gamez/zeffect/zeff_anim_json.*, gamez/zclass/cls_mechlib_json.*, cls_zbd_json.*, gamez/zsiscript/si_script_json.* |
| `install/` | mech3/data_folder.*, data_version.*, disc_install.*, source_archive.*, source_install.*, source_map.*, patch_import.*, patch12*.cpp/h (the official 1.2 patch's edits on 1.0 data, and our records in dialogs: MODS, UPGRADES) |
| `cheats/` | mech3/cheats.*, app_cheats.cpp |
| `debug/` | frontend/sdl/run_log.*, mech3/load_watch.*, app_hook_census.cpp, frontend/debug_world.*, gamez/zgame/unported.h (and the hit log's code where it sits) |
| `editor/` | mech3/net_save.* (the editor's own mod), our editor dialogs' additions in game_editor* |
| `ai/` | mech3/zai_nav.* (W58: the walkability grid, `nav_allies`/`nav_enemies`) |
| `game_setup/` | mech3/game_setup.* (the launch and the debug start), app_pocket.cpp |

Ours files that stay, because they are the port's own plumbing: the app_*
wiring between ported systems (app_fmv, app_movie, app_turret, app_force:
the original's calls made through our hooks), the struct headers without
addresses of their own (cls_data.h, zinput.h, zsnd.h, twod.h, vec3.h,
lance_armor.h, armor_display.h, collision_hit.h), zerr.cpp (the report
sink), zutl_path.* and zutl_msgdll.* (FIX (A): the mounts and
Mech3Msg.dll's strings), zutl_winnames.cpp, gmod_screen.*, zgeo_x87.h,
movie_shape.* (the Indeo layer's shapes), mech_repair_store.cpp, zfix.h
(the switches). These are the port's FIX (A) layer and wiring, not
features.

Mixed files with the most "ours" in them, where a feature reaches into a
ported file: app_engine.*, app_menus.cpp, app_states.cpp, mp_lobby.cpp and
mp_connect.cpp (the mods' identity), zai_attack.cpp (nav), app_hooks.h,
app_mission.cpp, app_mech.*, pilot.cpp (saved mod rows), cls_mechlib.cpp,
zeff_anim_init.cpp, zreader.cpp, si_script.cpp (the JSON loaders), hud.*,
options_menu.* (UPGRADES), mech_config.cpp and mech_names.* (mod rows),
controls_menu.cpp, mainmenu.h (MODS), zinterp.* (cheats, the console).

## 2. Layout

`src/upgrades/<area>/` as in the table above, one CMake list of its own
(`MW3_UPGRADES_SOURCES`), built into mw3core as now. A ported file never
includes an upgrades header except through one marked line. The ported
code's calls into upgrades go through one header per area,
`upgrades/<area>/hooks.h`: small functions, each documented with the ported
function it is called from.

The marker: a call into upgrades from a ported function is one line,

    if (upgrades::anim_from_mod(...)) ...   // UPGRADE (mods): docs/modding.md

with `UPGRADE (<switch or feature>)` the way `FIX (bug)` marks a fix. The
ported function then reads as the original's plus marked lines.

## 3. The upgrades inside ported functions today, and their one call each

| Where (ported function) | What | Becomes |
|---|---|---|
| zrdr::load 0x556740 (zreader.cpp: mod_copy, the load hook) | a mod's loose .zrd, JSON, zip | `upgrades::reader_entry(archive, name)` before the archive's read; the patch12 load hook stays a hook (set by install/) |
| load_motion 0x5a9ad0 (mech_anim.cpp: motion_from_mod, the next copy's fallback) | motion JSON; a broken copy from the next mount | `upgrades::motion_from_mod(s, name)`; the fallback loop moves with it |
| mechlib_load 0x4fc8f0, mechlib_read_materials 0x4fc9e0 (cls_mechlib.cpp: mod_entry) | mechlib JSON | `upgrades::mechlib_entry(lib, name, &bytes)` |
| anim_read_zbd 0x50a0e0 (zeff_anim_init.cpp) | anim JSON | `upgrades::anim_from_mod(path, &bytes)` |
| Mech::control axes 0x427870 (mech_control.cpp, `throttle_reverse`) | a lever's reverse | `upgrades::lever_throttle(...)` behind its switch |
| the AI's way, wired in app_mission.cpp (`nav_allies`/`nav_enemies`) | the walkability grid round walls | `upgrades::nav_around(...)` behind its switch (zai_nav moves to ai/) |
| the menus' canvas (app_menus.cpp, lab_view.cpp, `wide_menus`) | 16:9 menus | `upgrades::wide_canvas(...)`; app_menus is wiring, so its part stays there marked |
| mech_names / mech_config tables (rows past the exe's 19 mechs and 37 weapons) | mod rows | the exe's tables stay as data; `mech_count()` and the row lookups past them call `upgrades::mod_mech(i)` |
| options_menu (OPTIONS' UPGRADES button) and mainmenu (MODS) | our buttons | the button binds stay one marked line each; the pages move to upgrades/ui |
| zinterp's console and cheat words | cheats | `upgrades::cheat(word)` from the console's input |
| mp_lobby / mp_connect (the mods' identity in the hello) | lobby checks | one marked call each; the identity code is in mods/ already |

On pm the same, plus PM's Upgrades page switches (PM_AI's improvements, the
MFB regroup, water), each a marked call behind its switch from the PM AI's
ported functions; listed when the pm move is planned.

## 4. The report: tools/analysis/differences.py

Plain text, regenerated (and a CI check that it runs clean):
- every file under src/upgrades/, by area, with its one-line purpose (the
  file's first comment);
- every `FIX (A|B|bug|trap)` line: file:line, the function's address (the
  nearest `@ 0x` above it), and its fixes.md row, matched by that address or
  the row's file name;
- every `UPGRADE (...)` line likewise, with its switch (zfix::Options or
  mw3.cfg key) or "always" for features;
- the checks: a FIX with no fixes.md row, an UPGRADE with no switch or doc,
  a file in upgrades/ that a ported file includes other than through its
  area's hooks.h.

## 5. The cost on pm

pm merges main ("MW3's fixes merge from main into pm"). A move made only of
renames (git mv, the include lines, CMake) merges into pm with git's rename
detection, since pm changes those files little. Steps:
1. main: one commit of pure moves (no logic), then one commit adding the
   UPGRADE markers and hooks.h calls; the tests must pass unchanged after
   each.
2. pm: merge main straight away (renames carried), then one pm commit moving
   pm's own ours files (the PM Upgrades page's code, PM-only cheats) the same
   way.
3. Both devs between sittings, nothing in flight; dev 2's worktree rebased
   after. Cherry-picks after the move use the new paths. Before it, any
   branch in flight is merged first.
