#pragma once
#include <map>
#include <string>
#include <vector>

// Ours (docs/modding.md, Plain errors on the MODS screen; modder step 3): a
// mod's check and the mount log as sentences for a player, on the in-game
// MODS screen and the launcher's.
namespace mech3 {

// What `mod_check` found in a mod (it fills one when given), and what this
// start's mount left out of it.
struct ModFindings {
  bool overlay = false;     // no mod.txt: checked as files laid over an install
  bool against_install = false;  // replaces and adds counted (an install to compare with)
  int files = 0, replaces = 0, adds = 0, ignored = 0, unread_anims = 0;
  std::vector<std::string> mechs, weapons, worlds, ia_worlds, campaigns;
  std::vector<std::string> refused;   // "<what>: <why>", --check-mod's errors
  std::vector<std::string> warnings;  // "<what>: <why>", --check-mod's warnings
  std::vector<std::string> left_out;  // "<what>: <why>", the mount log's (this start)
};

// The sentences, in this order: what it changes, what it adds, what is
// ignored or unread, each thing refused, each note, what this start left out.
std::vector<std::string> mod_sentences(const ModFindings& f);

// One finding ("mech Thor B: there is a mech Thor B already") as a sentence's
// subject and reason ("The mech Thor B", "there is a mech Thor B already").
std::string mod_finding_subject(const std::string& what);

// What this start's mount left out, by mod folder (mods_mount fills it each
// mount): "<what>: <why>", "the mod" for a mod left out whole.
std::map<std::string, std::vector<std::string>>& mods_left_out();

// A mod's sentences: mod_check against `install` (the game's files it
// replaces or adds; "" counts them only), and what this start's mount left
// out of the mod in `folder` (its folder name in mods\, "" none). `refused`
// gets the check's error count.
std::vector<std::string> mod_report(const std::string& dir, const std::string& install, const std::string& folder,
                                    int* refused = nullptr);

// A copy (a campaign saved from another: mod.txt's base=, docs/mods-campaigns-plan.md)
// against what it was saved from: the mods under it over the base= chain,
// nearest first, then the game's own files.
struct CopyChanges {
  bool is_copy = false;             // the mod has a base=
  std::string base, base_name;      // the entry it was saved from, and its name as shown
  std::vector<std::string> under;   // the mods under it (folders), nearest first; none for a copy of the game's own
  std::string missing;              // an entry of the chain that is not there: nothing below it could be compared
  // Its files (as in the mod, '/' between), by what they do to what is under it.
  std::vector<std::string> replaced;       // a mod under it has the file, with other bytes
  std::vector<std::string> replaced_game;  // no mod under it has it; the game's install does
  std::vector<std::string> added;          // nothing under it has it
  std::vector<std::string> same;           // a mod under it has it byte for byte
  // Its mod.txt rows by name ("mech Thor B", "weapon GAUSS2", "world w1", "campaign night.txt"), against the mods under it.
  std::vector<std::string> rows_changed, rows_added;
};
// `folder` is the copy's folder in <write dir>/mods; `install` the game's ("": the game's own files are not looked at,
// and a file no mod under it has counts as added).
CopyChanges mod_copy_changes(const std::string& write_dir, const std::string& install, const std::string& folder);
// As sentences for REPORT, after the mod's own (mod_report); none when it is not a copy. With part of the
// chain missing only what is certain is said (what it replaces in the mods still there).
std::vector<std::string> mod_copy_sentences(const CopyChanges& c);
std::vector<std::string> mod_copy_report(const std::string& write_dir, const std::string& install, const std::string& folder);

// Text wrapped to lines of at most `width` characters, at spaces.
std::vector<std::string> mod_report_wrap(const std::string& text, size_t width);

}  // namespace mech3
