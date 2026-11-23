#pragma once
// Ours (MechWarrior 1 Remake's FIXES.md item 30; 2026-11-18): runs no player is at never touch the system's
// pointer. A hidden, --shot, --frames or scripted run, and every test program, never puts the window in relative
// mode (which on the Mac warps the pointer to the window's centre, over and over) nor moves the pointer: the game's
// own mouse state is decided as before (its checks unchanged), only SDL is not asked. A player's run is unchanged.
// MW3_POINTER_HANDS_OFF=1 turns it on for any run too.
// On main it also keeps the host keyboard's modifiers (Shift, Ctrl, Alt, Caps Lock) out of scripted typing (sdl_input.cpp).
#include <cstdlib>
#include <cstring>

namespace platform {

namespace hands_off_detail {
inline bool& on() {
  static bool v = [] {
    const char* e = std::getenv("MW3_POINTER_HANDS_OFF");
    return e && *e && std::strcmp(e, "0") != 0;
  }();
  return v;
}
}  // namespace hands_off_detail

inline void set_pointer_hands_off(bool on) { hands_off_detail::on() = on; }
inline bool pointer_hands_off() { return hands_off_detail::on(); }

}  // namespace platform
