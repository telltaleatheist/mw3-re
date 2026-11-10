#pragma once
#include <functional>
#include <string>

// Ours (docs/mods-campaigns-plan.md, Inside a campaign entry): a run started
// into a MODS campaign entry (--entry ID with --entry-play or --entry-ia)
// shows that entry's own screen only, never the main menu: the main menu's
// state opens the screen as soon as it is up, opens it again when a mission
// comes back to it, and the screen's BACK leaves (the program starts again
// plain, on MODS). Unset, the original's main menu.
namespace mech3 {

struct EntryRun {
  enum Screen { kNone, kPlay, kInstantAction } screen = kNone;
  std::string id;               // the entry (campaign_entries.h)
  std::string mod, campaign;    // a mod's campaign: its folder and campaign= id; "" the game's own
  std::function<void()> leave;  // the program's: the game closed and started again on MODS
  bool opened = false;          // its screen opened since the main menu was last entered
  bool left = false;            // leave called
};
EntryRun& entry_run();

}  // namespace mech3
