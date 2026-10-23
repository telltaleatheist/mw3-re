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
zrdr::Node* mirror(uint32_t a) {
  if (!a) return nullptr;
  auto it = g_mirror.find(a);
  if (it != g_mirror.end()) return it->second;
  g_lists.emplace_back(1);
  zrdr::Node* n = &g_lists.back()[0];
  n->type = static_cast<int32_t>(peek32(a));
  if (n->type == zrdr::List) {
    const uint32_t list = peek32(a + 4);
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
      } else {
        it2.i = static_cast<int32_t>(v);
      }
    }
    n->list = items.data();
  }
  g_mirror[a] = n;
  return n;
}

uint32_t g_owner_addr = 0;
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
};
Hooks g_hooks;

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
  put(b, 0x14, static_cast<uint32_t>(c.serial)), putf(b, 0x1c, c.x1c);
  put(b, 0x28, c.x28), put(b, 0x2c, c.x2c), put(b, 0x30, c.x30), put(b, 0x3c, c.x3c);
  put3(b, 0x50, c.x50);
  for (int k = 0; k < 4; ++k) put(b, 0x94 + 4 * k, c.x94[k]);
  put3(b, 0x308, c.up), put3(b, 0x320, c.angles0), put3(b, 0x32c, c.angles);
  put(b, 0x350, c.x350);
  put(b, 0x398, c.x398.flags), put(b, 0x39c, c.x398.x04), put(b, 0x3a0, c.x398.x08), put(b, 0x3a4, c.x398.x0c);
  put(b, 0x3a8, c.x398.x10), put(b, 0x3ac, c.x398.x14);
  put(b, 0x3b0, lo(c.node));
  put(b, 0x5e8, c.x5e8), put(b, 0x5ec, static_cast<uint32_t>(c.x5ec)), put3(b, 0x5fc, c.x5fc);
  put3(b, 0x608, c.fire_c_default), put3(b, 0x614, c.fire_l_default), put3(b, 0x620, c.fire_r_default);
  put3(b, 0x62c, c.fpnt_c), put3(b, 0x638, c.fpnt_r), put3(b, 0x644, c.fpnt_l);
  put(b, 0x650, static_cast<uint32_t>(c.x650));
  put3(b, 0x6d8, c.gun_pos);
  put(b, 0x700, lo(c.body)), put(b, 0x704, lo(c.turret_node)), put(b, 0x708, lo(c.gun)), put(b, 0x70c, lo(c.shadow));
  putf(b, 0x724, c.fire_c_scale), putf(b, 0x740, c.health), putf(b, 0x744, c.health_max);
  put(b, 0x760, c.x760);
}

}  // extern "C"
