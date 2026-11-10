#include "mech3/mod_install.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

#include "gamez/zutil/zutl_path.h"
#include "upgrades/mods/campaign_entries.h"
#include "mech3/source_archive.h"
#include "upgrades/mods/mod_export.h"

namespace mech3 {
namespace fs = std::filesystem;

namespace {

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::string read_text(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
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

// Only a folder holding our marker is removed (never one the player made).
void remove_ours(const fs::path& dir, const char* marker) {
  std::error_code ec;
  if (!fs::is_regular_file(dir / marker, ec)) return;
  fs::remove_all(dir, ec);
}

// The child of `dir` named `name` whatever its case, or empty.
fs::path child(const fs::path& dir, const std::string& name) {
  std::error_code ec;
  for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
    if (lower(it->path().filename().string()) == name) return it->path();
  return {};
}

bool looks_like_root(const fs::path& dir) {
  std::error_code ec;
  for (const char* d : {"zbd", "video", "music"}) {
    const fs::path c = child(dir, d);
    if (!c.empty() && fs::is_directory(c, ec)) return true;
  }
  const fs::path exe = child(dir, "mech3.exe");
  return !exe.empty() && fs::is_regular_file(exe, ec);
}

// The mod's root: the shallowest folder with a mod.txt, else with the
// game's own folders (zbd, video, music) or Mech3.exe, else a lone folder
// followed down, else the top.
fs::path find_root(const fs::path& top) {
  std::error_code ec;
  std::vector<fs::path> level{top};
  for (int depth = 0; depth < 4 && !level.empty(); ++depth) {
    for (const fs::path& d : level) {
      const fs::path m = child(d, "mod.txt");
      if (!m.empty() && fs::is_regular_file(m, ec)) return d;
    }
    for (const fs::path& d : level)
      if (looks_like_root(d)) return d;
    std::vector<fs::path> next;
    for (const fs::path& d : level)
      for (fs::directory_iterator it(d, ec), end; !ec && it != end; it.increment(ec))
        if (it->is_directory(ec)) next.push_back(it->path());
    std::sort(next.begin(), next.end());
    level = std::move(next);
  }
  fs::path at = top;
  for (int i = 0; i < 4; ++i) {  // files one folder down: a lone folder followed
    std::vector<fs::path> dirs;
    bool files = false;
    for (fs::directory_iterator it(at, ec), end; !ec && it != end; it.increment(ec)) {
      if (it->is_directory(ec)) dirs.push_back(it->path());
      else files = true;
    }
    if (files || dirs.size() != 1) break;
    at = dirs.front();
  }
  return at;
}

std::vector<std::string> files_under(const fs::path& dir) {
  std::vector<std::string> out;
  std::error_code ec;
  for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
    if (it->is_regular_file(ec)) out.push_back(fs::relative(it->path(), dir, ec).generic_string());
  std::sort(out.begin(), out.end());
  return out;
}

void remove_empty_dirs(const fs::path& dir) {
  std::error_code ec;
  std::vector<fs::path> dirs;
  for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
    if (it->is_directory(ec)) dirs.push_back(it->path());
  std::sort(dirs.rbegin(), dirs.rend());  // deepest first
  for (const fs::path& d : dirs)
    if (fs::is_empty(d, ec)) fs::remove(d, ec);
}

std::string stem_of(const std::string& source) {
  fs::path p(source);
  while (!p.empty() && p.filename().empty()) p = p.parent_path();  // a folder given with a trailing '/'
  std::string s = p.filename().string();
  for (const char* ext : {".tar.gz", ".tar.xz", ".zip", ".7z", ".rar", ".tar", ".tgz"}) {
    const size_t n = std::strlen(ext);
    if (s.size() > n && lower(s.substr(s.size() - n)) == ext) {
      s.resize(s.size() - n);
      break;
    }
  }
  return s;
}

std::string size_text(int64_t n) {
  if (n >= int64_t{1} << 20) return std::to_string(n >> 20) + " MB";
  if (n >= 1024) return std::to_string(n >> 10) + " KB";
  return std::to_string(n) + " bytes";
}

// Unpacks an archive into `to`, refusing it whole on the first bad entry.
std::string unpack(const std::string& source, const fs::path& to, const ModAddLimits& limits) {
  if (!source::archives_supported()) return "this build cannot read archives: add the mod's folder instead";
  source::ArchiveReader r;
  if (!r.open_file(source)) return "cannot read it as an archive" + (r.error().empty() ? "" : ": " + r.error());
  int64_t bytes = 0, count = 0;
  std::map<std::string, std::string> seen;  // each file's name in lower case, as first given
  std::vector<uint8_t> buf(1 << 16);
  source::ArchiveReader::Entry e;
  std::error_code ec;
  while (r.next(&e)) {
    std::string rel;
    if (e.other) return "it holds a link or special file (" + e.path + "), which a mod cannot have";
    if (!mod_entry_path_ok(e.path, &rel))
      return (mod_entry_escapes(e.path) ? "it holds a file that would land outside its folder ("
                                        : "it holds a name Windows cannot hold (") + e.path + ")";
    if (rel.empty()) continue;
    const fs::path dst = to / fs::path(rel);
    if (e.dir) {
      fs::create_directories(dst, ec);
      continue;
    }
    if (++count > limits.max_files) return "it holds more than " + std::to_string(limits.max_files) + " files";
    if (const auto [it, fresh] = seen.emplace(lower(rel), rel); !fresh && it->second != rel)
      return "it holds two files whose names differ only in case (" + it->second + ", " + rel + ")";
    if (e.size > 0 && bytes + e.size > limits.max_bytes)
      return "it unpacks to more than " + size_text(limits.max_bytes);
    fs::create_directories(dst.parent_path(), ec);
    if (fs::is_directory(dst, ec)) return "it holds a file and a folder of the same name (" + rel + ")";
    std::ofstream f(dst, std::ios::binary | std::ios::trunc);
    if (!f) return "cannot write " + rel;
    for (;;) {
      const int64_t n = r.read(buf.data(), buf.size());
      if (n < 0) return "a damaged entry (" + rel + ")" + (r.error().empty() ? "" : ": " + r.error());
      if (n == 0) break;
      bytes += n;
      if (bytes > limits.max_bytes) return "it unpacks to more than " + size_text(limits.max_bytes);
      f.write(reinterpret_cast<const char*>(buf.data()), n);
      if (!f) return "cannot write " + rel + " (is the disk full?)";
    }
  }
  if (!r.error().empty()) return "a damaged archive: " + r.error();
  if (count == 0) return "the archive is empty";
  return "";
}

// Copies a folder into `to`, refusing links (they could point anywhere).
std::string copy_folder(const fs::path& from, const fs::path& to, const ModAddLimits& limits) {
  std::error_code ec;
  int64_t bytes = 0, count = 0;
  std::map<std::string, std::string> seen;
  for (fs::recursive_directory_iterator it(from, fs::directory_options::none, ec), end; !ec && it != end; it.increment(ec)) {
    const fs::path rel = fs::relative(it->path(), from, ec);
    if (it->is_symlink(ec)) return "it holds a link (" + rel.generic_string() + "), which a mod cannot have";
    if (it->is_directory(ec)) {
      fs::create_directories(to / rel, ec);
      continue;
    }
    if (!it->is_regular_file(ec)) return "it holds a special file (" + rel.generic_string() + ")";
    std::string clean;
    if (!mod_entry_path_ok(rel.generic_string(), &clean))
      return "it holds a name Windows cannot hold (" + rel.generic_string() + ")";
    if (const auto [at, fresh] = seen.emplace(lower(clean), clean); !fresh && at->second != clean)
      return "it holds two files whose names differ only in case (" + at->second + ", " + clean + ")";
    if (++count > limits.max_files) return "it holds more than " + std::to_string(limits.max_files) + " files";
    bytes += static_cast<int64_t>(it->file_size(ec));
    if (bytes > limits.max_bytes) return "it holds more than " + size_text(limits.max_bytes);
    fs::create_directories((to / rel).parent_path(), ec);
    if (!fs::copy_file(it->path(), to / rel, fs::copy_options::overwrite_existing, ec))
      return "cannot copy " + rel.generic_string() + (ec ? ": " + ec.message() : "");
  }
  if (ec) return "cannot read the folder: " + ec.message();
  if (count == 0) return "the folder is empty";
  return "";
}

// Old staging folders (a run that stopped halfway) cleared, ours only.
void sweep_staging(const fs::path& base) {
  std::error_code ec;
  for (fs::directory_iterator it(base, ec), end; !ec && it != end; it.increment(ec))
    if (it->is_directory(ec)) remove_ours(it->path(), kModStagingMarker);
}

}  // namespace

// Whether a path leaves its folder (absolute, a drive, ".."), as opposed to
// only naming something Windows cannot hold: for the message.
bool mod_entry_escapes(const std::string& path) {
  std::string p = path;
  std::replace(p.begin(), p.end(), '\\', '/');
  if (p.empty() || p[0] == '/' || (p.size() > 1 && p[1] == ':')) return true;
  return ("/" + p + "/").find("/../") != std::string::npos;
}

// A name Windows can hold and that is not a device (CON, PRN, AUX, NUL,
// COM1-9, LPT1-9, with any extension, any case); no trailing dot or space.
// Checked everywhere, so a mod added on the Mac still works on Windows.
bool windows_name_ok(const std::string& part) {
  if (part.empty() || part.back() == '.' || part.back() == ' ') return false;
  const std::string base = lower(part.substr(0, part.find('.')));
  for (const char* d : {"con", "prn", "aux", "nul"})
    if (base == d) return false;
  if (base.size() == 4 && (base.rfind("com", 0) == 0 || base.rfind("lpt", 0) == 0) && base[3] >= '1' && base[3] <= '9')
    return false;
  return true;
}

bool mod_own_file(const std::string& rel) {
  const std::string l = lower(rel);
  // original\ (ours: modding.md, Animations a def at a time, Both ways): the
  // files a modder ships for the original game, which ours never reads.
  return l == "mod.txt" || l == lower(kModAddedMarker) || l.rfind("original/", 0) == 0;
}

bool mod_entry_path_ok(const std::string& path, std::string* clean) {
  std::string p = path;
  std::replace(p.begin(), p.end(), '\\', '/');
  if (p.empty() || p[0] == '/' || p.size() > 1024) return false;
  std::string out, part;
  int parts = 0;
  for (size_t i = 0; i <= p.size(); ++i) {
    if (i < p.size() && p[i] != '/') {
      const unsigned char c = static_cast<unsigned char>(p[i]);
      if (c < 0x20 || c == ':') return false;  // a drive ("C:"), an alternate stream, control characters
      if (std::strchr("<>\"|?*", c)) return false;  // what Windows cannot hold in a name
      part += p[i];
      continue;
    }
    if (part == "..") return false;
    if (!part.empty() && part != "." && !windows_name_ok(part)) return false;
    if (!part.empty() && part != ".") {
      if (++parts > 32) return false;
      out += (out.empty() ? "" : "/") + part;
    }
    part.clear();
  }
  if (clean) *clean = out;
  return true;
}

std::string mod_folder_name(const std::string& name) {
  std::string s;
  for (char c : name) {
    const unsigned char u = static_cast<unsigned char>(c);
    s += std::isalnum(u) || c == ' ' || c == '-' || c == '_' || c == '.' || c == '(' || c == ')' ? c : '_';
  }
  while (!s.empty() && (s.back() == ' ' || s.back() == '.')) s.pop_back();
  while (!s.empty() && (s.front() == ' ' || s.front() == '.')) s.erase(s.begin());
  if (s.size() > 64) s.resize(64);
  if (s.empty()) return "mod";
  return windows_name_ok(s) ? s : s + "_";  // "CON" and its kind: not a folder Windows can make
}

ModGame mod_game_of(const std::vector<std::string>& rel_paths) {
  static const char* const kMw3[] = {"zbd/c4b/", "zbd/t1/", "zbd/c1/readerm3.zbd", "zbd/c1/readerm4.zbd",
                                     "zbd/c2/readerm1.zbd", "zbd/c2/readerm2.zbd", "zbd/c3/readerm1.zbd",
                                     "zbd/c3/readerm2.zbd", "zbd/c3/readerm3.zbd", "zbd/c3/readerm4.zbd",
                                     "zbd/c3/readerm5.zbd", "zbd/c3/readermp3.zbd", "zbd/c3/readermp4.zbd",
                                     "zbd/c4/readerm1.zbd", "zbd/c4/readerm2.zbd", "zbd/c4/readerm3.zbd"};
  static const char* const kPm[] = {"zbd/c1/readerm5.zbd", "zbd/c2/readerm8.zbd", "zbd/c3/readerm7.zbd",
                                    "zbd/c4/readerm9.zbd", "zbd/c4/readerm10.zbd"};
  bool mw3 = false, pm = false;
  for (const std::string& r : rel_paths) {
    const std::string p = lower(r);
    for (const char* k : kMw3) mw3 |= p.rfind(k, 0) == 0;
    for (const char* k : kPm) pm |= p == k;
    const size_t slash = p.rfind('/');
    const std::string name = slash == std::string::npos ? p : p.substr(slash + 1);
    pm |= p.rfind("zbd/", 0) == 0 && (name.rfind("readersa", 0) == 0 || name == "readeria4.zbd");
  }
  return mw3 == pm ? ModGame::kUnknown : mw3 ? ModGame::kMw3 : ModGame::kPm;
}

ModAdd mod_add_prepare(const std::string& write_dir, const std::string& source, const std::string& install,
                       const ModAddLimits& limits) {
  ModAdd a;
  a.source = source;
  std::error_code ec;
  const fs::path src = fs::path(source);
  const bool folder = fs::is_directory(src, ec);
  if (!folder && !fs::is_regular_file(src, ec)) return a.error = "it is not there any more", a;
  const fs::path mods = fs::path(write_dir) / "mods";
  if (folder) {  // never the game's own folders: the write dir, a mod already in mods\, the install
    const std::string c = fs::weakly_canonical(src, ec).generic_string();
    auto at = [&](const fs::path& p) { return fs::weakly_canonical(p, ec).generic_string(); };
    const std::string m = at(mods);
    if (c == at(fs::path(write_dir)) || c == m || c.rfind(m + "/", 0) == 0 || (!install.empty() && c == at(fs::path(install))))
      return a.error = "that folder is the game's own: pick the mod's folder", a;
  }
  const fs::path base = fs::path(write_dir) / "mod-staging";
  fs::create_directories(base, ec);
  sweep_staging(base);
  const auto t = std::chrono::steady_clock::now().time_since_epoch().count();
  fs::path staging;
  for (int i = 0; i < 100; ++i) {
    staging = base / ("add-" + std::to_string(t % 1000000007) + "-" + std::to_string(i));
    if (fs::create_directory(staging, ec)) break;
    staging.clear();
  }
  if (staging.empty()) return a.error = "cannot make a folder in " + base.generic_string(), a;
  { std::ofstream(staging / kModStagingMarker) << "A mod being added by the game; removed when it is done.\n"; }
  a.staging = staging.string();
  const fs::path files = staging / "files";
  fs::create_directories(files, ec);
  const std::string why = folder ? copy_folder(src, files, limits) : unpack(source, files, limits);
  if (!why.empty()) {
    mod_add_discard(a);
    a.error = why;
    return a;
  }
  // Only programs (a mod shipped as its own installer): nothing to add.
  {
    const std::vector<std::string> all = files_under(files);
    auto program = [](const std::string& f) {
      const std::string l = lower(f);
      for (const char* e : {".exe", ".msi", ".bat", ".com"})
        if (l.size() > 4 && l.compare(l.size() - 4, 4, e) == 0) return true;
      return false;
    };
    if (!all.empty() && std::all_of(all.begin(), all.end(), program)) {
      mod_add_discard(a);
      a.error = "it holds only a program (" + fs::path(all.front()).filename().string() +
                "), an installer of its own: run it on Windows, then add the folder it makes";
      return a;
    }
  }
  const fs::path root = find_root(files);
  a.root = root.string();
  // An exported mod's checksums (upgrades/mods/mod_export.h): every file as
  // it was packed, nothing added.
  if (const std::string bad = mod_checksums_verify(a.root, &a.checksums); !bad.empty()) {
    mod_add_discard(a);
    a.error = bad;
    return a;
  }
  const fs::path mt = child(root, "mod.txt");
  a.has_mod_txt = !mt.empty() && fs::is_regular_file(mt, ec);
  a.name = stem_of(source);
  if (a.has_mod_txt) {
    if (mt.filename() != "mod.txt") fs::rename(mt, root / "mod.txt", ec);
    ModInfo info;
    if (mod_parse(read_text(root / "mod.txt"), &info) && !info.name.empty()) a.name = info.name;
    const std::string g = lower(info.game);
    a.game = g.rfind("pm", 0) == 0 ? ModGame::kPm : g.empty() || g.rfind("mw3", 0) == 0 ? ModGame::kMw3 : ModGame::kUnknown;
  }
  // A whole install: only what differs from the player's, without programs
  // (an exe patch cannot apply) and the player's own kind of file.
  const fs::path exe = child(root, "mech3.exe");
  a.full_install = !exe.empty() && fs::is_regular_file(exe, ec);
  if (a.full_install) {
    std::map<std::string, fs::path> theirs;
    for (const std::string& rel : files_under(fs::path(install))) theirs[lower(rel)] = fs::path(install) / fs::path(rel);
    for (const std::string& rel : files_under(root)) {
      const std::string l = lower(rel);
      const fs::path p = root / fs::path(rel);
      const bool program = l.size() > 4 && (l.compare(l.size() - 4, 4, ".exe") == 0 || l.compare(l.size() - 4, 4, ".dll") == 0);
      if (program || !zutl::mod_may_serve(rel)) {
        fs::remove(p, ec), ++a.other_dropped;
        continue;
      }
      const auto it = theirs.find(l);
      if (it != theirs.end() && same_bytes(p, it->second)) fs::remove(p, ec), ++a.same_dropped;
    }
    remove_empty_dirs(root);
    if (files_under(root).empty()) {
      mod_add_discard(a);
      a.error = "every file in it is the same as your install's: nothing to add";
      return a;
    }
  }
  if (!a.has_mod_txt) a.game = mod_game_of(files_under(root));
  return a;
}

ModAddResult mod_add_finish(const std::string& write_dir, ModAdd& a, ModGame game, const std::string& install) {
  ModAddResult r;
  std::error_code ec;
  const fs::path root = fs::path(a.root);
  if (a.staging.empty() || !fs::is_directory(root, ec)) {
    r.message = "Not added: it is gone from the staging folder.";
    mod_add_discard(a);
    return r;
  }
  if (!a.has_mod_txt) {
    std::ofstream f(root / "mod.txt", std::ios::binary);
    f << "# Written by the game when this mod was added (it had no mod.txt).\n"
      << "name=" << a.name << "\nfor=" << (game == ModGame::kPm ? "pm" : "mw3") << "\n";
  }
  r.report = mod_check(root.string(), install, &r.check_errors);
  if (r.check_errors > 0) {
    std::string first;
    for (const std::string& l : r.report)
      if (l.find("error") != std::string::npos) {
        first = l;
        break;
      }
    r.message = "Not added: the game would refuse " + std::to_string(r.check_errors) +
                (r.check_errors == 1 ? " thing" : " things") + " in it" + (first.empty() ? "." : ": " + first);
    mod_add_discard(a);
    return r;
  }
  // A name no folder in mods\ has, whatever its case: never one replaced.
  const fs::path mods = fs::path(write_dir) / "mods";
  fs::create_directories(mods, ec);
  const std::string base = mod_folder_name(a.name);
  std::string name = base;
  for (int n = 2; n < 1000; ++n) {
    if (child(mods, lower(name)).empty()) break;
    name = base + " (" + std::to_string(n) + ")";
  }
  if (!child(mods, lower(name)).empty()) {
    r.message = "Not added: no free name for it in the mods folder.";
    mod_add_discard(a);
    return r;
  }
  {
    std::ofstream f(root / kModAddedMarker);
    f << "Added by the game from " << a.source << ".\n"
      << "The game only ever removes or replaces a mod folder with this file in it.\n";
  }
  fs::rename(root, mods / fs::path(name), ec);
  if (ec) {
    r.message = "Not added: cannot move it into the mods folder (" + ec.message() + ").";
    mod_add_discard(a);
    return r;
  }
  mod_add_discard(a);
  r.ok = true;
  r.folder = name;
  mod_parse(read_text(mods / fs::path(name) / "mod.txt"), &r.info);
  r.info.folder = name;
  r.info.path = (mods / fs::path(name)).string();
  return r;
}

ModAddDone mod_add_run(const std::string& write_dir, const std::string& source, const std::string& install,
                       const std::function<int(const std::string& name)>& ask, std::vector<std::string>* enabled) {
  ModAddDone d;
  ModAdd a = mod_add_prepare(write_dir, source, install);
  if (!a.error.empty()) return d.message = "Not added: " + a.error + ".", d;
  ModGame game = a.game;
  if (game == ModGame::kUnknown) {
    const int g = ask ? ask(a.name) : 1;
    if (g != 1 && g != 2) {
      mod_add_discard(a);
      return d.message = "Not added: no game chosen for " + a.name + ".", d;
    }
    game = g == 2 ? ModGame::kPm : ModGame::kMw3;
  }
  const ModAddResult r = mod_add_finish(write_dir, a, game, install);
  if (!r.ok) return d.message = r.message, d;
  const ModInfo& i = r.info;
  d.ok = true, d.folder = r.folder, d.name = i.name.empty() ? r.folder : i.name;
  d.campaign_only = !i.campaigns.empty() && i.mechs.empty() && i.weapons.empty() && i.ia_worlds.empty();
  if (!d.campaign_only && enabled && std::find(enabled->begin(), enabled->end(), r.folder) == enabled->end()) {
    enabled->push_back(r.folder);
    entry_mods_save(write_dir, *enabled);  // the run's entry's ticks (campaign_entries.h)
  }
  d.message = d.campaign_only ? "Added " + d.name + ": its campaign is listed."
              : !enabled ? "Added " + d.name + (mod_is_campaign(i) ? ": its campaign is listed." : ": tick it for a campaign, then PLAY.")
                         : "Added " + d.name + ": " + entry_on_words(write_dir) + (i.campaigns.empty() ? "." : "; its campaign is listed.");
  return d;
}

void mod_add_discard(ModAdd& a) {
  if (a.staging.empty()) return;
  const fs::path staging = fs::path(a.staging);
  remove_ours(staging, kModStagingMarker);
  std::error_code ec;
  const fs::path base = staging.parent_path();
  if (fs::is_directory(base, ec) && fs::is_empty(base, ec)) fs::remove(base, ec);  // ours, and empty
  a.staging.clear();
}

}  // namespace mech3
