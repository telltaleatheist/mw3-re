#include "upgrades/mods/entry_archives.h"
#include "upgrades/mods/mp_mods.h"
#include "mech3/mods.h"
#include "mech3/mod_install.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include "gamez/zclass/cls_mechlib.h"
#include "gamez/zclass/cls_mechlib_json.h"
#include "gamez/zeffect/zeff_anim_json.h"
#include "upgrades/mods/anim_defs.h"
#include "upgrades/mods/mod_needs.h"
#include "upgrades/mods/mod_report.h"
#include "upgrades/json/gamez_json.h"
#include "gamez/zclass/cls_zbd.h"
#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zinterp/zinterp.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zutl_zip.h"
#include "gamez/zsiscript/si_script_json.h"
#include "gamez/zsiscript/si_script.h"
#include "gamez/zsound/zsnd.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/zutl_zar.h"
#include "crt/msvc_string.h"
#include "mech3/mech_names.h"
#include "mech3/mech_reader.h"
#include "mech3/mission.h"
#include "mech3/mod_campaign.h"
#include "upgrades/reference/reference.h"

namespace mech3 {

namespace fs = std::filesystem;

// mech_anim.h's motion archive (its Handle is not mission.h's).
bool open_motions(const char* path);
void close_motions();
bool load_motion(zsis::Script& s, const char* name);
void script_free_parts(zsis::Script& s);

namespace {
// A world's file in a mod's folder: its gamez.zbd, or unzbd's JSON of it (a gamez\\ folder or gamez.zip,
// docs/modding.md), any case.
bool mod_world_file(const std::string& root, const std::string& world) {
  std::error_code ec;
  for (const char* rel : {"/gamez.zbd", "/gamez/metadata.json", "/gamez.zip"})
    if (fs::is_regular_file(zutl::any_case(root, "zbd/" + world + rel), ec)) return true;
  return false;
}
}  // namespace

namespace {
std::string trim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
  return s.substr(a, b - a);
}
std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
}  // namespace

std::string& mods_game() {
  static std::string g = "mw3pm";  // on branch pm: Pirate's Moon's (main's is "mw3")
  return g;
}

// Ours: a mod's animation files Pirate's Moon's reader (0x00523140) does not read: unzbd's
// JSON (zbd/<world>/anim.zip, anim/), def and node files (zbd/anim/). Its anim.zbd it reads.
bool anim_file_unread(const std::string& low) {
  if (mods_game() != "mw3pm" || low.rfind("zbd/", 0) != 0) return false;
  if (low.rfind("zbd/anim/", 0) == 0) return true;
  const size_t s = low.find('/', 4);
  if (s == std::string::npos) return false;
  const std::string tail = low.substr(s + 1);
  return tail == "anim.zip" || tail.rfind("anim/", 0) == 0;
}

bool mod_parse(const std::string& text, ModInfo* out) {
  std::istringstream in(text);
  for (std::string line; std::getline(in, line);) {
    line = trim(line);
    if (line.empty() || line[0] == '#') continue;
    const size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    const std::string key = lower(trim(line.substr(0, eq))), value = trim(line.substr(eq + 1));
    if (upgrades::reference::key_is(key, "name")) out->name = value;
    else if (upgrades::reference::key_is(key, "version")) out->version = value;
    else if (upgrades::reference::key_is(key, "author")) out->author = value;
    else if (upgrades::reference::key_is(key, "for")) out->game = value;
    else if (upgrades::reference::key_is(key, "description")) out->description = value;
    else if (upgrades::reference::key_is(key, "world") && !value.empty()) out->worlds.push_back(value);
    else if (upgrades::reference::key_is(key, "mech") && !value.empty()) out->mechs.push_back(value);
    else if (upgrades::reference::key_is(key, "weapon") && !value.empty()) out->weapons.push_back(value);
    else if (upgrades::reference::key_is(key, "campaign") && !value.empty()) out->campaigns.push_back(value);
    else if (upgrades::reference::key_is(key, "anim")) out->anim_defs_only = lower(value) == "defs";
    else if (upgrades::reference::key_is(key, "needs") && !value.empty()) {
      // The last word is a version when it starts with a digit (or v and a digit): "Green HUD 1.2".
      const size_t sp = value.find_last_of(" \t");
      const std::string last = sp == std::string::npos ? std::string() : value.substr(sp + 1);
      const bool version = !last.empty() && (std::isdigit(static_cast<unsigned char>(last[0])) ||
                                             ((last[0] == 'v' || last[0] == 'V') && last.size() > 1 &&
                                              std::isdigit(static_cast<unsigned char>(last[1]))));
      out->needs.emplace_back(version ? trim(value.substr(0, sp)) : value, version ? last : std::string());
    }
    else if (upgrades::reference::key_is(key, "conflicts") && !value.empty()) out->conflicts.push_back(value);
    else if (upgrades::reference::key_is(key, "game_version")) out->game_version = value;
    else if (upgrades::reference::key_is(key, "kind")) out->kind = value;
    else if (upgrades::reference::key_is(key, "base")) out->base = value;
    else if (upgrades::reference::key_is(key, "ia_world") && !value.empty()) {
      const size_t sp = value.find_first_of(" \t");
      out->ia_worlds.emplace_back(value.substr(0, sp), sp == std::string::npos ? std::string() : trim(value.substr(sp)));
    }
  }
  if (out->name.empty()) out->error = "mod.txt has no name= line";
  // The first word exactly (a version may follow): "mw3pm" is Pirate's Moon's, "mw3" MechWarrior 3's.
  else if (!out->game.empty() && lower(out->game).substr(0, lower(out->game).find_first_of(" \t")) != mods_game())
    out->error = "it is for " + out->game + ", not " + (mods_game() == "mw3pm" ? "Pirate's Moon" : "MechWarrior 3");
  return out->error.empty();
}

std::vector<ModInfo> mods_found(const std::string& write_dir) {
  std::vector<ModInfo> out;
  std::error_code ec;
  if (write_dir.empty()) return out;
  for (fs::directory_iterator it(fs::path(write_dir) / "mods", ec), end; !ec && it != end; it.increment(ec)) {
    if (!it->is_directory(ec)) continue;
    ModInfo m;
    m.folder = it->path().filename().string();
    m.path = it->path().generic_string();  // zutl's / (its joins and the mods' prefixes)
    std::ifstream f(it->path() / "mod.txt");
    if (!f) {
      m.error = "no mod.txt";
    } else {
      std::stringstream ss;
      ss << f.rdbuf();
      mod_parse(ss.str(), &m);
    }
    out.push_back(m);
  }
  std::sort(out.begin(), out.end(), [](const ModInfo& a, const ModInfo& b) { return a.folder < b.folder; });
  return out;
}

namespace {
// Whether the game would find `entry`: in the first of `roots` holding the
// archive `rel` (the game's reader.zbd or mechlib.zbd, a mod's before the
// install's), or as a loose file beside it (`loose`, a reader file's).
bool archive_has(const std::vector<std::string>& roots, const std::string& rel, const std::string& entry, bool loose) {
  std::error_code ec;
  const fs::path archive(rel);
  const std::string json = entry + ".json";  // mech3ax's, in a folder named as the archive or its zip (docs/modding.md)
  for (const std::string& root : roots) {
    if (loose && fs::is_regular_file(fs::path(root) / "zbd" / entry, ec)) return true;
    const fs::path dir = fs::path(root) / archive.parent_path();
    if (fs::is_regular_file(dir / archive.stem() / json, ec)) return true;
    if (zutl_zip::Zip zip; fs::is_regular_file(dir / (archive.stem().string() + ".zip"), ec) &&
                           zip.open(dir / (archive.stem().string() + ".zip")))
      for (const zutl_zip::Zip::Entry& e : zip.entries)
        if (e.name.size() == json.size() && std::equal(e.name.begin(), e.name.end(), json.begin(), [](char x, char y) {
              return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
            }))
          return true;
    const fs::path a = fs::path(root) / rel;
    // The archive kept as its changed entries (entry_archives.h): one of them is the game's; any other entry is
    // the next holder's.
    if (const std::string cut = a.string() + kEntriesSuffix; fs::is_regular_file(cut, ec))
      for (const std::string& e : archive_entry_names(cut))
        if (crt::msvc_stricmp(e.c_str(), entry.c_str()) == 0) return true;
    if (!fs::is_regular_file(a, ec)) continue;
    zutl_zar::Archive z;
    zutl_zar::init(z);
    const bool has = zutl_zar::open(z, a.string().c_str()) && zutl_zar::find(z, entry.c_str()) >= 0;
    zutl_zar::close(z);
    zutl_zar::destroy(z);
    return has;
  }
  return false;
}
// Why a mech row cannot be built from `roots`, "" when it can: its definition
// and its model must be there, or the row is left out (ours: an Instant
// Action launch or the lab would find nothing).
std::string mech_row_missing(const ModMech& r, const std::vector<std::string>& roots) {
  if (!archive_has(roots, "zbd/reader.zbd", "dfn_" + r.name + ".zrd", true)) return "no dfn_" + r.name + ".zrd in the reader archive";
  if (!archive_has(roots, "zbd/mechlib.zbd", "mech_" + r.model + ".flt", false)) return "no mech_" + r.model + ".flt in mechlib.zbd";
  return "";
}
}  // namespace

std::vector<ModIaWorld>& mod_ia_worlds() {
  static std::vector<ModIaWorld> w;
  return w;
}

std::vector<std::string> mods_mount(const std::string& write_dir, const std::vector<std::string>& enabled,
                                    const std::vector<std::string>& later) {
  std::vector<std::string> log;
  // Ours (mod_report.h): what each mod has left out, for the MODS screens' sentences.
  mods_left_out().clear();
  const auto left = [](const std::string& mod, const std::string& what_why) { mods_left_out()[mod].push_back(what_why); };
  std::vector<std::pair<std::string, ModIaWorld>> ia;  // the mod's name, the entry
  const std::vector<ModInfo> found = mods_found(write_dir);
  // A remount starts from the exe's tables (the mods' rows and their cached definitions gone).
  free_dfns_above(kBuiltinMechs);
  mech_rows_clear();
  weapon_rows_clear();
  mod_campaigns().clear();
  std::vector<ModCampaign> campaigns;  // checked against the worlds once every mod's are known
  std::vector<std::string> done;
  for (const std::string& want : enabled) {
    if (std::find(done.begin(), done.end(), want) != done.end()) continue;  // named twice: the first place counts
    done.push_back(want);
    const auto it = std::find_if(found.begin(), found.end(), [&](const ModInfo& m) { return m.folder == want; });
    if (it == found.end()) {
      log.push_back("mod " + want + ": not in " + (fs::path(write_dir) / "mods").generic_string() + ", left out");
      left(want, "the mod: it is not in " + (fs::path(write_dir) / "mods").generic_string());
      continue;
    }
    if (!it->error.empty()) {
      log.push_back("mod " + want + ": " + it->error + ", left out");
      left(want, "the mod: " + it->error);
      continue;
    }
    zutl::mount_mod(it->path);
    log.push_back("mod " + want + " (" + it->name + (it->version.empty() ? "" : " " + it->version) + ") mounted");
    // Its new worlds: a folder no world has yet, with its files in the mod.
    for (const std::string& w : it->worlds) {
      std::error_code wec;
      if (mission_dir_index(w.c_str()) >= 0) {
        log.push_back("  world " + w + ": there is a world " + w + " already, left out");
        left(want, "world " + w + ": there is a world " + w + " already");
      } else if (w.find_first_of("/\\. ") != std::string::npos || !mod_world_file(it->path, w)) {
        log.push_back("  world " + w + ": no zbd/" + w + "/gamez.zbd in the mod, left out");
        left(want, "world " + w + ": there is no zbd/" + w + "/gamez.zbd in the mod");
      } else {
        mod_worlds().push_back(w);
        log.push_back("  world " + w + " added (world " + std::to_string(mission_dir_count() - 1) + ")");
      }
    }
    for (const auto& [folder, name] : it->ia_worlds) ia.push_back({want, ModIaWorld{folder, name.empty() ? folder : name}});
    for (const std::string& file : it->campaigns) {
      ModCampaign c;
      const bool ok = campaign_load(it->path, want, file, &c);  // a folder's pilots: one id per mod, whatever its case
      if (!ok) log.push_back("  campaign " + file + ": " + c.error + ", left out"), left(want, "campaign " + file + ": " + c.error);
      else if (std::any_of(campaigns.begin(), campaigns.end(), [&](const ModCampaign& o) { return o.mod == want && lower(o.id) == lower(c.id); }))
        log.push_back("  campaign " + file + ": the mod has a campaign " + c.id + " already, left out"),
            left(want, "campaign " + file + ": the mod has a campaign " + c.id + " already");
      else campaigns.push_back(c);
    }
    // Its new mechs, after the exe's and any earlier mod's; each one's
    // definition and model where the game will look (the enabled mods in
    // order, the first wins, then the patch and the install).
    std::vector<std::string> roots;
    for (const std::string& m : enabled)
      if (const auto f = std::find_if(found.begin(), found.end(), [&](const ModInfo& x) { return x.folder == m; });
          f != found.end() && f->error.empty() && std::find(roots.begin(), roots.end(), f->path) == roots.end())
        roots.push_back(f->path);
    roots.insert(roots.end(), later.begin(), later.end());
    for (const std::string& line : it->mechs) {
      ModMech row;
      std::string why;
      bool ok = mech_row_parse(line, &row, &why);
      if (ok && mech_index(row.name.c_str()) >= 0) ok = false, why = "there is a mech " + row.name + " already";  // said before its files
      if (ok && !(why = mech_row_missing(row, roots)).empty()) ok = false;
      if (!ok || !mech_row_add(row, &why))
        log.push_back("  mech " + line + ": " + why + ", left out"), left(want, "mech " + (row.name.empty() ? line : row.name) + ": " + why);
      else
        log.push_back("  mech " + row.name + " added (mech " + std::to_string(mech_count() - 1) + ", " + std::to_string(row.mass) +
                      " t, the models of " + row.model + ")");
    }
    for (const std::string& line : it->weapons) {
      ModWeapon row;
      std::string why;
      if (!weapon_row_parse(line, &row, &why) || !weapon_row_add(row, &why))
        log.push_back("  weapon " + line + ": " + why + ", left out"), left(want, "weapon " + (row.name.empty() ? line : row.name) + ": " + why);
      else
        log.push_back("  weapon " + row.name + " added (weapon " + std::to_string(weapon_count() - 1) + ")");
    }
    // Each file: what it replaces (a copy later in the mounts) or adds.
    std::vector<std::string> files;
    std::error_code ec;
    for (fs::recursive_directory_iterator f(it->path, ec), end; !ec && f != end; f.increment(ec))
      if (f->is_regular_file(ec)) files.push_back(fs::relative(f->path(), it->path, ec).generic_string());
    std::sort(files.begin(), files.end());
    int replaced = 0, added = 0, ignored = 0;
    for (const std::string& rel : files) {
      if (mod_own_file(rel)) continue;
      if (!zutl::mod_may_serve(rel)) {
        log.push_back("  " + rel + ": the player's own kind of file, ignored");
        ++ignored;
        continue;
      }
      bool over = false;
      if (const std::string of = entries_archive_of(rel); !of.empty()) {  // its changed entries: laid over the game's below
        log.push_back("  " + of + ": its changed entries (" + std::to_string(archive_entry_names((fs::path(it->path) / rel).string()).size()) + ")");
        ++replaced;
        continue;
      }
      for (const std::string& root : later) over = over || fs::is_regular_file(fs::path(root) / rel, ec);
      log.push_back("  " + rel + (over ? " replaces the game's" : " added"));
      ++(over ? replaced : added);
    }
    log.push_back("  " + std::to_string(replaced) + " replaced, " + std::to_string(added) + " added, " +
                  std::to_string(ignored) + " ignored");
  }
  // Instant Action's new entries, once every mod's worlds are known.
  for (const auto& [mod, w] : ia) {
    if (mission_dir_index(w.folder.c_str()) < 0) {
      log.push_back("mod " + mod + ": Instant Action world " + w.folder + " is no world, left out");
      left(mod, "ia_world " + w.folder + ": there is no such world");
      continue;
    }
    mod_ia_worlds().push_back(w);
    log.push_back("mod " + mod + ": Instant Action world \"" + w.name + "\" on " + w.folder);
  }
  // The campaigns, each mission's world one that has its files somewhere mounted.
  std::vector<std::string> roots;
  for (const std::string& m : done)
    if (const auto f = std::find_if(found.begin(), found.end(), [&](const ModInfo& x) { return x.folder == m; });
        f != found.end() && f->error.empty())
      roots.push_back(f->path);
  roots.insert(roots.end(), later.begin(), later.end());
  auto known = [&](const std::string& w) {
    std::error_code ec;
    if (mission_dir_index(w.c_str()) < 0) return false;
    for (const std::string& r : roots)
      if (mod_world_file(r, w)) return true;
    return false;
  };
  for (const ModCampaign& c : campaigns) {
    if (const CampaignMission* m = campaign_missing_world(c, known)) {
      log.push_back("mod " + c.mod + ": campaign " + c.file + ": line " + std::to_string(m->line) + ": no world " + m->world +
                    " in an enabled mod or the game, the campaign left out");
      left(c.mod, "campaign " + c.file + ": line " + std::to_string(m->line) + " needs the world " + m->world +
                      ", which no enabled mod or the game has");
      continue;
    }
    mod_campaigns().push_back(c);
    log.push_back("mod " + c.mod + ": campaign \"" + c.name + "\" (" + c.file + ", " + std::to_string(c.missions.size()) +
                  " missions) added");
  }
  // Ours (upgrades/mods/entry_archives.h): the archives the mods hold as their changed entries, built over the
  // game's own in a folder of ours and mounted in front, so the game reads whole archives.
  {
    std::vector<std::string> mod_roots(roots.begin(), roots.end() - static_cast<std::ptrdiff_t>(later.size()));
    if (const std::string merged = entries_merge(write_dir, mod_roots, later, &log); !merged.empty()) zutl::mount_front(merged);
  }
  return log;
}

bool mod_file_is_gameplay(const std::string& rel) {
  const std::string p = lower(rel);
  const std::string name = p.substr(p.rfind('/') == std::string::npos ? 0 : p.rfind('/') + 1);
  auto ends = [&](const char* e) { return name.size() >= std::strlen(e) && name.compare(name.size() - std::strlen(e), std::string::npos, e) == 0; };
  auto starts = [&](const char* b) { return name.rfind(b, 0) == 0; };
  if (name == "mod.txt" || name == "strings.txt" || !zutl::mod_may_serve(rel)) return false;
  if (p.rfind("music/", 0) == 0 || p.rfind("video/", 0) == 0) return false;
  if (ends(".png") || ends(".wav") || ends(".ogg") || ends(".txt") || ends(".avi")) return false;
  if (ends(".zbd") && (starts("rimage") || starts("rmechtex") || starts("rtexture") || starts("texture") || starts("mechtex") ||
                       starts("sounds")))
    return false;
  return true;
}

uint64_t mods_gameplay_hash(const std::string& write_dir, const std::vector<std::string>& enabled, const std::string* mounted_over) {
  uint64_t h = 1469598103934665603ull;  // FNV-1a
  auto mix = [&](const char* d, size_t n) {
    for (size_t i = 0; i < n; ++i) h = (h ^ static_cast<uint8_t>(d[i])) * 1099511628211ull;
  };
  bool any = false;
  const std::vector<ModInfo> found = mods_found(write_dir);
  std::vector<std::string> done;
  for (const std::string& want : enabled) {
    if (std::find(done.begin(), done.end(), want) != done.end()) continue;
    done.push_back(want);
    const auto it = std::find_if(found.begin(), found.end(), [&](const ModInfo& m) { return m.folder == want; });
    if (it == found.end() || !it->error.empty()) continue;
    // The mech= rows are gameplay data as parsed (spacing and comments aside).
    for (const std::string& line : it->mechs) {
      ModMech r;
      std::string why;
      if (!mech_row_parse(line, &r, &why)) continue;
      const std::string key = "mech=" + r.name + " " + r.model + " " + std::to_string(r.tech) + " " + std::to_string(r.mass) + " " +
                              std::to_string(r.weight_class);
      any = true;
      mix(key.data(), key.size() + 1);
    }
    for (const std::string& line : it->weapons) {
      ModWeapon r;
      std::string why;
      if (!weapon_row_parse(line, &r, &why)) continue;
      const std::string key = "weapon=" + r.name + " " + std::to_string(r.ammo) + " " + std::to_string(r.tech);
      any = true;
      mix(key.data(), key.size() + 1);
    }
    std::vector<std::string> files;
    std::error_code ec;
    for (fs::recursive_directory_iterator f(it->path, ec), end; !ec && f != end; f.increment(ec))
      if (f->is_regular_file(ec)) files.push_back(fs::relative(f->path(), it->path, ec).generic_string());
    std::sort(files.begin(), files.end());
    if (mounted_over) {  // ours (mp_mods.h): what each file changes in the mounted game, by its mounted path
      std::vector<std::pair<std::string, std::string>> parts;
      for (const std::string& rel : files) {
        if (!mod_file_is_gameplay(rel) && entries_archive_of(rel).empty()) continue;
        std::string key, bytes;
        if (mp_mounted_part((fs::path(it->path) / rel).string(), rel, *mounted_over, &key, &bytes)) parts.emplace_back(key, bytes);
      }
      std::sort(parts.begin(), parts.end());
      for (const auto& [key, bytes] : parts) {
        any = true;
        mix(key.data(), key.size() + 1);
        mix(bytes.data(), bytes.size());
      }
      continue;
    }
    for (const std::string& rel : files) {
      if (!mod_file_is_gameplay(rel)) continue;
      any = true;
      const std::string key = lower(rel);
      mix(key.data(), key.size() + 1);
      std::ifstream in(fs::path(it->path) / rel, std::ios::binary);
      char buf[65536];
      while (in.read(buf, sizeof buf) || in.gcount() > 0) mix(buf, static_cast<size_t>(in.gcount()));
    }
  }
  return any ? h : 0;
}

ModsIdentity& mods_identity() {
  static ModsIdentity id;
  return id;
}

void mods_identity_set(const std::string& write_dir, const std::vector<std::string>& enabled, const std::string& install) {
  ModsIdentity& id = mods_identity();
  id = ModsIdentity{};
  id.hash = mods_gameplay_hash(write_dir, enabled);
  id.mp_hash = mods_gameplay_hash(write_dir, enabled, &install);
  if (id.hash == 0 && id.mp_hash == 0) return;
  const std::vector<ModInfo> found = mods_found(write_dir);
  for (const std::string& want : enabled) {
    const auto it = std::find_if(found.begin(), found.end(), [&](const ModInfo& m) { return m.folder == want; });
    const std::string name = it != found.end() && !it->name.empty() ? it->name : want;
    // strings, images, sounds only: not named
    if (mods_gameplay_hash(write_dir, {want}) != 0 && std::find(id.names.begin(), id.names.end(), name) == id.names.end())
      id.names.push_back(name);
    if (const uint64_t own = mods_gameplay_hash(write_dir, {want}, &install);
        own != 0 && std::find(id.mp_names.begin(), id.mp_names.end(), name) == id.mp_names.end())
      id.mp_names.push_back(name), id.hashes.push_back(own);
  }
}

namespace {
std::vector<std::string> mod_files(const std::string& path) {
  std::vector<std::string> files;
  std::error_code ec;
  for (fs::recursive_directory_iterator f(path, ec), end; !ec && f != end; f.increment(ec))
    if (f->is_regular_file(ec)) {
      const std::string rel = lower(fs::relative(f->path(), path, ec).generic_string());
      if (mod_own_file(rel)) continue;
      // An archive kept as its changed entries (entry_archives.h) shares by entry: two mods that change
      // different entries of one archive both apply.
      if (const std::string of = entries_archive_of(rel); !of.empty()) {
        for (const std::string& e : archive_entry_names(f->path().string())) files.push_back(of + ": " + lower(e));
        continue;
      }
      files.push_back(rel);
    }
  std::sort(files.begin(), files.end());
  return files;
}
std::vector<std::string> row_names(const ModInfo& m) {
  std::vector<std::string> names;
  for (const auto* rows : {&m.mechs, &m.weapons}) {
    const std::string kind = rows == &m.mechs ? "mech " : "weapon ";
    for (const std::string& line : *rows) {
      std::istringstream in(line);
      std::string name;
      if (in >> name) names.push_back(kind + lower(name));
    }
  }
  return names;
}
}  // namespace

// A mod's animations a def at a time: its def files' (world, key), the world
// "" for all of them, and the worlds it has a whole anim file for.
struct ModAnims {
  std::vector<std::pair<std::string, std::string>> defs;
  std::set<std::string> whole;
};
ModAnims mod_anims(const std::string& dir) {
  ModAnims a;
  std::error_code ec;
  const fs::path zbd = fs::path(dir) / "zbd";
  if (!fs::is_directory(zbd, ec)) return a;
  for (fs::recursive_directory_iterator it(zbd, ec), end; !ec && it != end; it.increment(ec)) {
    if (!it->is_regular_file(ec)) continue;
    const std::string rel = fs::relative(it->path(), dir, ec).generic_string(), low = lower(rel);
    std::string world;
    if (anim_file_unread(low)) continue;  // not read by Pirate's Moon: nothing shared
    if (anim_defs::is_def_file(dir, rel, &world)) {
      if (const std::string key = anim_defs::def_file_key(it->path().string()); !key.empty()) a.defs.emplace_back(world, key);
      continue;
    }
    // zbd/<world>/anim.zbd, anim.zip or anim/metadata.json
    const size_t s1 = low.find('/'), s2 = s1 == std::string::npos ? s1 : low.find('/', s1 + 1);
    if (s2 == std::string::npos) continue;
    const std::string w = low.substr(s1 + 1, s2 - s1 - 1), tail = low.substr(s2 + 1);
    if (tail == "anim.zbd" || tail == "anim.zip" || tail == "anim/metadata.json") a.whole.insert(w);
  }
  return a;
}

std::vector<ModsListEntry> mods_list(const std::string& write_dir, const std::vector<std::string>& enabled, bool check,
                                     const std::string& install) {
  const std::vector<ModInfo> found = mods_found(write_dir);
  std::vector<ModsListEntry> out;
  for (const std::string& want : enabled) {
    const auto it = std::find_if(found.begin(), found.end(), [&](const ModInfo& m) { return m.folder == want; });
    const bool listed = std::any_of(out.begin(), out.end(), [&](const ModsListEntry& e) { return e.info.folder == want; });
    if (it == found.end() || listed) continue;
    ModsListEntry e;
    e.info = *it;
    e.on = true;
    out.push_back(e);
  }
  for (const ModInfo& m : found)
    if (std::none_of(out.begin(), out.end(), [&](const ModsListEntry& e) { return e.info.folder == m.folder; })) {
      ModsListEntry e;
      e.info = m;
      out.push_back(e);
    }
  // Ours (mod_needs.h): what each says it needs, against the mods on (an off one's as if turned on).
  for (ModsListEntry& e : out) {
    std::vector<std::string> on = enabled;
    if (!e.on) on.push_back(e.info.folder);
    if (e.info.error.empty()) e.needs = mod_needs_warnings(e.info, found, on);
  }
  // What each enabled one shares with an enabled one before it (usable ones: an unusable mod mounts nothing).
  std::vector<std::pair<std::vector<std::string>, std::vector<std::string>>> before;  // files, row names
  std::vector<std::string> before_folders;
  std::vector<ModAnims> before_anims;
  for (ModsListEntry& e : out) {
    std::error_code ec;
    for (fs::recursive_directory_iterator f(e.info.path, ec), end; !ec && f != end; f.increment(ec))
      if (f->is_regular_file(ec) && anim_file_unread(lower(fs::relative(f->path(), e.info.path, ec).generic_string()))) ++e.anims_unread;
  }
  for (ModsListEntry& e : out) {
    if (!e.on || !e.info.error.empty()) continue;
    const std::vector<std::string> files = mod_files(e.info.path), rows = row_names(e.info);
    ModAnims anims = mod_anims(e.info.path);
    if (e.info.anim_defs_only) anims.whole.clear();  // anim=defs: its whole files are the original's copy
    for (size_t k = 0; k < before.size(); ++k) {
      bool shared = false;
      for (const auto& [w, key] : anims.defs) {
        const bool taken = std::any_of(before_anims[k].defs.begin(), before_anims[k].defs.end(), [&](const auto& b) {
          return b.second == key && (b.first == w || b.first.empty() || w.empty());
        });
        if (taken) e.anims_taken.push_back((w.empty() ? std::string("all worlds") : w) + " " + key), shared = true;
      }
      std::set<std::string> worlds;  // where this mod's def files meet an earlier whole file
      for (const auto& d : anims.defs) {
        if (d.first.empty()) worlds.insert(before_anims[k].whole.begin(), before_anims[k].whole.end());
        else if (before_anims[k].whole.count(d.first)) worlds.insert(d.first);
      }
      for (const std::string& w : worlds)
        if (std::find(e.anim_bases.begin(), e.anim_bases.end(), w) == e.anim_bases.end()) e.anim_bases.push_back(w), shared = true;
      for (const std::string& f : files) {
        // An entry of an archive kept as changed entries ("<archive>: <entry>") is also the earlier mod's when
        // that mod holds the whole archive: its archive wins whole.
        const size_t colon = f.find(": ");
        if (std::binary_search(before[k].first.begin(), before[k].first.end(), f) ||
            (colon != std::string::npos && std::binary_search(before[k].first.begin(), before[k].first.end(), f.substr(0, colon))))
          e.shadowed.push_back(f), shared = true;
      }
      for (const std::string& r : rows)
        if (std::find(before[k].second.begin(), before[k].second.end(), r) != before[k].second.end()) e.rows_taken.push_back(r), shared = true;
      if (shared) e.conflicts_with.push_back(before_folders[k]);
    }
    before.emplace_back(files, rows);
    before_folders.push_back(e.info.folder);
    before_anims.push_back(anims);
  }
  if (check)
    for (ModsListEntry& e : out) {
      int errors = 0;
      e.report = mod_report(e.info.path, install, e.info.folder, &errors);
      e.check_errors = errors;
      e.check_warnings = 0;
      for (const std::string& l : e.report) e.check_warnings += l.rfind("Wrong: ", 0) == 0;
    }
  return out;
}

std::vector<std::string> mods_enabled(const std::string& write_dir) {
  std::vector<std::string> out;
  std::ifstream f(fs::path(write_dir) / "mw3.cfg");
  for (std::string line; std::getline(f, line);) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.rfind("mod=", 0) == 0) out.push_back(line.substr(4));
  }
  return out;
}

bool mods_save_enabled(const std::string& write_dir, const std::vector<std::string>& enabled) {
  if (write_dir.empty()) return false;
  const fs::path cfg = fs::path(write_dir) / "mw3.cfg";
  // Each line kept with its own ending, byte for byte (a CRLF file stays CRLF); ours take the file's.
  std::vector<std::string> lines;
  size_t at = std::string::npos;
  std::string eol = "\n";
  bool last_open = false;  // the file's last line without an ending: given one, as it was given before
  {
    std::ifstream f(cfg, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    const std::string text = ss.str();
    bool seen_eol = false;
    for (size_t pos = 0; pos < text.size();) {
      size_t end = text.find('\n', pos);
      std::string line = text.substr(pos, end == std::string::npos ? std::string::npos : end + 1 - pos);
      pos = end == std::string::npos ? text.size() : end + 1;
      if (!seen_eol && line.size() >= 2 && line.compare(line.size() - 2, 2, "\r\n") == 0) eol = "\r\n";
      if (!line.empty() && line.back() == '\n') seen_eol = true;
      if (line.rfind("mod=", 0) == 0) {
        if (at == std::string::npos) at = lines.size();
        continue;
      }
      lines.push_back(line);
    }
    last_open = !lines.empty() && !lines.back().empty() && lines.back().back() != '\n';
  }
  if (last_open) lines.back() += eol;
  if (at == std::string::npos) at = lines.size();
  std::vector<std::string> mods;
  for (const std::string& m : enabled) mods.push_back("mod=" + m + eol);
  lines.insert(lines.begin() + static_cast<long>(at), mods.begin(), mods.end());
  // Written beside it, then renamed over it: a crash or a full disk midway
  // leaves the old file whole (rename replaces an existing file on Windows
  // too: MoveFileEx with MOVEFILE_REPLACE_EXISTING).
  std::error_code ec;
  fs::create_directories(write_dir, ec);
  const fs::path tmp = fs::path(write_dir) / "mw3.cfg.new";
  {
    std::ofstream o(tmp, std::ios::trunc | std::ios::binary);
    for (const std::string& l : lines) o << l;
    o.flush();
    if (!o) {
      o.close();
      if (fs::is_regular_file(tmp, ec)) fs::remove(tmp, ec);  // ours only: never a folder in its place
      return false;
    }
  }
  fs::rename(tmp, cfg, ec);
  if (ec) {
    if (fs::is_regular_file(tmp, ec)) fs::remove(tmp, ec);
    return false;
  }
  return true;
}

std::vector<std::string> mods_apply_strings(std::map<int32_t, std::string>& text,
                                            const std::map<std::string, int32_t>& ids,
                                            std::map<std::string, int32_t>* names) {
  std::vector<std::string> log;
  std::vector<std::pair<std::string, int32_t>> named;
  const std::vector<std::string>& mods = zutl::mounts().mods;
  // The last mod first, so the first one's lines are the ones left.
  for (auto m = mods.rbegin(); m != mods.rend(); ++m) {
    std::ifstream f(fs::path(*m) / "strings.txt");
    if (!f) continue;
    int used = 0, n = 0;
    for (std::string line; std::getline(f, line);) {
      ++n;
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (n == 1 && line.rfind("\xef\xbb\xbf", 0) == 0) line.erase(0, 3);  // a UTF-8 mark
      if (trim(line).empty() || trim(line)[0] == '#') continue;
      const size_t eq = line.find('=');
      std::string key = trim(line.substr(0, eq == std::string::npos ? 0 : eq));
      int32_t id = -1;
      // NAME@id: a message name the game's table lacks (or has elsewhere) given its number, with its text
      // (upgrades/mods/import_texts.h: a modded Mech3Msg.dll's own names).
      std::string fresh;
      if (const size_t at = key.find('@'); at != std::string::npos) fresh = trim(key.substr(0, at)), key = trim(key.substr(at + 1));
      if (!key.empty() && std::isdigit(static_cast<unsigned char>(key[0]))) {
        char* end = nullptr;
        errno = 0;
        const long long v = std::strtoll(key.c_str(), &end, 0);  // long is 32 bits on Windows: it would saturate
        if (errno != ERANGE && end && *end == 0 && v >= 0 && v <= 0x7fffffff) id = static_cast<int32_t>(v);
      } else if (const auto it = ids.find(key); it != ids.end()) {
        id = it->second;
      }
      std::string value, game;
      if (eq != std::string::npos) {
        const std::string raw = line.substr(eq + 1);
        for (size_t i = 0; i < raw.size(); ++i) {
          if (raw[i] == '\\' && i + 1 < raw.size() && (raw[i + 1] == 'n' || raw[i + 1] == '\\')) {
            value += raw[i + 1] == 'n' ? "\r\n" : "\\";
            ++i;
          } else {
            value += raw[i];
          }
        }
      }
      if (eq == std::string::npos || id < 0) {
        log.push_back("  " + (fs::path(*m) / "strings.txt").generic_string() + " line " + std::to_string(n) + ": no id= here, left out");
        continue;
      }
      if (!zutl::game_text(value, &game)) {
        log.push_back("  " + (fs::path(*m) / "strings.txt").generic_string() + " line " + std::to_string(n) +
                      ": a character the game cannot show, left out");
        continue;
      }
      text[id] = game.substr(0, 0x1ff);  // the original's 0x200-byte buffers
      if (!fresh.empty()) named.emplace_back(fresh, id);
      ++used;
    }
    log.insert(log.end() - 0, "mod strings: " + std::to_string(used) + " from " + (fs::path(*m) / "strings.txt").generic_string());
  }
  // The names last, so no line's lookup above meets a name another line gave (the last mod's first: the first wins).
  if (names)
    for (const auto& [name, id] : named) (*names)[name] = id;
  return log;
}

namespace {
// --check-mod's look inside a texture or image library (rimage, rtexture*,
// rmechtex*): its table, then every texture through the bounded load (M3).
// Findings are warnings in the mods' one form, never errors (a texture left out shows as missing; the rest of
// the library and the mod are used); a library whose table does not read is left out of the mod, said.
void check_texture_library(const std::string& host, const std::string& rel, int* entries,
                           const std::function<void(const std::string&, const ModWarning&, bool)>& warn,
                           const std::function<void(const std::string&, const std::string&)>& leave) {
  const zimg::State saved = zimg::state();
  zimg::state() = zimg::State{};
  const std::string folder = fs::path(host).parent_path().string() + "/";
  zimg::state().dir = folder.c_str();
  zimg::Library lib{};
  std::snprintf(lib.name, sizeof lib.name, "%s", fs::path(host).filename().string().c_str());
  if (!zimg::open(lib) || !lib.entries) {
    leave(rel, "it is not a texture library the game can read (its table of names)");
  } else {
    for (int i = 0; i < lib.header.texture_count; ++i) {
      ++*entries;
      char name[32];
      std::memcpy(name, lib.entries[i].name, sizeof name);
      name[sizeof name - 1] = 0;
      zimg::Texture* t = zimg::load(lib, name);
      for (const ModWarning& note : zimg::load_notes()) warn(rel + ": texture " + name, note, t == nullptr);
      if (t) zimg::free_texture(t);
      else if (zimg::load_notes().empty())
        warn(rel + ": texture " + name, {"it cannot be read", "leaves this texture out", "", ""}, true);
    }
  }
  zimg::close(lib);
  std::free(lib.entries);
  for (uint16_t* p : zimg::state().palettes) std::free(p);
  zimg::state() = saved;
}

// interp.zbd: its directory, then every script's lines within its bytes.
void check_interp(const std::string& host, const std::string& rel, int* entries,
                  const std::function<void(const std::string&)>& leave,
                  const std::function<void(const std::string&, const std::string&, const std::string&)>& item) {
  zint::Interp in;
  in.search_path = fs::path(host).parent_path().string();
  if (!zint::open_archive(in, fs::path(host).filename().string().c_str())) {
    leave("not a script archive the game can read (its header or directory)");
    return;
  }
  std::vector<char> buf(0x1000);
  for (int32_t i = 0; i < in.entry_count; ++i) {
    ++*entries;
    const std::string name = in.dir[i].name;
    zint::ScriptFile* f = zint::archive_find(in, name.c_str());
    if (!f) {
      item(rel + ": script " + name, "its place in the file is past the file's end",
           "this script is treated as not there; the rest of the file is used");
      continue;
    }
    in.binary = 1;
    for (int n = 0; n < 1000000 && zint::read_line(in, f, buf.data()); ++n) {
      std::free(in.line);
      in.line = nullptr;
    }
    if (f->cut) item(rel + ": script " + name, "a line of it runs past the script's end", "the script is read up to that line");
  }
  std::fclose(in.archive.f);
  in.archive.f = nullptr;
}
}  // namespace

namespace {
const char* const kLeavesOut = "IMPORT leaves this file out; the game's own, if it has one, is used";
const char* const kNoModTxt = "has no mod.txt (mods are the rebuild's own)";
const char* const kOurKind = "does not read this kind of file (it is the rebuild's own)";
const char* const kEntryInstead = "this file is not read; an archive's entry of that name is used";
const char* const kArchiveInstead = "this file is not read; the archive's entry is used";
}  // namespace

std::vector<std::string> mod_check(const std::string& dir, const std::string& install, int* errors, ModFindings* facts) {
  std::vector<std::string> out;
  *errors = 0;
  // The check reads the folder it is given by its host path (IMPORT's staging folder, a mod in the mods folder,
  // the install beside it), which the running game's confined names refuse (zutl::Mounts::confined): inside the
  // game every archive of a mod read as "not an archive" and was left out. The names here are the caller's, not
  // data's, so the check runs unconfined and puts the setting back (docs/modding.md, The check inside the game).
  struct Unconfined {
    bool was = zutl::mounts().confined;
    Unconfined() { zutl::mounts().confined = false; }
    ~Unconfined() { zutl::mounts().confined = was; }
  } unconfined;
  ModFindings ignored_facts;
  ModFindings& fx = facts ? *facts : ignored_facts;
  fx = ModFindings{};
  fx.against_install = !install.empty();
  // An error is the mod's fault as a whole, and the only thing that refuses it: no folder, a mod.txt that cannot
  // be read or is another game's, nothing in it usable. One file's or one line's fault is a warning in four
  // parts (mod_warning.h): that file or line left out or partly read, the rest of the mod used (Owen, 2026-11-11).
  auto error = [&](const std::string& what) { out.push_back("error: " + what), ++*errors, fx.refused.push_back(what); };
  int usable_rows = 0;
  auto warn = [&](const std::string& where, const ModWarning& w, bool left_out) {
    out.push_back("warning: " + mod_warning_line(where, w));
    fx.notes.push_back({where, w, mod_warning_group(w, left_out), false});
  };
  // A line of mod.txt the game cannot use: left out, as the mount leaves it out.
  auto row = [&](const std::string& where, const std::string& wrong) {
    warn(where, {wrong, "this line of mod.txt is left out and the rest of the mod is used", kNoModTxt, ""}, true);
  };
  // A file the game cannot use at all: IMPORT leaves it out of the copy it adds.
  auto leave = [&](const std::string& rel, const std::string& wrong, const std::string& original = "") {
    warn(rel, {wrong, kLeavesOut, original, ""}, true);
    if (std::find(fx.unread_files.begin(), fx.unread_files.end(), rel) == fx.unread_files.end()) fx.unread_files.push_back(rel);
  };
  // One thing in a file (a sound, a script): that one is not used, the rest of the file is.
  auto item = [&](const std::string& where, const std::string& wrong, const std::string& did) { warn(where, {wrong, did, "", ""}, true); };
  // One of the rebuild's own kinds (JSON, a zip, loose text): not read, what it stood in for used.
  auto ours = [&](const std::string& where, const std::string& wrong, const std::string& did) { warn(where, {wrong, did, kOurKind, ""}, true); };
  std::error_code ec;
  if (!fs::is_directory(dir, ec)) {
    error(dir + " is no folder");
    return out;
  }
  ModInfo m;
  std::ifstream f(fs::path(dir) / "mod.txt");
  if (!f) {
    // Files laid over the game's folder with no mod.txt play as an install,
    // so the folder is checked all the same (Freako's PM mods ship so).
    out.push_back("no mod.txt: checked as an install overlay");
    fx.overlay = true;
  } else {
    std::stringstream ss;
    ss << f.rdbuf();
    if (mod_parse(ss.str(), &m)) out.push_back("mod.txt: " + m.name + (m.version.empty() ? "" : " " + m.version));
    else error("mod.txt: " + m.error);
  }
  for (const std::string& w : m.worlds) {
    if (mission_dir_index(w.c_str()) >= 0) row("world " + w, "there is a world " + w + " already");
    else if (w.find_first_of("/\\. ") != std::string::npos || !mod_world_file(dir, w))
      row("world " + w, "no zbd/" + w + "/gamez.zbd");
    else if (!fs::is_regular_file(fs::path(dir) / (w + ".gs"), ec))
      warn("world " + w, {"no " + w + ".gs (its build script) at the mod's root",
                          "the world is listed, but no mission on it can load until that file is there", kNoModTxt, ""}, true);
    else out.push_back("world " + w + ": ok"), fx.worlds.push_back(w), ++usable_rows;
  }
  for (const auto& [folder, name] : m.ia_worlds) {
    const bool own = std::find(m.worlds.begin(), m.worlds.end(), folder) != m.worlds.end();
    if (!own && mission_dir_index(folder.c_str()) < 0) row("ia_world " + folder, "no such world");
    else ++usable_rows, out.push_back("ia_world " + folder + " \"" + (name.empty() ? folder : name) + "\": ok"), fx.ia_worlds.push_back(name.empty() ? folder : name);
  }
  std::vector<std::string> ids;
  for (const std::string& file : m.campaigns) {
    ModCampaign c;
    if (!campaign_load(dir, fs::path(dir).filename().string(), file, &c)) {
      row("campaign " + file, c.error);
      continue;
    }
    if (std::find(ids.begin(), ids.end(), lower(c.id)) != ids.end()) {
      row("campaign " + file, "the mod has a campaign " + c.id + " already");
      continue;
    }
    ids.push_back(lower(c.id));
    out.push_back("campaign " + file + " \"" + c.name + "\": " + std::to_string(c.missions.size()) + " missions");
    fx.campaigns.push_back(c.name), ++usable_rows;
    // Each world the mod's own or the game's; another mod's is known only when mounted.
    for (const CampaignMission& cm : c.missions) {
      const bool own = std::find(m.worlds.begin(), m.worlds.end(), cm.world) != m.worlds.end() &&
                       mod_world_file(dir, cm.world);
      const bool game = mission_dir_index(cm.world.c_str()) >= 0 &&
                        (install.empty() || fs::is_regular_file(zutl::any_case(install, "zbd/" + cm.world + "/gamez.zbd"), ec));
      if (!own && !game)
        out.push_back("warning: campaign " + file + ": line " + std::to_string(cm.line) + ": no world " + cm.world +
                      " in the mod or the game: it plays only with a mod that adds it");
    }
  }
  for (const std::string& line : m.mechs) {
    ModMech r;
    std::string why;
    if (!mech_row_parse(line, &r, &why)) row("mech " + line, why);
    else if (mech_index(r.name.c_str()) >= 0) row("mech " + r.name, "there is a mech " + r.name + " already");
    else if (const std::string miss = install.empty() ? "" : mech_row_missing(r, {dir, install}); !miss.empty())
      row("mech " + r.name, miss + " (the mod's, else the install's)");
    else
      ++usable_rows, fx.mechs.push_back(r.name), out.push_back("mech " + r.name + ": ok, " + std::to_string(r.mass) + " t, the models of " + r.model +
                    (fs::is_regular_file(fs::path(dir) / "zbd" / ("dfn_" + r.name + ".zrd"), ec) ? ", its zbd/dfn_" + r.name + ".zrd"
                                                                                                  : ", its dfn_" + r.name + ".zrd from a reader archive"));
  }
  for (const std::string& line : m.weapons) {
    ModWeapon r;
    std::string why;
    if (!weapon_row_parse(line, &r, &why)) row("weapon " + line, why);
    else if (weapon_index(r.name.c_str()) >= 0 || crt::msvc_stricmp(r.name.c_str(), "PPC") == 0)
      row("weapon " + r.name, "there is a weapon " + r.name + " already");
    else out.push_back("weapon " + r.name + ": ok, its definition from the mod's weapons.zrd"), fx.weapons.push_back(r.name), ++usable_rows;
  }
  // Every file.
  std::vector<std::string> files;
  for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
    if (it->is_regular_file(ec)) files.push_back(fs::relative(it->path(), dir, ec).generic_string());
  std::sort(files.begin(), files.end());
  int archives = 0, entries = 0;
  for (const std::string& rel : files) {
    const std::string low_rel = lower(rel);
    if (mod_own_file(low_rel)) continue;
    if (!zutl::mod_may_serve(rel)) {
      out.push_back(rel + ": the player's own kind of file, which a mod cannot replace (ignored)");
      ++fx.ignored;
      continue;
    }
    // An archive kept as its changed entries (entry_archives.h): checked as the archive it stands for.
    const std::string cut_of = entries_archive_of(rel);
    const std::string low = cut_of.empty() ? low_rel : lower(cut_of);
    const bool over = !install.empty() && fs::is_regular_file(zutl::any_case(install, cut_of.empty() ? rel : cut_of), ec);
    if (!cut_of.empty()) {
      const std::vector<std::string> changed = archive_entry_names((fs::path(dir) / rel).generic_string());
      out.push_back(rel + ": the changed entries of " + cut_of + " (" + std::to_string(changed.size()) + ")");
      std::string names;
      for (size_t i = 0; i < changed.size() && i < 6; ++i) names += (i ? ", " : "") + changed[i];
      if (changed.size() > 6) names += " and " + std::to_string(changed.size() - 6) + " more";
      fx.cut.push_back(cut_of + ": " + names);
      fx.cut_entries += static_cast<int>(changed.size());
    } else {
      out.push_back(rel + (install.empty() ? "" : over ? ": replaces the game's" : ": added"));
    }
    ++fx.files, ++(over ? fx.replaces : fx.adds);
    // Which of the game's missions it replaces (REPORT says them): a mission's own archive, or its world's files.
    if (over && low.rfind("zbd/", 0) == 0 && std::count(low.begin(), low.end(), '/') == 2) {
      const std::string world = low.substr(4, low.find('/', 4) - 4), leaf = low.substr(low.rfind('/') + 1);
      const auto once = [](std::vector<std::string>& v, const std::string& s) {
        if (std::find(v.begin(), v.end(), s) == v.end()) v.push_back(s);
      };
      if (leaf.size() > 11 && leaf.rfind("readerm", 0) == 0 && leaf.compare(leaf.size() - 4, 4, ".zbd") == 0 &&
          std::all_of(leaf.begin() + 7, leaf.end() - 4, [](char c) { return c >= '0' && c <= '9'; }))
        once(fx.missions, world + " " + std::to_string(std::atoi(leaf.c_str() + 7)));
      else if (leaf == "gamez.zbd" || leaf == "anim.zbd" || leaf == "reader.zbd") {
        bool has_missions = false;  // a world the campaign or training plays on
        for (int n = 1; n <= 9 && !has_missions; ++n)
          has_missions = fs::is_regular_file(zutl::any_case(install, "zbd/" + world + "/readerm" + std::to_string(n) + ".zbd"), ec);
        if (has_missions) once(fx.mission_worlds, world);
      }
    }
    const std::string host = (fs::path(dir) / rel).generic_string();
    const std::string name = low.substr(low.rfind('/') == std::string::npos ? 0 : low.rfind('/') + 1);
    if (anim_file_unread(low)) {
      out.push_back("  not read by Pirate's Moon, whose animations come from a world's anim.zbd only");
      ++fx.unread_anims;
      continue;
    }
    // Not zar archives (M3): the texture and image libraries, the scripts.
    if (name == "rimage.zbd" || name.rfind("rtexture", 0) == 0 || name.rfind("rmechtex", 0) == 0 ||
        name.rfind("texture", 0) == 0 || name.rfind("mechtex", 0) == 0) {
      if (name.size() > 4 && name.compare(name.size() - 4, 4, ".zbd") == 0) {
        ++archives;
        check_texture_library(host, rel, &entries, warn, [&](const std::string& r, const std::string& w) { leave(r, w); });
        continue;
      }
    }
    if (name == "interp.zbd") {
      ++archives;
      check_interp(host, rel, &entries, [&](const std::string& wrong) { leave(rel, wrong); }, item);
      continue;
    }
    if (name == "gamez.zbd") {  // a world, not an archive: its header (the rest needs the world up)
      zcls::ZbdHeader h;
      std::FILE* f = zcls::open_zbd(host.c_str(), &h);
      if (f) std::fclose(f);
      else leave(rel, "not a world the game can read (its signature or version)");
      continue;
    }
    if (name == "anim.zbd") {  // a world's animations: the read (its binding needs the world)
      zeff::anim() = zeff::AnimState{};
      zeff::anim_allow_loading(1);
      zeff::anim().host_path = host;  // a folder anywhere: 0x005090f0's 127 would refuse a long one
      zeff::anim_read_zbd();
      if (zeff::anim().defs == nullptr && zeff::anim().def_count() == 0) leave(rel, "its animations cannot be read");
      else out.push_back("  " + std::to_string(zeff::anim().def_count()) + " animations read (bound to the world in play)");
      zeff::anim_free_all();
      zeff::anim() = zeff::AnimState{};
      continue;
    }
    if (name.size() > 4 && name.compare(name.size() - 4, 4, ".zbd") == 0) {
      zutl_zar::Archive a;
      zutl_zar::init(a);
      if (!zutl_zar::open(a, host.c_str())) {
        leave(rel, "not an archive the game can read (its table)");
        zutl_zar::destroy(a);
        continue;
      }
      ++archives;
      std::vector<std::string> names;
      for (int i = 0; i < a.count; ++i) names.push_back(a.entries[i].name);
      zutl_zar::destroy(a);
      if (name == "mechlib.zbd") {  // every tree through the bounded reader
        zmod::models() = zmod::ModelPool{};
        zmod::reset_pool();
        zcls::MechLib lib;
        zutl_zar::init(lib.archive);
        if (!zcls::mechlib_open(lib, host.c_str()) || lib.materials.empty()) {
          leave(rel, "its version, format or materials cannot be read");
        } else {
          for (const std::string& e : names) {
            if (e == "version" || e == "format" || e == "materials") continue;
            ++entries;
            if (!zcls::mechlib_load(lib, e.c_str())) leave(rel, "its model " + e + " cannot be read");
          }
        }
        zcls::mechlib_close(lib);
        continue;
      }
      if (name == "motion.zbd") {  // every motion through the bounded reader
        if (open_motions(host.c_str())) {
          for (const std::string& e : names) {
            ++entries;
            zsis::Script sc;
            if (!load_motion(sc, e.c_str())) leave(rel, "its motion " + e + " cannot be read");
            script_free_parts(sc);
          }
          close_motions();
        } else {
          leave(rel, "the motion archive cannot be opened");
        }
        continue;
      }
      if (name.rfind("sounds", 0) == 0) {  // every sound through the bounded WAV read (M3)
        zutl_zar::Archive sa;
        zutl_zar::init(sa);
        if (zutl_zar::open(sa, host.c_str())) {
          for (const std::string& e : names) {
            ++entries;
            zsnd::Wave w;
            w.path = e;
            if (!zsnd::wave_read_archive(w, sa))
              item(rel + ": sound " + e, "it cannot be read as a sound", "this sound is not played; the rest of the file is used");
          }
        }
        zutl_zar::destroy(sa);
        continue;
      }
      if (name.rfind("reader", 0) != 0) {  // the reader archives' entries: through the bounded reader
        out.push_back("  its table read; what its " + std::to_string(names.size()) +
                      " entries hold is not checked yet (docs/modding.md, M3)");
        continue;
      }
      zrdr::init();
      if (!zrdr::open_archive(host.c_str(), true)) {
        leave(rel, "the reader cannot open it");
        continue;
      }
      for (const std::string& e : names) {
        ++entries;
        zrdr::Node* n = zrdr::load(e.c_str());
        // What a hand-edited entry has wrong, in the reader's plain words: for the mod's maker, never a refusal.
        if (n) {
          for (const ModWarning& note : zrdr::load_notes()) warn(rel + ": " + e, note, false);
          zrdr::free_node(n);
        } else {
          // Nothing of the entry reads: the map or mission it belongs to would not load, so IMPORT leaves the file out.
          ModWarning note = zrdr::load_notes().empty() ? ModWarning{"it cannot be read", "", "", ""} : zrdr::load_notes().front();
          note.did = std::string("nothing of the entry can be read, so ") + kLeavesOut;
          warn(rel + ": " + e, note, true);
          if (std::find(fx.unread_files.begin(), fx.unread_files.end(), rel) == fx.unread_files.end()) fx.unread_files.push_back(rel);
        }
      }
      zrdr::close_archives(true);
    } else if (name.size() > 4 && name.compare(name.size() - 4, 4, ".zrd") == 0) {
      zrdr::init();
      std::string why;
      if (std::FILE* t = std::fopen(host.c_str(), "rb")) why = zrdr::text_error(t), std::fclose(t);
      if (!why.empty()) ours(rel, why, kEntryInstead);
      else if (zrdr::Node* n = zrdr::load(host.c_str())) zrdr::free_node(n);
      else ours(rel, "not a text .zrd the reader can read", kEntryInstead);
    } else if (name == "metadata.json" && low.size() > 20 && low.compare(low.size() - 20, 20, "/gamez/metadata.json") == 0) {
      // a world as unzbd's JSON: its five files read whole, as the game reads them
      const std::string folder = host.substr(0, host.size() - std::strlen("metadata.json"));
      std::vector<uint8_t> zbd;
      std::string why;
      ++entries;
      const auto file = [&](const std::string& n) {
        std::ifstream in(folder + n, std::ios::binary);
        std::stringstream t;
        t << in.rdbuf();
        return t.str();
      };
      if (!zcls::gamez_from_json(file, &zbd, &why))
        ours(rel.substr(0, rel.size() - std::strlen("metadata.json")), why, "this world's JSON is not read; the gamez.zbd is used");
      else out.push_back("  its world read from unzbd's JSON (" + std::to_string(zbd.size()) + " bytes)");
    } else if (name == "gamez.zip") {
      zutl_zip::Zip z;
      if (!z.open(host)) {
        ours(rel, "not a zip the game can read", "the world in it is not read; the gamez.zbd is used");
        continue;
      }
      const auto file = [&](const std::string& n) {
        for (const zutl_zip::Zip::Entry& e : z.entries) {
          std::string l = e.name;
          for (char& c : l) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
          std::vector<uint8_t> d;
          std::string err;
          if (l == n && z.read(e, &d, &err)) return std::string(d.begin(), d.end());
        }
        return std::string();
      };
      std::vector<uint8_t> zbd;
      std::string why;
      ++entries;
      if (!zcls::gamez_from_json(file, &zbd, &why)) ours(rel, why, "the world in it is not read; the gamez.zbd is used");
      else out.push_back("  its world read from unzbd's JSON (" + std::to_string(zbd.size()) + " bytes)");
    } else if (anim_defs::is_node_file(rel)) {  // a def's own node (modding.md, step 4): its tree and materials read
      ++entries;
      std::string why;
      if (!anim_defs::node_file_reads(host, &why)) ours(rel, why, "this node is left out, and the animations on it");
      else out.push_back("  a node of its own for its animations, read in a world that has none of that name");
    } else if (std::string world; anim_defs::is_def_file(dir, rel, &world)) {
      // one animation (modding.md, Animations a def at a time): read alone, as the merge reads it
      ++entries;
      const std::string folder = (fs::path(host).parent_path() / "").generic_string();
      const auto file = [&](const std::string& n) {
        std::ifstream in(folder + n, std::ios::binary);
        std::stringstream t;
        t << in.rdbuf();
        return t.str();
      };
      std::vector<uint8_t> def;
      std::string key, why;
      if (!zeff::anim_def_from_json(file(fs::path(host).filename().string()), file, &def, &key, &why))
        ours(rel, why, "this animation is left out");
      else
        out.push_back("  the animation " + key + " for " + (world.empty() ? std::string("every world") : world) +
                      ": replaces the world's of that name, else added (" + std::to_string(def.size()) + " bytes)");
    } else if (name == "metadata.json" && low.size() > 19 && low.compare(low.size() - 19, 19, "/anim/metadata.json") == 0) {
      // a world's animations as unzbd's JSON: the folder read whole, as the game reads it
      const std::string folder = host.substr(0, host.size() - std::strlen("metadata.json"));
      std::vector<uint8_t> zbd;
      std::string why;
      ++entries;
      const auto file = [&](const std::string& n) {
        std::ifstream in(folder + n, std::ios::binary);
        std::stringstream t;
        t << in.rdbuf();
        return t.str();
      };
      if (!zeff::anim_from_json(file, &zbd, nullptr, &why))
        ours(rel.substr(0, rel.size() - std::strlen("metadata.json")), why, "these animations' JSON is not read; the anim.zbd is used");
      else out.push_back("  its animations read from unzbd's JSON (" + std::to_string(zbd.size()) + " bytes)");
    } else if (name == "anim.zip") {
      zutl_zip::Zip z;
      if (!z.open(host)) {
        ours(rel, "not a zip the game can read", "the animations in it are not read; the anim.zbd's are used");
        continue;
      }
      std::map<std::string, const zutl_zip::Zip::Entry*> by_name;
      for (const zutl_zip::Zip::Entry& e : z.entries) {
        std::string l = e.name;
        for (char& c : l) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        by_name[l] = &e;
      }
      const auto file = [&](const std::string& n) {
        std::string l = n;
        for (char& c : l) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        std::vector<uint8_t> d;
        std::string err;
        const auto hit = by_name.find(l);
        return hit != by_name.end() && z.read(*hit->second, &d, &err) ? std::string(d.begin(), d.end()) : std::string();
      };
      std::vector<uint8_t> zbd;
      std::string why;
      ++entries;
      if (!zeff::anim_from_json(file, &zbd, nullptr, &why)) ours(rel, why, "the animations in it are not read; the anim.zbd's are used");
      else out.push_back("  its animations read from unzbd's JSON (" + std::to_string(zbd.size()) + " bytes)");
    } else if (low.rfind("zbd/mechlib/", 0) == 0 && name.size() > 5 && name.compare(name.size() - 5, 5, ".json") == 0) {  // mechlib's
      std::ifstream in(host, std::ios::binary);
      std::stringstream text;
      text << in.rdbuf();
      std::string why;
      std::vector<uint8_t> entry;
      ++entries;
      const bool ok = name == "materials.json" ? zcls::mechlib_materials_from_json(text.str(), &entry, &why)
                                               : zcls::mechlib_model_from_json(text.str(), &entry, &why);
      if (!ok) ours(rel, why, kArchiveInstead);
    } else if (low == "zbd/mechlib.zip") {
      zutl_zip::Zip z;
      if (!z.open(host)) {
        ours(rel, "not a zip the game can read", "what is in it is not read; the archive's entries are used");
        continue;
      }
      for (const zutl_zip::Zip::Entry& e : z.entries) {
        std::string lower = e.name;
        for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lower.size() <= 5 || lower.compare(lower.size() - 5, 5, ".json") != 0 || lower == "manifest.json") continue;
        ++entries;
        std::vector<uint8_t> data, entry;
        std::string why;
        const std::string text = z.read(e, &data, &why) ? std::string(data.begin(), data.end()) : std::string();
        const bool ok = !text.empty() && (lower == "materials.json" ? zcls::mechlib_materials_from_json(text, &entry, &why)
                                                                     : zcls::mechlib_model_from_json(text, &entry, &why));
        if (!ok) ours(rel + ":" + e.name, why, kArchiveInstead);
      }
    } else if (low.rfind("zbd/motion/", 0) == 0 && name.size() > 5 && name.compare(name.size() - 5, 5, ".json") == 0) {  // a motion as unzbd's JSON
      std::ifstream in(host, std::ios::binary);
      std::stringstream text;
      text << in.rdbuf();
      std::string why;
      std::vector<uint8_t> entry;
      ++entries;
      if (!zsis::motion_from_json(text.str(), &entry, &why)) ours(rel, why, "this file is not read; the archive's motion is used");
    } else if (low == "zbd/motion.zip") {
      zutl_zip::Zip z;
      if (!z.open(host)) {
        ours(rel, "not a zip the game can read", "the motions in it are not read; the archive's are used");
        continue;
      }
      for (const zutl_zip::Zip::Entry& e : z.entries) {
        if (e.name.size() <= 5 || e.name.compare(e.name.size() - 5, 5, ".json") != 0 || e.name == "manifest.json") continue;
        ++entries;
        std::vector<uint8_t> data, entry;
        std::string why;
        if (!(z.read(e, &data, &why) && zsis::motion_from_json(std::string(data.begin(), data.end()), &entry, &why)))
          ours(rel + ":" + e.name, why, "this file is not read; the archive's motion is used");
      }
    } else if (name.size() > 9 && name.compare(name.size() - 9, 9, ".zrd.json") == 0) {  // mech3ax's JSON, beside its archive
      std::ifstream in(host, std::ios::binary);
      std::stringstream text;
      text << in.rdbuf();
      std::string why;
      ++entries;
      if (zrdr::Node* n = zrdr::parse_json(text.str(), &why)) zrdr::free_node(n);
      else ours(rel, why, kEntryInstead);
    } else if (name.rfind("reader", 0) == 0 && name.size() > 4 && name.compare(name.size() - 4, 4, ".zip") == 0) {
      zutl_zip::Zip z;
      if (!z.open(host)) {
        ours(rel, "not a zip the game can read", "what is in it is not read; the archive's entries are used");
        continue;
      }
      for (const zutl_zip::Zip::Entry& e : z.entries) {
        if (e.name.size() <= 9 || e.name.compare(e.name.size() - 9, 9, ".zrd.json") != 0) continue;
        ++entries;
        std::vector<uint8_t> data;
        std::string why;
        zrdr::Node* n = z.read(e, &data, &why) ? zrdr::parse_json(std::string(data.begin(), data.end()), &why) : nullptr;
        if (n) zrdr::free_node(n);
        else ours(rel + ":" + e.name, why, kEntryInstead);
      }
    }
  }
  // strings.txt, as the game would take it.
  if (fs::is_regular_file(fs::path(dir) / "strings.txt", ec)) {
    zutl::Mounts saved = zutl::mounts();
    zutl::mounts() = zutl::Mounts{};
    zutl::mount_mod(dir);
    std::map<int32_t, std::string> text;
    for (const std::string& l : mods_apply_strings(text, {})) out.push_back(l.rfind("  ", 0) == 0 ? "warning:" + l.substr(1) : l);
    zutl::mounts() = saved;
    ++usable_rows;
  }
  // What IMPORT left out of this copy when it added it (its record, in the mod's folder), said again.
  const std::vector<ModNote> before = mod_left_out_read(dir);
  for (const ModNote& n : before) warn(n.where, n.w, true);
  // Nothing of it usable: every file and line left out. The mod's fault as a whole.
  const int gone = static_cast<int>(fx.unread_files.size());
  if ((gone > 0 || !before.empty() || !fx.notes.empty()) && fx.files - gone <= 0 && usable_rows == 0 && *errors == 0)
    error("the mod: nothing in it can be used (every file and line of it is left out)");
  // The plain warnings (a campaign's world not there, strings.txt's lines) are notes too, in the check's order.
  {
    size_t at = 0;
    std::vector<ModNote> all;
    for (const std::string& l : out) {
      if (l.rfind("warning: ", 0) != 0) continue;
      fx.warnings.push_back(l.substr(9));
      if (at < fx.notes.size() && l == "warning: " + mod_warning_line(fx.notes[at].where, fx.notes[at].w)) all.push_back(fx.notes[at++]);
      else all.push_back({"", {l.substr(9), "", "", ""}, ModWarningGroup::kSlip, true});
    }
    fx.notes = all;
  }
  out.push_back(std::to_string(files.size()) + " files, " + std::to_string(archives) + " archives, " + std::to_string(entries) +
                " reader entries checked; " + std::to_string(*errors) + " errors, " + std::to_string(fx.notes.size()) + (fx.notes.size() == 1 ? " warning" : " warnings"));
  return out;
}

}  // namespace mech3
