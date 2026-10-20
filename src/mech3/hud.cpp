#include "mech3/hud.h"

#include <cmath>
#include <cstdlib>
#include <cstdio>

#include "crt/msvc_math.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zutil/zfix.h"
#include "gamez/ztwod/twod_host.h"
#include "gamez/zvideo/zvid_format.h"
#include "mech3/options.h"

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

ztwod::Rect HudHooks::display_rect() {
  const Rect& r = display();
  return {r.x, r.y, r.x_end, r.y_end};
}
ztwod::Rect HudHooks::window_rect() {
  const Rect& r = window();
  return {r.x, r.y, r.x_end, r.y_end};
}

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

// The original's calloc(4, n): n 32-bit pointers.
HudLight::HudLight(int32_t n)
    : Image(0), count(n), images(static_cast<zimg::Texture**>(std::calloc(n, sizeof(zimg::Texture*)))) {}
HudLight::~HudLight() { std::free(images); }

void HudLight::select(int32_t i) {
  if (current == i) return;
  current = i;
  set_image(images[i]);
}

namespace {
ztwod::ClipLine2D clip_line() { return ztwod::ClipLine2D(0, 0, 0, 0, 0); }  // 0x0043d620
constexpr int32_t kRadarStep = 10;  // [0x005e39c0]: pixels a zoom step
}  // namespace

// Mech3.exe 1.2 @ 0x0043d260
HudRadar::HudRadar(float fov) : FilledBox(0, 0, 0, 0, nullptr), lines{clip_line(), clip_line(), clip_line()} {
  color = zvid::pack_bytes(0, 0xff, 0);
  source = nullptr;
  if (text1.color_top != 0xff00 || text1.color_bottom != 0xff00) {
    text1.color_top = text1.color_bottom = 0xff00;
    text1.needs_render = 1;
  }
  lines[0].color = zvid::pack_bytes(0, 0xff, 0);
  lines[1].color = zvid::pack_bytes(0, 0xff, 0);
  lines[2].color = zvid::pack_bytes(0xff, 0, 0);
  ztwod::Host& h = ztwod::host();
  light.images[0] = h.image_get("hud_light_green");
  light.images[1] = h.image_get("hud_light_yellow");
  light.images[2] = h.image_get("hud_light_red");
  light.select(0);
  light.set_pos(0xb, 0xb);
  active = 0;
  HudRadar::hide();
  for (auto& l : lines) l.hide();
  r[0] = 0x32;
  r[1] = 0xb4;
  cx[0] = 0x37;
  cy[0] = 0x37;
  cx[1] = 0x140;
  cy[1] = 0xf0;
  r_steps = x_steps = y_steps = 0;
  to_r = to_x = to_y = 0;
  x58 = 0;
  set_zoom(false);
  x = cx[0];
  y = cy[0];
  dirty();
  radius = 0;
  radius_sq = 0;
  set_fov(static_cast<float>(static_cast<double>(fov) * 0.017453292519943295));  // 0x005c00c8
  const float down[3] = {0.0f, 0.0f, -1.0f};
  set_dir(down);
  text2.set_pos(cx[0] - r[0], cy[0] + r[0] + 0xf);
  text2.shadow = 1;
  text2.shadow_dx = text2.shadow_dy = 1;
  text2.shadow_color = 0x184e18;
  if (text2.color_top != 0x1fa201 || text2.color_bottom != 0x1fa201) {
    text2.color_top = text2.color_bottom = 0x1fa201;
    text2.needs_render = 1;
  }
  xdb8 = xdb4 = 0;
  xbd8 = xbd4 = 0;
}

// Mech3.exe 1.2 @ 0x0043dc00
void HudRadar::hide() {
  to_r = 0;
  changed = 1;
}

// Mech3.exe 1.2 @ 0x0043da00: with the radar active, the sound for the zoom.
void HudRadar::select_sound() {
  if (active) g_hooks->unported("0x0043da00", {g_hooks->id(this), zoomed});
}

// Mech3.exe 1.2 @ 0x0043da50: the size to move to and the steps there.
void HudRadar::set_zoom(bool on) {
  zoomed = on ? 1 : 0;
  to_r = r[zoomed];
  to_x = cx[zoomed];
  to_y = cy[zoomed];
  x_steps = std::abs((to_x - get_x()) / kRadarStep);
  y_steps = std::abs((to_y - get_y()) / kRadarStep);
  r_steps = std::abs((to_r - radius) / kRadarStep);
  if (source) g_hooks->radar_zoom_sound(source, zoomed != 0);
  select_sound();
  changed = 1;
}

// Mech3.exe 1.2 @ 0x0043f0c0
void HudRadar::set_fov(float a) {
  half_fov = a * 0.5f;
  cos_half = static_cast<float>(std::cos(static_cast<double>(half_fov)));
  sin_half = static_cast<float>(std::sin(static_cast<double>(half_fov)));
}

// Mech3.exe 1.2 @ 0x0043f130: on a flipped map the direction turned round.
void HudRadar::set_dir(const float* d) {
  if (d) dir[0] = d[0], dir[1] = d[1], dir[2] = d[2];
  float v[3] = {dir[0], dir[1], dir[2]};
  if (active) {
    HudHooks& k = *g_hooks;
    if (k.entry_flip(k.mission_entry()) > 0.0f) v[0] = -v[0], v[1] = -v[1], v[2] = -v[2];  // 0x005c007c
  }
  const float rr = static_cast<float>(radius);
  int32_t y1 = get_y() + crt::msvc_ftol((v[2] * cos_half - v[0] * sin_half) * rr);
  int32_t x1 = get_x() + crt::msvc_ftol((v[2] * sin_half + v[0] * cos_half) * rr);
  lines[0].set(get_x(), get_y(), x1, y1);
  y1 = get_y() + crt::msvc_ftol((v[0] * sin_half + v[2] * cos_half) * rr);
  x1 = get_x() + crt::msvc_ftol((v[0] * cos_half - v[2] * sin_half) * rr);
  lines[1].set(get_x(), get_y(), x1, y1);
}

// Mech3.exe 1.2 @ 0x004b5440
HudViewWindow::HudViewWindow(const char* name) {
  HudHooks& k = *g_hooks;
  node = k.window_node_new();
  k.node_set_name(node, name);
  set_area(ztwod::Rect{});
}

// Inlined (0x004b5440, 0x0043f660): the ratio of the area to its target, then
// the node's size and place.
void HudViewWindow::set_area(const ztwod::Rect& r) {
  area = r;
  target = r;
  scaled = !(target.right == area.right && target.bottom == area.bottom && target.left == area.left &&
             target.top == area.top);
  if (scaled) {
    sx = static_cast<float>(area.right - area.left) / static_cast<float>(target.right - target.left);
    sy = static_cast<float>(area.bottom - area.top) / static_cast<float>(target.bottom - target.top);
    isx = 1.0f / sx;  // 0x005c91a0
    isy = 1.0f / sy;
  }
  g_hooks->window_set_size(node, area.right - area.left, area.bottom - area.top);
  g_hooks->window_set_pos(node, area.left, area.top);
}

// Mech3.exe 1.2 @ 0x004b5150
HudZView::HudZView(const char* name, HudViewWindow* w, uint32_t wd) {
  HudHooks& k = *g_hooks;
  camera = k.camera_node_new();
  k.node_set_name(camera, name);
  x10 = 0;
  window = w;
  k.camera_set_window(camera, w ? w->node : 0);
  world = wd;
  k.camera_set_world(camera, k.world_node(wd));
  hardware = ztwod::host().video_mode();
  k.camera_set_zone(camera, 0xffffff01);  // [0x005e8854]: the default zone
}

// Mech3.exe 1.2 @ 0x0043f660
HudZoomView::HudZoomView(int32_t px, int32_t py, int32_t w, int32_t h)
    : Object2D(0, 0), view("camx", new HudViewWindow("winx"), g_hooks->world()) {
  rect = {px, py, px + w, py + h};
  view.window->set_area(rect);
  HudHooks& k = *g_hooks;
  k.camera_set_zoom(view.camera, 1.0f);
  k.camera_set_fov(view.camera, 0.87266463f, 1.0471976f);  // 50 and 60 degrees
  k.camera_set_clip(view.camera, 1.0f, 500.0f);
  x168 = 0;
  HudZoomView::hide();
}

// Mech3.exe 1.2 @ 0x0043fc60
void HudZoomView::hide() {
  Object2D::hide();
  g_hooks->node_set_active(view.camera, false);
  if (x4c != 0xffff) g_hooks->unported("0x0043fc60.light", {x4c});  // a light's flag (0x004b5560); never at build
}

// Mech3.exe 1.2 @ 0x00442ac0
HudZoomWindow::HudZoomWindow(int32_t px, int32_t py, int32_t w, int32_t h) : HudZoomView(px, py, w, h) {
  base_w = w;
  base_h = h;
  HudZoomWindow::hide();
  set_rect(px, py, w, h);
  light.show_for(-1.0f);
  text.hide();
  HudHooks& k = *g_hooks;
  k.node_set_name(view.camera, "zoomcam");
  k.camera_set_fov(view.camera, 0.08726646f, 0.08726646f);  // 5 degrees
  float near_z, far_z;
  k.player_camera_clip(&near_z, &far_z);
  k.camera_set_clip(view.camera, near_z, far_z);
  x174 = 0;
  k.camera_set_zoom(view.camera, 10.0f);
  light.center = 1;
  ztwod::Host& hs = ztwod::host();
  light.images[0] = hs.image_get("hud_zoomoff");
  light.images[1] = hs.image_get("hud_zoomon");
  light.images[2] = hs.image_get("hud_zoomover");
  light.images[3] = hs.image_get("hud_zoomcant");
}

// Mech3.exe 1.2 @ 0x00443880
void HudZoomWindow::hide() {
  HudZoomView::hide();
  if (x4c != 0xffff) g_hooks->unported("0x0043fc60.light", {x4c});
  text.hide();
  x174 = 0;
}

// Mech3.exe 1.2 @ 0x004438f0
void HudZoomWindow::set_rect(int32_t px, int32_t py, int32_t w, int32_t h) {
  int32_t sw = crt::msvc_ftol(static_cast<float>(w) * zoom);
  int32_t sh = crt::msvc_ftol(static_cast<float>(h) * zoom);
  if (sw < 2) sw = 2;
  if (sh < 2) sh = 2;
  px += (w - sw) / 2;
  py += (h - sh) / 2;
  const ztwod::Rect r = g_hooks->window_rect();
  if (px < r.left) px = r.left;
  if (px + sw > r.right) px = r.right - sw;
  if (py < r.top) py = r.top;
  if (py + sh > r.bottom) py = r.bottom - sh;
  set_view_rect(&px, &py, &sw, &sh);
  light.set_pos(sw / 2 + px, sh / 2 + py);
  text.set_pos(px - 1, py + sh + 1);
  if (round) {
    frame.set_pos(sw / 2 + px, sh / 2 + py);
    frame.radius = sw / 2;
    frame.radius_sq = frame.radius * frame.radius;
    frame.color = zvid::pack_bytes(1, 0xa2, 0x1f);
    return;
  }
  const int32_t x0 = px - 1, y0 = py - 1, bw = sw + 1, bh = sh + 1;
  box.set_pos(x0, y0);
  box.set_point(0, x0, y0);
  box.set_point(1, bw + x0, y0);
  box.set_point(2, bw + x0, bh + y0);
  box.set_point(3, x0, bh + y0);
  box.set_point(4, x0, y0);
  box.color = zvid::pack_bytes(1, 0xa2, 0x1f);
}

// Mech3.exe 1.2 @ 0x0043fa40: an edge past the display's inner border moved
// back (one per axis), the window's area set.
void HudZoomWindow::set_view_rect(int32_t* px, int32_t* py, int32_t* w, int32_t* h) {
  rect = {*px, *py, *px + *w, *py + *h};
  const ztwod::Rect d = g_hooks->display_rect();
  const int32_t right = d.right - 2, left = d.left + 2, top = d.top + 2, bottom = d.bottom - 2;
  bool moved = true;
  if (right < rect.left) rect.left = right;
  else if (rect.left < left) rect.left = left;
  else if (right < rect.right) rect.right = right;
  else if (rect.right < left) rect.right = left;
  else moved = false;
  if (moved) {
    *px = rect.left;
    *w = rect.right - rect.left;
  }
  moved = true;
  if (bottom < rect.top) rect.top = bottom;
  else if (rect.top < top) rect.top = top;
  else if (bottom < rect.bottom) rect.bottom = bottom;
  else if (rect.bottom < top) rect.bottom = top;
  else moved = false;
  if (moved) {
    *py = rect.top;
    *h = rect.bottom - rect.top;
  }
  view.window->set_area(rect);
}

// Mech3.exe 1.2 @ 0x00443b60
HudReticle::HudReticle(const char* on, const char* off) : Image(0) {
  locked = 0;
  x950 = 0;
  x954 = 0.0f;
  Image::set_pos(0, 0);
  ztwod::Host& h = ztwod::host();
  img_on = h.image_get(on);
  img_off = h.image_get(off);
  img_over = h.image_get("ret_over");
  img_cant = h.image_get("ret_cant");
  img_x920 = h.image_get("ret_on");
  center = 1;
  u40 = 1;
  state = 5;
  dirty();
  if (state != 0) {  // inlined: the state set to off
    state = 0;
    if (!locked) set_image(img_off);
  }
  Object2D::show_for(-1.0f);
  zoom.hide();
  for (uint32_t& v : x928) v = 0;
}

// Mech3.exe 1.2 @ 0x00443d00
void HudReticle::hide() {
  Object2D::hide();
  zoom.hide();
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
