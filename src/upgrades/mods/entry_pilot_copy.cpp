#include "upgrades/mods/entry_pilot_copy.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

#include "mech3/mod_campaign.h"

namespace fs = std::filesystem;

namespace mech3 {
namespace {

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

bool plain_name(const std::string& n) {
  if (n.empty() || n == "." || n == "..") return false;
  return n.find_first_of("/\\:") == std::string::npos;
}

// A pilot's saved game: <name>CCMM.mw3 (CampaignGame::path_for), in any letter case.
bool game_of(const std::string& file, const std::string& name) {
  const std::string f = lower(file), n = lower(name);
  if (f.size() != n.size() + 8 || f.compare(0, n.size(), n) != 0 || f.compare(f.size() - 4, 4, ".mw3") != 0) return false;
  return std::all_of(f.begin() + static_cast<std::ptrdiff_t>(n.size()), f.end() - 4, [](char c) { return c >= '0' && c <= '9'; });
}

// The regular files of `dir` (not followed into, links left out), by name.
std::vector<std::string> files_of(const fs::path& dir) {
  std::vector<std::string> out;
  std::error_code ec;
  for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
    if (it->is_regular_file(ec) && !it->is_symlink(ec)) out.push_back(it->path().filename().string());
  std::sort(out.begin(), out.end());
  return out;
}

struct Made {
  std::vector<fs::path> files, dirs;  // in the order made
  // Only what this call made: its files, then its folders if they are empty again.
  void undo() {
    std::error_code ec;
    for (auto f = files.rbegin(); f != files.rend(); ++f) fs::remove(*f, ec);
    for (auto d = dirs.rbegin(); d != dirs.rend(); ++d) fs::remove(*d, ec);  // not remove_all: an empty folder or nothing
    files.clear(), dirs.clear();
  }
  bool folder(const fs::path& d) {
    std::error_code ec;
    if (fs::is_directory(d, ec)) return true;
    if (!folder(d.parent_path())) return false;
    if (!fs::create_directory(d, ec) || ec) return false;
    dirs.push_back(d);
    return true;
  }
  // A new file only: one already there is never replaced.
  bool copy(const fs::path& from, const fs::path& to) {
    std::error_code ec;
    if (fs::exists(fs::symlink_status(to, ec))) return false;
    if (!fs::copy_file(from, to, fs::copy_options::none, ec) || ec) {
      fs::remove(to, ec);  // a part of one, made by this call
      return false;
    }
    files.push_back(to);
    return true;
  }
};

}  // namespace

std::vector<std::string> main_menu_pilots(const std::string& write_dir) {
  std::vector<std::string> out;
  for (const std::string& f : files_of(fs::path(write_dir) / "pilots"))
    if (f.size() > 4 && lower(f).compare(f.size() - 4, 4, ".mw3") == 0) out.push_back(f.substr(0, f.size() - 4));
  return out;
}

PilotCopy entry_copy_pilot(const std::string& write_dir, const std::string& name, const std::string& entry_dir,
                           const std::string& configs_dir) {
  PilotCopy r;
  auto refuse = [&](const std::string& why) {
    r.ok = false, r.why = why, r.copied.clear();
    return r;
  };
  std::error_code ec;
  if (!plain_name(name)) return refuse("\"" + name + "\" is not a pilot's name");
  const fs::path from = fs::path(write_dir) / "pilots", entry = entry_dir, to = entry / "pilots";
  if (write_dir.empty() || entry_dir.empty() || !fs::is_regular_file(entry / kCampaignMarker, ec))
    return refuse(entry.generic_string() + " has no " + kCampaignMarker + ": not a campaign's folder of the game's");
  // The pilot's own file, by its name in any letter case (the original's file names).
  std::string file;
  for (const std::string& f : files_of(from))
    if (lower(f) == lower(name) + ".mw3") file = f;
  if (file.empty()) return refuse("there is no pilot " + name + " to copy");
  const std::string stem = file.substr(0, file.size() - 4);
  for (const std::string& f : files_of(to))
    if (lower(f) == lower(file)) return refuse("this campaign already has a pilot named " + stem);
  for (const std::string& f : files_of(to / "games"))
    if (game_of(f, stem)) return refuse("this campaign already has a saved game of a pilot named " + stem);

  // What to copy: (from, to) pairs, each file's .rows with it.
  std::vector<std::pair<fs::path, fs::path>> jobs;
  auto with_rows = [&](const fs::path& a, const fs::path& b) {
    jobs.emplace_back(a, b);
    const fs::path rows = a.string() + ".rows";
    if (fs::is_regular_file(rows, ec) && !fs::is_symlink(rows, ec)) jobs.emplace_back(rows, fs::path(b.string() + ".rows"));
  };
  with_rows(from / file, to / file);
  for (const std::string& f : files_of(from / "games"))
    if (game_of(f, stem)) with_rows(from / "games" / f, to / "games" / f);
  if (!configs_dir.empty()) {
    std::vector<std::string> there;
    for (const std::string& f : files_of(configs_dir)) there.push_back(lower(f));
    for (const std::string& f : files_of(fs::path(write_dir) / "configs")) {
      if (f.size() <= 4 || lower(f).compare(f.size() - 4, 4, ".mw3") != 0) continue;
      if (std::find(there.begin(), there.end(), lower(f)) != there.end()) {
        ++r.configs_kept;  // the entry's own loadouts for that mech stay
        continue;
      }
      // Anything of that mech's already there (a .rows without its file, a folder of the name):
      // that one loadout is left out, never the pilot's copy refused for it.
      const fs::path to_file = fs::path(configs_dir) / f, to_rows = to_file.string() + ".rows";
      if (fs::exists(fs::symlink_status(to_file, ec)) || fs::exists(fs::symlink_status(to_rows, ec))) {
        ++r.configs_skipped;
        continue;
      }
      with_rows(fs::path(write_dir) / "configs" / f, to_file);
    }
  }
  Made made;
  for (const auto& [a, b] : jobs) {
    if (made.folder(b.parent_path()) && made.copy(a, b)) continue;
    made.undo();
    r.configs_kept = r.configs_skipped = 0;
    return refuse("could not copy " + a.filename().string() + " into " + b.parent_path().generic_string() + "; nothing was copied");
  }
  for (const fs::path& f : made.files) r.copied.push_back(f.string());
  r.ok = true;
  return r;
}

}  // namespace mech3
