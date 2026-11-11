#include "upgrades/mods/mods_delete.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>

#include "mech3/mod_campaign.h"
#include "mech3/mod_install.h"
#include "mech3/mods.h"
#include "mech3/net_save.h"
#include "upgrades/mods/campaign_entries.h"

namespace mech3 {

namespace {
namespace fs = std::filesystem;

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}
bool is_link(const fs::path& p) { return path_is_link(p.string()); }
// A plain file that is really there (not a link to one).
bool plain_file(const fs::path& p) {
  std::error_code ec;
  return !is_link(p) && fs::is_regular_file(fs::symlink_status(p, ec));
}
std::string counted(int n, const char* one, const char* many) { return std::to_string(n) + " " + (n == 1 ? one : many); }
// What goes with a campaign, as a sentence: "Its 2 pilots and 5 saved games <here >go with it." or, with none,
// "It has no pilots or saved games<here>." (never "0 pilots and 0 saved games").
std::string goes_with(const std::string& write_dir, const std::string& entry, const char* here) {
  int p = 0, s = 0;
  entry_counts(write_dir, entry, &p, &s);
  if (!p && !s) return std::string("It has no pilots or saved games") + (*here ? " here" : "") + ".";
  return "Its " + counted(p, "pilot", "pilots") + " and " + counted(s, "saved game", "saved games") + (*here ? " here" : "") + " go with it.";
}
}  // namespace

void entry_counts(const std::string& write_dir, const std::string& entry, int* pilots, int* saves) {
  auto count = [](const fs::path& dir) {
    int n = 0;
    std::error_code ec;
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
      const std::string f = lower(it->path().filename().string());
      if (plain_file(it->path()) && f.size() > 4 && f.compare(f.size() - 4, 4, ".mw3") == 0) ++n;
    }
    return n;
  };
  const fs::path dir = fs::path(entry_dir(write_dir, entry)) / "pilots";
  if (pilots) *pilots = count(dir);
  if (saves) *saves = count(dir / "games");
}

DeletePlan delete_plan_for_entry(const std::string& write_dir, const std::string& entry, const std::string& inside) {
  DeletePlan p;
  const std::vector<CampaignEntry> all = campaign_entries(write_dir);
  const CampaignEntry* e = find_entry(all, entry);
  if (!e) return p.refused = "Choose a campaign.", p;
  const std::string name = e->name.empty() ? e->id : e->name;
  p.entry = e->id;
  if (!inside.empty() && inside == e->id) return p.entry.clear(), p.refused = name + " is being played: leave it first.", p;
  if (e->kind == CampaignEntry::kOfficial) {
    p.kind = DeletePlan::kReset, p.label = "RESET";
    p.question = "Reset " + name + " to the original? Its ticked mods are cleared. " + goes_with(write_dir, e->id, "here") +
                 " Your pilots in the main game are not changed.";
    return p;
  }
  if (e->kind == CampaignEntry::kMultiplayer) return p.entry.clear(), p.refused = "Multiplayer's mods are turned off on its own screen (ALL OFF).", p;
  if (e->kind == CampaignEntry::kInstantAction) return p.entry.clear(), p.refused = "Instant Action's mods are turned off on its own screen (ALL OFF).", p;
  if (e->missing) {
    p.kind = DeletePlan::kLeftover, p.label = "DELETE";
    p.question = "Delete what is left of " + name + "? " + goes_with(write_dir, e->id, "");
    return p;
  }
  // A campaign with its mod: nothing another campaign is built on.
  for (const CampaignEntry& o : all)
    if (o.id != e->id && !o.missing && (o.base == e->id || (!o.mod.empty() && o.mod != e->mod && o.base == e->mod)))
      return p.entry.clear(), p.refused = name + " cannot be deleted: " + (o.name.empty() ? o.id : o.name) + " is built on it. Delete that one first.", p;
  const fs::path mod = fs::path(write_dir) / "mods" / e->mod;
  if (!is_link(mod) && plain_file(mod / ai::kEditorMarker)) {
    // A mod can bring several campaigns; a copy of ours is one mod, one campaign.
    p.kind = DeletePlan::kCopy, p.label = "DELETE", p.mod = e->mod, p.mod_marker = ai::kEditorMarker;
    p.question = "Delete " + name + "? " + goes_with(write_dir, e->id, "");
    return p;
  }
  if (!is_link(mod) && plain_file(mod / kModAddedMarker)) {
    p.kind = DeletePlan::kImportedCampaign, p.label = "DELETE", p.mod = e->mod, p.mod_marker = kModAddedMarker;
    p.entry.clear();  // its pilots and saved games are kept
    p.question = "Delete " + name + "? Its pilots and saved games are kept: import it again to go on.";
    return p;
  }
  p.entry.clear();
  p.by_hand = true;
  p.refused = name + " was not added by the game: remove its folder yourself (MODS FOLDER).";
  return p;
}

DeletePlan delete_plan_for_mod(const std::string& write_dir, const std::string& folder) {
  DeletePlan p;
  ModInfo info;
  bool found = false;
  for (const ModInfo& m : mods_found(write_dir))
    if (m.folder == folder) info = m, found = true;
  if (!found) return p.refused = "Choose a mod.", p;
  const std::string name = info.name.empty() ? folder : info.name;
  const fs::path dir = fs::path(write_dir) / "mods" / folder;
  const std::vector<CampaignEntry> all = campaign_entries(write_dir);
  for (const CampaignEntry& o : all)
    if (!o.missing && o.mod != folder && o.base == folder)
      return p.refused = name + " cannot be deleted: " + (o.name.empty() ? o.id : o.name) + " is built on it. Delete that one first.", p;
  const char* marker = !is_link(dir) && plain_file(dir / kModAddedMarker) ? kModAddedMarker
                       : !is_link(dir) && plain_file(dir / ai::kEditorMarker) ? ai::kEditorMarker : nullptr;
  if (!marker) return p.by_hand = true, p.refused = name + " was not added by the game: remove its folder yourself (MODS FOLDER).", p;
  // The campaigns it is ticked for, by their names, the game's own and Instant Action's among them.
  std::string where;
  for (const CampaignEntry& e : all) {
    const std::vector<std::string> ticks = entry_ticks(write_dir, e.id);
    if (std::find(ticks.begin(), ticks.end(), folder) != ticks.end()) where += (where.empty() ? "" : ", ") + (e.name.empty() ? e.id : e.name);
  }
  p.kind = DeletePlan::kImportedMod, p.label = "DELETE", p.mod = folder, p.mod_marker = marker;
  p.question = "Delete " + name + "? " + (where.empty() ? std::string("It is not ticked for any campaign.") : "It is ticked for: " + where + ".");
  return p;
}

bool delete_run(const std::string& write_dir, const DeletePlan& plan, std::string* why) {
  std::string none;
  std::string& said = why ? *why : none;
  if (plan.kind == DeletePlan::kNone) return said = plan.refused.empty() ? "nothing to delete" : plan.refused, false;
  if (plan.kind == DeletePlan::kReset) {
    // The folder whole (it is ours: its marker), then made again as a first start makes it: no ticks, no pilots.
    const std::vector<CampaignEntry> all = campaign_entries(write_dir);
    const CampaignEntry* e = find_entry(all, plan.entry);
    if (!e || e->kind != CampaignEntry::kOfficial) return said = "only the game's own campaign is reset", false;
    std::error_code ec;
    if (fs::exists(fs::symlink_status(fs::path(entry_dir(write_dir, e->id)), ec)) &&
        !remove_marked_folder(write_dir, kEntriesFolder, e->id, kCampaignMarker, &said))
      return false;
    return entry_ensure(write_dir, *e, &said);
  }
  // The mod's folder first: if it cannot go, the campaign's pilots and saved games are not touched.
  if (!plan.mod.empty() && !remove_marked_folder(write_dir, "mods", plan.mod, plan.mod_marker, &said)) return false;
  if (!plan.entry.empty()) {
    std::error_code ec;
    if (fs::exists(fs::symlink_status(fs::path(entry_dir(write_dir, plan.entry)), ec)) &&
        !remove_marked_folder(write_dir, kEntriesFolder, plan.entry, kCampaignMarker, &said)) {
      if (!plan.mod.empty()) said = "its mod was deleted, but " + said;
      return false;
    }
  }
  return true;
}

}  // namespace mech3
