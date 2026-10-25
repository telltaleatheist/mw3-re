// Probes for test_vehicle_glow.py (docs/vehicles.md, The glow): zWeapon's
// glow 0x005869d0 / 0x00586a30 / 0x00586af0 with the light pool's take and
// return (0x00586b20, 0x00586b70) over a pool the test lays out. Handles are
// the emulator's addresses; the zClass calls go to the shared callback.
#include <cstdint>
#include <cstring>
#include <map>

#include "gamez/zweapon/zweapon.h"
#include "gamez/zweapon/zwep_init.h"
#include "probe_app.h"

namespace {
using zwep::Handle;
std::map<uint32_t, uint32_t> g_links;  // a light's +0x40
uint32_t u(Handle h) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(h)); }
Handle h(uint32_t v) { return reinterpret_cast<Handle>(static_cast<uintptr_t>(v)); }
uint32_t fb(float f) {
  uint32_t i;
  std::memcpy(&i, &f, 4);
  return i;
}
float bf(uint32_t i) {
  float f;
  std::memcpy(&f, &i, 4);
  return f;
}
zwep::WeaponDef g_def;
}  // namespace

extern "C" {

// The pool: `n` lights (their addresses) linked first to last; the world.
void probe_glow_pool(const uint32_t* lights, int32_t n) {
  g_links.clear();
  zwep::RunHooks& r = zwep::run_hooks();
  r = zwep::RunHooks{};
  r.link = [](Handle l) { return h(g_links[u(l)]); };
  r.set_link = [](Handle l, Handle next) { g_links[u(l)] = u(next); };
  r.light_range = [](Handle l, float a, float b) { probe_app::call("0x004fadf0", {u(l), fb(a), fb(b)}); };
  r.light_color = [](Handle l, float x, float y, float z) { probe_app::call("0x004fb510", {u(l), fb(x), fb(y), fb(z)}); };
  r.world_add_light = [](Handle l) { probe_app::call("0x004f2d50", {u(l)}); };
  r.world_remove_light = [](Handle l) { probe_app::call("0x004f2e00", {u(l)}); };
  r.add_child = [](Handle p, Handle c) { probe_app::call("0x004eb9b0", {u(p), u(c)}); };
  r.remove_child = [](Handle p, Handle c) { probe_app::call("0x004ebb40", {u(p), u(c)}); };
  for (int32_t i = 0; i < n; ++i) g_links[lights[i]] = i + 1 < n ? lights[i + 1] : 0;
  zwep::run_state().lights = n ? h(lights[0]) : nullptr;
}

// One op on the glow `g` (7 words in and out: its six, then the node; the
// definition's address, its flags +0x6c and colour +0x1d4): 0 the clear,
// 1 a hit (returns its answer), 2 the end. `pool` out: the head.
int32_t probe_glow_op(int32_t op, uint32_t* g, uint32_t def, uint32_t flags, const uint32_t* color, uint32_t amount,
                      uint32_t* pool) {
  zwep::Glow w;
  w.flags = g[0], w.weapon = h(g[1]), w.x08 = bf(g[2]), w.level = bf(g[3]), w.light = h(g[4]), w.x14 = g[5];
  g_def.flags = flags;
  for (int k = 0; k < 3; ++k) g_def.target_heat[3 + k] = bf(color[k]);
  int32_t r = 0;
  if (op == 0) w.clear();
  else if (op == 1) r = zwep::glow_hit(w, h(g[6]), &g_def, bf(amount));
  else zwep::glow_end(w, h(g[6]));
  g[0] = w.flags, g[1] = w.weapon == &g_def ? def : u(w.weapon), g[2] = fb(w.x08), g[3] = fb(w.level);
  g[4] = u(w.light), g[5] = w.x14;
  *pool = u(zwep::run_state().lights);
  return r;
}

// A light's link (+0x40) in the port's pool.
uint32_t probe_glow_link(uint32_t l) { return g_links[l]; }

}  // extern "C"
