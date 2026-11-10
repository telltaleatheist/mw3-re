#include "mech3/game_editor_vehicles.h"

#include <algorithm>

#include "mech3/zai_target.h"

namespace mech3 {

namespace {
VehicleEditorHooks g_default_hooks;
VehicleEditorHooks* g_hooks = &g_default_hooks;
VehicleEditor* g_editor = nullptr;  // [0x0060b310]

// The main tab's pages in the order 0x004b8790 adds them (their templates'
// titles), and Attack's own tab's.
const char* const kPages[] = {"p1", "p2", "Attack", "Retreat", "p5", "Fire", "Valve"};

VehicleEditorItem* item(int32_t i) {
  VehicleEditor& e = *g_editor;
  return i >= 0 && i < static_cast<int32_t>(e.items.size()) ? &e.items[size_t(i)] : nullptr;
}

// Mech3.exe 1.2 @ 0x004bc260: the team word rounded through its mask
// (0x004d71a0), and the Team radio buttons from the player's masks.
void team_radio() {
  VehicleEditor& e = *g_editor;
  uint32_t t = static_cast<uint32_t>(e.params.team()), mask = 1u << (t & 31);
  while ((t >>= 5) != 0) mask |= 1u << (t & 31);
  e.params.put<int32_t>(0x18, static_cast<int32_t>(ai::team_number_of(mask)));
  const uint32_t allies = g_hooks->player_allies(), enemies = g_hooks->player_enemies();
  if (!allies && !enemies) return;  // no player: left as it was
  e.team_side = (allies & mask) ? 0 : (enemies & mask) ? 1 : 2;
}

// Mech3.exe 1.2 @ 0x004bb7e0: the settings shown (gathered first) put on
// vehicle i. The record built from them (0x004d9940, then 0x004d9b70 back
// to play units) goes into aiv.zrd's cache when it differs from the one read
// for the vehicle (0x004e0680), and on the brain when it differs from the
// brain's; the five "as the brain has it" words take the brain's values then.
void store(int32_t i) {
  VehicleEditor& e = *g_editor;
  VehicleEditorItem* it = item(i);
  ai::AiParams* brain = it ? g_hooks->brain_params(it->vehicle) : nullptr;
  if (!brain) return;
  ai::AivRecord rec;
  static_cast<ai::AiParams&>(rec) = e.params;
  rec.finish();
  ai::AivRecord* kept = e.made.read_named(it->name.c_str());
  const auto same = [&](const ai::AiParams& o) {
    return ai::same_name(o.pilot, rec.pilot) && ai::same_name(o.sound_tag, rec.sound_tag) && o.same_vlist(rec) &&
           rec.same_numbers(o);
  };
  if (!kept || !same(*kept)) {
    static_cast<ai::AiParams&>(e.made) = rec;  // 0x004df710 on the record read, then 0x004e06e0
    ai::aiv_file().edit(it->name.c_str(), e.made);
  }
  // The exe resets each word to 2 after reading the next one, so +0x980's is
  // never reset (docs/editor.md, The vehicle editor).
  const int32_t k0 = e.keep[4], k1 = e.keep[0], k2 = e.keep[2], k3 = e.keep[1], k4 = e.keep[3];
  rec.put<int32_t>(0x3c, k0 == 2 ? brain->get<int32_t>(0x3c) : k0);
  e.keep[4] = 2;
  rec.put<int32_t>(0x98, k1 == 2 ? brain->get<int32_t>(0x98) : k1);
  e.keep[0] = 2;
  rec.put<int32_t>(0xa0, k2 == 2 ? brain->get<int32_t>(0xa0) : k2);
  e.keep[2] = 2;
  rec.put<int32_t>(0x9c, k3 == 2 ? brain->get<int32_t>(0x9c) : k3);
  e.keep[1] = 2;
  rec.set_bit(0xcc, 2, k4 == 2 ? (brain->raw[0xcc] & 4) != 0 : k4 != 0);
  e.keep[2] = 2;
  if (!same(*brain)) {
    *brain = rec;  // 0x004d99e0
    ai::AiTarget t;
    t.set_team(static_cast<uint32_t>(rec.team()));
    g_hooks->set_masks(it->vehicle, t.team, t.allies, t.enemies);  // +0xc0..+0xc8, [0x0060b56c] cleared
  }
}

// Mech3.exe 1.2 @ 0x004bc140: vehicle i's settings shown: its record read
// (0x004e0680), or one made from its brain and kept in the cache; the five
// "as the brain has it" words reset, the units the file's (0x004d9b20).
void load(int32_t i) {
  VehicleEditor& e = *g_editor;
  VehicleEditorItem* it = item(i);
  ai::AiParams* brain = it ? g_hooks->brain_params(it->vehicle) : nullptr;
  if (!brain) return;
  const ai::AivRecord* kept = e.made.read_named(it->name.c_str());
  if (!kept) {
    e.made.reset();  // 0x004df6f0
    e.made.mech3_set = 0;
    static_cast<ai::AiParams&>(e.made) = *brain;  // 0x004df710
    ai::aiv_file().edit(it->name.c_str(), e.made);
  }
  e.params = e.made;
  std::fill(std::begin(e.keep), std::end(e.keep), 2);
  e.params.to_file_units();
  team_radio();
  // The pages' own fills (0x004044c0(0), 0x004bc570, 0x004bc640, 0x004bc790,
  // 0x004bc930) come with the pages (S7d).
}

void build() {
  VehicleEditor& e = *g_editor;
  e.form = EditorForm{};
  e.form.title = "Mech AI";
  FormField list{"##vehicle", FormField::kChoice};
  for (const VehicleEditorItem& v : e.items) list.choices.push_back(v.name);
  list.i = &e.selected;
  list.changed = [] { vehicle_editor_select(g_editor ? g_editor->selected : 0); };
  FormField tabs{"##pages", FormField::kTabs};
  tabs.choices.assign(std::begin(kPages), std::end(kPages));
  tabs.i = &e.tab;
  FormField pause{"Pause", FormField::kFlag};
  e.pause_box = e.pause ? 1 : 0;
  pause.flag = &e.pause_box;
  pause.changed = [] {
    if (g_editor) g_editor->pause = g_editor->pause_box, vehicle_editor_pause();
  };
  e.form.fields = {list, tabs, pause};
  e.form.buttons = {{"OK", vehicle_editor_ok}, {"Apply", vehicle_editor_apply}, {"Refresh", vehicle_editor_refresh}};
}

int32_t shown_selection = 0;  // the selection loaded last (the combo's change stores the old one first)
}  // namespace

void set_vehicle_editor_hooks(VehicleEditorHooks* h) { g_hooks = h ? h : &g_default_hooks; }
VehicleEditorHooks& vehicle_editor_hooks() { return *g_hooks; }
VehicleEditor*& vehicle_editor() { return g_editor; }

// Mech3.exe 1.2 @ 0x004b86e0 with OnInitDialog 0x004b8790: made once, its
// lists filled (the vehicles, 0x004bb290), the first vehicle loaded.
void edit_vehicles(int32_t) {
  if (g_editor) {
    form_show(&g_editor->form);
    return;
  }
  g_editor = new VehicleEditor;
  g_editor->items = g_hooks->vehicles();
  g_editor->selected = 0;
  shown_selection = 0;
  load(0);
  build();
  form_show(&g_editor->form);
}

// Mech3.exe 1.2 @ 0x004b8d70
void vehicle_editor_select(int32_t i) {
  if (!g_editor) return;
  store(shown_selection);  // 0x004b8dc0 on the old selection
  g_editor->selected = i;
  shown_selection = i;
  load(i);  // 0x004b8df0
}

// Mech3.exe 1.2 @ 0x004b8d40
void vehicle_editor_apply() {
  if (!g_editor) return;
  store(g_editor->selected);
}

// Mech3.exe 1.2 @ 0x004b8d20
void vehicle_editor_ok() {
  vehicle_editor_apply();
  vehicle_editor_close();
}

// Mech3.exe 1.2 @ 0x004b9e40: the lists filled again and the vehicle shown
// found again by its name, else the first.
void vehicle_editor_refresh() {
  if (!g_editor) return;
  VehicleEditorItem* it = item(g_editor->selected);
  const std::string name = it ? it->name : std::string();
  g_editor->items = g_hooks->vehicles();
  int32_t at = 0;
  for (size_t k = 0; k < g_editor->items.size(); ++k)
    if (g_editor->items[k].name == name) {
      at = static_cast<int32_t>(k);
      break;
    }
  g_editor->selected = shown_selection = at;
  load(at);
  build();
  form_show(&g_editor->form);
}

// Mech3.exe 1.2 @ 0x004bc3f0
void vehicle_editor_pause() {
  if (g_editor) g_hooks->pause(g_editor->pause == 1);
}

// DestroyWindow -> 0x004b8b70: the object deleted and [0x0060b310] cleared.
void vehicle_editor_close() {
  if (!g_editor) return;
  form_hide(&g_editor->form);
  delete g_editor;
  g_editor = nullptr;
}

}  // namespace mech3
