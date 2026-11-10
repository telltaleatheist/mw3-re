#pragma once
#include <functional>
#include <string>

// Ours (Owen, 2026-11-09; docs/editor.md, The way in from MODS): a mod's
// mission entered by the MODS screen's EDIT runs with the game editor open
// from its first frame, its saves going into the mod itself, and EXIT GAME
// EDIT taking the player back to MODS. Shift+F12 in play is untouched.
namespace mech3 {

class Engine;

struct ModsEditSession {
  bool active = false;   // a mission entered by EDIT is loading or running
  bool entered = false;  // the editor opened on it
  bool leave = false;    // EXIT GAME EDIT pressed: back to MODS on the Core state's next update
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
// From the Core state's update: after EXIT GAME EDIT, the mission cleaned up
// (0x00491780) and the main menu up (MODS opened again from it). True when it went.
bool mods_edit_leave(Engine& en);
// The session over, by whatever way the mission ended (the main menu's enter).
void mods_edit_end();

// Ours (Owen, 2026-11-09; docs/editor.md, The way in): the editor is reached
// only from a mission the MODS screen launched, EDIT's or a custom campaign's
// (ENTER), there by Shift+F12 and the pause's EDITOR. Elsewhere (the campaign,
// Pirate Missions, Instant Action, training, multiplayer) neither does
// anything. --debug-camera, a developer's command-line flag no player
// reaches, opens it anywhere.
bool editor_reachable();
// Shift+F12 or the pause's EDITOR in such a mission: the editor opened
// (Shift+F12 again closes it), its saves into the mission's mod (EDIT's, the
// campaign's), or for --debug-camera elsewhere into "editor edits".
void editor_from_level();

}  // namespace mech3
