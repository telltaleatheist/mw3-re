#include "upgrades/mods/import_folder.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>

#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/zutl_zar.h"
#include "mech3/data_version.h"

namespace mech3 {
namespace fs = std::filesystem;
namespace {

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

// A folder's child by name, whatever its case ("" when none).
fs::path child(const fs::path& dir, const std::string& name) {
  std::error_code ec;
  for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
    if (lower(it->path().filename().string()) == name) return it->path();
  return {};
}

bool same_bytes(const fs::path& a, const fs::path& b) {
  std::error_code ec;
  if (fs::file_size(a, ec) != fs::file_size(b, ec) || ec) return false;
  std::ifstream fa(a, std::ios::binary), fb(b, std::ios::binary);
  char x[65536], y[65536];
  while (fa && fb) {
    fa.read(x, sizeof x), fb.read(y, sizeof y);
    if (fa.gcount() != fb.gcount() || std::memcmp(x, y, static_cast<size_t>(fa.gcount())) != 0) return false;
  }
  return fa.eof() && fb.eof();
}

bool ends_with(const std::string& s, const char* e) {
  const size_t n = std::strlen(e);
  return s.size() >= n && s.compare(s.size() - n, n, e) == 0;
}

// The program and what runs with it: never part of a mod here (an exe patch
// cannot apply to the port; dgVoodoo and its like are the original's helpers).
bool program_file(const std::string& lower_rel) {
  const std::string name = lower_rel.substr(lower_rel.rfind('/') == std::string::npos ? 0 : lower_rel.rfind('/') + 1);
  for (const char* e : {".exe", ".dll", ".msi", ".bat", ".com", ".cmd", ".lnk", ".ini", ".conf", ".cfg", ".reg"})
    if (ends_with(name, e)) return true;
  return name.rfind("dgvoodoo", 0) == 0 || name.rfind("ddrawcompat", 0) == 0 || name.rfind("unins", 0) == 0;
}

// Every regular file under `dir` as (lower-case relative path, path); links are not followed and not listed.
std::map<std::string, fs::path> files_of(const fs::path& dir, int* links = nullptr) {
  std::map<std::string, fs::path> out;
  std::error_code ec;
  for (fs::recursive_directory_iterator it(dir, fs::directory_options::none, ec), end; !ec && it != end; it.increment(ec)) {
    if (it->is_symlink(ec)) {
      if (links) ++*links;
      if (it->is_directory(ec)) it.disable_recursion_pending();
      continue;
    }
    if (it->is_regular_file(ec)) out[lower(fs::relative(it->path(), dir, ec).generic_string())] = it->path();
  }
  return out;
}

const char* game_name(ModGame g) { return g == ModGame::kPm ? "Pirate's Moon" : "MechWarrior 3"; }

std::string files_word(int n) { return std::to_string(n) + (n == 1 ? " file" : " files"); }

// Only a folder holding our marker is removed (never one the player made).
void remove_ours(const fs::path& dir) {
  std::error_code ec;
  if (!dir.empty() && fs::is_regular_file(dir / kModStagingMarker, ec)) fs::remove_all(dir, ec);
}

}  // namespace

bool import_folder_is_install(const std::string& folder) {
  std::error_code ec;
  const fs::path dir = fs::path(folder);
  if (!fs::is_directory(dir, ec)) return false;
  const fs::path exe = child(dir, "mech3.exe"), zbd = child(dir, "zbd");
  return !exe.empty() && fs::is_regular_file(exe, ec) && !zbd.empty() && fs::is_directory(zbd, ec);
}

std::string import_folder_version(const std::string& folder) {
  int n = 0, of_10 = 0, of_12 = 0;
  const DataFile* files = data_files(&n);
  const std::map<std::string, fs::path> have = files_of(fs::path(folder));
  for (int i = 0; i < n; ++i) {
    const auto it = have.find(lower(files[i].path));
    if (it == have.end()) continue;
    std::error_code ec;
    const uintmax_t size = fs::file_size(it->second, ec);
    const bool s12 = size == files[i].size_12, s10 = files[i].size_10 && size == files[i].size_10;
    if (ec || (!s12 && !s10)) continue;  // a mod's own, or another version's
    std::ifstream f(it->second, std::ios::binary);
    uint32_t crc = 0;
    char buf[65536];
    while (f.read(buf, sizeof buf) || f.gcount() > 0) crc = zutl_zar::crc32(crc, buf, static_cast<size_t>(f.gcount()));
    if (s12 && crc == files[i].crc_12) ++of_12;
    else if (s10 && crc == files[i].crc_10) ++of_10;
  }
  return of_12 && !of_10 ? "1.2" : of_10 && !of_12 ? "1.0" : "";
}

void import_folder_discard(ImportFolder& f) {
  if (f.staging.empty()) return;
  const fs::path staging = fs::path(f.staging);
  remove_ours(staging);
  std::error_code ec;
  const fs::path base = staging.parent_path();
  if (fs::is_directory(base, ec) && fs::is_empty(base, ec)) fs::remove(base, ec);  // ours, and empty
  f.staging.clear(), f.picked.clear();
}

std::string import_folder_report(const ImportFolder& f) {
  if (!f.install) return {};
  std::string t = f.warning.empty() ? "" : f.warning + " ";
  t += "Took " + files_word(f.taken) + " that differ from your install";
  if (f.same) t += "; left " + std::to_string(f.same) + " the same";
  if (f.programs) t += "; the program and its dlls are not part of a mod here (" + std::to_string(f.programs) + ")";
  if (f.own) t += "; pilots, saves and settings stay yours (" + std::to_string(f.own) + ")";
  if (f.other) t += "; " + files_word(f.other) + " outside the game's folders left";
  return t + ".";
}

std::string import_folder_report_short(const ImportFolder& f) {
  if (!f.install) return {};
  if (!f.warning.empty())
    return "Took " + files_word(f.taken) + ": this folder is version " + f.version + " and your game " + f.install_version +
           ", so their differences are in the mod too.";
  return "Took " + files_word(f.taken) + " that differ from your install" +
         (f.programs ? "; the program and its dlls are not part of a mod here." : ".");
}

ImportFolder import_folder_prepare(const std::string& write_dir, const std::string& folder, const std::string& install,
                                   const ModAddLimits& limits) {
  ImportFolder f;
  if (!import_folder_is_install(folder)) return f;
  f.install = true;
  std::error_code ec;
  const fs::path src = fs::path(folder), mine = fs::path(install);
  auto at = [&](const fs::path& p) { return fs::weakly_canonical(p, ec).generic_string(); };
  // Never the game's own folders: the player's install, the write dir, one inside the other.
  if (install.empty() || !fs::is_directory(mine, ec)) return f.error = "there is no install of yours to compare it with", f;
  const std::string c = at(src), i = at(mine), w = at(fs::path(write_dir));
  if (c == i) return f.error = "that folder is your own install: pick the folder the mod was installed over", f;
  if (c == w || c.rfind(w + "/", 0) == 0 || w.rfind(c + "/", 0) == 0)
    return f.error = "that folder holds the game's own settings: pick the folder the mod was installed over", f;
  int links = 0;
  const std::map<std::string, fs::path> theirs = files_of(src, &links), ours = files_of(mine);
  // The other game's install (mod_install's check by the files only one game ships).
  {
    std::vector<std::string> a, b;
    for (const auto& [rel, p] : theirs) a.push_back(rel);
    for (const auto& [rel, p] : ours) b.push_back(rel);
    f.game = mod_game_of(a);
    const ModGame here = mod_game_of(b);
    if (f.game != ModGame::kUnknown && here != ModGame::kUnknown && f.game != here)
      return f.error = std::string("that folder is a ") + game_name(f.game) + " install, and yours is " + game_name(here), f;
  }
  // Another version of the data than the player's: said before anything is taken (it still imports).
  f.version = import_folder_version(folder), f.install_version = import_folder_version(install);
  if (!f.version.empty() && !f.install_version.empty() && f.version != f.install_version)
    f.warning = "This folder is version " + f.version + " and your game is " + f.install_version +
                ", so the differences between the versions are taken too: for a clean mod, install it onto a copy of the same version.";
  // The folders the player's install has at its top: what a mod of this kind may serve.
  std::set<std::string> tops;
  for (fs::directory_iterator it(mine, ec), end; !ec && it != end; it.increment(ec))
    if (it->is_directory(ec) && !it->is_symlink(ec)) tops.insert(lower(it->path().filename().string()));
  std::vector<std::pair<std::string, fs::path>> take;  // the relative path as the modded folder spells it
  int64_t bytes = 0;
  f.other = links;
  for (const auto& [rel, p] : theirs) {
    const size_t slash = rel.find('/');
    if (program_file(rel)) ++f.programs;
    else if (!zutl::mod_may_serve(rel)) ++f.own;
    else if (slash == std::string::npos || !tops.count(rel.substr(0, slash))) ++f.other;
    else if (const auto it = ours.find(rel); it != ours.end() && same_bytes(p, it->second)) ++f.same;
    else {
      bytes += static_cast<int64_t>(fs::file_size(p, ec));
      take.emplace_back(fs::relative(p, src, ec).generic_string(), p);
    }
  }
  if (take.empty()) return f.error = "every file in it is the same as your install's: nothing to add", f;
  if (static_cast<int64_t>(take.size()) > limits.max_files)
    return f.error = "it differs from your install in more than " + std::to_string(limits.max_files) + " files", f;
  if (bytes > limits.max_bytes) return f.error = "the files that differ are more than a mod may hold", f;
  // A staging folder of ours, the files under a folder named as the modded one (the mod's name).
  // Its own base, not mod-staging: mod_install's add clears every staging folder there before it copies from ours.
  const fs::path base = fs::path(write_dir) / "mod-import";
  fs::create_directories(base, ec);
  for (fs::directory_iterator it(base, ec), end; !ec && it != end; it.increment(ec))  // a run that stopped halfway: ours only
    if (it->is_directory(ec) && !it->is_symlink(ec)) remove_ours(it->path());
  const auto t = std::chrono::system_clock::now().time_since_epoch().count();
  fs::path staging;
  for (int n = 0; n < 1000; ++n) {
    staging = base / ("import-" + std::to_string(t % 1000000007) + "-" + std::to_string(n));
    if (fs::create_directory(staging, ec)) break;
    staging.clear();
  }
  if (staging.empty()) return f.error = "cannot make a folder in " + base.generic_string(), f;
  { std::ofstream(staging / kModStagingMarker) << "A mod being imported by the game; removed when it is done.\n"; }
  f.staging = staging.string();
  std::string name = fs::path(c).filename().string();
  if (name.empty() || name == "." || name == "..") name = "imported mod";
  const fs::path to = staging / mod_folder_name(name);
  for (const auto& [rel, p] : take) {
    const fs::path dst = to / fs::path(rel);
    fs::create_directories(dst.parent_path(), ec);
    if (!fs::copy_file(p, dst, fs::copy_options::none, ec)) {
      const std::string why = "cannot copy " + rel + (ec ? ": " + ec.message() : "");
      import_folder_discard(f);
      f.error = why;
      return f;
    }
  }
  f.taken = static_cast<int>(take.size());
  f.picked = to.string();
  return f;
}

ModAddDone import_folder_run(const std::string& write_dir, const std::string& source, const std::string& install,
                             const std::function<int(const std::string& name)>& ask, std::vector<std::string>* enabled,
                             std::string* report) {
  if (report) report->clear();
  ImportFolder f = import_folder_prepare(write_dir, source, install);
  if (!f.install) return mod_add_run(write_dir, source, install, ask, enabled);
  ModAddDone d;
  if (!f.error.empty()) return d.message = "Not added: " + f.error + ".", d;
  // The game is the modded install's when its files say; else asked as for any mod.
  const ModGame game = f.game;
  d = mod_add_run(write_dir, f.picked, install,
                  [&](const std::string& name) { return game == ModGame::kPm ? 2 : game == ModGame::kMw3 ? 1 : ask ? ask(name) : 1; },
                  enabled, source);
  import_folder_discard(f);
  if (d.ok) {
    std::printf("mw3: import: %s: %s\n", d.name.c_str(), import_folder_report(f).c_str());
    if (report) *report = import_folder_report_short(f);
  }
  return d;
}

}  // namespace mech3
