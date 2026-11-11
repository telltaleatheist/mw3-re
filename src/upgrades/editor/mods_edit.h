#pragma once
#include <functional>
#include <string>

// Ours (Owen, 2026-11-09; docs/editor.md, The way in from MODS): a mod's
// mission entered by the MODS screen's EDIT runs with the game editor open
// from its first frame, its saves going into the mod itself. The editor's
// exit (EXIT GAME EDIT, Shift+F12, the pause's PLAY MISSION) goes on with the
// mission in play (Owen, 2026-11-11: one switch between playing and editing,
// never a way out); MODS comes back when the mission is left or ends.
namespace mech3 {

struct ModsEditSession {
  bool active = false;   // a mission entered by EDIT is loading or running
  bool entered = false;  // the editor opened on it
  bool editor = true;    // false: MODS's TEST (modding.md, Quick testing), played without the editor up
  std::string mod;       // the mod's folder (saves go there)
  std::function<void()> finish;  // the program's part of the end: its stand-in pilot and launch let go
};
ModsEditSession& mods_edit_session();

// The session begun (the program has set its launch): saves into `mod`.
// Without `editor` (TEST's) the editor is not opened on the first frame and
// MODS comes back however the mission ends.
void mods_edit_begin(const std::string& mod, std::function<void()> finish, bool editor = true);
// From the HUD's frame (0x0040e4b0's caller): the editor opened (0x0040e430,
// as Shift+F12 opens it) on the mission's first frame.
void mods_edit_frame();
// The session over, by whatever way the mission ended (the main menu's enter).
void mods_edit_end();

// Ours (Owen, 2026-11-09; docs/editor.md, The way in): the editor is reached
// only from a mission the MODS screen launched, EDIT's or a custom campaign's
// (ENTER), there by Shift+F12 and the pause's switch. Elsewhere (the campaign,
// Pirate Missions, Instant Action, training, multiplayer) neither does
// anything. --debug-camera, a developer's command-line flag no player
// reaches, opens it anywhere.
bool editor_reachable();
// Shift+F12 or the pause's switch in such a mission: the editor opened, or
// closed when it is up (the mission goes on in play), its saves into the
// mission's mod (EDIT's, the campaign's), or for --debug-camera elsewhere
// into "editor edits".
void editor_from_level();
// The pause's switch says what a press does: "EDIT MISSION" in play, "PLAY
// MISSION" with the editor up.
bool editor_switch_edits();

}  // namespace mech3
