#include "mech3/game_editor_vehicles.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "gamez/znetwork/znet_object.h"
#include "mech3/game_editor_net.h"
#include "mech3/mech_config.h"
#include "mech3/mech_names.h"
#include "mech3/zai_target.h"
#include "mech3/zainet.h"
#include "mech3/zainet_inout_view.h"

namespace mech3 {

namespace {
VehicleEditorHooks g_default_hooks;
VehicleEditorHooks* g_hooks = &g_default_hooks;
VehicleEditor* g_editor = nullptr;  // [0x0060b310]
std::vector<uint32_t>* g_trace = nullptr;
std::vector<int32_t>* g_ids = nullptr;

// The main tab's pages in the order 0x004b8790 adds them (their templates'
// titles), and Attack's own tab's.
const char* const kPageTitles[] = {"p1", "p2", "Attack", "Retreat", "p5", "Fire", "Valve"};

void trace(uint32_t at) {
  if (g_trace) g_trace->push_back(at);
}

VehicleEditorItem* item(int32_t i) {
  VehicleEditor& e = *g_editor;
  return i >= 0 && i < static_cast<int32_t>(e.items.size()) ? &e.items[size_t(i)] : nullptr;
}

// A settings word exchanged in place (the exe passes its address).
template <class T, class F>
void word(ai::AiParams& p, int32_t off, F&& f) {
  T v = p.get<T>(off);
  f(v);
  p.put<T>(off, v);  // not reached when the field fails, as with the exe's
}

// 0x0058b160 then vt+0x28: the net numbered so, null when none.
ai::AiNet* net_numbered(int32_t id) {
  znet::Object* o = znet::find(id);
  return o ? static_cast<ai::AiNet*>(o)->as_net() : nullptr;
}

const ConfigList* configs_of_selected() {
  VehicleEditorItem* it = item(g_editor->selected);
  return it ? g_hooks->configs(it->vehicle) : nullptr;
}

// Mech3.exe 1.2 @ 0x004bc260: the team word rounded through its mask
// (0x004d71a0), and the Team radio buttons from the player's masks.
void team_radio() {
  VehicleEditor& e = *g_editor;
  uint32_t t = static_cast<uint32_t>(e.params.team()), mask = 1u << (t & 31);
  while ((t >>= 5) != 0) mask |= 1u << (t & 31);
  e.params.put<int32_t>(0x18, static_cast<int32_t>(ai::team_number_of(mask)));
  if (!g_hooks->has_player()) return;
  const uint32_t allies = g_hooks->player_allies(), enemies = g_hooks->player_enemies();
  e.team_side = (allies & mask) ? 0 : (enemies & mask) ? 1 : 2;
}

// Mech3.exe 1.2 @ 0x004bc300: the team a radio picks, the first of the
// player's allies (0), enemies (1) or neither (2); -1 when there is none.
void team_from_radio(int32_t which) {
  if (!g_hooks->has_player() || which < 0 || which > 2) return;
  const uint32_t allies = g_hooks->player_allies(), enemies = g_hooks->player_enemies();
  int32_t t = -1;
  for (int32_t b = 0; b < 32 && t < 0; ++b) {
    const uint32_t m = 1u << b;
    if (which == 0 ? (allies & m) != 0 : which == 1 ? (enemies & m) != 0 : !(allies & m) && !(enemies & m)) t = b;
  }
  g_editor->params.put<int32_t>(0x18, t);
}

// The lists' fills and gathers, on p2's combos: home net 1238 (+0x1f4),
// configuration 1273 (+0x234), chassis 1274 (+0x274).
DlgCombo& combo(int32_t id) { return g_editor->pages[kPageP2].at(id).combo; }

// Mech3.exe 1.2 @ 0x004bc480 with 0x004bc420: every net object, by its
// net's name, its id as the item's data.
void nets_fill() {
  DlgCombo& c = combo(1238);
  c.reset();
  for (znet::Object* o : znet::registry().list) {
    ai::AiNet* n = static_cast<ai::AiNet*>(o)->as_net();  // every znet object is a net
    c.set_data(c.add(n->name()), n->get_id());
  }
}

// Mech3.exe 1.2 @ 0x004bc860: "default" (-1) and the 19 chassis by name.
void chassis_fill() {
  DlgCombo& c = combo(1274);
  c.reset();
  c.set_data(c.add("default"), -1);
  for (int32_t i = 0; i < 19; ++i) c.set_data(c.add(mech_name(i)), i);  // 0x00484650
}

// Mech3.exe 1.2 @ 0x004bc640: the vehicle's mech's loadouts but the
// player's own (+0x1c), each by its index.
void configs_fill() {
  trace(0x004bc640);
  if (g_editor->selected < 0 || !item(g_editor->selected)) return;
  const ConfigList* l = configs_of_selected();
  DlgCombo& c = combo(1273);
  c.reset();
  if (!l) return;
  for (size_t i = 0; i < l->configs.size(); ++i) {
    const Config* k = l->configs[i];
    if (k && !k->player_made) c.set_data(c.add(k->name ? k->name : ""), static_cast<int32_t>(i));
  }
}

// Mech3.exe 1.2 @ 0x004bc790: the loadout +0x120 picks shown by its name,
// none when it is out of range or the player's own; nothing without a mech.
void config_show() {
  trace(0x004bc790);
  if (g_editor->selected < 0 || !item(g_editor->selected)) return;
  const ConfigList* l = configs_of_selected();
  if (!l) return;
  const uint32_t at = static_cast<uint32_t>(g_editor->params.get<int32_t>(0xbc));
  const Config* k = at < l->configs.size() ? l->configs[at] : nullptr;
  if (k && !k->player_made)
    combo(1273).select_string(k->name ? k->name : "");
  else
    combo(1273).set_cur(-1);
}

// Mech3.exe 1.2 @ 0x004bc750: the loadout picked into +0x120.
void config_gather() {
  trace(0x004bc750);
  const DlgCombo& c = combo(1273);
  if (c.cur != -1) g_editor->params.put<int32_t>(0xbc, c.data(c.cur));
}

// Mech3.exe 1.2 @ 0x004bc930: the chassis +0x128 names shown ("default" for
// -1). Past the 19, 0x00484650's "notmech" matches nothing: the list keeps
// what it showed.
void chassis_show() {
  trace(0x004bc930);
  const int32_t m = g_editor->params.get<int32_t>(0xc4);
  combo(1274).select_string(m != -1 ? mech_name(m) : "default");
}

// Mech3.exe 1.2 @ 0x004bc8f0: the chassis picked into +0x128.
void chassis_gather() {
  trace(0x004bc8f0);
  const DlgCombo& c = combo(1274);
  if (c.cur != -1) g_editor->params.put<int32_t>(0xc4, c.data(c.cur));
}

// Mech3.exe 1.2 @ 0x004bc570: Show's editor on the home net (+0x68) or
// freed, the boundary drawn or taken down, the home net shown in its list.
void home_net_show() {
  trace(0x004bc570);
  VehicleEditor& e = *g_editor;
  ai::AiNet* n = net_numbered(e.params.get<int32_t>(0x04));
  if (e.show)
    net_editor_show(e.shown, n);
  else
    net_editor_slot_free(e.shown);
  if (!e.show_inout)
    ai::inout_view_show(nullptr);  // 0x004c4a40(0)
  else if (ai::inout_view_empty())
    ai::inout_view_show("ionet");  // 0x005e88d4
  if (n)
    combo(1238).select_string(n->name());
  else
    combo(1238).set_cur(-1);
}

// Mech3.exe 1.2 @ 0x004bc4e0: the net picked into +0x68, and Show's editor
// moved to it (or freed).
void home_net_gather() {
  trace(0x004bc4e0);
  VehicleEditor& e = *g_editor;
  const DlgCombo& c = combo(1238);
  if (c.cur == -1) return;
  e.params.put<int32_t>(0x04, c.data(c.cur));
  if (e.show)
    net_editor_show(e.shown, net_numbered(e.params.get<int32_t>(0x04)));
  else
    net_editor_slot_free(e.shown);
}

// Mech3.exe 1.2 @ 0x004b8bc0: p1, the activation and the follow ranges.
// The editor's settings are at +0x64 (offsets below are the settings').
void p1_exchange(DataExchange& dx) {
  VehicleEditor& e = *g_editor;
  ai::AiParams& p = e.params;
  // DDX_Control 1229: the "Range to" list (kept as data).
  word<int32_t>(p, 0x88, [&](int32_t& v) { ddx_check(dx, 1231, v); });  // Always active
  word<float>(p, 0x1c, [&](float& v) { ddx_text(dx, 1209, v); });
  word<float>(p, 0x28, [&](float& v) { ddx_text(dx, 1212, v); });
  word<int32_t>(p, 0x3c, [&](int32_t& v) { ddx_check(dx, 1217, v); });  // Enable Initial
  word<float>(p, 0x20, [&](float& v) { ddx_text(dx, 1210, v); });
  word<float>(p, 0x2c, [&](float& v) { ddx_text(dx, 1214, v); });
  word<float>(p, 0x24, [&](float& v) { ddx_text(dx, 1211, v); });
  word<int32_t>(p, 0x54, [&](int32_t& v) { ddx_check(dx, 1251, v); });  // Disable
  ddx_check(dx, 1257, e.keep[4]);                                        // Enable Now (+0x984)
  DlgCombo& range_to = dx.dlg->at(1229).combo;  // "player", "nearest enemy"
  if (!dx.save) range_to.set_cur(p.get<int32_t>(0x8c) == 0 ? 1 : 0);
  if (dx.save) p.put<int32_t>(0x8c, range_to.cur == 0 ? 1 : 0);
}

// Mech3.exe 1.2 @ 0x004b8eb0: p2, the vehicle.
void p2_exchange(DataExchange& dx) {
  VehicleEditor& e = *g_editor;
  ai::AiParams& p = e.params;
  uint8_t& f130 = p.raw[0xcc];  // the editor's +0x130 and +0x131
  uint8_t& f131 = p.raw[0xcd];
  if (!dx.save) {
    e.p2_pilot = p.pilot ? p.pilot : "";
    e.p2_objective = (f130 & 0x10) >> 4;
    e.p2_shutdown = (f130 & 0x20) >> 5;
  }
  // DDX_Control 1274, 1273, 1238: the chassis, configuration and home net lists.
  ddx_radio(dx, 1197, e.team_side);
  word<uint32_t>(p, 0x18, [&](uint32_t& v) { ddx_text(dx, 1111, v); });  // the team
  word<float>(p, 0x08, [&](float& v) { ddx_text(dx, 1200, v); });        // x
  word<float>(p, 0x0c, [&](float& v) { ddx_text(dx, 1201, v); });        // y
  word<float>(p, 0x14, [&](float& v) { ddx_text(dx, 1203, v); });        // yaw
  word<float>(p, 0x10, [&](float& v) { ddx_text(dx, 1202, v); });        // z
  word<int32_t>(p, 0x04, [&](int32_t& v) { ddx_text(dx, 1117, v); });    // the home net's id
  ddx_text(dx, 1243, e.p2_pilot);
  ddv_max_chars(dx, e.p2_pilot, 0x20);
  ddx_check(dx, 1249, e.show);
  ddx_check(dx, 1275, e.p2_objective);
  ddx_check(dx, 1276, e.p2_shutdown);
  int32_t v = f130 >> 7;
  ddx_check(dx, 1279, v);  // can configure
  f130 = static_cast<uint8_t>((v != 0) << 7 | (f130 & 0x7f));
  v = e.show_inout;
  ddx_check(dx, 1282, v);
  e.show_inout = v != 0;
  v = (f131 & 2) == 2 ? 2 : f131 & 1;  // Lancemate, three-state: bit 1 "as it was"
  ddx_check(dx, 1283, v);
  if (v == 2) {
    f131 = static_cast<uint8_t>((f131 | 2) & 0xfe);
  } else {
    f131 = static_cast<uint8_t>(f131 & 0xfd);
    f131 = static_cast<uint8_t>((f131 & 0xfe) | (v != 0));
  }
  v = p.get<int32_t>(0xc8);  // the skin
  ddx_text(dx, 1284, v);
  ddv_min_max_int(dx, v, 0, 100);
  p.put<int32_t>(0xc8, v);
  if (!dx.save) {
    config_show();
    home_net_show();
    chassis_show();
  } else {
    chassis_gather();
    config_gather();
    home_net_gather();
    std::free(p.pilot);  // 0x0049d250
    p.pilot = e.p2_pilot.empty() ? nullptr : strdup(e.p2_pilot.c_str());
    f130 = static_cast<uint8_t>((f130 & 0xef) | (e.p2_objective == 1) << 4);
    f130 = static_cast<uint8_t>((f130 & 0xdf) | (e.p2_shutdown == 1) << 5);
  }
}

void (*const kExchange[kPages])(DataExchange&) = {p1_exchange, p2_exchange};

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
// "as the brain has it" words reset, the units the file's (0x004d9b20); the
// pages' lists filled.
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
  net_editor_slot_free(e.shown);  // 0x004044c0(0)
  home_net_show();
  configs_fill();
  config_show();
  chassis_show();
}

// Mech3.exe 1.2 @ 0x004b8dc0: the dialog gathered, vehicle i stored, shown again.
void store_shown(int32_t i) {
  vehicle_editor_update(true);
  store(i);
  vehicle_editor_update(false);
}

// Mech3.exe 1.2 @ 0x004b8df0
void load_shown(int32_t i) {
  load(i);
  vehicle_editor_update(false);
}

void show_message() {
  VehicleEditor& e = *g_editor;
  const DdxMessage& m = ddx_message();
  if (m.text.empty()) return;
  e.message = EditorForm{};
  e.message.title = "Mech AI##message";
  e.message.modal = true;
  e.message.fields = {{m.text, FormField::kLabel}};
  e.message.buttons = {{"OK", [] {
                          if (g_editor) form_hide(&g_editor->message);
                        }}};
  form_show(&e.message);
}

// The form's fields on a page's controls.
FormField edit(const char* label, int32_t page, int32_t id, bool read_only = false) {
  DlgControl& c = g_editor->pages[page].at(id);
  FormField f{label, FormField::kText};
  f.text = &c.text, f.limit = &c.limit, f.read_only = read_only, f.page = page;
  return f;
}
FormField check(const char* label, int32_t page, int32_t id, int32_t max = 1) {
  DlgControl& c = g_editor->pages[page].at(id);
  c.max_check = max;
  FormField f{label, FormField::kCheck};
  f.i = &c.check, f.max = max, f.page = page;
  return f;
}
FormField list(const char* label, int32_t page, int32_t id, void (*on)()) {
  DlgControl& c = g_editor->pages[page].at(id);
  FormField f{label, FormField::kChoice};
  f.combo = &c.combo, f.i = &c.combo.cur, f.page = page;
  if (on) f.changed = [on] { if (g_editor) on(); };
  return f;
}
FormField button(const char* label, int32_t page, void (*on)()) {
  FormField f{label, FormField::kButton};
  f.page = page;
  f.changed = [on] { if (g_editor) on(); };
  return f;
}

// Mech3.exe 1.2 @ 0x004b8790's page making: each page's controls as its
// template has them (166, 167), and the lists filled once.
void make_pages() {
  VehicleEditor& e = *g_editor;
  e.main.at(1218).max_check = 1;
  Dialog& p1 = e.pages[kPageP1];
  for (int32_t id : {1231, 1217, 1251}) p1.at(id).max_check = 1;
  p1.at(1257).max_check = 2;
  DlgCombo& range_to = p1.at(1229).combo;
  range_to.add("player");          // 0x005e1718
  range_to.add("nearest enemy");   // 0x005e88dc
  Dialog& p2 = e.pages[kPageP2];
  for (int32_t id : {1273, 1274, 1238}) p2.at(id).combo.sorted = true;  // CBS_SORT
  for (int32_t id : {1275, 1279, 1276, 1282}) p2.at(id).max_check = 1;
  for (int32_t id : {1249, 1283}) p2.at(id).max_check = 2;
  p2.at(1197).radios = 3;  // Friend, Foe, Other
  nets_fill();
  chassis_fill();
}

void build() {
  VehicleEditor& e = *g_editor;
  e.form = EditorForm{};
  e.form.title = "Mech AI";
  FormField vehicles{"##vehicle", FormField::kChoice};
  for (const VehicleEditorItem& v : e.items) vehicles.choices.push_back(v.name);
  vehicles.i = &e.selected;
  vehicles.changed = [] { vehicle_editor_select(g_editor ? g_editor->selected : 0); };
  FormField tabs{"##pages", FormField::kTabs};
  tabs.choices.assign(std::begin(kPageTitles), std::end(kPageTitles));
  tabs.i = &e.tab;
  tabs.changed = [] {
    if (g_editor) vehicle_editor_tab(g_editor->tab);
  };
  FormField pause{"Pause", FormField::kCheck};
  pause.i = &e.main.at(1218).check;
  pause.changed = vehicle_editor_pause;
  e.form.fields = {vehicles, tabs};
  // p1 (template 167).
  e.form.fields.push_back(check("Always active", kPageP1, 1231));
  e.form.fields.push_back(check("Disable", kPageP1, 1251));
  e.form.fields.push_back(edit("Activation range", kPageP1, 1209));
  e.form.fields.push_back(list("Range to", kPageP1, 1229, nullptr));
  e.form.fields.push_back(check("Enable Initial", kPageP1, 1217));
  e.form.fields.push_back(check("Enable Now", kPageP1, 1257, 2));
  e.form.fields.push_back(edit("Leave Net Range to Target", kPageP1, 1210));
  e.form.fields.push_back(edit("Return to Net Range from Net", kPageP1, 1211));
  e.form.fields.push_back(edit("Approach Range to Target", kPageP1, 1212));
  e.form.fields.push_back(edit("Minimum Range to Target", kPageP1, 1214));
  // p2 (template 166).
  e.form.fields.push_back(list("Configuration", kPageP2, 1273, vehicle_editor_config));
  e.form.fields.push_back(list("Default Chassis", kPageP2, 1274, vehicle_editor_chassis));
  e.form.fields.push_back(edit("Skin", kPageP2, 1284));
  e.form.fields.push_back(edit("Pilot Name", kPageP2, 1243));
  e.form.fields.push_back(check("Objective Conditional", kPageP2, 1275));
  e.form.fields.push_back(check("can configure", kPageP2, 1279));
  e.form.fields.push_back(check("Shutdown on start", kPageP2, 1276));
  e.form.fields.push_back(list("Home Net", kPageP2, 1238, vehicle_editor_home_net));
  e.form.fields.push_back(button("Net Edit", kPageP2, vehicle_editor_net_edit));
  e.form.fields.push_back(edit("Net ID", kPageP2, 1117, true));
  e.form.fields.push_back(check("Show", kPageP2, 1249, 2));
  e.form.fields.push_back(check("Show InOut", kPageP2, 1282));
  e.form.fields.push_back(edit("x", kPageP2, 1200));
  e.form.fields.push_back(edit("y", kPageP2, 1201));
  e.form.fields.push_back(edit("z", kPageP2, 1202));
  e.form.fields.push_back(edit("yaw", kPageP2, 1203));
  e.form.fields.push_back(button("Send home", kPageP2, vehicle_editor_send_home));
  FormField team{"Team", FormField::kRadio};
  team.choices = {"Friend", "Foe", "Other"};
  team.i = &e.pages[kPageP2].at(1197).check, team.page = kPageP2;
  team.changed = vehicle_editor_team_radio;
  e.form.fields.push_back(team);
  FormField team_id = edit("Team number", kPageP2, 1111, true);
  team_id.left = vehicle_editor_team_left;
  e.form.fields.push_back(team_id);
  e.form.fields.push_back(check("Lancemate", kPageP2, 1283, 2));
  e.form.fields.push_back(pause);
  e.form.buttons = {{"OK", vehicle_editor_ok}, {"Apply", vehicle_editor_apply}, {"Refresh", vehicle_editor_refresh}};
}

int32_t shown_selection = 0;  // the selection loaded last (the combo's change stores the old one first)
}  // namespace

void set_vehicle_editor_hooks(VehicleEditorHooks* h) { g_hooks = h ? h : &g_default_hooks; }
VehicleEditorHooks& vehicle_editor_hooks() { return *g_hooks; }
VehicleEditor*& vehicle_editor() { return g_editor; }
std::vector<uint32_t>*& vehicle_editor_trace() { return g_trace; }
std::vector<int32_t>*& vehicle_editor_ids() { return g_ids; }

bool vehicle_page_update(int32_t page, bool save) {
  if (!g_editor || page < 0 || page >= kPages || !kExchange[page]) return true;
  const bool ok = update_data(g_editor->pages[page], save, kExchange[page], g_ids);
  if (!ok) show_message();
  return ok;
}

// Mech3.exe 1.2 @ 0x004b8680: the dialog's own exchange (the main tab's
// page shown first, then the vehicle list and the tab as controls, Pause).
bool vehicle_editor_update(bool save) {
  if (!g_editor) return true;
  VehicleEditor& e = *g_editor;
  if (!save) team_radio();
  if (e.page >= 0) vehicle_page_update(e.page, save);
  return update_data(e.main, save, [&](DataExchange& dx) { ddx_check(dx, 1218, e.pause); });
}

// Mech3.exe 1.2 @ 0x004b80f0 (the editor, Show's slot on the frame's list)
// and OnInitDialog's pages and lists.
VehicleEditor* vehicle_editor_create() {
  g_editor = new VehicleEditor;
  net_editor_slot_add(&g_editor->shown);  // 0x004043c0
  make_pages();
  return g_editor;
}

// Mech3.exe 1.2 @ 0x004b86e0 with OnInitDialog 0x004b8790: made once, the
// pages and lists made (the vehicles, 0x004bb290), the first vehicle loaded
// and the first page of each tab shown (0x004b7c80).
void edit_vehicles(int32_t) {
  if (g_editor) {
    form_show(&g_editor->form);
    return;
  }
  vehicle_editor_create();
  g_editor->items = g_hooks->vehicles();
  g_editor->selected = 0;
  shown_selection = 0;
  load_shown(0);
  build();
  vehicle_editor_tab(kPageP1);
  g_editor->show_inout = !ai::inout_view_empty();
  if (g_editor->show_inout) ai::inout_view_show("ionet");
  form_show(&g_editor->form);
}

// Mech3.exe 1.2 @ 0x004b7c80: the page leaving gathered, the one coming shown.
void vehicle_editor_tab(int32_t page) {
  if (!g_editor || page == g_editor->page) return;
  if (g_editor->page >= 0) vehicle_page_update(g_editor->page, true);
  g_editor->page = page;
  g_editor->tab = page;
  vehicle_page_update(page, false);
}

// Mech3.exe 1.2 @ 0x004b8d70
void vehicle_editor_select(int32_t i) {
  if (!g_editor) return;
  store_shown(shown_selection);  // 0x004b8dc0 on the old selection
  g_editor->selected = i;
  shown_selection = i;
  load_shown(i);  // 0x004b8df0
}

// Mech3.exe 1.2 @ 0x004b8d40
void vehicle_editor_apply() {
  if (!g_editor) return;
  store_shown(g_editor->selected);
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
  nets_fill();
  chassis_fill();
  int32_t at = 0;
  for (size_t k = 0; k < g_editor->items.size(); ++k)
    if (g_editor->items[k].name == name) {  // CB_FINDSTRINGEXACT
      at = static_cast<int32_t>(k);
      break;
    }
  g_editor->selected = shown_selection = at;
  load_shown(at);
  const int32_t page = g_editor->page;
  build();
  g_editor->tab = page;
  form_show(&g_editor->form);
}

// Mech3.exe 1.2 @ 0x004bc3f0
void vehicle_editor_pause() {
  if (!g_editor) return;
  vehicle_editor_update(true);
  g_hooks->pause(g_editor->pause == 1);
}

// Mech3.exe 1.2 @ 0x004b9270: the team taken and rounded, shown again.
void vehicle_editor_team_left() {
  if (!g_editor) return;
  vehicle_page_update(kPageP2, true);
  team_radio();
  vehicle_page_update(kPageP2, false);
}

// Mech3.exe 1.2 @ 0x004b9290: another radio picked, its team taken.
void vehicle_editor_team_radio() {
  if (!g_editor) return;
  const int32_t was = g_editor->team_side;
  vehicle_page_update(kPageP2, true);
  if (was == g_editor->team_side) return;
  team_from_radio(g_editor->team_side);
  vehicle_page_update(kPageP2, false);
}

// Mech3.exe 1.2 @ 0x004b9f30
void vehicle_editor_home_net() {
  if (!g_editor) return;
  vehicle_page_update(kPageP2, true);
  home_net_gather();
  vehicle_page_update(kPageP2, false);
}

// Mech3.exe 1.2 @ 0x004b9f50: the dialog gathered, window 181 shown with no file.
void vehicle_editor_net_edit() {
  if (!g_editor) return;
  vehicle_editor_update(true);
  net_window_show();
}

// Mech3.exe 1.2 @ 0x004b9f70 -> 0x004bc960: the vehicle put at the place
// shown at the last exchange (no gather first) and on its home net.
void vehicle_editor_send_home() {
  if (!g_editor) return;
  VehicleEditor& e = *g_editor;
  ai::AiNet* n = net_numbered(e.params.get<int32_t>(0x04));
  VehicleEditorItem* it = item(e.selected);
  if (!n || !it) return;
  const float at[3] = {e.params.get<float>(0x08), e.params.get<float>(0x0c), e.params.get<float>(0x10)};
  g_hooks->send_home(it->vehicle, n, at, e.params.get<float>(0x14));
}

// Mech3.exe 1.2 @ 0x004b9fb0
void vehicle_editor_config() {
  if (!g_editor) return;
  vehicle_page_update(kPageP2, true);
  config_gather();
  vehicle_page_update(kPageP2, false);
}

// Mech3.exe 1.2 @ 0x004b9fd0
void vehicle_editor_chassis() {
  if (!g_editor) return;
  vehicle_page_update(kPageP2, true);
  chassis_gather();
  vehicle_page_update(kPageP2, false);
}

// DestroyWindow -> 0x004b8b70: the object deleted and [0x0060b310] cleared
// (its dtor takes Show's slot off the frame's list, 0x00404420).
void vehicle_editor_close() {
  if (!g_editor) return;
  form_hide(&g_editor->form);
  form_hide(&g_editor->message);
  net_editor_slot_remove(&g_editor->shown);
  delete g_editor;
  g_editor = nullptr;
}

}  // namespace mech3
