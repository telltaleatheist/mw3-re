#include "upgrades/mods/campaign_entries.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "mech3/mod_campaign.h"

namespace fs = std::filesystem;

namespace mech3 {
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

// Written beside it, then renamed over it (mods_save_enabled's way).
bool write_text(const fs::path& p, const std::string& text) {
  std::error_code ec;
  fs::create_directories(p.parent_path(), ec);
  const fs::path tmp = p.string() + ".new";
  {
    std::ofstream o(tmp, std::ios::trunc | std::ios::binary);
    if (!(o << text) || !o.flush()) {
      fs::remove(tmp, ec);
      return false;
    }
  }
  fs::rename(tmp, p, ec);
  if (ec) fs::remove(tmp, ec);
  return !ec;
}

// The game's own worlds: a mod holding their files plays over the game's campaign.
bool official_world_file(const std::string& rel) {
  const std::string r = lower(rel);
  for (const char* w : {"zbd/c1/", "zbd/c2/", "zbd/c3/", "zbd/c4/", "zbd/c4b/", "zbd/t1/"})
    if (r.rfind(w, 0) == 0) return true;
  return false;
}

const ModInfo* named(const std::vector<ModInfo>& all, const std::string& name) {
  for (const ModInfo& o : all)
    if (lower(o.folder) == lower(name)) return &o;
  for (const ModInfo& o : all)
    if (!o.name.empty() && lower(o.name) == lower(name)) return &o;
  return nullptr;
}

void add_once(std::vector<std::string>& v, const std::string& s) {
  if (std::find(v.begin(), v.end(), s) == v.end()) v.push_back(s);
}

// A mod's entries: each campaign= (its file read for the name), else the mod over the game's own campaign.
void mod_entries(const ModInfo& m, std::vector<CampaignEntry>& out) {
  const CampaignEntry::Kind kind = m.base.empty() ? CampaignEntry::kImported : CampaignEntry::kCopy;
  const std::string shown = m.name.empty() ? m.folder : m.name;
  if (m.campaigns.empty()) {
    CampaignEntry e;
    e.kind = kind, e.id = m.folder, e.name = shown, e.mod = m.folder, e.base = m.base.empty() ? official_entries().front().id : m.base;
    out.push_back(e);
    return;
  }
  for (const std::string& file : m.campaigns) {
    ModCampaign c;
    CampaignEntry e;
    e.kind = kind, e.mod = m.folder, e.base = m.base;
    if (campaign_load(m.path, m.folder, file, &c)) {
      e.campaign = c.id, e.name = c.name.empty() ? shown : c.name;
    } else {
      e.campaign = c.id.empty() ? fs::path(file).stem().string() : c.id;
      e.name = shown, e.error = c.error;
    }
    e.id = m.folder + "~" + e.campaign;
    out.push_back(e);
  }
}

}  // namespace

std::vector<CampaignEntry> official_entries() {
  CampaignEntry mw3, ia;
  mw3.kind = CampaignEntry::kOfficial, mw3.id = "mw3", mw3.name = "MechWarrior 3";
  ia.kind = CampaignEntry::kInstantAction, ia.id = "ia", ia.name = "Instant Action";
  return {mw3, ia};
}

bool mod_is_campaign(const ModInfo& m) {
  const std::string k = lower(m.kind);
  if (k == "campaign") return true;
  if (k == "assets") return false;
  if (!m.campaigns.empty() || !m.worlds.empty() || !m.base.empty()) return true;
  std::error_code ec;
  const fs::path root(m.path);
  for (fs::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec))
    if (it->is_regular_file(ec) && official_world_file(fs::relative(it->path(), root, ec).generic_string())) return true;
  return false;
}

std::vector<CampaignEntry> campaign_entries(const std::string& write_dir) {
  std::vector<CampaignEntry> out = official_entries();
  for (const ModInfo& m : mods_found(write_dir))
    if (m.error.empty() && mod_is_campaign(m)) mod_entries(m, out);
  // The folders whose mod is gone (removed, renamed, unusable): their saves stay listed.
  std::vector<std::string> extra;
  std::error_code ec;
  for (fs::directory_iterator it(fs::path(write_dir) / kEntriesFolder, ec), end; !ec && it != end; it.increment(ec)) {
    if (!it->is_directory(ec) || !fs::is_regular_file(it->path() / kCampaignMarker, ec)) continue;
    const std::string id = it->path().filename().string();
    if (!find_entry(out, id)) extra.push_back(id);
  }
  std::sort(extra.begin(), extra.end());
  for (const std::string& id : extra) {
    CampaignEntry e;
    e.kind = CampaignEntry::kImported, e.id = id, e.missing = true;
    const size_t t = id.find('~');
    e.mod = id.substr(0, t), e.campaign = t == std::string::npos ? std::string() : id.substr(t + 1);
    e.name = id, e.error = "its mod is not in the mods folder";
    out.push_back(e);
  }
  return out;
}

const CampaignEntry* find_entry(const std::vector<CampaignEntry>& all, const std::string& id) {
  for (const CampaignEntry& e : all)
    if (e.id == id) return &e;
  return nullptr;
}

std::string entry_dir(const std::string& write_dir, const std::string& id) {
  return (fs::path(write_dir) / kEntriesFolder / id).generic_string();
}
std::string entry_pilots_folder(const std::string& id) { return std::string(kEntriesFolder) + "\\" + id + "\\pilots"; }

bool entry_ensure(const std::string& write_dir, const CampaignEntry& e, std::string* why) {
  const fs::path dir(entry_dir(write_dir, e.id));
  std::error_code ec;
  if (fs::exists(dir, ec) && !fs::exists(dir / kCampaignMarker, ec) && !fs::is_empty(dir, ec)) {
    if (why) *why = dir.generic_string() + " holds files the game did not put there";
    return false;
  }
  fs::create_directories(dir / "pilots" / "games", ec);
  if (!fs::exists(dir / kCampaignMarker, ec))
    std::ofstream(dir / kCampaignMarker) << "\"" << e.name << "\" (" << e.id
                                         << "): its mods, pilots and saves, made by the game. Deleting this folder deletes them.\n";
  if (!fs::exists(dir / kCampaignMarker, ec)) {
    if (why) *why = "cannot write " + (dir / kCampaignMarker).generic_string();
    return false;
  }
  return true;
}

std::vector<std::string> entry_ticks(const std::string& write_dir, const std::string& id) {
  std::vector<std::string> out;
  std::istringstream in(read_text(fs::path(entry_dir(write_dir, id)) / kEntryTicksFile));
  for (std::string line; std::getline(in, line);) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.rfind("mod=", 0) == 0 && line.size() > 4) add_once(out, line.substr(4));
  }
  return out;
}

bool entry_save_ticks(const std::string& write_dir, const CampaignEntry& e, const std::vector<std::string>& ticks) {
  if (!entry_ensure(write_dir, e, nullptr)) return false;
  std::string text;
  for (const std::string& t : ticks) text += "mod=" + t + "\n";
  return write_text(fs::path(entry_dir(write_dir, e.id)) / kEntryTicksFile, text);
}

std::vector<std::string> entry_mount_list(const std::string& write_dir, const CampaignEntry& e,
                                          std::vector<std::string>* locked) {
  const std::vector<CampaignEntry> all = campaign_entries(write_dir);
  const std::vector<ModInfo> mods = mods_found(write_dir);
  std::vector<std::string> fixed;
  // Its own mod, then each base's (a copy over what it was saved from), a loop cut.
  for (const CampaignEntry* at = &e; at && !at->mod.empty() && !at->missing;) {
    if (std::find(fixed.begin(), fixed.end(), at->mod) != fixed.end()) break;
    fixed.push_back(at->mod);
    at = at->base.empty() ? nullptr : find_entry(all, at->base);  // the game's own: no mod, the walk ends
  }
  for (size_t i = 0; i < fixed.size(); ++i)  // what they need, by folder or name
    if (const ModInfo* m = named(mods, fixed[i]))
      for (const auto& [need, version] : m->needs)
        if (const ModInfo* n = named(mods, need); n && n->error.empty()) add_once(fixed, n->folder);
  if (locked) *locked = fixed;
  std::vector<std::string> out = fixed;
  for (const std::string& t : entry_ticks(write_dir, e.id))
    if (const ModInfo* m = named(mods, t); m && m->folder == t) add_once(out, t);
  return out;
}

std::vector<std::string> entry_missing_mods(const std::string& write_dir, const CampaignEntry& e) {
  const std::vector<CampaignEntry> all = campaign_entries(write_dir);
  const std::vector<ModInfo> mods = mods_found(write_dir);
  std::vector<std::string> out;
  auto usable = [&](const std::string& f) {
    for (const ModInfo& m : mods)
      if (m.folder == f) return m.error.empty();
    return false;
  };
  std::vector<std::string> seen;  // a loop of bases cut
  for (const CampaignEntry* at = &e; at && !at->mod.empty() && std::find(seen.begin(), seen.end(), at->id) == seen.end();) {
    seen.push_back(at->id);
    if (!usable(at->mod)) add_once(out, at->mod);
    if (at->base.empty()) break;
    const std::string base = at->base;
    at = find_entry(all, base);
    if (!at) add_once(out, base);  // what it was saved from, gone
  }
  for (const std::string& t : entry_ticks(write_dir, e.id))
    if (!usable(t)) add_once(out, t);
  return out;
}

std::vector<std::string> entries_migrate(const std::string& write_dir) {
  std::vector<std::string> log;
  if (write_dir.empty()) return log;
  const fs::path cfg = fs::path(write_dir) / "mw3.cfg";
  const std::string text = read_text(cfg);
  std::istringstream in(text);
  for (std::string line; std::getline(in, line);) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line == std::string(kEntriesMovedKey) + "=1") return log;
  }
  const std::vector<ModInfo> mods = mods_found(write_dir);
  const std::vector<std::string> on = mods_enabled(write_dir);
  std::vector<std::string> assets, campaigns;
  for (const std::string& f : on)
    if (const ModInfo* m = named(mods, f); m && m->folder == f && m->error.empty())
      (mod_is_campaign(*m) ? campaigns : assets).push_back(f);
  const std::vector<CampaignEntry> all = campaign_entries(write_dir);
  std::error_code ec;
  // Custom campaigns' pilots from before (pilots\mods\<mod>\<id>\, our marker in them).
  for (const CampaignEntry& e : all) {
    if (e.campaign.empty()) continue;
    const fs::path old = fs::path(write_dir) / "pilots" / "mods" / e.mod / e.campaign;
    if (!fs::is_regular_file(old / kCampaignMarker, ec)) continue;
    const fs::path to = fs::path(entry_dir(write_dir, e.id)) / "pilots";
    if (fs::exists(to, ec)) continue;  // something there already: left as it is
    if (!entry_ensure(write_dir, e, nullptr)) continue;
    fs::remove_all(to, ec);  // only the empty pilots\games just made
    fs::rename(old, to, ec);
    if (ec) continue;
    fs::remove(to / kCampaignMarker, ec);  // the entry's own marker is one up
    log.push_back("mods: " + e.id + "'s pilots moved from pilots/mods/" + e.mod + "/" + e.campaign);
  }
  for (const CampaignEntry& e : all) {
    const bool give = e.id == official_entries().front().id || (!e.mod.empty() && std::find(campaigns.begin(), campaigns.end(), e.mod) != campaigns.end());
    if (!give || assets.empty() || fs::exists(fs::path(entry_dir(write_dir, e.id)) / kEntryTicksFile, ec)) continue;
    if (entry_save_ticks(write_dir, e, assets))
      log.push_back("mods: " + e.id + " takes the " + std::to_string(assets.size()) + " mods mw3.cfg had on");
  }
  // mw3.cfg: every line kept byte for byte, the key added at the end (none made where there is none).
  if (!fs::is_regular_file(cfg, ec)) return log;
  std::string eol = text.find("\r\n") != std::string::npos ? "\r\n" : "\n";
  std::string out = text;
  if (!out.empty() && out.back() != '\n') out += eol;
  out += std::string(kEntriesMovedKey) + "=1" + eol;
  if (write_text(cfg, out)) log.push_back("mods: moved to their campaigns (mw3.cfg's " + std::string(kEntriesMovedKey) + "=1)");
  return log;
}

}  // namespace mech3
