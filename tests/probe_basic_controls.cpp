// Probe for test_basic_controls.py (docs/vehicles.md, The basic vehicle's own controls): the basic controls'
// init 0x004ae3c0 and setup 0x004ae6b0 and their answers (0x004ae340, 0x004ae350, 0x004ae6a0), over records
// mirrored from the original's [0x005f4c4c] list; every call out goes to the shared callback, the controls,
// vehicle and records by the original's addresses.
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <vector>

#include "mech3/veh_basic_controls.h"
#include "mech3/veh_basic_object.h"
#include "probe_app.h"

namespace {
namespace m3 = mech3;
uint32_t lo(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
m3::Handle hd(uint32_t v) { return reinterpret_cast<m3::Handle>(static_cast<uintptr_t>(v)); }
uint32_t peek(uint32_t a) { return probe_app::call("peek32", {a}); }
float peekf(uint32_t a) {
  const uint32_t b = peek(a);
  float f;
  std::memcpy(&f, &b, 4);
  return f;
}

uint32_t g_ctl = 0, g_owner = 0;
m3::BasicControls* g_c = nullptr;
m3::BasicVehicle* g_v = nullptr;
std::map<const void*, uint32_t> g_rec_addr;

struct Hooks : m3::BasicControlsHooks {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return probe_app::call(key, args);
  }
  uint32_t ref(const void* p) override {
    if (g_c && p == static_cast<const void*>(g_c)) return g_ctl;
    if (g_v && p == static_cast<const void*>(g_v)) return g_owner;
    if (auto it = g_rec_addr.find(p); it != g_rec_addr.end()) return it->second;
    return lo(p);
  }
  void base_init(m3::BasicControls& c) override { unported("0x004ac8a0", {ref(&c)}); }
  void base_setup(m3::BasicControls& c, zrdr::Node* def, uint32_t a2) override {
    unported("0x004acbc0", {ref(&c), lo(def), a2, 0});
  }
  int32_t node_goal(m3::Handle n) override { return static_cast<int32_t>(peek(lo(n) + 0x2c)); }
  float reader_float(zrdr::Node* n, int32_t i) override {  // the list (its +0x04), item i's value at +4 + 8 i
    return peekf(peek(lo(n) + 4) + 4 + 8 * static_cast<uint32_t>(i));
  }
};
Hooks g_hooks;
}  // namespace

extern "C" {

// The original's controls at `ctl` over the vehicle at `owner` (its node `node`, 0 none), the net number `net`,
// the record list's head `head` (read from the original's memory: +0x18, +0x30, +0x38..+0x50, +0x58), +0x38 and
// +0xa08 before; then init, and with `def` the setup (def, arg2, net2). out: +0x38, +0xa04, +0xa08, +0xa0c,
// +0xa14, +0xa18, +0xa1c..+0xa30 (bits), +0xa44 (bits), the vehicle's +0xe0, then path_mode, path_goal (with a
// node), destroyed.
int probe_basic_controls(uint32_t ctl, uint32_t owner, uint32_t node, int32_t net, uint32_t head, int32_t state0,
                         uint32_t rec0, uint32_t def, uint32_t arg2, int32_t net2, uint32_t* out) {
  m3::set_basic_controls_hooks(&g_hooks);
  g_rec_addr.clear();
  m3::ai::nets105_free();
  std::map<uint32_t, m3::Net105Record*> by_addr;
  for (uint32_t a = head; a; a = peek(a + 0x58)) {
    auto* r = new m3::Net105Record;
    r->x18 = static_cast<int32_t>(peek(a + 0x18));
    r->kind = static_cast<int32_t>(peek(a + 0x30));
    r->activate_rad = peekf(a + 0x38), r->attack_rad = peekf(a + 0x3c), r->attack_dwell = peekf(a + 0x40);
    r->not_pursuit_dwell = peekf(a + 0x44), r->pursuit_far = peekf(a + 0x48), r->pursuit_near = peekf(a + 0x4c);
    r->return_range = peekf(a + 0x50);
    g_rec_addr[r] = a;
    by_addr[a] = r;
    m3::ai::nets105_append(r);
  }
  m3::BasicVehicle v;
  v.node = hd(node);
  g_v = &v;
  g_ctl = ctl, g_owner = owner;
  m3::BasicControls c(nullptr, 0);  // no owner yet: the constructor's init does nothing
  g_c = &c;
  c.owner = &v;
  c.net = net;
  c.state = state0;
  c.record = rec0 ? by_addr[rec0] : nullptr;
  c.init();
  if (def) c.setup_with(static_cast<zrdr::Node*>(hd(def)), arg2, net2);
  int n = 0;
  auto bits = [](float f) {
    uint32_t b;
    std::memcpy(&b, &f, 4);
    return b;
  };
  out[n++] = static_cast<uint32_t>(c.state), out[n++] = static_cast<uint32_t>(c.net);
  out[n++] = c.record ? g_rec_addr[c.record] : 0, out[n++] = lo(c.net_node);
  out[n++] = static_cast<uint32_t>(c.mode), out[n++] = bits(c.xa18);
  for (float f : {c.attack_dwell, c.not_pursuit_dwell, c.attack_rad, c.pursuit_far, c.pursuit_near, c.return_range, c.activate_rad})
    out[n++] = bits(f);
  out[n++] = v.xe0 == &v ? owner : lo(v.xe0);
  out[n++] = static_cast<uint32_t>(c.path_mode());
  out[n++] = c.net_node ? static_cast<uint32_t>(c.path_goal()) : 0;
  out[n++] = c.destroyed() ? 1 : 0;
  m3::ai::nets105_free();
  g_c = nullptr, g_v = nullptr;
  m3::set_basic_controls_hooks(nullptr);
  return n;
}

}  // extern "C"
