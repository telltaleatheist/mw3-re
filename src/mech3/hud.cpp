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

int32_t HudHooks::display_w() { return display().w; }
int32_t HudHooks::display_h() { return display().h; }

ztwod::Rect HudHooks::display_rect() {
  const Rect& r = display();
  return {r.x, r.y, r.x_end, r.y_end};
}
ztwod::Rect HudHooks::window_rect() {
  const Rect& r = window();
  return {r.x, r.y, r.x_end, r.y_end};
}

const char* HudHooks::message(int32_t id) {
  unported("0x00576040", {id});
  return nullptr;
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

// Mech3.exe 1.2 @ 0x0043ab10: the corners at the rect's place (Direct3D's
// depth 0.9); a new image of the colour and alpha, or the old one resized.
void HudPanel::set_size(int32_t w, int32_t h) {
  const float fx = static_cast<float>(rect.left), fy = static_cast<float>(rect.top);
  const float y1 = static_cast<float>(h) + fy, x1 = static_cast<float>(w) + fx;
  const float c[4][3] = {{fx, fy, 0.9f}, {fx, y1, 0.9f}, {x1, y1, 0.9f}, {x1, fy, 0.9f}};
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 3; ++j) corners[i][j] = c[i][j];
  rect.bottom = rect.top + h;
  rect.right = rect.left + w;
  const int32_t n = w * h;
  if (!image) {
    auto* t = static_cast<zimg::Texture*>(std::calloc(1, sizeof(zimg::Texture)));  // 0x00517c10
    set_image(t);
    t->flags = 0x61;                                     // 0x00517c60
    colors = static_cast<uint16_t*>(std::malloc(static_cast<size_t>(n) * 2));
    alphas = static_cast<uint8_t*>(std::malloc(static_cast<size_t>(n)));
  } else if (image->pixel_count == n) {
    image->width = static_cast<int16_t>(w);             // 0x00517c90
    image->height = static_cast<int16_t>(h);
    image->width_again = static_cast<int16_t>(w);
    image->pixel_count = static_cast<int16_t>(h) * static_cast<int16_t>(w);
    return;
  } else {
    colors = static_cast<uint16_t*>(std::realloc(colors, static_cast<size_t>(n) * 2));
    alphas = static_cast<uint8_t*>(std::realloc(alphas, static_cast<size_t>(n)));
  }
  image->pixels = colors;                                // 0x00517c70
  image->alpha = alphas;
  if (alphas) image->flags |= 2;
  image->width = static_cast<int16_t>(w);               // 0x00517c90
  image->height = static_cast<int16_t>(h);
  image->width_again = static_cast<int16_t>(w);
  image->pixel_count = static_cast<int16_t>(h) * static_cast<int16_t>(w);
  set_color(color);
  set_alpha(alpha);
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
  read_layout(nullptr);
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
  read_layout(nullptr);
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
  source = nullptr;
  HudZoomView::hide();
}

// Mech3.exe 1.2 @ 0x0043fc10
void HudZoomView::show_for(float seconds) {
  Object2D::show_for(seconds);
  g_hooks->node_set_active(view.camera, true);
  if (x4c != 0xffff) g_hooks->unported("0x0043fc60.light", {x4c});
}

// Mech3.exe 1.2 @ 0x0043fc60
void HudZoomView::hide() {
  Object2D::hide();
  g_hooks->node_set_active(view.camera, false);
  if (x4c != 0xffff) g_hooks->unported("0x0043fc60.light", {x4c});  // a light's flag (0x004b5560); never at build
}

// Mech3.exe 1.2 @ 0x00442620
HudMfdView::HudMfdView(int32_t px, int32_t py, int32_t w, int32_t h) : HudZoomView(px, py, w, h) {
  g_hooks->camera_set_flag8(view.camera, true);
  panel.set_size(w, h);
  panel.set_pos(px, py);
  panel.set_color(zvid::pack_bytes(0, 0x40, 0));  // 0x0043aae0
  panel.set_opacity(hud_opacity());
  box.set_point(0, px, py);
  box.set_point(1, px + w, py);
  box.set_point(2, px + w, py + h);
  box.set_point(3, px, py + h);
  box.set_point(4, px, py);
  box.color = zvid::pack_bytes(1, 0xa2, 0x1f);
  box.show_for(-1.0f);
  panel.show_for(-1.0f);
  show_for(-1.0f);
}

// Mech3.exe 1.2 @ 0x00447460: at the part's place in the silhouette, blue.
HudDamagePart::HudDamagePart(DamageImages* d, int32_t px, int32_t py) : Image(0), images(d) {
  Image::set_pos(d->x + px, d->y + py);
  set_image(d->image[0]);
}

namespace {
// 0x0043afc0 on a part's entry: the set loaded, the entry back.
DamageImages* loaded(int i) {
  load_damage_set();
  return &g_damage.parts[i];
}
// The parts by the table's index: own head, rtorso, ltorso, ctorso, rarm,
// larm, rleg, lleg; a target's head, ltorso, rtorso, ctorso, larm, rarm, lleg, rleg.
constexpr int kOwnOrder[8] = {5, 0, 4, 3, 2, 6, 7, 1};
constexpr int kTargetOrder[8] = {5, 4, 0, 3, 6, 2, 1, 7};
const int* order(bool t) { return t ? kTargetOrder : kOwnOrder; }
}  // namespace

// Mech3.exe 1.2 @ 0x00447680, 0x00447820
HudDamageView::HudDamageView(int32_t px, int32_t py, bool t)
    : Object2D(0, 0), bg(static_cast<zimg::Texture*>(nullptr), px, py, 0),
      parts{{loaded(order(t)[0]), px, py}, {loaded(order(t)[1]), px, py}, {loaded(order(t)[2]), px, py},
            {loaded(order(t)[3]), px, py}, {loaded(order(t)[4]), px, py}, {loaded(order(t)[5]), px, py},
            {loaded(order(t)[6]), px, py}, {loaded(order(t)[7]), px, py}} {
  rect = {px, py, px + g_damage.bg_w, py + g_damage.bg_h};
}

// Mech3.exe 1.2 @ 0x00442060 (target false), 0x00441da0 (true)
HudMfd::HudMfd(int32_t px, int32_t py, int32_t w, int32_t h, bool t) : Object2D(0, 0), damage(px, py, t) {
  HudHooks& k = *g_hooks;
  view = new HudMfdView(px, py, w, h);
  nav = k.nav_list_new(px, py + 0xe, h - 0xf, 5, 5);  // [0x005e39f8] / [0x005e39f0] and the next: 5
  damage.bg.set_image(g_damage.bg[0]);
  title.set_text("%s", k.message(k.multiplayer_game() ? 0xfbb : 0xdbe));
  title.set_pos(px, py - title.text_height() - 1);
  if (title.color_top != 0xff || title.color_bottom != 0xff) {
    title.color_top = title.color_bottom = 0xff;
    title.needs_render = 1;
  }
  title.show_for(-1.0f);
  mode = k.saved_mfd_mode();
  if (!mode) mode = ztwod::host().video_mode() != 0 ? 1 : 3;  // 0x00441d90
}

// Mech3.exe 1.2 @ 0x00447c00
bool HudDamageView::set_mech(void* m) {
  mech = m;
  if (m) {
    show_for(-1.0f);
    return true;
  }
  hide();
  return false;
}

namespace {
ztwod::Line2D zero_line() { return ztwod::Line2D(0, 0, 0, 0, 0); }  // 0x004400e0
const char* const kArrows[12] = {
    "hud_enarrow_left",     "hud_enarrow_right",     "hud_enarrow_up",     "hud_enarrow_down",
    "hud_allyarrow_left",   "hud_allyarrow_right",   "hud_allyarrow_up",   "hud_allyarrow_down",
    "hud_othererrow_left",  "hud_othererrow_right",  "hud_othererrow_up",  "hud_othererrow_down"};
void set_colors(ztwod::FontText& t, uint32_t c) {
  if (t.color_top != c || t.color_bottom != c) {
    t.color_top = t.color_bottom = c;
    t.needs_render = 1;
  }
}
}  // namespace

// Mech3.exe 1.2 @ 0x0043fcb0
HudTarget::HudTarget(int32_t px, int32_t py, int32_t w, int32_t h)
    : HudMfd(px, py, w, h, true), lines{zero_line(), zero_line(), zero_line()} {
  target = nullptr;
  texts[0].set_text_id(0x22a);
  line_h = texts[0].text_height();
  texts[0].set_pos(px, py - line_h * 2 - 1);
  texts[1].set_pos(px, py - line_h - 1);
  texts[2].set_pos(px, py - line_h * 3 - 1);
  set_colors(texts[0], 0xff00);
  texts[0].set_text("");  // [0x005f4c90]
  set_colors(texts[1], 0xff00);
  texts[1].set_text("");
  set_colors(texts[2], 0xff0000);
  texts[2].set_text("");
  ztwod::Host& hs = ztwod::host();
  for (int i = 0; i < 12; ++i) arrows[i] = hs.image_get(kArrows[i]);
  shape1.hide();
  for (auto& l : lines) l.hide();
  marker.hide();
  for (int i = 0; i < 3; ++i) texts[i].hide();
  set_target(nullptr);
  mode = g_hooks->saved_target_mode();  // 0x0043fca0
  if (!mode) mode = ztwod::host().video_mode() != 0 ? 1 : 3;
}

// Mech3.exe 1.2 @ 0x004415f0
void HudTarget::hide() {
  Object2D::hide();
  for (auto& l : lines) l.hide();
  for (int i = 0; i < 3; ++i) texts[i].hide();
}

// Mech3.exe 1.2 @ 0x004400f0
void HudTarget::set_mode(int32_t m) {
  mode = m;
  if (m && m != g_hooks->saved_target_mode()) g_hooks->save_target_mode(m);
}

// Mech3.exe 1.2 @ 0x00441800 (a target whose +0 is set: shown; not ported
// yet). Targets are read as the original's objects (hud.md).
void HudTarget::set_target(void* t) {
  target = t;
  if (g_hooks->target_valid(t)) {
    g_hooks->unported("0x00441800", {g_hooks->id(this), g_hooks->id(t)});
    return;
  }
  hide();
  set_shown(false);
}

// Mech3.exe 1.2 @ 0x00441780
void HudTarget::set_shown(bool on) {
  if (on && g_hooks->target_valid(target)) {
    g_hooks->unported("0x00441780", {g_hooks->id(this), 1});
    return;
  }
  shown = 0;
  x240c = 0.0f;
  update_texts(true);
}

// Mech3.exe 1.2 @ 0x00441660 (the clear path; with a target, not ported yet)
void HudTarget::update_texts(bool clear) {
  if (!clear && g_hooks->target_valid(target)) {
    g_hooks->unported("0x00441660", {g_hooks->id(this), 0});
    return;
  }
  set_mode(0);
  damage.set_mech(nullptr);
  texts[0].set_text_plain("");  // [0x005f4c90]
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

// ---------------------------------------------------------------------------
// The set-up (hud.md section 3)

namespace {
// 0x00557110 on a key: (x, y) or (w, h).
bool ivec2(zrdr::Node* n, const char* key, int32_t out[2]) { return zrdr::get_ivec2(n, key, out); }
constexpr const char* kModeBlock[] = {nullptr, nullptr, nullptr, "V640x480", nullptr, "V640x480", "V800x600",
                                      "V1024x768", "V1152x864", "V1280x1024", "V1600x1200"};
// The parts the equipment list counts, by mech +0x3ac index ([0x005e3a30..0x005e3a50]).
constexpr int32_t kEquipParts[8] = {0, 12, 3, 8, 14, 6, 7, 4};
}  // namespace

// Mech3.exe 1.2 @ 0x0043bb80: an unlisted video mode keeps +0x22e3c as it
// is (FIX (bug) in the constructor, hud.md section 3).
void Hud::setup(Mech* m) {
  HudHooks& k = *g_hooks;
  if (m) mech = m;
  reader = k.load_hud_zrd();
  if (reader) {
    const int32_t v = k.vmode();
    if (v >= 0 && v <= 10 && kModeBlock[v]) block = zrdr::find(reader, kModeBlock[v]);
  }
  if (block) {
    int32_t a[2], b[2];
    zrdr::Node* n = zrdr::find(block, "MFD");
    if (n && ivec2(n, "size", b) && ivec2(n, "position", a)) mfd.place(a[0], a[1], b[0], b[1]);
    n = zrdr::find(block, "TARGET");
    if (n && ivec2(n, "size", b) && ivec2(n, "position", a)) target.place(a[0], a[1], b[0], b[1]);
    n = zrdr::find(block, "TIMER");
    if (n && ivec2(n, "position", a)) timer.set_pos(a[0], a[1]);
  }
  reset();
  reticle.attach(this);
  mfd.attach(this);
  target.attach(this);
  target.set_target(k.mech_target_slot(m));
  radar.attach(this);
  equipment.read_layout(this);
  weapons.read_layout(this);
  compass.read_layout(this);
  gauges.read_layout(this);
  set_active(m ? (k.mech_dead(m) ? 0 : 1) : 0);
  if (m) reticle.zoom.round = k.mech_kind(m) == 2;  // 0x00442df0
  if (reader) {
    reader = reinterpret_cast<zrdr::Node*>(static_cast<intptr_t>(zrdr::free_node(reader)));
    block = nullptr;
  }
}

// Mech3.exe 1.2 @ 0x0043baf0
void Hud::set_active(int32_t on) {
  active = on;
  if (on == 1)
    radar.show_for(-1.0f);
  else
    radar.hide();
}

// Mech3.exe 1.2 @ 0x0043be10
void Hud::reset() {
  for (uint32_t& v : x22e08) v = 0;
  radar.set_fade(0.0f);
}

// Mech3.exe 1.2 @ 0x0043dc70
void HudRadar::set_fade(float f) {
  if (f > 1.0f) f = 1.0f;
  else if (f < 0.0f) f = 0.0f;
  x54 = zgame::bits(f);  // 0x005a3bb0
  for (auto& l : lines) l.x4c = zgame::bits(f);  // 0x005a6b20
}

// Mech3.exe 1.2 @ 0x0043d630
void HudRadar::set_amber(bool on) {
  if (x58 != (on ? 1 : 0)) {
    const uint16_t c = on ? zvid::pack_bytes(0xff, 0xe2, 4) : zvid::pack_bytes(0, 0xff, 0);
    color = c;
    lines[0].color = c;
    lines[1].color = c;
    x58 = on ? 1 : 0;
  }
  changed = 1;
}

// Mech3.exe 1.2 @ 0x0043d6d0: the zoomed radar centred on the display, its
// radius 180 at 480 lines.
void HudRadar::attach(void* hud) {
  if (!hud) return;
  HudHooks& k = *g_hooks;
  source = hud;
  Mech* m = static_cast<Hud*>(hud)->mech;
  set_amber(m && k.mech_flag_bc4(m));
  cx[1] = k.display_w() / 2;
  cy[1] = k.display_h() / 2;
  r[1] = k.display_h() * 0xb4 / 0x1e0;
  to_r = r[zoomed];
  to_x = cx[zoomed];
  to_y = cy[zoomed];
  set_pos(cx[zoomed], cy[zoomed]);
  radius = r[zoomed];
  radius_sq = radius * radius;
  x_steps = std::abs(to_x - get_x());
  y_steps = std::abs(to_y - get_y());
  r_steps = std::abs(to_r - radius);
  changed = 1;
  set_fov(k.player_camera_fov());
}

// Mech3.exe 1.2 @ 0x0043db70
void HudRadar::show_for(float seconds) {
  to_r = r[zoomed];
  Object2D::show_for(seconds);
  lines[0].show_for(seconds);
  lines[1].show_for(seconds);
  if (xbd4 > 3) polys[0].show_for(-1.0f);
  if (xbd8 > 3) polys[1].show_for(-1.0f);
  changed = 1;
}

// Mech3.exe 1.2 @ 0x0043f500
void HudCompass::read_layout(const Hud* hud) {
  if (!hud || !hud->block) return;
  zrdr::Node* n = zrdr::find(hud->block, "COMPASS");
  int32_t a[2];
  if (ivec2(n, "tape", a)) tape.set_pos(a[0], a[1]);
  if (ivec2(n, "frame", a)) frame.set_pos(a[0], a[1]);
}

// Mech3.exe 1.2 @ 0x00444dc0: the frames moved, then the texts, bars and
// heat marks laid out again from them.
void HudGauges::read_layout(const Hud* hud) {
  if (!hud || !hud->block) return;
  zrdr::Node* n = zrdr::find(hud->block, "GAUGES");
  int32_t a[2];
  if (ivec2(n, "speed_frame", a)) speed_frame.set_pos(a[0], a[1]);
  if (ivec2(n, "fuel_frame", a)) fuel_frame.set_pos(a[0], a[1]);
  if (ivec2(n, "heat_bar", a)) heat_bar.set_pos(a[0], a[1]);
  if (ivec2(n, "heat_meter", a)) heat_meter.set_pos(a[0], a[1]);
  if (ivec2(n, "heat_text", a)) fuel_text.set_pos(a[0], a[1]);
  if (ivec2(n, "heat_arrow", a)) toggler.set_pos(a[0], a[1]);
  if (ivec2(n, "coolant_frame", a)) coolant_frame.set_pos(a[0], a[1]);
  speed_text.set_pos(speed_frame.get_x() + 0x1b, speed_frame.get_y() + 0x30);
  coolant_text.set_pos(coolant_frame.get_x() + 0x16, coolant_frame.get_y() + 0x21);
  speed_bar.set_box(speed_frame.get_x() + 0xf, speed_frame.get_y() + 5, 4, 0x2f);
  coolant_bar.set_box(coolant_frame.get_x() + 9, coolant_frame.get_y() + 7, 4, image_h(coolant_frame) - 8);
  fuel_bar.set_box(fuel_frame.get_x() + 0x10, fuel_frame.get_y() + 5, 4, image_h(fuel_frame) - 8);
  toggler_y = heat_meter.get_y() - 1 + (image_h(heat_meter) - (image_h(toggler) >> 1));
  x1444 = 0;
  heat_bottom = heat_bar.get_y() + image_h(heat_bar);
  heat_h = image_h(heat_bar);
  x1440 = 0;
  heat_w = heat_bar.image ? heat_bar.image->width : 0;
}

// Mech3.exe 1.2 @ 0x00448200: with a mech, the lines go below the point
// unless the weapons and the listed parts' equipment pass 16.
void HudEquipment::read_layout(Hud* h) {
  if (!h || !h->mech) {
    hud = nullptr;
    return;
  }
  HudHooks& k = *g_hooks;
  if (h->block) {
    if (zrdr::Node* n = zrdr::find(h->block, "EQUIPMENT")) {
      zrdr::get_int(n, "width", &column);
      int32_t a[2];
      if (ivec2(n, "position", a)) place(a[0], a[1], 0);
    }
  }
  hud = h;
  for (HudEquipLine& l : lines) l.reset();
  panel.set_opacity(hud_opacity());
  panel.set_color(zvid::pack_bytes(0, 0x40, 0));  // 0x0043aae0
  int32_t n = k.weapons_count(k.mech_weapons(h->mech));
  for (int32_t i : kEquipParts) {
    const int32_t c = k.mech_part_count(h->mech, i);
    if (c > 0) n += c;
  }
  below = n < 0x11;
}

// Mech3.exe 1.2 @ 0x00446120
void HudWeapons::read_layout(Hud* h) {
  x1143c = 0;
  if (!h || !h->mech) {
    weapons = 0;
    hud = nullptr;
    return;
  }
  HudHooks& k = *g_hooks;
  if (h->block) {
    if (zrdr::Node* n = zrdr::find(h->block, "WEAPONS")) {
      zrdr::get_int(n, "width", &column);
      int32_t a[2];
      if (ivec2(n, "position", a)) set_pos(a[0], a[1]);
    }
  }
  hud = h;
  weapons = k.mech_weapons(h->mech);
  for (HudWeaponLine& l : lines) l.set_weapon(nullptr);
  k.weapons_refresh(this);
}

// Mech3.exe 1.2 @ 0x00445d10 (with a weapon: its name and the look, not ported yet)
void HudWeaponLine::set_weapon(void* w) {
  weapon = w;
  show_for(-1.0f);
  if (w) {
    g_hooks->unported("0x00445d10", {g_hooks->id(this), g_hooks->id(w)});
    return;
  }
  hide();
  hide();  // 0x00445dc0 with no weapon
}

// Mech3.exe 1.2 @ 0x004479a0
void HudDamageView::move(int32_t px, int32_t py) {
  for (HudDamagePart& p : parts) p.set_pos(p.images->x + px, p.images->y + py);  // 0x00447530
  bg.set_pos(px, py);
  rect = {px, py, px + g_damage.bg_w, py + g_damage.bg_h};
}

// Mech3.exe 1.2 @ 0x0043f890
void HudZoomView::place(int32_t px, int32_t py, int32_t w, int32_t h) {
  rect = {px, py, w + px, h + py};
  view.window->set_area(rect);
}

// Mech3.exe 1.2 @ 0x004427b0
void HudMfdView::place(int32_t px, int32_t py, int32_t w, int32_t h) {
  HudZoomView::place(px, py, w, h);
  panel.set_size(w, h);
  panel.set_pos(px, py);
  box.set_point(0, px, py);
  box.set_point(1, w + px, py);
  box.set_point(2, w + px, h + py);
  box.set_point(3, px, h + py);
  box.set_point(4, px, py);
}

// Mech3.exe 1.2 @ 0x00442230
void HudMfd::place(int32_t px, int32_t py, int32_t w, int32_t h) {
  damage.move(px, py);
  view->place(px, py, w, h);
  g_hooks->nav_list_place(nav, px, py + 0xe, h - 0xf, 6, 4);  // [0x005e3a00], [0x005e3a04]
  title.set_pos(px, py - title.text_height() - 1);
}

// Mech3.exe 1.2 @ 0x004423c0
void HudMfd::attach(void* h) {
  hud = h;
  view->set_source(h);
  Mech* m = h ? static_cast<Hud*>(h)->mech : nullptr;
  damage.set_mech(m);
  g_hooks->nav_list_attach(nav, m);
}

// Mech3.exe 1.2 @ 0x00440110
void HudTarget::place(int32_t px, int32_t py, int32_t w, int32_t h) {
  HudMfd::place(px, py, w, h);
  texts[0].set_pos(px, py + line_h * -2 - 1);
  texts[1].set_pos(px, py - line_h - 1);
  texts[2].set_pos(px, py + line_h * -3 - 1);
}

// Mech3.exe 1.2 @ 0x00441760
void HudTarget::attach(void* h) {
  HudMfd::attach(h);
  update_texts(false);
}

// Mech3.exe 1.2 @ 0x00444250
void HudReticle::attach(void* h) {
  if (h) source = h;
  zoom.set_source(source);
  reset();
}

// Mech3.exe 1.2 @ 0x00443cc0: the lock tone's channels stopped.
void HudReticle::reset() {
  x950 = 0;
  x954 = 0.6f;  // [0x005e3a0c]
  g_hooks->hud_sound_stop(0, 0);
  g_hooks->hud_sound_stop(0xc, 0);
}

// Mech3.exe 1.2 @ 0x0043b190. The pieces are made in the members' order.
Hud::Hud(Mech* m) {
  if (zfix::options().bugs) block = nullptr;
  load_damage_set();
  mech = nullptr;
  setup(m);
  add(&compass);
  add(&gauges);
  add(&target);
  add(&mfd);
  add(&weapons);
  add(&equipment);
  timer.show_for(-1.0f);
}

}  // namespace mech3
