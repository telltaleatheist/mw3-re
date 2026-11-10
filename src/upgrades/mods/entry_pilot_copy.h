#pragma once
#include <string>
#include <vector>

// Ours (docs/mods-campaigns-plan.md, Pilots and saves): a pilot of the main
// menu's campaign copied into a MODS campaign entry, where it goes on alone.
// The copy is the pilot's file, its saved games and the saved loadouts; the
// originals are only read, never written, moved or removed. Nothing already
// in the entry is replaced, and a copy that fails part-way leaves no file of
// its own behind (only the files this call made are removed).
namespace mech3 {

struct PilotCopy {
  bool ok = false;
  std::string why;                  // when not ok, for the player
  std::vector<std::string> copied;  // the files made, as full paths (none left when not ok)
  int configs_kept = 0;             // loadout files the entry already had: left as they were
  int configs_skipped = 0;          // loadouts left out because something of that name was in the way there
};

// The main menu's pilots that can be copied: the names of <write dir>/pilots/*.mw3, sorted.
std::vector<std::string> main_menu_pilots(const std::string& write_dir);

// Copies pilot `name` from <write dir>/pilots (its <name>.mw3, the saved games
// games/<name>CCMM.mw3, each one's .rows beside it) into <entry dir>/pilots,
// and the saved loadouts <write dir>/configs/*.mw3 into `configs_dir`.
// `entry_dir` is the entry's own folder (campaigns/<id>, our marker in it:
// one without the marker is refused) and `configs_dir` its loadouts' folder
// (empty: no loadouts copied). Refused, with nothing written: a name that is
// not a plain file name, no such pilot, a pilot or a saved game of that name
// already in the entry (in any letter case). The loadouts are not so strict:
// a mech's file the entry has is kept, and one whose place is taken by a stray
// file (its .rows without it, a folder of its name) is skipped; the pilot's
// copy goes on without it.
// `saves` false: the pilot's file and the loadouts only, none of its saved
// games (a different campaign's entry: entry_pilots.h).
PilotCopy entry_copy_pilot(const std::string& write_dir, const std::string& name, const std::string& entry_dir,
                           const std::string& configs_dir, bool saves = true);

}  // namespace mech3
