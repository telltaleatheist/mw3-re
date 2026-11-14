#include "pm/gamez/zclass/light.h"

#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/list.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zvideo/zvid_ddd3d.h"
#include "gamez/zmodel/gmod_light.h"

namespace pm::zcls {

using ::zcls::Node;

namespace {
int32_t g_stale_hold = 0;  // [0x00630064]
}  // namespace

// Pirate's Moon 1.0 @ 0x004ff730
void set_stale_hold(int32_t on) { g_stale_hold = on; }
// Pirate's Moon 1.0 @ 0x004ff740
int32_t stale_hold() { return g_stale_hold; }
// Pirate's Moon 1.0 @ 0x004ff750: a static light's change makes the world's bake stale, unless held.
void stale() {
  if (!g_stale_hold) ::zcls::node_touch_frame();
}

namespace {
const char kFile[] = "C:\\zipper\\branchep1\\gamez\\zClass\\Light.c";
}  // namespace

// PM's offsets: +0x00..+0xaf as MW3's, +0xb0..+0xdf pm_colors, +0xe0 on
// MW3's +0xb0 on.
Node* light_create() {
  Node* node = ::zcls::node_alloc();
  if (!node) return zerr::report(0x400, kFile, 0xd5, "Null node pointer."), nullptr;
  node->put(0x74, 1.0f), node->put(0x78, 1.0f), node->put(0x7c, -2.0f);  // through the box pointer (+0x70)
  node->put(0x80, 2.0f), node->put(0x84, 2.0f), node->put(0x88, -1.0f);
  node->set_flags(node->flags() | 0x100);
  node->put<int32_t>(0x34, 9);
  auto* l = new ::zcls::Light{};
  node->data = l;
  l->put(0x84, 0.0f), l->put(0x88, 1.0f), l->put(0x8c, 0.0f);
  l->put(0x60, 0.0f), l->put(0x64, 0.0f), l->put(0x68, 0.0f);
  l->put(0xa4, 1.0f), l->put(0xa8, 1.0f), l->put(0xac, 1.0f);
  l->put(0x9c, 0.0f), l->put(0xa0, 1.0f);
  const float colors[12] = {1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1};  // PM +0xb0..+0xdc
  for (int i = 0; i < 12; ++i) l->pm_colors[i] = colors[i];
  l->put(0xb4, 32.0f), l->put(0xb8, 64.0f), l->put(0xbc, 1024.0f), l->put(0xc0, 4096.0f);  // PM +0xe4..
  l->put(0xc4, 0.03125f);
  l->put<uint32_t>(0xb0, (l->get<uint32_t>(0xb0) & 0xfffff8b3u) | 0x8b3);  // PM +0xe0
  ::zcls::node_set_active(node, 1);
  l->put<int32_t>(0xc8, 0);  // PM +0xf8, +0xfc
  l->nodes = nullptr;
  ::zcls::push_front(::zcls::kLights, node);
  return node;
}

namespace {
constexpr uint32_t kRecalc = 1, kStatic = 0x200, kOwn = 0x400;

// PM's +0xb0..+0xdc: ambient colour, diffuse mixed, ambient mixed, the two summed.
struct Colors {
  float amb[3], dif[3], amb_mix[3], sum[3];
};
Colors& colors(::zcls::Light& l) {
  static_assert(sizeof(Colors) == sizeof l.pm_colors);
  return *reinterpret_cast<Colors*>(l.pm_colors);
}
uint32_t flags_of(const ::zcls::Light& l) { return l.get<uint32_t>(0xb0); }
void set_flags(::zcls::Light& l, uint32_t f) { l.put<uint32_t>(0xb0, f); }

::zcls::Light* data_of(Node* node, int line) {
  if (!node) return zerr::report(0x400, kFile, line, "Null node pointer."), nullptr;
  auto* l = static_cast<::zcls::Light*>(node->data);
  if (!l) zerr::report(0x400, kFile, line + 1, "Null class data pointer");
  return l;
}


void sum_colors(::zcls::Light& l) {
  Colors& c = colors(l);
  for (int k = 0; k < 3; ++k) c.sum[k] = c.dif[k] + c.amb_mix[k];
}

// The static bit follows: on a light flagged 0x100, none of 0x400 and 0xe0, white.
uint32_t with_static(const ::zcls::Light& l, uint32_t f) {
  const bool on = (f & 0x100) && !(f & kOwn) && !(f & 0xe0) && l.get<uint32_t>(0xa4) == 0x3f800000u &&
                  l.get<uint32_t>(0xa8) == 0x3f800000u && l.get<uint32_t>(0xac) == 0x3f800000u;
  return (f & ~kStatic) | (on ? kStatic : 0) | kRecalc;
}
}  // namespace

// Pirate's Moon 1.0 @ 0x0050efc0
int light_set_orientation(Node* node, float x, float y, float z) {
  ::zcls::Light* l = data_of(node, 0x3ac);
  if (!l) return 5;
  const uint32_t f = flags_of(*l);
  l->put(0x0, x), l->put(0x4, y), l->put(0x8, z);
  set_flags(*l, f | kRecalc);
  if (f & kStatic) stale();
  return 0;
}

// Pirate's Moon 1.0 @ 0x0050e6f0
int light_set_ambient(Node* node, float v) {
  ::zcls::Light* l = data_of(node, 0x1b6);
  if (!l) return 5;
  Colors& c = colors(*l);
  const uint32_t f = flags_of(*l);
  l->put(0xa0, v);
  for (int k = 0; k < 3; ++k) c.amb_mix[k] = v * c.amb[k];
  set_flags(*l, f | kRecalc);
  sum_colors(*l);
  if (f & kStatic) stale();
  return 0;
}

// Pirate's Moon 1.0 @ 0x0050e7c0
int light_set_diffuse(Node* node, float v) {
  ::zcls::Light* l = data_of(node, 0x1dc);
  if (!l) return 5;
  Colors& c = colors(*l);
  const uint32_t f = flags_of(*l);
  l->put(0x9c, v);
  for (int k = 0; k < 3; ++k) c.dif[k] = v * l->get<float>(0xa4 + k * 4);
  set_flags(*l, f | kRecalc);
  sum_colors(*l);
  if (f & kStatic) stale();
  return 0;
}

// Pirate's Moon 1.0 @ 0x0050ea00
int light_set_bit4(Node* node) {
  ::zcls::Light* l = data_of(node, 0x2b9);
  if (!l) return 5;
  const uint32_t f = flags_of(*l), now = (f & ~8u) | 0x10;
  set_flags(*l, now);
  if ((f & kStatic) && ((f ^ now) & 0x10)) stale();
  set_flags(*l, now | kRecalc);
  return 0;
}

// Pirate's Moon 1.0 @ 0x0050ea80
int light_set_ambient_only(Node* node, int on) {
  ::zcls::Light* l = data_of(node, 0x2d4);
  if (!l) return 5;
  const uint32_t f = (flags_of(*l) & ~0x20u) | ((on & 1) << 5) | kRecalc;
  const uint32_t now = with_static(*l, f);
  set_flags(*l, now);
  if ((now ^ f) & kStatic) stale();
  return 0;
}

// Pirate's Moon 1.0 @ 0x0050eb60: unlike its neighbour, not flagged for the update.
int light_set_diffuse_only(Node* node, int on) {
  ::zcls::Light* l = data_of(node, 0x2f8);
  if (!l) return 5;
  const uint32_t f = (flags_of(*l) & ~0x40u) | ((on & 1) << 6);
  const uint32_t now = (with_static(*l, f) & ~kRecalc) | (f & kRecalc);
  set_flags(*l, now);
  if ((now ^ f) & kStatic) stale();
  return 0;
}

// Pirate's Moon 1.0 @ 0x0050e900. Bit 2 from on & 1. As MW3's 0x004faa20 the stale test compares the old bit as
// 0 / -1 with the argument itself.
int light_set_bit2(Node* node, int on) {
  ::zcls::Light* l = data_of(node, 0x214);
  if (!l) return 5;
  const uint32_t f = flags_of(*l);
  set_flags(*l, ((static_cast<uint32_t>(on) & 1) << 2) | (f & ~4u));
  if ((f & kStatic) && ((f & 4) ? -1 : 0) != on) stale();
  set_flags(*l, flags_of(*l) | kRecalc);
  return 0;
}

// Pirate's Moon 1.0 @ 0x0050e980: bit 3 on, bit 4 off.
int light_set_bit3(Node* node) {
  ::zcls::Light* l = data_of(node, 0x236);
  if (!l) return 5;
  const uint32_t f = flags_of(*l), now = (f & ~0x10u) | 8;
  set_flags(*l, now);
  if ((f & kStatic) && ((f ^ now) & 8)) stale();
  set_flags(*l, now | kRecalc);
  return 0;
}

namespace {
// 0x0050ec30 / 0x0050ed00: one bit from on & 1, then the static bit again; not flagged for the update.
int set_mode_bit(Node* node, int on, uint32_t bit, int line) {
  ::zcls::Light* l = data_of(node, line);
  if (!l) return 5;
  const uint32_t f = (flags_of(*l) & ~bit) | ((static_cast<uint32_t>(on) & 1) ? bit : 0);
  const uint32_t now = with_static(*l, f) & ~kRecalc;
  set_flags(*l, now | (f & kRecalc));
  if ((now ^ f) & kStatic) stale();
  return 0;
}
}  // namespace

// Pirate's Moon 1.0 @ 0x0050ec30: the light map (bit 7; the script's LightSetLightMap, the light reader, the event).
int light_set_light_map(Node* node, int on) { return set_mode_bit(node, on, 0x80, 0x2aa); }

// Pirate's Moon 1.0 @ 0x0050ed00: MW3's 0x004fad30, bit 8.
int light_set_directional(Node* node, int on) { return set_mode_bit(node, on, 0x100, 0x2c5); }

// Pirate's Moon 1.0 @ 0x0050edd0
int light_set_range(Node* node, float a, float b) {
  ::zcls::Light* l = data_of(node, 0x330);
  if (!l) return 5;
  const float lo = a < b ? a : b;
  float hi = b < a ? a : b;
  if (b == a) {
    zerr::report(0x200, kFile, 0x336, "gwLightSetRanges: r1 == r2");
    hi = lo + 10.0f;  // 0x005de7a8
  }
  const uint32_t f = flags_of(*l);
  l->put(0xb4, lo), l->put(0xb8, hi);
  set_flags(*l, f | kRecalc);
  l->put(0xbc, lo * lo), l->put(0xc0, hi * hi);
  l->put(0xc4, 1.0f / (hi - lo));
  if (f & kStatic) stale();
  return 0;
}

// Pirate's Moon 1.0 @ 0x0050ef50
int light_set_position(Node* node, float x, float y, float z) {
  ::zcls::Light* l = data_of(node, 0x38d);
  if (!l) return 5;
  const uint32_t f = flags_of(*l);
  l->put(0xc, x), l->put(0x10, y), l->put(0x14, z);
  set_flags(*l, f | kRecalc);
  if (f & kStatic) stale();
  return 0;
}

// Pirate's Moon 1.0 @ 0x0050f460: the colour, and both mixes by it.
int light_set_color(Node* node, float r, float g, float b) {
  ::zcls::Light* l = data_of(node, 0x4e3);
  if (!l) return 5;
  Colors& c = colors(*l);
  const float rgb[3] = {r, g, b};
  const float amb = l->get<float>(0xa0), dif = l->get<float>(0x9c);
  const uint32_t f = flags_of(*l) | kRecalc;
  l->put(0xa4, r), l->put(0xa8, g), l->put(0xac, b);
  for (int k = 0; k < 3; ++k) c.amb_mix[k] = rgb[k] * amb, c.dif[k] = rgb[k] * dif;
  sum_colors(*l);
  const uint32_t now = with_static(*l, f);
  set_flags(*l, now);
  if ((now ^ f) & kStatic) stale();
  ::zvid::set_point_light_color(l->floats(0xa4));  // 0x005898c0
  return 0;
}

// Pirate's Moon 1.0 @ 0x0050f600
int light_set_diffuse_color(Node* node, float r, float g, float b) {
  ::zcls::Light* l = data_of(node, 0x504);
  if (!l) return 5;
  Colors& c = colors(*l);
  const uint32_t f = flags_of(*l) | kRecalc;
  const float d = l->get<float>(0x9c);
  l->put(0xa4, r), l->put(0xa8, g), l->put(0xac, b);
  c.dif[0] = r * d, c.dif[1] = g * d, c.dif[2] = b * d;
  sum_colors(*l);
  const uint32_t now = with_static(*l, f);
  set_flags(*l, now);
  if ((now ^ f) & kStatic) stale();
  ::zvid::set_point_light_color(l->floats(0xa4));  // 0x005898c0
  return 0;
}

// Pirate's Moon 1.0 @ 0x0050f760
int light_set_ambient_color(Node* node, float r, float g, float b) {
  ::zcls::Light* l = data_of(node, 0x521);
  if (!l) return 5;
  Colors& c = colors(*l);
  const uint32_t f = flags_of(*l) | kRecalc;
  const float a = l->get<float>(0xa0);
  c.amb[0] = r, c.amb[1] = g, c.amb[2] = b;
  c.amb_mix[0] = r * a, c.amb_mix[1] = g * a, c.amb_mix[2] = b * a;
  const uint32_t now = with_static(*l, f);
  set_flags(*l, now);
  if ((now ^ f) & kStatic) stale();
  return 0;
}

// Pirate's Moon 1.0 @ 0x0050e890
int light_set_own_colors(Node* node, int on) {
  ::zcls::Light* l = data_of(node, 0x1f4);
  if (!l) return 5;
  const uint32_t f = flags_of(*l);
  set_flags(*l, (f & ~kOwn) | (on & 1 ? kOwn : 0) | kRecalc);
  if (f & kStatic) stale();
  return 0;
}

}  // namespace pm::zcls

namespace pm::zcls {

// Pirate's Moon 1.0 @ 0x00507f30's light override (inline there; the bits 0x004fd040 / 0x004fd0d0 /
// 0x004fd160 / 0x004fd1f0, the directed light 0x00563740): node +0x2c bit 7 sets the sun's ambient to
// bits 9..16 / 255, bit 8 its diffuse to bits 17..24 / 255, the bake's stamp held meanwhile. 24 mechlib
// nodes carry it, each mech's cockpit "grill" (an ambient of 0x59).
bool light_override(Node* node, ::zcls::LightOverride* save) {
  const uint32_t bits = node ? node->get<uint32_t>(0x2c) : 0;
  const bool amb = (bits >> 7) & 1, dif = (bits >> 8) & 1;
  if (!amb && !dif) return false;
  const ::zmod::LightList& L = ::zmod::light_list();
  Node* sun = L.directed >= 0 ? L.list[L.directed].node : nullptr;
  if (!sun || !sun->data) return false;
  auto* l = static_cast<::zcls::Light*>(sun->data);
  *save = ::zcls::LightOverride{};
  save->light = sun;
  save->hold = stale_hold();
  set_stale_hold(1);
  if (amb) {
    save->ambient = l->get<float>(0xa0), save->ambient_set = 1;
    pm::zcls::light_set_ambient(sun, static_cast<float>((bits >> 9) & 0xff) * (1.0f / 255.0f));
  }
  if (dif) {
    save->diffuse = l->get<float>(0x9c), save->diffuse_set = 1;
    pm::zcls::light_set_diffuse(sun, static_cast<float>((bits >> 17) & 0xff) * (1.0f / 255.0f));
  }
  return true;
}

// The end of 0x00507f30: the ambient back, then the diffuse, then the hold.
void light_restore(const ::zcls::LightOverride& save) {
  if (!save.light) return;
  if (save.ambient_set) pm::zcls::light_set_ambient(save.light, save.ambient);
  if (save.diffuse_set) pm::zcls::light_set_diffuse(save.light, save.diffuse);
  set_stale_hold(save.hold);
}

}  // namespace pm::zcls
