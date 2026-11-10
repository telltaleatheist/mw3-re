#include "upgrades/mods/mods_tick_list.h"

#include <algorithm>

#include "mech3/mods_tables.h"

namespace mech3 {

void ModTickList::fill(const std::string& dir, const std::vector<std::string>& on) {
  write_dir = dir, ticks = on;
  const int32_t was = list.selected;
  rows = mods_list(write_dir, ticks);
  list.clear_items();
  for (size_t i = 0; i < rows.size(); ++i) list.add_item(mods_row_text(rows[i]).c_str(), static_cast<int32_t>(i));
  list.layout();
  if (list.item_count()) list.FileList::select(was >= 0 && was < list.item_count() ? was : 0);
}

const ModsListEntry* ModTickList::chosen() const {
  const int32_t i = list.selected;
  return i >= 0 && i < static_cast<int32_t>(rows.size()) ? &rows[static_cast<size_t>(i)] : nullptr;
}

std::string ModTickList::note() const {
  const ModsListEntry* m = chosen();
  return m ? mods_row_note(*m, rows) : std::string();
}

void ModTickList::all_off() { fill(write_dir, {}); }

bool ModTickList::on_button(int32_t id, bool* changed) {
  if (changed) *changed = false;
  if (id == kList) return true;  // a row chosen: the screen shows its note
  if (id != kToggle && id != kUp && id != kDown) return false;
  warned.clear();
  const ModsListEntry* m = chosen();
  if (!m) return true;
  const std::string folder = m->info.folder;
  std::vector<std::string> next = ticks;
  const bool turning_on = id == kToggle && std::find(next.begin(), next.end(), folder) == next.end();
  if (id == kToggle) mods_toggle(next, folder);
  else if (!mods_move(next, folder, id == kUp ? -1 : 1)) return true;
  fill(write_dir, next);
  for (size_t i = 0; i < rows.size(); ++i)
    if (rows[i].info.folder == folder) {
      list.FileList::select(static_cast<int32_t>(i));
      // Turned on all the same, its warnings said (mod_needs.h).
      if (turning_on && !rows[i].needs.empty())
        warned = rows[i].needs[0] + (rows[i].needs.size() > 1 ? " (" + std::to_string(rows[i].needs.size() - 1) + " more: REPORT)" : "");
    }
  if (changed) *changed = true;
  return true;
}

}  // namespace mech3
