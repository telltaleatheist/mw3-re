// Probe for test_basic_follow.py (docs/vehicles.md, The basic vehicle's own controls, part 2): the path follower
// 0x00401050 and its modes over a scene mirrored from the original's memory: the vehicle (and the ring on its +0xe0),
// its controls, the record and its nodes with their link records, the first weapon slot, the target. Calls out go to
// the shared callback; the vehicles, nodes and records by the original's addresses.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <vector>

#include "mech3/veh_basic_controls.h"
#include "mech3/veh_basic_object.h"
#include "probe_app.h"

namespace {
namespace m3 = mech3;
using m3::ai::Net105Node;
uint32_t lo(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
m3::Handle hd(uint32_t v) { return reinterpret_cast<m3::Handle>(static_cast<uintptr_t>(v)); }
uint32_t peek(uint32_t a) { return probe_app::call("peek32", {a}); }
float peekf(uint32_t a) {
  const uint32_t b = peek(a);
  float f;
  std::memcpy(&f, &b, 4);
  return f;
}
uint32_t bits(float f) {
  uint32_t b;
  std::memcpy(&b, &f, 4);
  return b;
}

std::map<const void*, uint32_t> g_addr;  // the port's objects -> the original's addresses
std::map<uint32_t, void*> g_obj;
std::vector<float> g_pos_store;           // positions handed back (kept alive)
uint32_t g_target = 0;

void bind(const void* p, uint32_t a) { g_addr[p] = a, g_obj[a] = const_cast<void*>(p); }

struct Hooks : m3::BasicControlsHooks {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return probe_app::call(key, args);
  }
  uint32_t ref(const void* p) override {
    if (auto it = g_addr.find(p); it != g_addr.end()) return it->second;
    return lo(p);
  }
  // A place: the stub's answer is the original's vector; read it here (three floats kept for the caller).
  const float* vec_at(uint32_t a) {
    if (!a) return nullptr;
    auto* p = new float[3]{peekf(a), peekf(a + 4), peekf(a + 8)};
    return p;  // a few a frame: the probe's run is short
  }
  const float* entity_position(m3::Handle e) override { return vec_at(unported("0x005a85e0", {ref(e)})); }
  const float* entity_lock(m3::Handle e) override { return vec_at(unported("0x005a8630", {ref(e)})); }
  bool entity_active(m3::Handle e) override { return (peek(ref(e) + 8) & 0xff) != 0; }
  m3::Handle entity_velocity(m3::Handle e) override { return hd(ref(e) + 0x30); }
  void entity_refresh(m3::Handle e) override {
    const uint32_t data = peek(peek(ref(e) + 0x14) + 0x38);
    if (!(peek(data) & 0x20)) return;
    unported("0x0051e9d0", {}), unported("0x0051fde0", {});
    unported("0x004ed280", {peek(ref(e) + 0x14), 1}), unported("0x0051ea00", {});
  }
  float centroid_distance_sq(m3::Handle v, m3::Handle t) override {  // 0x0051dfa0 on the two centroids' data +0x84
    const uint32_t a = peek(peek(ref(t) + 0x14) + 0x38) + 0x84, b = peek(peek(ref(v) + 0x14) + 0x38) + 0x84;
    const float x = peekf(a) - peekf(b), y = peekf(a + 4) - peekf(b + 4), z = peekf(a + 8) - peekf(b + 8);
    return z * z + y * y + x * x;
  }
  uint32_t def_flags(m3::Handle def) override { return peek(ref(def) + 0x6c); }
  bool line_clear(m3::Handle v, m3::Handle t) override { return (unported("0x004d5980", {ref(v), ref(t), 0}) & 0xff) != 0; }
  void sweep(m3::Handle v, const float pts[6]) override {
    unported("0x004a0910", {ref(v), bits(pts[0]), bits(pts[1]), bits(pts[2]), bits(pts[3]), bits(pts[4]), bits(pts[5]), 2});
  }
  bool sweep_hit(m3::BasicControls& c) override { return peek(ref(&c) + 0x360) || peek(ref(&c) + 0x354); }
  void sweep_clear(m3::Handle v) override { unported("0x004a0100", {ref(v)}); }
  void beam_start(m3::Handle beam, int32_t serial) override { unported("0x00581230", {lo(beam), serial}); }
  float now() override { return peekf(0x60991c); }
  int32_t rand() override { return static_cast<int32_t>(unported("rand", {})); }
  void node_free(Net105Node* n) override { unported("0x004024a0", {ref(n)}); }
  Net105Node* node_make(m3::BasicControls& c) override {
    const uint32_t a = probe_app::call("new", {0x34});  // the original's operator new (silent in the trace)
    Net105Node* n = c.record->make_node();
    bind(n, a);
    const uint32_t rec = probe_app::call("@malloc", {0x3c});
    n->link_rec[0] = static_cast<m3::ai::Net105Link*>(std::calloc(1, 0x3c));
    bind(n->link_rec[0], rec);
    return n;
  }
};
Hooks g_hooks;

struct Scene {
  std::vector<std::unique_ptr<m3::BasicVehicle>> vehicles;
  std::vector<std::unique_ptr<m3::BasicControls>> controls;
  std::vector<std::unique_ptr<m3::WeaponSlot>> slots;
  m3::ai::Net105* net = nullptr;
};
Scene* g_scene = nullptr;

m3::ai::Net105Link* mirror_link(uint32_t a) {
  if (!a) return nullptr;
  if (auto it = g_obj.find(a); it != g_obj.end()) return static_cast<m3::ai::Net105Link*>(it->second);
  auto* r = static_cast<m3::ai::Net105Link*>(std::calloc(1, 0x3c));
  for (uint32_t i = 0; i < 15; ++i) {
    const uint32_t w = peek(a + 4 * i);
    std::memcpy(reinterpret_cast<uint8_t*>(r) + 4 * i, &w, 4);
  }
  bind(r, a);
  return r;
}

Net105Node* mirror_node(uint32_t a, std::vector<std::pair<Net105Node*, uint32_t>>& pending) {
  if (!a) return nullptr;
  if (auto it = g_obj.find(a); it != g_obj.end()) return static_cast<Net105Node*>(it->second);
  Net105Node* n = g_scene->net->make_node();
  bind(n, a);
  for (int i = 0; i < 3; ++i) n->place[i] = peekf(a + 4 + 4 * static_cast<uint32_t>(i));
  n->number = static_cast<int32_t>(peek(a + 0x2c));
  n->x28 = static_cast<int32_t>(peek(a + 0x28));
  for (int i = 0; i < 3; ++i) n->link_rec[i] = mirror_link(peek(a + 0x1c + 4 * static_cast<uint32_t>(i)));
  pending.push_back({n, a});
  return n;
}

// The controls at `ctl` over the vehicle at `veh`, as the original's memory holds them.
m3::BasicControls* mirror_vehicle(uint32_t veh, uint32_t ctl, std::vector<std::pair<Net105Node*, uint32_t>>& pending) {
  if (auto it = g_obj.find(veh); it != g_obj.end()) return static_cast<m3::BasicControls*>(static_cast<m3::BasicVehicle*>(it->second)->controls);
  auto v = std::make_unique<m3::BasicVehicle>();
  auto c = std::make_unique<m3::BasicControls>(nullptr, 0);
  bind(v.get(), veh), bind(c.get(), ctl);
  v->forward.x = peekf(veh + 0x24), v->forward.y = peekf(veh + 0x28), v->forward.z = peekf(veh + 0x2c);
  v->controls = c.get();
  c->owner = v.get();
  c->state = static_cast<int32_t>(peek(ctl + 0x38));
  c->serial = static_cast<int32_t>(peek(ctl + 0x14));
  c->velocity.x = peekf(ctl + 0x64), c->velocity.y = peekf(ctl + 0x68), c->velocity.z = peekf(ctl + 0x6c);
  c->angles.x = peekf(ctl + 0x32c);
  c->x350 = peek(ctl + 0x350);
  c->hit_object = static_cast<int32_t>(peek(ctl + 0x374)), c->hit_wall = static_cast<int32_t>(peek(ctl + 0x37c));
  c->fire_on = peek(ctl + 0x390), c->beam_on = peek(ctl + 0x394);
  c->health_max = peekf(ctl + 0x740), c->health = peekf(ctl + 0x744);
  c->x760 = peek(ctl + 0x760);
  c->motion.mode = static_cast<int32_t>(peek(ctl + 0x764));
  c->x7ec = peekf(ctl + 0x7ec);
  c->record = g_scene->net;
  c->net_node = mirror_node(peek(ctl + 0xa0c), pending);
  c->link = static_cast<int32_t>(peek(ctl + 0xa10));
  c->mode = static_cast<int32_t>(peek(ctl + 0xa14));
  c->xa18 = peekf(ctl + 0xa18);
  c->attack_dwell = peekf(ctl + 0xa1c), c->not_pursuit_dwell = peekf(ctl + 0xa20), c->attack_rad = peekf(ctl + 0xa24);
  c->pursuit_far = peekf(ctl + 0xa28), c->pursuit_near = peekf(ctl + 0xa2c), c->return_range = peekf(ctl + 0xa30);
  for (int i = 0; i < 3; ++i) c->chase[i] = peekf(ctl + 0xa34 + 4 * static_cast<uint32_t>(i));
  c->crumb_at = peekf(ctl + 0xa40);
  c->ahead = peek(ctl + 0xa54);
  if (const uint32_t w = peek(ctl + 0x3b4)) {
    auto s = std::make_unique<m3::WeaponSlot>();
    s->def = hd(peek(w));
    s->x24 = peek(w + 0x24), s->x28 = peek(w + 0x28), s->x2c = peek(w + 0x2c), s->x30 = peek(w + 0x30);
    s->beam = hd(peek(w + 0x34));
    bind(s.get(), w);
    c->first = s.get();
    g_scene->slots.push_back(std::move(s));
  }
  m3::BasicControls* out = c.get();
  g_scene->vehicles.push_back(std::move(v));
  g_scene->controls.push_back(std::move(c));
  return out;
}
}  // namespace

extern "C" {

// The original's controls `ctl` (vehicle `veh`), the target `target`; the record's kind `kind`. Mirrors the scene
// (the +0xe0 ring and every node reachable), runs follow(target). out: per vehicle on the ring from `veh`, its
// controls' fields (25 words each, as test_basic_follow.py lists them); then the nodes made (place, number, link 0,
// its record's 15 words). Returns the words written.
int probe_basic_follow(uint32_t veh, uint32_t ctl, uint32_t target, int32_t kind, uint32_t* out) {
  m3::set_basic_controls_hooks(&g_hooks);
  g_addr.clear(), g_obj.clear();
  Scene scene;
  g_scene = &scene;
  m3::ai::nets105_free();
  scene.net = new m3::ai::Net105;
  scene.net->kind = kind;
  m3::ai::nets105_append(scene.net);
  g_target = target;
  std::vector<std::pair<Net105Node*, uint32_t>> pending;
  std::vector<uint32_t> ring;
  for (uint32_t a = veh;;) {
    ring.push_back(a);
    const uint32_t c = peek(a + 0xe4);
    mirror_vehicle(a, c, pending);
    const uint32_t nx = peek(a + 0xe0);
    if (!nx || nx == veh) break;
    a = nx;
  }
  for (size_t i = 0; i < ring.size(); ++i) {
    auto* v = static_cast<m3::BasicVehicle*>(g_obj[ring[i]]);
    const uint32_t nx = peek(ring[i] + 0xe0);
    v->xe0 = nx ? g_obj[nx] : nullptr;
  }
  while (!pending.empty()) {  // every node reachable through links
    auto [n, a] = pending.back();
    pending.pop_back();
    for (int i = 0; i < 3; ++i) n->links[i] = mirror_node(peek(a + 0x10 + 4 * static_cast<uint32_t>(i)), pending);
  }
  const size_t made_before = scene.net->made.size();
  auto* c = static_cast<m3::BasicControls*>(static_cast<m3::BasicVehicle*>(g_obj[veh])->controls);
  c->follow(hd(target));
  int n = 0;
  for (uint32_t a : ring) {
    auto* k = static_cast<m3::BasicControls*>(static_cast<m3::BasicVehicle*>(g_obj[a])->controls);
    const uint32_t w[] = {static_cast<uint32_t>(k->mode), static_cast<uint32_t>(k->link), g_addr[k->net_node],
                          k->ahead, bits(k->throttle), bits(k->turn), bits(k->heading.x), bits(k->heading.z),
                          k->x760, bits(k->pitch_in), bits(k->pitch_out), bits(k->in40), bits(k->out48), bits(k->xa18),
                          bits(k->crumb_at), bits(k->chase[0]), bits(k->chase[2]), k->x350, k->fire_on, k->beam_on,
                          k->x65c, k->x664 ? 1u : 0u, lo(k->x668), k->first ? k->first->x24 : 0, bits(k->player_at.x)};
    for (uint32_t x : w) out[n++] = x;
  }
  for (size_t i = made_before; i < scene.net->made.size(); ++i) {
    Net105Node* m = scene.net->made[i].get();
    if (!g_addr.count(m)) continue;  // a mirrored node, not made
    out[n++] = g_addr[m];
    for (float f : m->place) out[n++] = bits(f);
    out[n++] = static_cast<uint32_t>(m->number);
    out[n++] = g_addr.count(m->links[0]) ? g_addr[m->links[0]] : lo(m->links[0]);
    for (uint32_t i2 = 0; i2 < 15; ++i2) {
      uint32_t wd = 0;
      if (m->link_rec[0]) std::memcpy(&wd, reinterpret_cast<uint8_t*>(m->link_rec[0]) + 4 * i2, 4);
      out[n++] = wd;
    }
  }
  m3::ai::nets105_free();
  g_scene = nullptr;
  m3::set_basic_controls_hooks(nullptr);
  return n;
}

}  // extern "C"
