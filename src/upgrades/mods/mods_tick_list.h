#pragma once
#include <string>
#include <vector>

#include "mech3/mods.h"
#include "mech3/mods_widgets.h"

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
  // The rows from the write dir's mods\: `locked` first (a campaign's own
  // mod, its bases', what they need: on, their box reading "[on]", not the
  // player's to turn off), then `ticks` in their order, then the rest; `assets_only` leaves
  // out campaign mods that are not locked (campaign_entries.h,
  // mod_is_campaign). The row chosen is kept.
  void fill(const std::string& write_dir, const std::vector<std::string>& ticks, const std::vector<std::string>& locked = {},
            bool assets_only = false);
  // A button of the list's: true when it was one (the ticks may have changed: `changed`).
  bool on_button(int32_t id, bool* changed = nullptr);
  void all_off();
  const ModsListEntry* chosen() const;
  bool is_locked(const std::string& folder) const;
  std::string row_text(const ModsListEntry& m) const;  // as shown: "[on] Name" for a locked one
  std::string locked_why(const std::string& folder) const;  // "Part of this campaign: always on." / "Needed by <mod>: always on."
  std::string note() const;  // the chosen row's line, "" when nothing to say

  ModsList list;
  ShellButton toggle, up, down;
  std::string write_dir;
  std::vector<std::string> ticks;    // the folders the player ticked, in winning order (never the locked ones)
  std::vector<std::string> locked;
  bool assets_only = false;
  std::vector<ModsListEntry> rows;   // mods_list's, as shown
  std::string warned;                // a press's own word: turning one on, what it needs (mod_needs.h); a locked one, why
};

}  // namespace mech3
