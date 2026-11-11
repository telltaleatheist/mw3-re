#include "upgrades/mods/mods_tick_list.h"

#include <algorithm>

#include "mech3/mods_tables.h"
#include "upgrades/mods/campaign_entries.h"
#include "upgrades/mods/screen_fit.h"

namespace mech3 {

void ModTickList::fill(const std::string& dir, const std::vector<std::string>& on, const std::vector<std::string>& fixed, bool assets) {
  write_dir = dir, locked = fixed, assets_only = assets;
  ticks.clear();
  // A mod that is only a campaign has nothing to lay over Instant Action: not listed there, and not ticked.
  std::vector<std::string> hidden;
  if (no_campaign_only)
    for (const ModInfo& m : mods_found(write_dir))
      if (mod_is_only_campaign(m)) hidden.push_back(m.folder);
  auto is_hidden = [&](const std::string& f) { return std::find(hidden.begin(), hidden.end(), f) != hidden.end(); };
  for (const std::string& t : on)
    if (!is_locked(t) && !is_hidden(t)) ticks.push_back(t);
  const int32_t was = list.selected;
  std::vector<std::string> all = locked;
  all.insert(all.end(), ticks.begin(), ticks.end());
  rows.clear();
  for (ModsListEntry& m : mods_list(write_dir, all))
    if (is_locked(m.info.folder) || ((!assets_only || !mod_is_campaign(m.info)) && !is_hidden(m.info.folder))) rows.push_back(std::move(m));
  list.clear_items();
  // A long name is cut to the box with "..." (the selected row's font, the wider of the two); DETAILS has it whole.
  for (size_t i = 0; i < rows.size(); ++i)
    list.add_item(mods_text_fit(row_text(rows[i]), list.rows.empty() ? nullptr : list.rows[0], kModsRowPx, kModsRowChars).c_str(),
                  static_cast<int32_t>(i) | (is_locked(rows[i].info.folder) ? ModsList::kLockedRow : 0));
  list.layout();
  if (list.item_count()) list.FileList::select(was >= 0 && was < list.item_count() ? was : 0);
}

// A row as shown: mods_row_text's "[x] Name" or "[ ] Name"; a locked one's
// box reads "[on]" (the name whole, cut only as any row is at the box's edge).
std::string ModTickList::row_text(const ModsListEntry& m) const {
  std::string t = mods_row_text(m);
  if (locked.empty() || t.size() < 3) return t;
  // With "[on]" rows in the list the tick boxes are padded to its width, so every name starts in one column.
  if (is_locked(m.info.folder) && t.compare(0, 3, "[x]") == 0) t.replace(0, 3, "[on]");
  else t.insert(3, " ");
  return t;
}

bool ModTickList::is_locked(const std::string& folder) const { return std::find(locked.begin(), locked.end(), folder) != locked.end(); }

std::string ModTickList::locked_why(const std::string& folder) const {
  std::string name;
  for (const ModsListEntry& r : rows)
    if (r.info.folder == folder) name = r.info.name;
  for (const ModsListEntry& r : rows)
    if (is_locked(r.info.folder) && r.info.folder != folder)
      for (const auto& [need, version] : r.info.needs)
        if (need == folder || (!name.empty() && need == name))
          return "Needed by " + (r.info.name.empty() ? r.info.folder : r.info.name) + ": always on.";
  return "Part of this campaign: always on.";
}

const ModsListEntry* ModTickList::chosen() const {
  const int32_t i = list.selected;
  return i >= 0 && i < static_cast<int32_t>(rows.size()) ? &rows[static_cast<size_t>(i)] : nullptr;
}

std::string ModTickList::note() const {
  const ModsListEntry* m = chosen();
  if (!m) return {};
  const std::string n = mods_row_note(*m, rows);
  if (!is_locked(m->info.folder)) return n;
  return n.empty() ? locked_why(m->info.folder) : locked_why(m->info.folder) + " " + n;
}

void ModTickList::all_off() { fill(write_dir, {}, locked, assets_only); }

bool ModTickList::on_button(int32_t id, bool* changed) {
  if (changed) *changed = false;
  if (id == kList) return true;  // a row chosen: the screen shows its note
  if (id != kToggle && id != kUp && id != kDown) return false;
  warned.clear();
  const ModsListEntry* m = chosen();
  if (!m) return true;
  const std::string folder = m->info.folder;
  if (is_locked(folder)) {  // a campaign's own: said, nothing changed
    warned = locked_why(folder);
    return true;
  }
  std::vector<std::string> next = ticks;
  const bool turning_on = id == kToggle && std::find(next.begin(), next.end(), folder) == next.end();
  if (id == kToggle) mods_toggle(next, folder);
  else if (!mods_move(next, folder, id == kUp ? -1 : 1)) return true;
  fill(write_dir, next, locked, assets_only);
  for (size_t i = 0; i < rows.size(); ++i)
    if (rows[i].info.folder == folder) {
      list.FileList::select(static_cast<int32_t>(i));
      // Turned on all the same, its warnings said (mod_needs.h).
      if (turning_on && !rows[i].needs.empty())
        warned = rows[i].needs[0] + (rows[i].needs.size() > 1 ? " (" + std::to_string(rows[i].needs.size() - 1) + " more: INFO on the MODS screen)" : "");
    }
  if (changed) *changed = true;
  return true;
}

}  // namespace mech3
