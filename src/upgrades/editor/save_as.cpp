#include "upgrades/editor/save_as.h"

#include <cstring>
#include <filesystem>
#include <fstream>

#include "mech3/game_editor_net.h"
#include "mech3/mods.h"
#include "mech3/net_save.h"
#include "upgrades/editor/form.h"
#include "upgrades/editor/mods_edit.h"
#include "upgrades/mods/campaign_entries.h"

namespace mech3 {

namespace {
namespace fs = std::filesystem;

// A folder name from a name: what Windows refuses in one dropped.
std::string folder_of(const std::string& name) {
  std::string out;
  for (char c : name)
    if (!std::strchr("\\/:*?\"<>|~", c) && static_cast<unsigned char>(c) >= 0x20) out += c;  // '~' joins a mod and its campaign in an entry's id
  while (!out.empty() && (out.back() == ' ' || out.back() == '.')) out.pop_back();
  while (!out.empty() && out.front() == ' ') out.erase(out.begin());
  return out;
}

struct Box {
  EditorForm form;
  std::string name, said;
  std::function<void()> again;
} g_box;

void box_close() {
  form_hide(&g_box.form);
  g_box.again = nullptr;
}

// Save: the copy made under the name typed, then the command again. A name that cannot be used keeps the box, said.
bool take_name(const std::string& name) {
  SaveAsSession& s = save_as_session();
  std::string folder, why;
  if (!save_as_make_copy(s.write_dir, s.entry, name, &folder, &why)) {
    g_box.said = "Not saved: " + why;
    g_box.name = save_as_default_name(s.write_dir, s.entry);
    editor_status(g_box.said);
    return false;
  }
  s.copy = folder, s.made = true;
  ai::net_save_into_mod() = folder;
  mods_edit_session().mod = folder;
  current_entry() = folder;  // a copy's entry is its folder: what later saves and ticks are for
  editor_status("Saving as the new campaign " + name + " (mods\\" + folder + ")");
  return true;
}
}  // namespace

bool editor_owns_mod(const std::string& write_dir, const std::string& folder) {
  std::error_code ec;
  const fs::path dir = fs::path(write_dir) / "mods" / folder;
  return !write_dir.empty() && !folder.empty() && !fs::is_symlink(dir, ec) && fs::is_regular_file(dir / ai::kEditorMarker, ec);
}

std::string save_as_default_name(const std::string& write_dir, const std::string& entry) {
  const std::vector<CampaignEntry> all = campaign_entries(write_dir);
  const CampaignEntry* e = find_entry(all, entry);
  const std::string base = (e ? e->name : entry) + ", my edit";
  std::error_code ec;
  for (int k = 1; k < 1000; ++k) {
    const std::string name = k == 1 ? base : base + " " + std::to_string(k);
    if (!fs::exists(fs::path(write_dir) / "mods" / folder_of(name), ec)) return name;
  }
  return base;
}

bool save_as_make_copy(const std::string& write_dir, const std::string& entry, const std::string& name, std::string* folder,
                       std::string* why) {
  std::string none;
  std::string& said = why ? *why : none;
  const std::string f = folder_of(name);
  if (write_dir.empty()) return said = "there is no settings folder", false;
  if (f.empty()) return said = "it needs a name", false;
  const std::vector<CampaignEntry> all = campaign_entries(write_dir);
  const CampaignEntry* e = find_entry(all, entry);
  if (!e || e->missing) return said = "the campaign being edited is not among the campaigns", false;
  const fs::path dir = fs::path(write_dir) / "mods" / f;
  std::error_code ec;
  if (fs::exists(fs::symlink_status(dir, ec))) return said = "there is a mod folder " + f + " already; choose another name", false;
  for (const CampaignEntry& other : all)
    if (other.id == f) return said = "there is a campaign " + f + " already; choose another name", false;
  std::string game = mods_game() == "mw3" ? "mw3 1.2" : mods_game();  // this game's word (mods.cpp): mw3pm refuses "mw3"
  for (const ModInfo& m : mods_found(write_dir))
    if (m.folder == e->mod && !m.game.empty()) game = m.game;
  fs::create_directories(dir, ec);
  {
    std::ofstream marker(dir / ai::kEditorMarker, std::ios::binary);
    marker << "Made by the game editor's Save: a copy of the campaign \"" << e->name << "\" (" << e->id
           << "). The editor saves over its own files here (docs/editor.md).\n";
    std::ofstream txt(dir / "mod.txt", std::ios::binary);
    txt << "name=" << name << "\nkind=campaign\nbase=" << e->id << "\nfor=" << game << "\n";
    if (!marker || !txt) {
      marker.close(), txt.close();
      fs::remove(dir / ai::kEditorMarker, ec), fs::remove(dir / "mod.txt", ec), fs::remove(dir, ec);  // the three it just made
      return said = "the mods folder cannot be written", false;
    }
  }
  // Its own ticks: the ones the campaign it was saved from has now.
  const std::vector<CampaignEntry> now = campaign_entries(write_dir);
  if (const CampaignEntry* copy = find_entry(now, f)) entry_save_ticks(write_dir, *copy, entry_ticks(write_dir, e->id));
  if (folder) *folder = f;
  return true;
}

SaveAsSession& save_as_session() {
  static SaveAsSession s;
  return s;
}

void save_as_begin(const std::string& write_dir, const std::string& entry) {
  SaveAsSession& s = save_as_session();
  s = SaveAsSession{};
  s.write_dir = write_dir, s.entry = entry;
  const std::vector<CampaignEntry> all = campaign_entries(write_dir);
  const CampaignEntry* e = find_entry(all, entry);
  if (e && editor_owns_mod(write_dir, e->mod)) s.copy = e->mod;  // a copy of ours: saved into itself
  ai::net_save_into_mod() = s.copy;
}

void save_as_end() {
  box_close();
  save_as_session() = SaveAsSession{};
}

std::function<bool(std::string* name)>& save_as_ask_hook() {
  static std::function<bool(std::string* name)> h;
  return h;
}

EditorForm& save_as_form() { return g_box.form; }

bool editor_save_ready(std::function<void()> again) {
  SaveAsSession& s = save_as_session();
  if (s.entry.empty() || !s.copy.empty()) return true;  // outside an entry, or the copy is there
  if (save_as_ask_hook()) {  // checks: the answer at once
    std::string name = save_as_default_name(s.write_dir, s.entry);
    if (!save_as_ask_hook()(&name)) return editor_status("Nothing was saved."), false;
    return take_name(name);  // true: the command goes on in the same call
  }
  g_box.again = std::move(again);
  g_box.name = save_as_default_name(s.write_dir, s.entry);
  g_box.said = "The first save makes a copy of this campaign under a name of yours; the campaign itself is never changed.";
  g_box.form = EditorForm{};
  g_box.form.title = "Save as a new campaign";
  g_box.form.modal = true;
  FormField note;
  note.label = g_box.said;
  FormField name;
  name.label = "Name", name.kind = FormField::kText, name.text = &g_box.name;
  g_box.form.fields = {note, name};
  g_box.form.buttons = {{"Save",
                         [] {
                           std::function<void()> go = g_box.again;
                           if (!take_name(g_box.name)) {
                             if (!g_box.form.fields.empty()) g_box.form.fields[0].label = g_box.said;
                             return;  // the box stays, the reason on it, another name offered
                           }
                           box_close();
                           if (go) go();
                         }},
                        {"Cancel", [] {
                           box_close();
                           editor_status("Nothing was saved.");
                         }}};
  form_show(&g_box.form);
  return false;
}

}  // namespace mech3
