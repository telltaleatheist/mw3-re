#pragma once
#include <string>
#include <vector>

#include "mech3/mods_menu.h"

// Ours (docs/mods-campaigns-plan.md): the tick list of a campaign entry, one
// widget for every screen that shows one (Instant Action's MODS, and the MODS
// screen's right-hand table): every imported mod with [x] or [ ], in the
// order they win, ON/OFF, UP and DOWN, the chosen row's note (why it cannot
// be used, what it needs, what it shares with which earlier mod). The screen
// binds its four controls (MODLIST, TOGGLE, MOVEUP, MOVEDOWN in its dialog)
// and passes its buttons on; the ticks are the screen's to save.
namespace mech3 {

class ModTickList {
 public:
  enum Id : int32_t { kList = 0x7000, kToggle, kUp, kDown };
  explicit ModTickList(Engine& e) : list(e, kList), toggle(e, kToggle), up(e, kUp), down(e, kDown) {}
  // The rows from the write dir's mods\ with `ticks` on, in their order; the row chosen kept.
  void fill(const std::string& write_dir, const std::vector<std::string>& ticks);
  // A button of the list's: true when it was one (the ticks may have changed: `changed`).
  bool on_button(int32_t id, bool* changed = nullptr);
  void all_off();
  const ModsListEntry* chosen() const;
  std::string note() const;  // the chosen row's line, "" when nothing to say

  ModsList list;
  ShellButton toggle, up, down;
  std::string write_dir;
  std::vector<std::string> ticks;    // the folders on, in winning order
  std::vector<ModsListEntry> rows;   // mods_list's
  std::string warned;                // turning one on: what it needs (mod_needs.h), said once by the screen
};

}  // namespace mech3
