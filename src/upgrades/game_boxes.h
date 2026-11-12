#pragma once
#include <cstdint>
#include <functional>
#include <string>

// Ours (playtest W16, Owen, 2026-11-12): no system dialogs outside the Battlesport editor. A box the player
// sees is the game's own (dlgMessage.zrd's, ztwod's message box) whenever the game's screens are up; a
// system box only where nothing of the game can be drawn (before its window, after a crash).
namespace mech3 {

// The first start's notes (the data, the music) on a plain start without the launcher: shown once in the
// game's box when the main menu first comes up (MainState::enter), where a system box showed them before
// the window.
inline std::string& first_start_notes() {
  static std::string s;
  return s;
}

// The game's box while its screens are up (set by the engine at the main menu, cleared at its end), for
// the original's own MessageBoxA calls (platform::Alert): its answer, IDOK 1, IDCANCEL 2 ... Unset: the
// platform's system box.
inline std::function<int32_t(const std::string& text, const std::string& caption, uint32_t style)>& game_box() {
  static std::function<int32_t(const std::string&, const std::string&, uint32_t)> f;
  return f;
}

}  // namespace mech3
