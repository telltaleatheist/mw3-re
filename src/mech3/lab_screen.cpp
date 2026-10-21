// The mech lab's screen (docs/mechlab.md section 2), step 2: the screen,
// its own controls, the page and slot switches. Original file unknown
// (0x00448830..0x004561ff, between mech_data.cpp and mech_map.cpp).
#include "mech3/lab_screen.h"

#include <cstdarg>

#include "gamez/zreader/zreader.h"
#include "gamez/ztwod/twod_host.h"
#include "mech3/app_states.h"

namespace mech3 {

namespace {
int32_t g_none = -1;  // the tabs' and mech buttons' id until bound
}  // namespace

void LabLater::call(uint32_t slot, std::initializer_list<Arg> args) const {
  char key[24];
  std::snprintf(key, sizeof key, "lab.vt+0x%x", slot);
  const Arg* a = args.begin();
  switch (args.size()) {
    case 0: e->unported(key, {at}); break;
    case 1: e->unported(key, {at, a[0]}); break;
    default: e->unported(key, {at, a[0], a[1]}); break;
  }
}

void LabLater::call_fn(const char* key, std::initializer_list<Arg> args) const {
  const Arg* a = args.begin();
  if (args.size() == 0) e->unported(key, {at});
  else e->unported(key, {at, a[0]});
}

// ---------------------------------------------------------------------------
// The panels

// Mech3.exe 1.2 @ 0x0044b070
LabPanel::~LabPanel() {
  set_image(static_cast<zimg::Texture*>(nullptr));
  if (single) ztwod::host().image_release(single);
  if (twice) ztwod::host().image_release(twice);
  single = twice = nullptr;
}

// Mech3.exe 1.2 @ 0x0044b100
bool LabPanel::read(zrdr::Node* node, ztwod::Dialog* dialog) {
  const bool ok = ztwod::Image::read(node, dialog);
  if (!ok) return ok;
  if (const char* s = zrdr::get_string(node, "double")) twice = ztwod::host().image_get(s);
  if (const char* s = zrdr::get_string(node, "single")) single = ztwod::host().image_get(s);
  if (const char* s = zrdr::get_string(node, "double2")) twice2 = ztwod::host().image_get(s);
  return ok;
}

// Mech3.exe 1.2 @ 0x0044b180
void LabPanel::show_look(int32_t look) {
  if (look == 1) set_image(single);
  else if (look == 2) set_image(twice);
  else if (look == 3) set_image(twice2);
}

// ---------------------------------------------------------------------------
// The edit bar

// Mech3.exe 1.2 @ 0x0044a710
void LabEditBar::set_pos(int32_t px, int32_t py) {
  ztwod::Image::set_pos(label_x + px, label_y + py);
  if (!labels.empty()) labels[0]->set_pos(px, py);
}

// ---------------------------------------------------------------------------
// The pages' controls: their own steps

bool LabLoadoutList::read_control(zrdr::Node*, ztwod::Dialog*) {
  call(0x84);
  return true;
}
void LabLoadoutList::after_bind() { call(0x88); }
void LabLoadoutList::show_for(float seconds) { call(0x60, {zgame::bits(seconds)}); }
void LabLoadoutList::hide() { call(0x64); }

// Mech3.exe 1.2 @ 0x00449180
LabChassisList::~LabChassisList() {
  for (FileRow* r : rows) {
    if (r->attached) {
      delete r->attached;
      r->attached = nullptr;
    }
  }
}
// Mech3.exe 1.2 @ 0x00449220
void LabChassisList::set_store(Handle s) {
  selected = 0;
  x218 = 0;
  store = s;
  refill();
}
bool LabChassisList::read_control(zrdr::Node*, ztwod::Dialog*) {
  call(0x84);
  return true;
}
void LabChassisList::after_bind() { call(0x88); }
void LabChassisList::show_for(float seconds) { call(0x60, {zgame::bits(seconds)}); }
void LabChassisList::hide() { call(0x64); }

bool LabConfigList::read_control(zrdr::Node*, ztwod::Dialog*) {
  call(0x84);
  return true;
}
void LabConfigList::after_bind() { call(0x88); }
void LabConfigList::show_for(float seconds) { call(0x60, {zgame::bits(seconds)}); }
void LabConfigList::hide() { call(0x64); }

bool LabItemList::read_control(zrdr::Node*, ztwod::Dialog*) {
  call(0x84);
  return true;
}
void LabItemList::after_bind() { call(0x88); }
void LabItemList::show_for(float seconds) { call(0x60, {zgame::bits(seconds)}); }
void LabItemList::hide() { call(0x64); }

bool LabWeaponList::read_control(zrdr::Node*, ztwod::Dialog*) {
  call(0x84);
  return true;
}
void LabWeaponList::after_bind() { call(0x88); }
void LabWeaponList::show_for(float seconds) { call(0x60, {zgame::bits(seconds)}); }
void LabWeaponList::hide() { call(0x64); }

// Mech3.exe 1.2 @ 0x004569c0 (the panel), the buttons, 0x0044f040(7) (its rows)
LabEngineTable::LabEngineTable(Engine& e)
    : faster(e, 0x13), b2e0(e, 0x14), b438(e, 0x15), b590(e, 0x16) {
  for (int i = 0; i < 7; ++i) rows.push_back(new LabRow);
}
// Mech3.exe 1.2 @ 0x0044ef90
LabEngineTable::Rows::~Rows() {
  for (LabRow*& r : *this) {
    delete r;
    r = nullptr;
  }
}
bool LabEngineTable::read_control(zrdr::Node*, ztwod::Dialog*) {
  call(0x84);
  return true;
}
void LabEngineTable::after_bind() { call(0x88); }
void LabEngineTable::show_for(float seconds) { call(0x60, {zgame::bits(seconds)}); }
void LabEngineTable::hide() { call(0x64); }

// ---------------------------------------------------------------------------
// MESSAGE_AREA

// Mech3.exe 1.2 @ 0x0044e700: SOUND, played with each text.
bool LabMessage::read(zrdr::Node* node, ztwod::Dialog* dialog) {
  x4b0 = static_cast<uint32_t>(ztwod::host().sound_find(zrdr::get_string(node, "SOUND")));
  x4b4 = 1.0f;
  return ztwod::FontText::read(node, dialog);
}

// Mech3.exe 1.2 @ 0x0044e740
void LabMessage::set_text(const char* format, ...) {
  va_list args;
  va_start(args, format);
  set_text_v(format, args);
  va_end(args);
  if (x4b0) ztwod::host().sound_play(static_cast<int32_t>(x4b0), x4b4);
}

// ---------------------------------------------------------------------------
// The screen

// Mech3.exe 1.2 @ 0x0044b920
LabScreen::LabScreen(Engine& e, int32_t slots)
    : MenuScreen(e, 0),
      loadouts(e),
      tabs(e, g_none),
      mechs(e),
      chassis(e),
      configs(e),
      config_edit(e),
      skins(e),
      sort_name(e, 0x1f),
      sort_mass(e, 0x20),
      unassigned(e),
      assigned(e),
      ungrouped(e),
      grouped(e),
      engine_table(e) {
  const struct {
    LabLater* l;
    uint32_t at;
  } later[] = {{&loadouts, 0x11508}, {&chassis, 0x16e74}, {&configs, 0x17548}, {&unassigned, 0x19290},
               {&assigned, 0x1a13c}, {&ungrouped, 0x1a36c}, {&grouped, 0x1a570}, {&engine_table, 0x1a8c8}};
  for (const auto& x : later) x.l->e = &e, x.l->at = x.at;
  chassis_data = e.construct(0x457600, {});  // new(0x188)
  mech_data = e.construct(0x456cd0, {});     // new(0x5ac)
  armor_titles = e.construct(0x44fad0, {});  // new(0x31e0), made inline
  if (load("dlgMechLabII.zrd", "LABMENU", true)) {
    bind_primitive(&temp_background, "TEMP_BACKGROUND");
    bind_primitive(&panel_left, "PANEL_LEFT");
    bind_primitive(&panel_right, "PANEL_RIGHT");
    bind_primitive(&panel_center, "PANEL_CENTER");
    bind_primitive(&message, "MESSAGE_AREA");
    bind_button(&current_chassis, "CURRENT_CHASSIS");
    bind_primitive(&icon_glow, "ICON_GLOW");
    static const char* const kTabs[] = {"ICON_CHASSIS",    "ICON_WEAPONS",     "ICON_EQUIPMENT", "ICON_CRITICALS",
                                        "ICON_WEAPONGROUP", "ICON_MISSIONDATA", "ICON_ACCEPT"};
    for (int i = 0; i < 7; ++i) {
      tabs[i].id = i;
      bind_button(&tabs[i], kTabs[i]);
    }
    bind_primitive(&pilot_glow, "PILOT_GLOW");
    static const char* const kMechs[] = {"ICON_MECH1", "ICON_MECH2", "ICON_MECH3", "ICON_MECH4"};
    for (int i = 0; i < 4; ++i) {
      mechs[i].id = 8 + i;
      bind_button(&mechs[i], kMechs[i]);
    }
    // The parts by their number (0..8 the mech's parts) and the order bound.
    part_head.part = 6;
    bind_button(&part_head, "PART_HEAD");
    static const struct {
      int idx, part;
      const char* name;
    } kParts[] = {{0, 1, "PART_TORSO"}, {1, 7, "PART_LTORSO"}, {2, 8, "PART_RTORSO"}, {5, 4, "PART_LLEG"},
                  {6, 5, "PART_RLEG"},  {3, 2, "PART_LARM"},   {4, 3, "PART_RARM"}};
    for (const auto& p : kParts) {
      parts[p.idx].part = p.part;
      bind_button(&parts[p.idx], p.name);
    }
    bind_button(&chassis, "CHASSIS_LIST");
    bind_button(&configs, "CONFIG_LIST");
    bind_button(&config_edit, "CONFIG_EDIT");
    bind_button(&engine_table, "ENGINE_TABLE");
    bind_button(&ungrouped, "WEAPONS_UNGROUPED");
    bind_button(&grouped, "WEAPONS_GROUPED");
    bind_button(&sort_mass, "SORT_CHASSISMASS");
    bind_button(&sort_name, "SORT_CHASSISNAME");
    bind_button(&skins, "SKIN_LIST");
    bind_button(&unassigned, "UNASSIGNED_LIST");
    bind_button(&assigned, "ASSIGNED_LIST");
    bind_later(armor_titles, "ARMOR_TITLES");
    bind_later(mech_data, "MECH_DATA");
    bind_later(chassis_data, "CHASSIS_DATA");
    bind_button(&loadouts, "LOADOUT_LIST");
    unload();
  }
  set_flags_all(0);
  set_active(1);
  config_edit.hide();
  engine_table.hide();
  for (int i = 0; i < 7; ++i) show_page(i, false);
  tabs[5].enabled = 0;  // ICON_MISSIONDATA
  tabs[5].refresh();
  for (int i = slots < 4 ? slots : 4; i < 4; ++i) mechs[i].hide();
  message.set_text(e.message(0xdcc));
}

// Mech3.exe 1.2 @ 0x0044c600: the panels made with new deleted first, the
// members after (in the original's order, the reverse of their making).
LabScreen::~LabScreen() {
  Engine& e = engine();
  if (chassis_data) e.vcall(chassis_data, 0x00, {1});
  if (mech_data) e.vcall(mech_data, 0x00, {1});
  if (armor_titles) e.vcall(armor_titles, 0x00, {1});
}

// 0x0059f8b0 on a panel the port does not have yet: its reading and the
// after-bind are its own.
void LabScreen::bind_later(Handle h, const char* name) {
  zrdr::Node* buttons = node ? zrdr::find(node, "BUTTONS") : nullptr;
  if (!buttons || !zrdr::find(buttons, name)) return;
  Engine& e = engine();
  e.vcall(h, 0x84, {});
  e.vcall(h, 0x88, {});
}

// Mech3.exe 1.2 @ 0x0044d4d0: a press. A rename under way (CONFIG_EDIT's
// +0x38c) must finish first (0x0044a3a0, step 4) unless it is CONFIG_SAVE
// (16). 0..6 the pages, 8..11 the lance's slots; the rest are the pages' own.
void LabScreen::on_button(int32_t id, int32_t a, int32_t b) {
  (void)a, (void)b;
  Engine& e = engine();
  if (id != 0x10 && config_edit.x38c != 0 && e.unported("0x0044a3a0", {0}) == 0) return;
  if (id >= 0 && id < 7) {
    switch_page(id);
    return;
  }
  if (id >= 8 && id < 12) {
    states().lab.show_slot(id - 8);
    return;
  }
  e.unported("lab.press", {id});  // steps 3..8 (mechlab.md section 5)
}

// Mech3.exe 1.2 @ 0x0044ce30: ACCEPT takes the shown slot's mech back out of
// the lab's store and leaves (0x00448870); a page hides the old one's
// controls, shows its own and redraws (0x0044cf50).
void LabScreen::switch_page(int32_t p) {
  Engine& e = engine();
  LabState& lab = states().lab;
  if (p == 6) {
    const int32_t mech = e.lab_slot_mech(e.lab_slot(lab.pilot_, lab.slot_));
    e.lab_store_remove(lab.store_, mech);
    lab.accept();
    return;
  }
  show_page(page, false);
  page = p;
  show_page(p, true);
  e.unported("0x0044cf50", {});
}

// Mech3.exe 1.2 @ 0x0044ce90
void LabScreen::show_slots(int32_t slot) {
  Engine& e = engine();
  LabState& lab = states().lab;
  for (int i = 0; i < 4; ++i) {
    const int32_t m = e.lab_slot(lab.pilot_, i) ? e.lab_slot_mech(e.lab_slot(lab.pilot_, i)) : -1;
    mechs[i].set_mech(m, false, nullptr);
  }
  const int32_t m = e.lab_slot(lab.pilot_, lab.slot_) ? e.lab_slot_mech(e.lab_slot(lab.pilot_, lab.slot_)) : -1;
  mechs[slot].set_mech(m, true, &pilot_glow);
  chassis.set_store(lab.store_);
  chassis.select_mech(m);
  e.unported("0x0044cf50", {});
}

// Mech3.exe 1.2 @ 0x0044ca20
HelpButton* LabScreen::tab(int32_t p) { return p >= 0 && p < 7 ? &tabs[p] : nullptr; }

// Mech3.exe 1.2 @ 0x0044ca50: a page's controls shown or hidden. Every call
// shows the mech's data panel and CURRENT_CHASSIS; the glow goes to the
// page's tab (0x0044af90).
void LabScreen::show_page(int32_t p, bool show) {
  Engine& e = engine();
  // The shown slot's mech's tech (0x00484720), asked and dropped: only the
  // slot's lookups (twice) remain.
  const LabState& lab = states().lab;
  if (e.lab_slot(lab.pilot_, lab.slot_)) e.lab_slot(lab.pilot_, lab.slot_);
  e.vcall(mech_data, 0x60, {zgame::bits(-1.0f)});
  e.vcall(mech_data + 0x188, 0x98, {0, 0});
  current_chassis.show_for(-1.0f);
  uint32_t text = 0;
  if (p == 0) {
    if (show) {
      panel_left.show_look(2);
      panel_right.show_look(2);
    }
    if (show) chassis.show_for(-1.0f);
    else chassis.hide();
    if (show) configs.show_for(-1.0f);
    else configs.hide();
    if (show) e.vcall(chassis_data, 0x60, {zgame::bits(-1.0f)});
    else e.vcall(chassis_data, 0x64, {});
    if (show) loadouts.show_for(-1.0f);
    else loadouts.hide();
    const bool skins_on = show && e.vcall(e.field32(lab.pilot_, 0x6c), 0x2c, {}) != 3;
    if (skins_on) skins.show_for(-1.0f);
    else skins.hide();
    if (show) sort_name.show_for(-1.0f);
    else sort_name.hide();
    if (show) sort_mass.show_for(-1.0f);
    else sort_mass.hide();
    text = 0x18c;
  } else if (p == 3) {
    if (show) {
      panel_left.show_look(1);
      panel_right.show_look(1);
      const uint8_t on = lab.flag_ == 0 ? 1 : 0;
      e.vcall(mech_data + 0x188, 0x98, {on, on});
    }
    e.vcall(armor_titles, 0x64, {});
    if (show) assigned.show_for(-1.0f);
    else assigned.hide();
    if (show) part_head.show_for(-1.0f);
    else part_head.hide();
    for (LabPart& part : parts) {
      if (show) part.show_for(-1.0f);
      else part.hide();
    }
    if (show) unassigned.show_for(-1.0f);
    else unassigned.hide();
    if (!show) e.lab_part_pick(-1);
    text = 0x1aa;
  } else if (p == 4) {
    if (show) {
      panel_left.show_look(1);
      panel_right.show_look(1);
      e.lab_groups_refresh();
    }
    if (show) ungrouped.show_for(-1.0f);
    else ungrouped.hide();
    if (show) grouped.show_for(-1.0f);
    else grouped.hide();
    text = 0x18d;
  }
  if (text) {
    ztwod::Text* first = current_chassis.labels.empty() ? nullptr : current_chassis.labels[0];
    first->set_text_plain(e.message(static_cast<int32_t>(text)));
  }
  HelpButton* t = tab(p);
  icon_glow.hide();
  if (t) lab_tab_glow(*t, show, &icon_glow);
}

// Mech3.exe 1.2 @ 0x0044a890
void MechButton::set_mech(int32_t m, bool sel, ztwod::Image* glow) {
  if (m != mech) {
    e_.mech_name_of(m);  // 0x00484650, its answer unused
    zimg::Texture* old = img_normal;
    img_normal = set_image(static_cast<zimg::Texture*>(nullptr));
    ztwod::host().image_release(old);
    ztwod::host().image_release(img_disabled);
    img_disabled = nullptr;
    ztwod::host().image_release(img_activate);
    img_activate = nullptr;
    img_normal = set_image(mech_icon(e_, m, 1));
    img_disabled = mech_icon(e_, m, 3);
    img_activate = mech_icon(e_, m, 0);
    mech = m;
    if (!(name.flags & ztwod::kHidden)) show_name(true);  // the name already showing
  }
  LabScreen::lab_tab_glow(*this, sel, glow);
}

// Mech3.exe 1.2 @ 0x0044af90: the page's tab chosen (no blink, its
// ACTIVATE image) with the glow placed at its GLOW_OFFSET, or not.
void LabScreen::lab_tab_glow(HelpButton& t, bool on, ztwod::Image* glow) {
  t.selected = on ? 1 : 0;
  if (on) {
    t.set_blink(false);
    if (glow) {
      glow->show_for(-1.0f);
      glow->set_pos(t.get_x() + t.glow[0], t.get_y() + t.glow[1]);
    }
    t.set_image(t.img_activate);
    return;
  }
  t.set_image(t.img_normal);
}

}  // namespace mech3
