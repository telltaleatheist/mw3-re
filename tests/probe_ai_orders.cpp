// Probes for test_ai_orders.py (docs/ai.md 17.2): the brain's order stack
// (mech3/zai_brain) over fake nets and behaviours. Their parts and states
// report every call to the shared callback (calltrace.py), numbered in the
// order they are made, as the emulator's fakes do.
#include <cstdint>
#include <cstring>
#include <list>
#include <memory>
#include <vector>

#include "mech3/zai_brain.h"
#include "mech3/zai_steps.h"
#include "mech3/zai_target.h"
#include "mech3/zai_valve.h"
#include "mech3/zainet_region.h"
#include "probe_app.h"

namespace {
using probe_app::call;
namespace ai = mech3::ai;

uint32_t g_serial = 0;  // parts and states made, as the emulator counts them
uint32_t bits(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}

class FakePart : public ai::NetPart {
 public:
  FakePart(ai::Order* o, ai::AiNet* n, int32_t id) : NetPart(o), target(n), net_id(id), serial(++g_serial) {
    call("net.vt+0x5c", {net_id, serial});
  }
  ~FakePart() override { call("part.vt+0x0", {serial, 0}); }
  void run(float dt) override { call("part.vt+0x4", {serial, bits(dt)}); }
  ai::AiNet* net() const override {
    call("part.vt+0x8", {serial});
    return target;
  }
  ai::AiNet* target;
  int32_t net_id;
  uint32_t serial;
};

class FakeState : public ai::BehaviourState {
 public:
  FakeState(ai::Order* o, ai::Behaviour* b, int32_t id) : BehaviourState(o, b), beh_id(id), serial(++g_serial) {
    call("beh.vt+0x8c", {beh_id, serial});
  }
  ~FakeState() override { call("state.vt+0x0", {serial, 0}); }
  bool may_break_in() const override { return call("state.vt+0x4", {serial}) & 0xff; }
  bool may_fire() const override { return call("state.vt+0x8", {serial}) & 0xff; }
  void suspend() override { call("state.vt+0xc", {serial}); }
  void resume() override { call("state.vt+0x10", {serial}); }
  void run(float dt) override;
  int32_t beh_id;
  uint32_t serial;
};

std::unique_ptr<ai::Brain>& brain_ptr();
void FakeState::run(float dt) {
  if (call("state.vt+0x14", {serial, bits(dt)}) & 1) brain_ptr()->x104 = 1;  // asks to run again
}

class FakeNet : public ai::CompiledNet {
 public:
  FakeNet(int32_t id, bool compiled) : CompiledNet(id, 0), compiled_(compiled) {}
  ai::AiNet* as_compiled() override { return compiled_ ? this : nullptr; }
  bool allows_attack() override { return compiled_; }
  ai::NetPart* make_part(ai::Order& o) override { return new FakePart(&o, this, get_id()); }
  bool compiled_;
};

struct FakeBehaviour : ai::Behaviour {
  explicit FakeBehaviour(int32_t i) : id(i) {}
  ai::BehaviourState* make_state(ai::Order& o) override { return new FakeState(&o, this, id); }
  int32_t id;
};

// Nets 1..4 compiled, 5 a net that is not compiled, 100 the built-in one;
// behaviours 1..4, 100 the default one.
std::vector<std::unique_ptr<FakeNet>> g_nets;
std::vector<std::unique_ptr<FakeBehaviour>> g_behs;
std::unique_ptr<ai::Brain> g_brain;
std::unique_ptr<ai::Brain>& brain_ptr() { return g_brain; }

FakeNet* net_of(int32_t id) {
  for (auto& n : g_nets)
    if (n->get_id() == id) return n.get();
  return nullptr;
}
FakeBehaviour* beh_of(int32_t id) {
  for (auto& b : g_behs)
    if (b->id == id) return b.get();
  return nullptr;
}

// The gates' world (test_ai_gates.py): the owner at a made-up address,
// its position and contacts, the game time.
constexpr uint32_t kOwner = 0x2c7f0000;
float g_now = 0, g_pos[3] = {}, g_home[3] = {};
ai::AiTarget g_target;
ai::Contact g_contacts[2];
float fl(uint32_t u) {
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}

// The frame's world (test_ai_frame.py): the switch, the counter, the
// intensity, the weapon manager, contacts 1..3 with their vehicles.
bool g_ai_on = true, g_has_manager = false;
uint32_t g_awake = 0;
float g_intensity = 0;
ai::Contact g_seen[3];
zgame::Handle g_seen_vehicle[3] = {};
float g_vehicle_range2[2] = {};
constexpr uint32_t kVehicles = 0x2c7f2000;  // + 0x100 each

ai::Contact* contact_of(uint32_t i) { return i >= 1 && i <= 3 ? &g_seen[i - 1] : nullptr; }
int32_t contact_index(const ai::Contact* c) {
  for (int i = 0; i < 3; ++i)
    if (c == &g_seen[i]) return i + 1;
  return 0;
}

// The steering test's (test_ai_steer.py): the owner's contacts, each on an
// entity with a position and a flat forward.
struct Entity {
  zgame::Handle handle = 0;
  float pos[3] = {}, fwd[2] = {};
};
std::vector<Entity> g_entity(8);
std::vector<ai::Contact> g_steer_store(8);
std::vector<ai::Contact*> g_steer_contacts;
std::vector<int32_t> g_contact_entity;

struct Hooks : ai::BrainHooks {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override { return call(key, args); }
  ai::AiNet* builtin_net() override { return net_of(100); }
  ai::Behaviour* default_behaviour() override { return beh_of(100); }
  float now() override { return g_now; }
  float health(zgame::Handle v) override { return fl(call("owner.vt+0x98", {v})); }
  void position(zgame::Handle v, float out[3]) override {
    call("0x005a85e0", {v});
    std::memcpy(out, g_pos, 12);
    for (const Entity& en : g_entity)
      if (en.handle && en.handle == v) std::memcpy(out, en.pos, 12);
  }
  ai::AiTarget* targetable(zgame::Handle v) override { return v == kOwner ? &g_target : nullptr; }
  ai::Behaviour* manoeuvre_behaviour(int32_t which) override { return beh_of(which == 0 ? 98 : 88); }
  bool ai_on() override { return g_ai_on; }
  int32_t powered_down(zgame::Handle v) override { return static_cast<int32_t>(call("owner.vt+0xa0", {v})); }
  ai::Contact* pick(zgame::Handle v, int32_t side) override { return contact_of(call("0x004d6b90", {v, side})); }
  zgame::Handle contact_vehicle(const ai::Contact* c) override {
    const int32_t i = contact_index(c);
    return i ? g_seen_vehicle[i - 1] : 0;
  }
  float contact_range2(zgame::Handle v) override { return g_vehicle_range2[(v - kVehicles) / 0x100]; }
  void count_awake() override { ++g_awake; }
  float intensity() override { return g_intensity; }
  int32_t rand() override { return static_cast<int32_t>(call("rand", {})); }
  bool ammo_value(zgame::Handle v, float* out) override {
    if (!g_has_manager) return false;
    *out = fl(call("0x004b4ed0", {}));
    (void)v;
    return true;
  }
  const std::vector<ai::Contact*>* contacts(zgame::Handle v) override { return v == kOwner ? &g_steer_contacts : nullptr; }
  zgame::Handle contact_entity(const ai::Contact* c) override {
    for (size_t i = 0; i < g_steer_contacts.size(); ++i)
      if (g_steer_contacts[i] == c) return g_entity[g_contact_entity[i]].handle;
    return 0;
  }
  void forward(zgame::Handle e, float* x, float* z) override {
    for (const Entity& en : g_entity)
      if (en.handle && en.handle == e) *x = en.fwd[0], *z = en.fwd[1];
  }
  void radio_line(ai::Brain&, int32_t event) override { call("0x004e6d50", {event}); }
  void firing_reset(ai::Brain&) override { call("0x004e9590", {}); }
  void firing_run(ai::Brain&, float dt) override { call("0x004e9fb0", {bits(dt)}); }
  ai::AiNet* vehicle_net(zgame::Handle v) override { return net_of(static_cast<int32_t>(call("vehicle.vt+0xd8", {v}))); }
  ai::Behaviour* attack_behaviour(ai::AiNet* n) override {
    return beh_of(static_cast<int32_t>(call("0x004c0e10", {n ? n->get_id() : 0})));
  }
  bool attack(ai::Brain&, ai::AiNet* n) override { return call("0x004e9ca0", {n ? n->get_id() : 0}) & 1; }
  void own_order_run(ai::Brain&, float dt) override { call("own.vt+0x4", {bits(dt)}); }
  float heat(zgame::Handle v) override { return fl(call("owner.vt+0x9c", {v})); }
  bool clear_line(zgame::Handle v, zgame::Handle seen) override { return call("0x004d5980", {v, seen, 0u}) & 0xff; }
  bool region_inside(ai::Region* r, const float*) override { return call("0x004c57d0", {r->get_id()}) & 1; }
};

// The brain's controls: the AI mech controls' calls.
struct FakeControls : ai::BrainControls {
  zgame::Handle vehicle() override { return kOwner; }
  void stop() override { call("ictl.vt+0x14", {}); }
  void smooth(float dt) override { call("brain.vt+0x4", {bits(dt)}); }
  bool hit_lately() override { return call("ictl.vt+0xa0", {}) & 1; }
};
FakeControls g_controls;
Hooks g_hooks;

ai::Order* order_at(int32_t i) {
  if (i < 0 || i >= static_cast<int32_t>(g_brain->orders.size())) return nullptr;
  return &g_brain->orders[static_cast<size_t>(i)];
}
int32_t index_of(const ai::Order* o) {
  for (size_t i = 0; i < g_brain->orders.size(); ++i)
    if (&g_brain->orders[i] == o) return static_cast<int32_t>(i);
  return -1;
}
}  // namespace

extern "C" {

// A brain with no orders (its constructor's first order dropped unseen).
void probe_ao_new() {
  ai::set_brain_hooks(&g_hooks);
  probe_app::Mute mute;
  g_brain.reset();
  g_nets.clear();
  g_behs.clear();
  for (int32_t i = 1; i <= 4; ++i) g_nets.push_back(std::make_unique<FakeNet>(i, true));
  g_nets.push_back(std::make_unique<FakeNet>(5, false));
  g_nets.push_back(std::make_unique<FakeNet>(100, true));
  for (int32_t i : {1, 2, 3, 4, 100, 88, 98}) g_behs.push_back(std::make_unique<FakeBehaviour>(i));
  ai::AiParams rec;
  g_brain = std::make_unique<ai::Brain>(net_of(1), rec);
  g_brain->orders.clear();
  g_serial = 0;
}

void probe_ao_done() {
  probe_app::Mute mute;
  if (g_brain) g_brain->radio = nullptr, g_brain->params.vlist = nullptr;  // the probe's, not the brain's
  g_brain.reset();
  ai::set_brain_hooks(nullptr);
}

// One operation (test_ai_orders.py OPS); nets and behaviours by id, 0 none.
// The answer: a bool, an order's index, or 0.
int32_t probe_ao_op(int32_t op, int32_t a, int32_t b, uint32_t f) {
  ai::Brain& br = *g_brain;
  float dt;
  std::memcpy(&dt, &f, 4);
  switch (op) {
    case 0:  // 0x004e9d80, then 0x004e80b0 on the new order
      br.push_order();
      return br.current().reset(net_of(a), beh_of(b));
    case 1: return br.push(net_of(a), beh_of(b));
    case 2: return br.push_net(net_of(a));
    case 3: return br.push_behaviour(beh_of(a));
    case 4: return br.remake(net_of(a));
    case 5: return br.current().set_behaviour(beh_of(a));
    case 6: return br.current().reset(net_of(a), beh_of(b));
    case 7: br.pop(); return 0;
    case 8: return br.pop_if(net_of(a));
    case 9: return br.pop_if_object(net_of(a));
    case 10: return br.push_object(net_of(a));
    case 11: return br.clear_if_object(net_of(a));
    case 12: return index_of(br.top_unmarked());
    case 13: br.unwind_to(order_at(a)); return 0;
    case 14: return br.stop();
    case 15: return br.may_fire();
    case 16: br.current().run(dt); return 0;
    case 17: br.current().pop(); return 0;
    case 18:
      if (ai::Order* o = order_at(a)) o->x04 = static_cast<uint8_t>(b);
      return 0;
    default: return -1;
  }
}

// Set the current order's +0x14..+0x2f (the words given).
void probe_ao_fields(const uint32_t* w) {
  ai::Order& o = g_brain->current();
  o.x14 = static_cast<int32_t>(w[0]);
  std::memcpy(o.x18, w + 1, 12);
  std::memcpy(o.x24, w + 4, 12);
}

// The stack, first to last: per order x04, owner is the brain, its part's
// serial and net id, its state's serial and behaviour id, +0x14..+0x2f (7
// words): 13 words. The count.
int32_t probe_ao_stack(uint32_t* out, int32_t cap) {
  int32_t n = 0;
  for (const ai::Order& o : g_brain->orders) {
    if ((n + 1) * 13 > cap) return -1;
    uint32_t* w = out + 13 * n++;
    const auto* p = dynamic_cast<const FakePart*>(o.net.get());
    const auto* s = dynamic_cast<const FakeState*>(o.state.get());
    w[0] = o.x04;
    w[1] = o.owner == g_brain.get();
    w[2] = p ? p->serial : 0, w[3] = p ? static_cast<uint32_t>(p->net_id) : 0;
    w[4] = s ? s->serial : 0, w[5] = s ? static_cast<uint32_t>(s->beh_id) : 0;
    w[6] = static_cast<uint32_t>(o.x14);
    std::memcpy(w + 7, o.x18, 12);
    std::memcpy(w + 10, o.x24, 12);
  }
  return n;
}

// The gates' fields and world: the brain's +0x04..+0xcf, +0x22c, the home
// point (has[0]), +0xe8, +0x230, +0xf4; the time, the owner's position, its
// contacts at +0x80 and +0x98 by squared distance (has[1], has[2]).
void probe_ao_gate_set(const uint8_t* raw, uint32_t x22c, const uint32_t* f, const int32_t* has) {
  ai::Brain& b = *g_brain;
  std::memcpy(b.params.raw + 4, raw + 4, 0xcc);
  b.x22c = static_cast<uint8_t>(x22c);
  std::memcpy(g_home, f, 12);
  b.xe4 = has[0] ? g_home : nullptr;
  b.xe8 = fl(f[3]), b.x230 = fl(f[4]), b.xf4 = fl(f[5]);
  g_now = fl(f[6]);
  std::memcpy(g_pos, f + 7, 12);
  g_contacts[0].dist2 = fl(f[10]), g_contacts[1].dist2 = fl(f[11]);
  g_target.nearest_lance = has[1] ? &g_contacts[0] : nullptr;
  g_target.player_contact = has[2] ? &g_contacts[1] : nullptr;
}

// One gate: 0 may_engage (the net by id, the current state or none), 1
// must_break_off, 2 p6_clear, 3 sees_target, 4 manoeuvre. out: the answer,
// +0x22c, +0xf4.
void probe_ao_gate(int32_t op, int32_t net, int32_t with_state, uint32_t* out) {
  ai::Brain& b = *g_brain;
  const zgame::Handle owner = kOwner;
  uint32_t r = 0;
  switch (op) {
    case 0: r = b.may_engage(owner, net_of(net), with_state ? b.current().state.get() : nullptr); break;
    case 1: r = b.must_break_off(owner); break;
    case 2: r = b.p6_clear(); break;
    case 3: r = b.sees_target(owner); break;
    case 4: b.manoeuvre(); break;
  }
  out[0] = r, out[1] = b.x22c, out[2] = bits(b.xf4);
}

// The frame's state: w[0] AI on, [1] a manager, [2] the intensity, [3..5]
// contacts' squared distances, [6..8] their vehicles (0 none, 1, 2), [9, 10]
// the vehicles' ranges, [11] the target (+0x6c: a contact 1..3 or 0), [12]
// +0x2a4, [13] +0xdc, [14] +0xf0, [15] +0x100, [16] a radio (+0x2a8).
void probe_ao_frame_set(const uint32_t* w) {
  ai::Brain& b = *g_brain;
  g_ai_on = w[0] != 0, g_has_manager = w[1] != 0, g_intensity = fl(w[2]);
  for (int i = 0; i < 3; ++i) {
    g_seen[i].dist2 = fl(w[3 + i]);
    g_seen_vehicle[i] = w[6 + i] ? kVehicles + 0x100 * (w[6 + i] - 1) : 0;
  }
  g_vehicle_range2[0] = fl(w[9]), g_vehicle_range2[1] = fl(w[10]);
  g_target.target = contact_of(w[11]);
  g_target.player_contact = contact_of(w[17]);  // the gates set it; the frame's own choice
  b.x2a4 = static_cast<uint8_t>(w[12]);
  b.xdc = fl(w[13]);
  b.xf0 = static_cast<int32_t>(w[14]);
  b.x100 = fl(w[15]);
  static ai::Radio radio;
  b.radio = w[16] ? &radio : nullptr;
}

// One frame: out the answer, the awake count, +0x3c, +0xcc, +0xdc, +0xe8,
// +0xf4, +0x100, +0x230, +0x22c, +0x104.
void probe_ao_frame(uint32_t dt, uint32_t* out) {
  ai::Brain& b = *g_brain;
  g_awake = 0;
  out[0] = b.frame(g_controls, fl(dt));
  out[1] = g_awake;
  out[2] = b.params.get<uint32_t>(0x3c), out[3] = b.params.raw[0xcc];
  out[4] = bits(b.xdc), out[5] = bits(b.xe8), out[6] = bits(b.xf4), out[7] = bits(b.x100), out[8] = bits(b.x230);
  out[9] = b.x22c, out[10] = b.x104;
  b.radio = nullptr;
}

// The vlist test's (test_ai_vlist.py): valves 1..4, regions 1..2, and the
// list. rows: f00..f10 (5 floats), region (0, 1, 2), flags, flags2, valve
// (0, 1..4); 9 words each. values: the four valves' values. seen: what the
// contacts see.
std::vector<std::unique_ptr<ai::Valve>> g_valves;
std::vector<std::unique_ptr<ai::Region>> g_regions;
std::list<ai::VListEntry> g_vlist;
void probe_ao_vlist_set(const uint32_t* rows, int32_t n, const int32_t* values, const uint32_t* seen) {
  if (g_valves.empty()) {
    for (int i = 1; i <= 4; ++i) g_valves.push_back(std::make_unique<ai::Valve>("v"));
    for (int i = 1; i <= 2; ++i) g_regions.push_back(std::make_unique<ai::Region>("r", i, 0));
  }
  for (int i = 0; i < 4; ++i) g_valves[static_cast<size_t>(i)]->value->set(values[i]);
  for (int i = 0; i < 3; ++i) g_seen[i].seen = seen[i];
  g_vlist.clear();
  for (int r = 0; r < n; ++r) {
    const uint32_t* w = rows + 9 * r;
    ai::VListEntry e;
    e.f00 = fl(w[0]), e.f04 = fl(w[1]), e.f08 = fl(w[2]), e.f0c = fl(w[3]), e.f10 = fl(w[4]);
    e.region = w[5] ? g_regions[w[5] - 1].get() : nullptr;
    e.flags = static_cast<uint8_t>(w[6]), e.flags2 = static_cast<uint8_t>(w[7]);
    e.valve = w[8] ? g_valves[w[8] - 1].get() : nullptr;
    g_vlist.push_back(e);
  }
  g_brain->params.vlist = n >= 0 ? &g_vlist : nullptr;
}

// 0x004e9550 with the picked flag and target given; out: picked, the target
// (a contact 1..3 or 0), the four valves' values.
void probe_ao_vlist(int32_t picked, int32_t target, int32_t* out) {
  bool p = picked != 0;
  ai::Contact* t = contact_of(static_cast<uint32_t>(target));
  g_brain->vlist(g_controls, kOwner, &p, &t);
  out[0] = p, out[1] = contact_index(t);
  for (int i = 0; i < 4; ++i) out[2 + i] = g_valves[static_cast<size_t>(i)]->get();
  g_brain->params.vlist = nullptr;
}

// The steering test's world: n entities (handle, pos[3], fwd[2]: 6 words
// each) and m contacts (dist2, entity index: 2 words each).
void probe_ao_steer_world(const uint32_t* ents, int32_t n, const uint32_t* cs, int32_t m) {
  for (int i = 0; i < 8; ++i) g_entity[static_cast<size_t>(i)] = Entity{};
  for (int i = 0; i < n; ++i) {
    Entity& e = g_entity[static_cast<size_t>(i)];
    e.handle = ents[6 * i];
    for (int k = 0; k < 3; ++k) e.pos[k] = fl(ents[6 * i + 1 + k]);
    e.fwd[0] = fl(ents[6 * i + 4]), e.fwd[1] = fl(ents[6 * i + 5]);
  }
  g_steer_contacts.clear();
  g_contact_entity.clear();
  for (int i = 0; i < m; ++i) {
    ai::Contact& c = g_steer_store[static_cast<size_t>(i)];
    c.dist2 = fl(cs[2 * i]);
    g_steer_contacts.push_back(&c);
    g_contact_entity.push_back(static_cast<int32_t>(cs[2 * i + 1]));
  }
}

// One steering call on the brain's: 0 set(to, from, rate), 1 update(dt), 2
// near, 3 carrot; f the floats. out: the answer (3 floats for the carrot),
// then the 0x48 bytes.
void probe_ao_steer(int32_t op, const uint32_t* f, uint32_t* out) {
  ai::Steer& s = g_brain->steer;
  float a[7];
  for (int i = 0; i < 7; ++i) a[i] = fl(f[i]);
  out[0] = out[1] = out[2] = 0;
  switch (op) {
    case 0: s.set(a, a + 3, a[6], kOwner); break;
    case 1: s.update(a[0], kOwner); break;
    case 2: out[0] = s.near(); break;
    case 3: {
      float c[3];
      s.carrot(c);
      for (int i = 0; i < 3; ++i) out[i] = bits(c[i]);
      break;
    }
  }
  std::memcpy(out + 3, &s, 0x48);
}

// The steering block set outright (0x48 bytes).
void probe_ao_steer_load(const uint8_t* b) { std::memcpy(&g_brain->steer, b, 0x48); }

}  // extern "C"
