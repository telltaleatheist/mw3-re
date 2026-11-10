#include "upgrades/mods/save_mods.h"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <sstream>

#include "gamez/zsave/zsave.h"

namespace mech3 {

namespace {
struct Loaded {
  bool read = false;  // a saved game was read since the last note
  ModsIdentity id;    // its mods (none: hash 0)
  SaveEntry entry;    // its campaign entry (none: "")
};
Loaded g_loaded;

std::string joined(const ModsIdentity& id) {
  std::string s;
  for (const std::string& n : id.names) s += (s.empty() ? "" : ", ") + n;
  return s.empty() ? "(unnamed)" : s;
}

// An entry that mounts something of gameplay; else the save is the original's.
bool counts(const SaveEntry& e) { return !e.id.empty() && e.mounted_hash != 0; }

void save(zsave::Context& ctx, void*) {
  if (counts(save_entry())) {  // inside a campaign entry: the entry, in place of the mod list
    const std::string t = save_entry_text(save_entry());
    zsave::write(ctx, "entry", t.data(), static_cast<uint32_t>(t.size()));
    return;
  }
  const ModsIdentity& id = mods_identity();
  if (id.hash == 0) return;  // no gameplay mod: the original's save
  const std::string t = save_mods_text(id);
  zsave::write(ctx, "identity", t.data(), static_cast<uint32_t>(t.size()));
}

void load(zsave::Context&, const char* sub, const void* data, uint32_t size, void*) {
  const std::string name = sub ? sub : "";
  if (!data) return;
  if (name == "identity") g_loaded.id = save_mods_parse(std::string(static_cast<const char*>(data), size));
  if (name == "entry") g_loaded.entry = save_entry_parse(std::string(static_cast<const char*>(data), size));
}

std::string some(const std::vector<std::string>& names) {
  std::string s;
  for (const std::string& n : names) s += (s.empty() ? "" : ", ") + n;
  return s.empty() ? "none" : s;
}
std::string shown(const SaveEntry& e) { return e.name.empty() ? e.id : e.name; }

// The note when the save or the run is inside a campaign entry; "" when they match.
std::string entry_note(const SaveEntry& was, const ModsIdentity& was_mods, const SaveEntry& now) {
  if (!counts(was))  // saved outside an entry (a pilot copied over from the main menu, or an older build's save)
    return was_mods.hash != 0 ? "Saved with mods: " + joined(was_mods) + "; now in the campaign " + shown(now)
                              : "Saved without mods; the campaign " + shown(now) + " now has: " + (now.ticks.empty() ? "its own files" : some(now.ticks));
  if (!counts(now)) return "Saved in the campaign " + shown(was) + " with its mods; none now";
  if (was.id != now.id) return "Saved in another campaign (" + shown(was) + "); this is " + shown(now);
  if (was.ticks_hash != now.ticks_hash)
    return was.ticks == now.ticks ? "A mod of this campaign changed since the save (" + some(now.ticks) + ")"  // the same ticks, other files
                                  : "This campaign's mods changed since the save: then " + some(was.ticks) + "; now " + some(now.ticks);
  if (was.mounted_hash != now.mounted_hash || was.bases != now.bases) return "This campaign's own files changed since the save";
  return {};
}

}  // namespace

std::string save_mods_text(const ModsIdentity& id) {
  char h[32];
  std::snprintf(h, sizeof h, "%016" PRIx64, id.hash);
  std::string t = "# The mods this game was saved with (docs/modding.md)\nhash " + std::string(h) + "\n";
  for (const std::string& n : id.names) t += "mod " + n + "\n";
  return t;
}

// A line it does not know is passed over; no hash line reads as none.
ModsIdentity save_mods_parse(const std::string& text) {
  ModsIdentity id;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.rfind("hash ", 0) == 0) {
      uint64_t v = 0;
      if (std::sscanf(line.c_str() + 5, "%" SCNx64, &v) == 1) id.hash = v;
    } else if (line.rfind("mod ", 0) == 0 && line.size() > 4) {
      id.names.push_back(line.substr(4));
    }
  }
  return id;
}

SaveEntry& save_entry() {
  static SaveEntry e;
  return e;
}

void save_entry_clear() { save_entry() = SaveEntry{}; }

void save_entry_set(const std::string& write_dir, const CampaignEntry& e) {
  SaveEntry s;
  s.id = e.id, s.name = e.name;
  // The entries under it, by base=, until the game's own or one that is gone (or the chain comes round).
  const std::vector<CampaignEntry> all = campaign_entries(write_dir);
  for (std::string at = e.base; !at.empty() && at != e.id && s.bases.size() < 64;) {
    if (std::find(s.bases.begin(), s.bases.end(), at) != s.bases.end()) break;
    s.bases.push_back(at);
    const CampaignEntry* b = find_entry(all, at);
    at = b ? b->base : std::string();
  }
  const std::vector<std::string> ticks = entry_ticks(write_dir, e.id);
  s.ticks_hash = mods_gameplay_hash(write_dir, ticks);
  s.mounted_hash = mods_gameplay_hash(write_dir, entry_mount_list(write_dir, e));
  const std::vector<ModInfo> found = mods_found(write_dir);
  for (const std::string& t : ticks) {
    if (mods_gameplay_hash(write_dir, {t}) == 0) continue;  // strings, images, sounds only (or not there)
    const auto it = std::find_if(found.begin(), found.end(), [&](const ModInfo& m) { return m.folder == t; });
    const std::string name = it != found.end() && !it->name.empty() ? it->name : t;
    if (std::find(s.ticks.begin(), s.ticks.end(), name) == s.ticks.end()) s.ticks.push_back(name);
  }
  save_entry() = s;
}

std::string save_entry_text(const SaveEntry& e) {
  char h[64];
  std::string t = "# The MODS campaign this game was saved in (docs/modding.md)\nentry " + e.id + "\n";
  if (!e.name.empty()) t += "name " + e.name + "\n";
  for (const std::string& b : e.bases) t += "base " + b + "\n";
  std::snprintf(h, sizeof h, "ticks %016" PRIx64 "\n", e.ticks_hash);
  t += h;
  for (const std::string& n : e.ticks) t += "tick " + n + "\n";
  std::snprintf(h, sizeof h, "mounted %016" PRIx64 "\n", e.mounted_hash);
  return t + h;
}

SaveEntry save_entry_parse(const std::string& text) {
  SaveEntry e;
  std::istringstream in(text);
  std::string line;
  auto rest = [&](const char* key) -> const char* {
    const size_t n = std::char_traits<char>::length(key);
    return line.size() > n && line.compare(0, n, key) == 0 ? line.c_str() + n : nullptr;
  };
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    uint64_t v = 0;
    if (const char* r = rest("entry ")) e.id = r;
    else if (const char* r = rest("name ")) e.name = r;
    else if (const char* r = rest("base ")) e.bases.emplace_back(r);
    else if (const char* r = rest("tick ")) e.ticks.emplace_back(r);
    else if (const char* r = rest("ticks ")) e.ticks_hash = std::sscanf(r, "%" SCNx64, &v) == 1 ? v : 0;
    else if (const char* r = rest("mounted ")) e.mounted_hash = std::sscanf(r, "%" SCNx64, &v) == 1 ? v : 0;
  }
  return e;
}

// Priority 1: written before the animations' (0x32..0x34), so it is read first.
void save_mods_register() { zsave::add("Mods", save, load, 1, nullptr); }

void save_mods_begin_load() { g_loaded = Loaded{true, {}}; }

std::string save_mods_note() {
  if (!g_loaded.read) return {};
  g_loaded.read = false;
  if (counts(g_loaded.entry) || counts(save_entry())) {
    const std::string t = entry_note(g_loaded.entry, g_loaded.id, save_entry());
    if (!t.empty()) std::printf("mw3: %s (the mission's animations may not match the save)\n", t.c_str());
    return t;
  }
  const ModsIdentity& now = mods_identity();
  const ModsIdentity& was = g_loaded.id;
  if (was.hash == now.hash) return {};
  std::string t;
  if (was.hash == 0)
    t = "Saved without mods; now with " + joined(now);
  else if (now.hash == 0)
    t = "Saved with mods: " + joined(was) + "; none now";
  else
    t = "Saved with other mods: " + joined(was) + "; now " + joined(now);
  std::printf("mw3: %s (the mission's animations may not match the save)\n", t.c_str());
  return t;
}

}  // namespace mech3
