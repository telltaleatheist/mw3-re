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
  std::string mod;       // the mod's folder (saves go there)
  std::function<void()> finish;  // the program's part of the end: its stand-in pilot and launch let go
};
ModsEditSession& mods_edit_session();

// The session begun (the program has set its launch): saves into `mod`.
void mods_edit_begin(const std::string& mod, std::function<void()> finish);
// From the HUD's frame (0x0040e4b0's caller): the editor opened (0x0040e430,
// as Shift+F12 opens it) on the mission's first frame.
void mods_edit_frame();
// From the Core state's update: after EXIT GAME EDIT, the mission cleaned up
// (0x00491780) and the main menu up (MODS opened again from it). True when it went.
bool mods_edit_leave(Engine& en);
// The session over, by whatever way the mission ended (the main menu's enter).
void mods_edit_end();

}  // namespace mech3
