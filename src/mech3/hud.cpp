#include "mech3/hud.h"

#include <cmath>
#include <cstdio>

#include "crt/msvc_math.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zutil/zfix.h"
#include "gamez/ztwod/twod_host.h"
#include "gamez/zvideo/zvid_format.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

namespace {
DamageSet g_damage;
EquipmentSet g_equipment;
WeaponSet g_weapons;
HudHooks g_default_hooks;
HudHooks* g_hooks = &g_default_hooks;

// The parts' images by the table's order (0x00605888 + 0x20 i) and where each
// sits in the silhouette.
struct PartImages {
  int index;
  const char* names[5];
  int32_t x, y;
};
// In the constructor's order: head, ltorso, rtorso, ctorso, larm, rarm, lleg, rleg.
constexpr PartImages kParts[8] = {
    {5, {"dmg_head_blue", "dmg_head_grn", "dmg_head_yel", "dmg_head_red", "dmg_head_blk"}, 0x1e, 2},
    {4, {"dmg_ltorso_blue", "dmg_ltorso_grn", "dmg_ltorso_yel", "dmg_ltorso_red", "dmg_ltorso_blk"}, 0x2e, 6},
    {0, {"dmg_rtorso_blue", "dmg_rtorso_grn", "dmg_rtorso_yel", "dmg_rtorso_red", "dmg_rtorso_blk"}, 0x13, 6},
    {3, {"dmg_ctorso_blue", "dmg_ctorso_grn", "dmg_ctorso_yel", "dmg_ctorso_red", "dmg_ctorso_blk"}, 0x1f, 0xf},
    {6, {"dmg_larm_blue", "dmg_larm_grn", "dmg_larm_yel", "dmg_larm_red", "dmg_larm_blk"}, 0x3a, 8},
    {2, {"dmg_rarm_blue", "dmg_rarm_grn", "dmg_rarm_yel", "dmg_rarm_red", "dmg_rarm_blk"}, 2, 8},
    {1, {"dmg_lleg_blue", "dmg_lleg_grn", "dmg_lleg_yel", "dmg_lleg_red", "dmg_lleg_blk"}, 0x27, 0x2a},
    {7, {"dmg_rleg_blue", "dmg_rleg_grn", "dmg_rleg_yel", "dmg_rleg_red", "dmg_rleg_blk"}, 7, 0x2a},
};
}  // namespace

DamageSet& damage_set() { return g_damage; }
EquipmentSet& equipment_set() { return g_equipment; }
WeaponSet& weapon_set() { return g_weapons; }
HudHooks& hud_hooks() { return *g_hooks; }
void set_hud_hooks(HudHooks* h) { g_hooks = h ? h : &g_default_hooks; }

void HudHooks::piece(uint32_t ctor, std::initializer_list<zgame::Arg> args) {
  char key[16];
  std::snprintf(key, sizeof key, "0x%08x", ctor);
  unported(key, args);
}

PendingPiece::PendingPiece(uint32_t ctor, std::initializer_list<zgame::Arg> args) : Object2D(0, 0) {
  g_hooks->piece(ctor, args);
}

HudTimer::HudTimer() : FontText("00:00:00.00", 0x244, 0xf) { set_font(ztwod::host().font_find("mech_string")); }

// Mech3.exe 1.2 @ 0x0043ae50: a null name keeps the slot.
void DamageImages::set(const char* const names[5], int32_t px, int32_t py) {
  for (int i = 0; i < 5; ++i)
    if (names[i]) image[i] = ztwod::host().image_get(names[i]);
  x = px, y = py;
}

// Mech3.exe 1.2 @ 0x0043afc0 (also inlined in the constructor): once; the
// background size from the first background found.
void load_damage_set() {
  DamageSet& d = g_damage;
  if (d.loaded) return;
  for (const PartImages& p : kParts) d.parts[p.index].set(p.names, p.x, p.y);
  d.bg[0] = ztwod::host().image_get("dmg_bg_blue");
  d.bg[1] = ztwod::host().image_get("dmg_bg_red");
  d.bg[2] = ztwod::host().image_get("dmg_bg_grn");
  const zimg::Texture* t = d.bg[0] ? d.bg[0] : d.bg[1] ? d.bg[1] : d.bg[2];
  if (t) d.bg_w = t->width, d.bg_h = t->height;
  d.loaded = true;
}

// Mech3.exe 1.2 @ 0x0043f3e0
HudCompass::HudCompass()
    : Object2D(0, 0), tape("hud_compass", 0xf5, 0x13, 0), frame("hud_compass_frame", 0xf5, 0xb, 0), text(nullptr, 0, 0) {
  half = (frame.image ? frame.image->width : 0) / 2;
  src.top = 0;
  src.bottom = tape.image ? tape.image->height : 0;
  set_heading(0.0f);
  tape.src = &src;
  show_for(-1.0f);
}

// Mech3.exe 1.2 @ 0x0043f590: the tape's window around the heading, turned
// half round on a flipped map.
void HudCompass::set_heading(float h) {
  HudHooks& k = *g_hooks;
  if (k.mission_entry() && k.entry_flip(k.mission_entry()) > 0.0f) h = h - 3.1415927f;  // 0x005c0100
  while (h < 0.0f) h = h - -6.2831855f;            // 0x005c0104
  const int32_t x = crt::msvc_ftol(scale * h) + centre;
  src.right = x + half;
  src.left = x - half;
}

HudText::HudText() : FontText(nullptr, 0, 0) { set_font(ztwod::host().font_find("mech_string")); }

// Mech3.exe 1.2 @ 0x0043b170
uint16_t hud_green() { return zvid::pack_bytes(0, 0x40, 0); }

// Mech3.exe 1.2 @ 0x0043a7c0
float hud_opacity() { return ztwod::host().video_mode() != 0 ? 0.6f : 1.0f; }

// Mech3.exe 1.2 @ 0x0043a7e0
HudPanel::HudPanel() : Image(0) {
  color = zvid::pack_bytes(0, 0x40, 0);
  alpha = static_cast<uint8_t>(crt::msvc_ftol(hud_opacity() * 255.0f));  // 0x005c0078
}

// Mech3.exe 1.2 @ 0x0043a8f0
void HudPanel::set_pos(int32_t px, int32_t py) {
  Image::set_pos(px, py);
  const int32_t w = image ? image->width : 0, h = image ? image->height : 0;
  rect = {px, py, px + w, py + h};
  const float fx = static_cast<float>(px), fy = static_cast<float>(py);
  const float x1 = static_cast<float>(w) + fx, y1 = static_cast<float>(h) + fy;
  const float c[4][3] = {{fx, fy, 0.9f}, {fx, y1, 0.9f}, {x1, y1, 0.9f}, {x1, fy, 0.9f}};
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 3; ++j) corners[i][j] = c[i][j];
}

// Mech3.exe 1.2 @ 0x0043aaa0
void HudPanel::set_color(uint16_t c) {
  color = c;
  if (colors)
    for (int32_t i = 0; i < image->pixel_count; ++i) colors[i] = color;
}

// Mech3.exe 1.2 @ 0x0043a9d0
void HudPanel::set_alpha(uint8_t a) {
  alpha = a;
  if (!image) return;
  image->alpha = nullptr;
  if (alpha > 0xfd) {
    image->flags = 0x61;  // 0x00517c60
    return;
  }
  image->flags = 0x63;
  image->alpha = alphas;
  if (alphas)
    for (int32_t i = 0; i < image->pixel_count; ++i) alphas[i] = alpha;
}

// Mech3.exe 1.2 @ 0x0043aa60
void HudPanel::set_opacity(float f) {
  set_alpha(static_cast<uint8_t>(crt::msvc_ftol(std::floor(static_cast<double>(f * 255.0f)))));
}

// Mech3.exe 1.2 @ 0x00446700
HudObjectives::HudObjectives() : TextBox(0x19), scores(0x19) {
  ztwod::Host& h = ztwod::host();
  font_title = h.font_find("rtobjective_title");
  font_incomplete = h.font_find("rtobjective_incomplete");
  font_complete = h.font_find("rtobjective_complete");
  font_scores = h.font_find("rt_mp_scores");
  TextBox::set_font(font_title);
  scores.set_font(font_title);
  panel.show_for(-1.0f);
  panel.set_color(hud_green());
  panel.set_opacity(hud_opacity());
  hide();
}

// Mech3.exe 1.2 @ 0x00446870: the box's size measured and dropped.
void HudObjectives::set_pos(int32_t px, int32_t py) {
  TextBox::set_pos(px, py);
  scores.set_pos(px + 0x104, py);
  total_height();
  max_width();
}

// Mech3.exe 1.2 @ 0x00446820
void HudObjectives::show_for(float seconds) {
  Object2D::show_for(seconds);
  scores.show_for(seconds);
  g_hooks->unported("0x00446ad0", {g_hooks->id(this)});  // the panel's size: with the readers (hud.md)
}

// Mech3.exe 1.2 @ 0x00446850
void HudObjectives::hide() {
  Object2D::hide();
  scores.hide();
}

// Mech3.exe 1.2 @ 0x00447c40
HudEquipLine::HudEquipLine() : light(0) {
  ztwod::Host& h = ztwod::host();
  EquipmentSet& e = g_equipment;
  if (!e.light[0]) e.light[0] = h.image_get("hud_light_red");
  if (!e.light[1]) e.light[1] = h.image_get("hud_light_yellow");
  if (!e.light[2]) e.light[2] = h.image_get("hud_light_green");
  if (!e.font_red) e.font_red = h.font_find("wepstring_red");
  if (!e.font_green) e.font_green = h.font_find("wepstring_green");
  if (!e.font_yellow) e.font_yellow = h.font_find("wepstring_yellow");
  if (!e.font_grey) e.font_grey = h.font_find("wepstring_grey");
}

// Mech3.exe 1.2 @ 0x00447fe0: the value's right edge at the column, the light after it.
void HudEquipLine::place(int32_t px, int32_t py, int32_t col) {
  x = px;
  y = py;
  value.set_pos(col - 0xf + px, py);
  value.align = 2;
  light.set_pos(col - 9 + px, py + 1);
}

// Mech3.exe 1.2 @ 0x00447e10: empty, the hidden bit kept.
void HudEquipLine::reset() {
  item = -1;
  state = 2;
  flags &= ztwod::kHidden;
}

// Mech3.exe 1.2 @ 0x00448070
HudEquipment::HudEquipment() : Object2D(0, 0) {
  place(0x212, 0x1e, 0);
  hud = nullptr;  // 0x00448200 with no HUD
  show_for(-1.0f);
}

// Mech3.exe 1.2 @ 0x00448140: unmoved when already there.
void HudEquipment::place(int32_t px, int32_t py, int32_t dy) {
  if (below)
    py += dy;
  else
    px += -0xf - column;
  if (px == get_x() && py == get_y()) return;
  x = px;
  y = py;
  dirty();
  panel.set_pos(get_x(), get_y());
  for (HudEquipLine& l : lines) {
    l.place(px, py, column);
    l.reset();
    py += 10;
  }
}

// Mech3.exe 1.2 @ 0x00445760
HudWeaponLine::HudWeaponLine() : light(0), mark(0) {
  ztwod::Host& h = ztwod::host();
  WeaponSet& w = g_weapons;
  if (!w.light[0]) w.light[0] = h.image_get("hud_light_green");
  if (!w.light[1]) w.light[1] = h.image_get("hud_light_yellow");
  if (!w.light[2]) w.light[2] = h.image_get("hud_light_red");
  if (!w.font_red) w.font_red = h.font_find("wepstring_red");
  if (!w.font_green) w.font_green = h.font_find("wepstring_green");
  if (!w.font_yellow) w.font_yellow = h.font_find("wepstring_yellow");
  if (!w.font_grey) w.font_grey = h.font_find("wepstring_grey");
  hide();  // 0x00445dc0 with no weapon
  flags &= ztwod::kHidden;
}

// Mech3.exe 1.2 @ 0x00445ba0
void HudWeaponLine::place(int32_t px, int32_t py, int32_t col) {
  Image::set_pos(px, py);
  count_text.set_pos(col - 0xf + px, py);
  light.set_pos(col - 9 + px, py + 1);
  mark.set_pos(px - 10, py + 2);
}

// Mech3.exe 1.2 @ 0x00445a30
void HudWeaponLine::reset() {
  weapon = nullptr;
  red = 1;
  set_look(false, false);
  changed = 1;
  count = 0;
}

// Mech3.exe 1.2 @ 0x00445a60: red, the name red and the rest green; else all
// grey or all yellow.
bool HudWeaponLine::set_look(bool r, bool g) {
  if (red == r && grey == g) return false;
  const WeaponSet& w = g_weapons;
  const ztwod::Handle f1 = r ? w.font_red : g ? w.font_grey : w.font_yellow;
  const ztwod::Handle f2 = r ? w.font_green : f1;
  name.set_named_font(f1, false);
  text2.set_named_font(f2, false);
  if (count_text.set_named_font(f2, false)) count_text.align = 2;
  red = r;
  grey = g;
  changed = 1;
  return true;
}

// Mech3.exe 1.2 @ 0x00445f80
HudWeapons::HudWeapons() : Object2D(0, 0) {
  ztwod::Host& h = ztwod::host();
  select = h.image_get("hud_weapon_select");
  chain = h.image_get("hud_weapon_chain");
  poly.color = zvid::pack_bytes(1, 0xa2, 0x1f);
  set_pos(0x212, 0x1e);
  x1143c = 0;  // 0x00446120 with no HUD
  x38 = 0;
  hud = nullptr;
  panel.set_opacity(hud_opacity());
  panel.set_color(zvid::pack_bytes(0, 0x40, 0));  // 0x0043aae0
}

// Mech3.exe 1.2 @ 0x004460c0
void HudWeapons::set_pos(int32_t px, int32_t py) {
  x = px;
  y = py;
  dirty();
  for (HudWeaponLine& l : lines) {
    l.place(px, py, column);
    l.reset();
    py += 10;
  }
}

// Mech3.exe 1.2 @ 0x00446200
void HudWeapons::show_for(float seconds) {
  Object2D::show_for(seconds);
  shown = 1;
  g_hooks->unported("0x00446520", {g_hooks->id(this)});  // the lines from the mech: with the readers (hud.md)
}

// Mech3.exe 1.2 @ 0x00446220
void HudWeapons::hide() {
  Object2D::hide();
  shown = 0;
}

// Mech3.exe 1.2 @ 0x00448550
void HudBar::set_box(int32_t px, int32_t py, int32_t w, int32_t h) {
  set_point(0, static_cast<float>(px), static_cast<float>(py));
  set_point(1, static_cast<float>(px), static_cast<float>(h + py));
  const float x1 = static_cast<float>(w + 1 + px);
  set_point(2, x1, static_cast<float>(h + py));
  set_point(3, x1, static_cast<float>(py));
  height = h;
  width = w;
}

namespace {
// The gauges' texts: Arial 8, green with a dark green shadow.
void gauge_font(ztwod::FontText& t) {
  t.create_font("Arial", 8, 300, 6, 0, 0, 2);
  if (t.color_top != 0x1fa201 || t.color_bottom != 0x1fa201) {
    t.color_top = t.color_bottom = 0x1fa201;
    t.needs_render = 1;
  }
}
void gauge_shadow(ztwod::FontText& t) {
  t.shadow = 1;
  t.shadow_dx = t.shadow_dy = 1;
  t.shadow_color = 0x184e18;
}
int32_t image_h(const ztwod::Image& i) { return i.image ? i.image->height : 0; }
}  // namespace

// Mech3.exe 1.2 @ 0x00444760
HudGauges::HudGauges()
    : Object2D(0, 0), coolant_text(nullptr, 0, 0), fuel_text(nullptr, 0, 0),
      speed_frame("hud_speed_frame", 0x23f, 0x100, 0), coolant_frame("hud_coolant_frame", 8, 0x122, 0),
      fuel_frame("hud_fuel_frame", 0x20f, 0x128, 0), heat_bar("hud_heat_bar", 0x12, 0xa3, 0),
      heat_meter("hud_heat_meter", 0x10, 0xa3, 0), toggler("lab_toggler_green", 10, 0x96, 0) {
  speed_text.set_pos(speed_frame.get_x() + 0x1b, speed_frame.get_y() + 0x30);
  gauge_font(speed_text);
  gauge_shadow(speed_text);
  coolant_text.set_pos(coolant_frame.get_x() + 0x16, coolant_frame.get_y() + 0x21);
  gauge_font(coolant_text);
  coolant_text.set_text_id(0x1d5);
  gauge_shadow(coolant_text);
  fuel_text.set_pos(0x1e, 0x115);
  gauge_font(fuel_text);
  fuel_text.set_text_id(0x1d6);
  gauge_shadow(fuel_text);
  speed_bar.set_box(speed_frame.get_x() + 0xf, speed_frame.get_y() + 5, 4, 0x2f);
  coolant_bar.color = zvid::pack_bytes(0, 0, 0xff);
  coolant_bar.set_box(coolant_frame.get_x() + 9, coolant_frame.get_y() + 7, 4, image_h(coolant_frame) - 8);
  fuel_bar.color = zvid::pack_bytes(1, 0xa2, 0x1f);
  fuel_bar.set_box(fuel_frame.get_x() + 0x10, fuel_frame.get_y() + 5, 4, image_h(fuel_frame) - 8);
  toggler_y = heat_meter.get_y() + image_h(heat_meter) - (image_h(toggler) >> 1) - 1;
  x1444 = 0;
  heat_bottom = heat_bar.get_y() + image_h(heat_bar);
  heat_h = image_h(heat_bar);
  x1440 = 0;
  heat_w = heat_bar.image ? heat_bar.image->width : 0;
  show_for(-1.0f);
}

// Mech3.exe 1.2 @ 0x0043b190. The pieces are made in the members' order
// (the ones not ported yet are recorded calls).
Hud::Hud(Mech* m) {
  HudHooks& h = *g_hooks;
  if (zfix::options().bugs) block = nullptr;
  load_damage_set();
  mech = nullptr;
  h.setup(this, m);
  add(&compass);
  add(&gauges);
  add(&target);
  add(&mfd);
  add(&weapons);
  add(&equipment);
  timer.show_for(-1.0f);
}

}  // namespace mech3
