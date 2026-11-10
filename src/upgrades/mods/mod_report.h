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

// Text wrapped to lines of at most `width` characters, at spaces.
std::vector<std::string> mod_report_wrap(const std::string& text, size_t width);

}  // namespace mech3
