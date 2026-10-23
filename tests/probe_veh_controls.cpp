// Probes for test_veh_controls.py (docs/vehicles.md): the vehicles' controls
// base, its init 0x004ac8a0, setup 0x004acbc0 and 0x004accd0
// (mech3/veh_controls). Every call out goes to the shared callback; values the
// original's callees hand back through pointers come as "@" answers; the
// definition's reader nodes are mirrored from the emulator's memory.
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "gamez/zreader/zreader.h"
#include "mech3/veh_basic_object.h"
#include "mech3/veh_controls.h"
#include "probe_app.h"

namespace mech3 {
float player_gravity();
}

namespace {
namespace m3 = mech3;
using probe_app::call;

uint32_t lo(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
void* hd(uint32_t v) { return reinterpret_cast<void*>(static_cast<uintptr_t>(v)); }
uint32_t peek32(uint32_t a) { return call("peek32", {a}); }
float peekf(uint32_t a) { return zgame::from_bits(peek32(a)); }
float fans(const char* key) { return zgame::from_bits(call(key, {})); }

std::string text(uint32_t a) {
  std::string t;
  for (uint32_t i = 0; a && i < 256; ++i) {
    const char c = static_cast<char>(call("peek8", {a + i}));
    if (!c) break;
    t.push_back(c);
  }
  return t;
}

// The emulator's reader nodes as the port's: a node, its list one deep.
std::deque<std::vector<zrdr::Node>> g_lists;
std::deque<std::string> g_strings;
std::map<uint32_t, zrdr::Node*> g_mirror;
zrdr::Node* mirror_list(uint32_t list);
zrdr::Node* mirror(uint32_t a) {
  if (!a) return nullptr;
  auto it = g_mirror.find(a);
  if (it != g_mirror.end()) return it->second;
  g_lists.emplace_back(1);
  zrdr::Node* n = &g_lists.back()[0];
  n->type = static_cast<int32_t>(peek32(a));
  if (n->type == zrdr::List) n->list = mirror_list(peek32(a + 4));
  g_mirror[a] = n;
  return n;
}
zrdr::Node* mirror_list(uint32_t list) {
  const int32_t count = static_cast<int32_t>(peek32(list + 4));
  g_lists.emplace_back(static_cast<size_t>(count));
  std::vector<zrdr::Node>& items = g_lists.back();
  for (int32_t i = 0; i < count; ++i) {
    zrdr::Node& it2 = items[static_cast<size_t>(i)];
    it2.type = static_cast<int32_t>(peek32(list + 8 * i));
    const uint32_t v = peek32(list + 8 * i + 4);
    if (it2.type == zrdr::String) {
      g_strings.push_back(text(v));
      it2.s = g_strings.back().data();
    } else if (it2.type == zrdr::List && i > 0) {
      it2.list = mirror_list(v);
    } else {
      it2.i = static_cast<int32_t>(v);
    }
  }
  return items.data();
}

uint32_t g_owner_addr = 0;
uint32_t g_ctl_addr = 0;     // the original's controls block
bool g_run_slots = false;    // 0x004af2a0 itself, not its call
m3::BasicVehicle g_owner;
float g_matrix[12];
std::unique_ptr<m3::VehicleControls> g_c;

struct Hooks : m3::VehControlsHooks {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override { return call(key, args); }
  uint32_t ref(const void* p) override { return p == &g_owner ? g_owner_addr : lo(p); }
  void node_rotation(Handle node, float* x, float* y, float* z) override {
    unported("0x004fbe90", {low(node)});
    *x = fans("@rot.x"), *y = fans("@rot.y"), *z = fans("@rot.z");
  }
  Handle node_find(Handle root, const char* name) override { return h(unported("0x004f9d60", {low(root), name})); }
  Handle node_child(Handle node, const char* name) override { return h(unported("0x004eb0d0", {low(node), name})); }
  void node_position(Handle node, m3::Vec3c& out) override {
    const uint32_t m = unported("0x004fc3f0", {low(node)});
    if (m) out = {peekf(m + 0x24), peekf(m + 0x28), peekf(m + 0x2c)};
  }
  void node_translation(Handle node, float* x, float* y, float* z) override {
    unported("0x004fc020", {low(node)});
    *x = fans("@tr.x"), *y = fans("@tr.y"), *z = fans("@tr.z");
  }
  Handle node_model(Handle node) override {
    unported("0x004eb420", {low(node)});
    return h(call("@model", {}));
  }
  void node_set_model(Handle node, Handle model) override { unported("0x004eb370", {low(node), low(model)}); }
  void model_release(Handle model) override { unported("0x00539150", {low(model)}); }
  void node_remove_child(Handle parent, Handle child) override { unported("0x004ebb40", {low(parent), low(child)}); }
  void node_free(Handle node) override { unported("0x004f9050", {low(node)}); }
  zrdr::Node* reader_find(zrdr::Node* node, const char* key) override {
    return mirror(unported("0x005569e0", {low(node), key}));
  }
  Handle anim_find(const char* name) override { return h(unported("0x0050b450", {name})); }
  void anim_start(Handle def, Handle node) override { unported("0x00508c10", {low(def), low(node), 0, 0, 0}); }
  void owner_weapons(void* owner, zrdr::Node* def) override { unported("0x004b4660", {ref(owner), low(def)}); }
  bool reader_float(zrdr::Node* node, const char* key, float* out) override {
    const bool ok = unported("0x00556a60", {low(node), key}) != 0;
    const uint32_t v = call("@float", {});
    if (ok) *out = zgame::from_bits(v);
    return ok;
  }
  void report(int32_t level, const char* file, int32_t line, const char* message) override {
    unported("0x0049bfd0", {level, file, line, message});
  }
  void weapon_slots(m3::VehicleControls& c, void* owner, zrdr::Node* def) override {
    if (g_run_slots) return m3::vehicle_weapon_slots(c, owner, def);
    unported("0x004af2a0", {ref(owner), low(def)});
  }
  Handle weapon_def(const char* name) override { return h(unported("0x00581ed0", {name})); }
  const char* weapon_def_name(Handle def) override {
    static std::deque<std::string> names;
    names.push_back(text(peek32(low(def) + 4)));
    return names.back().c_str();
  }
  uint32_t weapon_def_flags(Handle def) override { return peek32(low(def) + 0x6c); }
  void node_set_active(Handle node, int32_t on) override { unported("0x004eb170", {low(node), on}); }
  Handle beam_make(Handle def, Handle owner_node, Handle node, const m3::Vec3c* muzzle, const m3::Vec3c* dir,
                   int32_t segments) override {
    return h(unported("0x00584a70", {low(def), low(owner_node), 0, low(node), field(muzzle), field(dir), segments}));
  }
  // A pointer into the port's controls as the same field of the original's.
  static uint32_t field(const void* p) {
    const auto* b = reinterpret_cast<const uint8_t*>(g_c.get());
    const auto* q = static_cast<const uint8_t*>(p);
    if (p == &g_c->muzzle) return g_ctl_addr + 0x608;
    if (p == &g_c->aim_dir) return g_ctl_addr + 0x5f0;
    return lo(q) - lo(b);
  }
};
Hooks g_hooks;

// The placement's calls (mech3/veh_basic_object).
struct VehHooks : m3::VehBasicHooks {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override { return call(key, args); }
  uint32_t ref(const void* p) override { return p == &g_owner ? g_owner_addr : lo(p); }
  int32_t ground(float x, float y, float z, zcls::GroundHits* out) override {
    const int32_t r = static_cast<int32_t>(call("0x004edab0", {zgame::bits(x), zgame::bits(y), zgame::bits(z)}));
    out->count = static_cast<uint8_t>(call("@hits.count", {}));
    out->hit[0].height = fans("@hits.height");
    return r;
  }
};
VehHooks g_veh_hooks;

void put(uint8_t* b, uint32_t off, uint32_t v) { std::memcpy(b + off, &v, 4); }
void putf(uint8_t* b, uint32_t off, float v) { std::memcpy(b + off, &v, 4); }
void put3(uint8_t* b, uint32_t off, const m3::Vec3c& v) { putf(b, off, v.x), putf(b, off + 4, v.y), putf(b, off + 8, v.z); }

}  // namespace

extern "C" {

// A fresh base over the owner at the original's `owner` (its node `node`),
// the serial counter at `serial`. The definition's mirrors are dropped.
void probe_vc_new(uint32_t owner, uint32_t node, int32_t serial) {
  m3::set_veh_controls_hooks(&g_hooks);
  g_mirror.clear(), g_lists.clear(), g_strings.clear();
  g_owner_addr = owner;
  g_owner.node = hd(node);
  std::memset(g_matrix, 0, sizeof g_matrix);
  g_owner.transform = g_matrix;
  g_c = std::make_unique<m3::VehicleControls>();
  g_c->owner = owner ? &g_owner : nullptr;
  m3::vehicle_serial() = serial;
}

// The +0x398 block as the test set the original's (the clear's work).
void probe_vc_set398(const uint32_t* v) {
  m3::Block398& b = g_c->x398;
  b.flags = v[0], b.x04 = v[1], b.x08 = v[2], b.x0c = v[3], b.x10 = v[4], b.x14 = v[5];
}

// For 0x004af2a0 alone: the original's block, the serial and gun it has,
// a slot's words as the test set them (def, index, node, node_r, x10, flags,
// x18, x1c, x20, x24, x28, x2c, x30, beam).
void probe_vc_slots_state(uint32_t ctl, int32_t serial, uint32_t gun) {
  g_ctl_addr = ctl;
  g_c->serial = serial;
  g_c->gun = hd(gun);
}
void probe_vc_set_slot(int i, const uint32_t* w) {
  m3::WeaponSlot& s = g_c->slots[i];
  s.def = hd(w[0]), s.index = static_cast<int32_t>(w[1]), s.node = hd(w[2]), s.node_r = hd(w[3]);
  s.flags = w[5], s.x18 = zgame::from_bits(w[6]), s.x24 = w[9], s.x28 = w[10], s.x2c = w[11], s.x30 = w[12];
  s.beam = hd(w[13]);
}
void probe_vc_weapon_slots(uint32_t def) {
  g_run_slots = true;
  m3::vehicle_weapon_slots(*g_c, &g_owner, reinterpret_cast<zrdr::Node*>(hd(def)));
  g_run_slots = false;
}

// For the placement: the controls' fields it reads and writes, as the test
// set the original's (throttle, turn, serial, x20, x24, x70 xyz, angles y,
// point_count, the support count), the 15 points and the 16 support words.
void probe_vc_place_state(const uint32_t* w, const float* points, const uint32_t* words) {
  m3::VehicleControls& c = *g_c;
  c.throttle = zgame::from_bits(w[0]), c.turn = zgame::from_bits(w[1]), c.serial = static_cast<int32_t>(w[2]);
  c.x20 = w[3], c.x24 = w[4];
  c.x70 = {zgame::from_bits(w[5]), zgame::from_bits(w[6]), zgame::from_bits(w[7])};
  c.angles.y = zgame::from_bits(w[8]);
  c.point_count = static_cast<int32_t>(w[9]), c.motion.support_count = static_cast<int32_t>(w[10]);
  for (int i = 0; i < 15; ++i) c.points[i] = {points[3 * i], points[3 * i + 1], points[3 * i + 2]};
  for (int i = 0; i < 16; ++i) c.motion.support_word[i] = words[i];
}
void probe_vc_place(const float* pos, float yaw) {
  m3::set_veh_basic_hooks(&g_veh_hooks);
  g_owner.controls = g_c.get();
  m3::vehicle_place(g_owner, pos, yaw);
  g_owner.controls = nullptr;
  m3::set_veh_basic_hooks(nullptr);
}

// 0x0049ef40 on the controls' block: the node, the mode's reader node (the
// original's), the names; the collision count through `count`.
void probe_vc_motion(uint32_t node, uint32_t mode, const char* name, const char* type, int32_t* count) {
  m3::vehicle_motion_read(*g_c, hd(node), reinterpret_cast<zrdr::Node*>(hd(mode)), name, type, count);
}
void probe_vc_set_mode(int32_t mode) { g_c->motion.mode = mode; }

// The speeds and turns (vt+0x114..+0x13c): the motion and controls values
// they read, then the slot's function (0..10 for +0x114..+0x13c).
void probe_vc_speed_state(float top, float slowing, float turn_rate, float throttle, float x70z) {
  g_c->motion.rates[1] = top, g_c->motion.rate_damping[1] = slowing, g_c->motion.turns[1] = turn_rate;
  g_c->throttle = throttle, g_c->x70.z = x70z;
}
float probe_vc_speed(int which, float a, float b) {
  g_owner.controls = g_c.get();
  float r = 0;
  switch (which) {
    case 0: r = m3::vehicle_move_speed(g_owner); break;
    case 1: r = m3::vehicle_speed_at(g_owner, a); break;
    case 2: r = m3::vehicle_share_for(g_owner, a); break;
    case 3: r = m3::vehicle_stopping_distance(g_owner, a); break;
    case 4: r = m3::vehicle_stopping_now(g_owner); break;
    case 5: r = m3::vehicle_stopping_at(g_owner, a); break;
    case 6: r = m3::vehicle_stop_share(g_owner, a); break;
    case 7: r = m3::vehicle_turn_time(g_owner, a); break;
    case 8: r = m3::vehicle_turn_time_now(g_owner); break;
    case 9: r = m3::vehicle_turn_time_at(g_owner, a); break;
    case 10: r = m3::vehicle_turn_share(g_owner, a, b); break;
  }
  g_owner.controls = nullptr;
  return r;
}

void probe_vc_init() { g_c->init(); }
void probe_vc_setup(uint32_t def) { g_c->setup(reinterpret_cast<zrdr::Node*>(hd(def))); }
void probe_vc_reset_1c() { g_c->reset_1c(); }
int32_t probe_vc_serial() { return m3::vehicle_serial(); }
float probe_vc_gravity() { return m3::player_gravity(); }

// The owner's matrix (the 3x3 the init writes).
void probe_vc_matrix(float* out) { std::memcpy(out, g_matrix, 9 * sizeof(float)); }

// The base as the original's bytes up to +0x7a4 (left as the test filled the
// rest).
void probe_vc_image(uint8_t* b) {
  const m3::VehicleControls& c = *g_c;
  putf(b, 0x08, c.throttle), putf(b, 0x0c, c.turn);
  put(b, 0x14, static_cast<uint32_t>(c.serial)), putf(b, 0x1c, c.x1c);
  put(b, 0x20, c.x20), put(b, 0x24, c.x24), put3(b, 0x70, c.x70);
  for (int i = 0; i < 15; ++i) put3(b, 0x160 + 12 * static_cast<uint32_t>(i), c.points[i]);
  put(b, 0x214, static_cast<uint32_t>(c.point_count));
  const m3::VehicleMotion& m = c.motion;
  const uint32_t mb = 0x764;
  put(b, mb, static_cast<uint32_t>(m.mode));
  putf(b, mb + 0x4, m.rates[0]), putf(b, mb + 0x8, m.rates[1]), putf(b, mb + 0xc, m.turns[0]), putf(b, mb + 0x10, m.turns[1]);
  putf(b, mb + 0x14, m.turn_damping), putf(b, mb + 0x18, m.rate_damping[0]), putf(b, mb + 0x1c, m.rate_damping[1]);
  putf(b, mb + 0x20, m.gun_hi), putf(b, mb + 0x24, m.gun_lo);
  for (int k = 0; k < 3; ++k) putf(b, mb + 0x58 + 4 * static_cast<uint32_t>(k), m.friction[k]);
  putf(b, mb + 0x64, m.stopping), putf(b, mb + 0x68, m.chas_smooth), put3(b, mb + 0x6c, m.chas_pitch);
  put3(b, mb + 0x78, m.chas_roll);
  put(b, mb + 0x84, static_cast<uint32_t>(m.support_count)), putf(b, mb + 0x88, m.mode_alt);
  for (int i = 0; i < 16; ++i) put3(b, mb + 0x8c + 12 * static_cast<uint32_t>(i), m.support[i]);
  for (int i = 0; i < 16; ++i) put(b, mb + 0x14c + 4 * static_cast<uint32_t>(i), m.support_word[i]);
  putf(b, mb + 0x18c, m.quicksand_slowdown), putf(b, mb + 0x190, m.lava_slowdown);
  for (int i = 0; i < 12; ++i) put3(b, mb + 0x194 + 12 * static_cast<uint32_t>(i), m.collision[i]);
  putf(b, mb + 0x248, m.mass), putf(b, mb + 0x24c, m.inv_mass), putf(b, mb + 0x250, m.a_damping);
  put(b, mb + 0x258, m.x258), put(b, mb + 0x260, m.x260);
  for (int k = 0; k < 7; ++k) putf(b, mb + 0x264 + 4 * static_cast<uint32_t>(k), m.wave[k]);
  putf(b, mb + 0x280, m.collision_d[0]), putf(b, mb + 0x284, m.collision_d[1]);
  put(b, mb + 0x288, lo(m.right_morphs)), put(b, mb + 0x28c, lo(m.left_morphs)), put(b, mb + 0x290, lo(m.chassis));
  put(b, mb + 0x294, lo(m.rtracks)), put(b, mb + 0x298, lo(m.ltracks)), put(b, mb + 0x29c, lo(m.destroy_anim));
  putf(b, 0x794, c.turret.lo_sin), putf(b, 0x798, c.turret.lo_cos), putf(b, 0x79c, c.turret.hi_sin);
  putf(b, 0x7a0, c.turret.hi_cos);
  put(b, 0x28, c.x28), put(b, 0x2c, c.x2c), put(b, 0x30, c.x30), put(b, 0x3c, c.x3c);
  put3(b, 0x50, c.x50);
  for (int k = 0; k < 4; ++k) put(b, 0x94 + 4 * k, c.x94[k]);
  put3(b, 0x308, c.up), put3(b, 0x320, c.angles0), put3(b, 0x32c, c.angles);
  put(b, 0x350, c.x350);
  put(b, 0x398, c.x398.flags), put(b, 0x39c, c.x398.x04), put(b, 0x3a0, c.x398.x08), put(b, 0x3a4, c.x398.x0c);
  put(b, 0x3a8, c.x398.x10), put(b, 0x3ac, c.x398.x14);
  put(b, 0x3b0, lo(c.node));
  put(b, 0x3b4, c.first ? g_ctl_addr + 0x3b8 + 0x38 * static_cast<uint32_t>(c.first - c.slots) : 0);
  for (int i = 0; i < 10; ++i) {
    const m3::WeaponSlot& s = c.slots[i];
    const uint32_t o = 0x3b8 + 0x38 * static_cast<uint32_t>(i);
    put(b, o, lo(s.def)), put(b, o + 4, static_cast<uint32_t>(s.index)), put(b, o + 8, lo(s.node));
    put(b, o + 0xc, lo(s.node_r)), put(b, o + 0x14, s.flags), putf(b, o + 0x18, s.x18), put(b, o + 0x24, s.x24);
    put(b, o + 0x28, s.x28), put(b, o + 0x2c, s.x2c), put(b, o + 0x30, s.x30), put(b, o + 0x34, lo(s.beam));
  }
  put(b, 0x5e8, c.x5e8), put(b, 0x5ec, static_cast<uint32_t>(c.x5ec)), put3(b, 0x5f0, c.aim_dir), put3(b, 0x5fc, c.x5fc);
  put3(b, 0x608, c.muzzle), put3(b, 0x614, c.fire_l), put3(b, 0x620, c.fire_r);
  put3(b, 0x62c, c.fpnt_c), put3(b, 0x638, c.fpnt_r), put3(b, 0x644, c.fpnt_l);
  put(b, 0x650, static_cast<uint32_t>(c.x650));
  put3(b, 0x6d8, c.gun_pos);
  put(b, 0x700, lo(c.body)), put(b, 0x704, lo(c.turret_node)), put(b, 0x708, lo(c.gun)), put(b, 0x70c, lo(c.shadow));
  putf(b, 0x724, c.fire_c_scale), putf(b, 0x740, c.health), putf(b, 0x744, c.health_max);
  put(b, 0x760, c.x760);
}

}  // extern "C"
