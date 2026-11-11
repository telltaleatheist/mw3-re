#include "upgrades/mods/entry_pilots.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "mech3/mods_menu.h"
#include "mech3/pilot.h"
#include "upgrades/mods/campaign_entries.h"
#include "upgrades/mods/entry_pilot_copy.h"
#include "upgrades/mods/entry_run.h"

namespace mech3 {

namespace {
std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
}  // namespace

bool entry_pilots_on() { return entry_run().screen != EntryRun::kNone && !entry_run().id.empty() && !mods_menu_write_dir().empty(); }

std::vector<std::string> entry_main_pilots() {
  std::vector<std::string> out;
  if (!entry_pilots_on()) return out;
  for (const std::string& name : main_menu_pilots(mods_menu_write_dir()))
    if (!find_pilot(name.c_str())) out.push_back(name);
  std::sort(out.begin(), out.end(), [](const std::string& a, const std::string& b) { return lower(a) < lower(b); });
  return out;
}

bool entry_is_main_pilot(const std::string& name) {
  for (const std::string& n : entry_main_pilots())
    if (lower(n) == lower(name)) return true;
  return false;
}

Pilot* entry_bring_pilot(const std::string& name, std::string* why) {
  std::string none;
  std::string& said = why ? *why : none;
  if (!entry_pilots_on()) return said = "this is not a campaign from MODS", nullptr;
  const std::string& dir = mods_menu_write_dir();
  const std::vector<CampaignEntry> all = campaign_entries(dir);
  const CampaignEntry* e = find_entry(all, entry_run().id);
  if (!e || !entry_ensure(dir, *e, &said)) return said.empty() ? (said = "its campaign is not there"), nullptr : nullptr;
  std::string real = name;  // the file's own spelling
  for (const std::string& n : main_menu_pilots(dir))
    if (lower(n) == lower(name)) real = n;
  const bool saves = entry_brings_saved_games(dir, *e);
  const PilotCopy c = entry_copy_pilot(dir, real, entry_dir(dir, e->id), entry_dir(dir, e->id) + "/configs", saves);
  if (!c.ok) return said = c.why, nullptr;
  std::printf("mw3: pilot %s brought into %s from the main game (%s, %zu files)\n", real.c_str(), e->id.c_str(),
              saves ? "with its saved games" : "without saved games", c.copied.size());
  Pilot* p = load_pilot(real.c_str(), false);
  if (!p) said = "its copy could not be read";
  return p;
}

}  // namespace mech3
