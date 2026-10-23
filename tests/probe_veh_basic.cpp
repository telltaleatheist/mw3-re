// Probes for test_veh_basic.py (docs/vehicles.md): the basic vehicle's entry
// 0x004ac320, constructor 0x004ac530, destructor 0x004ac650 and the HUD
// markers 0x004ac3d0 (mech3/veh_basic_object). Every call out goes to the
// shared callback; the vehicle takes the original's block address for the
// arguments that name it, and its image is laid out as the original's 0xfc
// bytes.
#include <cstdint>
#include <cstring>

#include "gamez/zgame/unported.h"
#include "mech3/veh_basic_object.h"
#include "probe_app.h"

namespace {
namespace m3 = mech3;

uint32_t lo(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
m3::Handle hd(uint32_t v) { return reinterpret_cast<m3::Handle>(static_cast<uintptr_t>(v)); }

uint32_t g_addr = 0;  // the original's block for the next vehicle
m3::BasicVehicle* g_v = nullptr;

struct Hooks : m3::VehBasicHooks {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return probe_app::call(key, args);
  }
  uint32_t ref(const void* p) override { return p == g_v ? g_addr : lo(p); }
  void on_new(uint32_t size, const void* p) override {
    (void)size;
    g_v = static_cast<m3::BasicVehicle*>(const_cast<void*>(p));
  }
  // The stand-in keeps the node only (the test's 0x004d6c90 writes +0x04).
  void entity_set_node(m3::Vehicle& v, m3::Handle node) override {
    unported("0x004d6c90", {ref(&v), low(node), 0, 0, 0});
    v.node = node;
  }
  void weapons_free(void* w) override { unported("0x004b01d0", {low(w)}); }
};
Hooks g_hooks;

void put(uint8_t* b, uint32_t off, uint32_t v) { std::memcpy(b + off, &v, 4); }
void putf(uint8_t* b, uint32_t off, float v) { std::memcpy(b + off, &v, 4); }
void put3(uint8_t* b, uint32_t off, const m3::Vec3& v) { putf(b, off, v.x), putf(b, off + 4, v.y), putf(b, off + 8, v.z); }

}  // namespace

extern "C" {

void probe_vb_hooks(int on) { m3::set_veh_basic_hooks(on ? &g_hooks : nullptr); }

// 0x004ac320(ctl, type, name) as the original's block `addr` would be; 1 when
// a vehicle was made.
int probe_vb_create(uint32_t addr, uint32_t ctl, const char* type, const char* name) {
  g_addr = addr;
  g_v = nullptr;
  return m3::vehicle_create(hd(ctl), type, name) != nullptr;
}

// The fields the destructor reads, set as the test set the original's.
void probe_vb_set(uint32_t ammo, uint32_t net_collision, uint32_t anim, uint32_t engine) {
  g_v->ammo = hd(ammo);
  g_v->net_collision = reinterpret_cast<m3::NetCollision*>(hd(net_collision));
  g_v->motion.anim = hd(anim);
  g_v->engine.entry = hd(engine);
}

void probe_vb_destroy() {
  m3::vehicle_destroy(*g_v);
  g_v->ammo = nullptr, g_v->net_collision = nullptr;  // fakes: not the port's to free
  delete g_v;
  g_v = nullptr;
}

// The vehicle as the original's 0xfc bytes (the vtables and the tracked list's
// pointers, +0x88..+0x93, left as the test fills them).
void probe_vb_image(uint8_t* b) {
  const m3::BasicVehicle& v = *g_v;
  put(b, 4, lo(v.node)), b[8] = v.active, b[9] = v.x09, put(b, 0xc, lo(v.transform));
  put(b, 0x10, lo(v.target_node)), put(b, 0x14, lo(v.centroid));
  put3(b, 0x18, v.x18), put3(b, 0x24, v.forward), put3(b, 0x30, v.velocity), put3(b, 0x3c, v.vec_3c);
  for (int k = 0; k < 4; ++k) putf(b, 0x48 + 4 * k, v.orientation[k]);
  put3(b, 0x58, v.vec_58), b[0x64] = v.x64;
  putf(b, 0x68, v.height);
  put(b, 0x6c, lo(v.target)), put(b, 0x70, lo(v.ally_target)), put(b, 0x74, lo(v.enemy_target));
  put(b, 0x78, lo(v.saved_target)), put(b, 0x7c, lo(v.own_lance)), put(b, 0x80, lo(v.nearest_lance));
  b[0x84] = v.x84;
  put(b, 0x94, lo(v.own_player)), put(b, 0x98, lo(v.player_contact));
  putf(b, 0x9c, v.sensor_range), putf(b, 0xa0, v.range), putf(b, 0xa4, v.range2);
  b[0xa8] = v.xa8;
  for (int k = 0; k < 3; ++k) put(b, 0xac + 4 * k, 0);  // the contacts: none
  put(b, 0xb8, lo(v.touching));
  b[0xbc] = v.xbc;
  put(b, 0xc0, v.team), put(b, 0xc4, v.allies), put(b, 0xc8, v.enemies), put(b, 0xcc, v.shadow ? 1u : 0u);
  put(b, 0xd0, lo(v.net)), put(b, 0xd4, lo(v.ammo)), put(b, 0xd8, lo(v.net_collision));
  put(b, 0xe0, v.xe0), put(b, 0xe4, lo(v.controls));
  put(b, 0xec, lo(v.engine.entry));
  put(b, 0xf0, v.motion.x00), put(b, 0xf4, lo(v.motion.anim)), put(b, 0xf8, v.motion.x08);
}

void probe_vb_markers(uint32_t* out) {
  m3::vehicle_markers_create();
  const m3::VehicleMarkers& m = m3::vehicle_markers();
  out[0] = lo(m.ally), out[1] = lo(m.enemy), out[2] = lo(m.other);
}

}  // extern "C"
