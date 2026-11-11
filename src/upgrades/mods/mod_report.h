#pragma once
#include <map>
#include <string>
#include <vector>

#include "upgrades/mods/mod_warning.h"

// Ours (docs/modding.md, Plain errors on the MODS screen; modder step 3): a
// mod's check and the mount log as sentences for a player, on the in-game
// MODS screen and the launcher's.
namespace mech3 {

// One warning of the check: where (the file, then the entry, row or item in it), the four parts, its group.
// A plain note (no parts: `w.wrong` is the whole sentence) is a harmless slip.
struct ModNote {
  std::string where;
  ModWarning w;
  ModWarningGroup group = ModWarningGroup::kSlip;
  bool plain = false;
};

// What `mod_check` found in a mod (it fills one when given), and what this
// start's mount left out of it.
struct ModFindings {
  bool overlay = false;     // no mod.txt: checked as files laid over an install
  bool against_install = false;  // replaces and adds counted (an install to compare with)
  int files = 0, replaces = 0, adds = 0, ignored = 0, unread_anims = 0;
  std::vector<std::string> mechs, weapons, worlds, ia_worlds, campaigns;
  std::vector<std::string> refused;   // "<what>: <why>", --check-mod's errors: the mod's as a whole (it is not added)
  std::vector<std::string> warnings;  // "<what>: <why>", --check-mod's warnings, as its lines
  std::vector<ModNote> notes;         // the same warnings in their parts, in the check's order
  std::vector<std::string> left_out;  // "<what>: <why>", the mount log's (this start)
  // The mod's files (their paths in it) the game cannot use: a warning each, and IMPORT leaves the file out
  // (one multiplayer map's entry is no reason to refuse a whole mod).
  std::vector<std::string> unread_files;
};

// The report, a line each, in this order: what it changes, what it adds, what is ignored or unread, why it is
// refused (the mod's as a whole), a totals line, then the warnings in three groups, each headed in plain words
// (what stops part of the mod working, what may crash the original game, harmless slips), each warning its place
// and its parts on lines of their own, and what this start left out. An empty line parts the groups.
std::vector<std::string> mod_sentences(const ModFindings& f);

// IMPORT's record of what it left out of a mod it added (ours): kModLeftOutFile in the mod's folder, a line a
// warning (where, wrong, did, original, fix, tabs between). The check says them again, so REPORT on the added
// mod still names the files that are no longer in it.
constexpr const char* kModLeftOutFile = "mw3-mod-left-out.txt";
std::vector<ModNote> mod_left_out_read(const std::string& dir);
bool mod_left_out_write(const std::string& dir, const std::vector<ModNote>& notes);

// REPORT's text file (ours): the report's lines written to <write dir>/mod-reports/<folder>.txt, one file a mod,
// replaced each time. The folder is the game's own, made with kModReportsMarker in it; a mod-reports folder
// without the marker is not ours and nothing is written into it. `path` gets the file's; false with `why`.
constexpr const char* kModReportsMarker = "mw3-mod-reports.txt";
bool mod_report_write(const std::string& write_dir, const std::string& folder, const std::vector<std::string>& lines,
                      std::string* path, std::string* why = nullptr);
std::string mod_reports_dir(const std::string& write_dir);

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
