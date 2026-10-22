// Probes for test_ai_orders.py (docs/ai.md 17.2): the brain's order stack
// (mech3/zai_brain) over fake nets and behaviours. Their parts and states
// report every call to the shared callback (calltrace.py), numbered in the
// order they are made, as the emulator's fakes do.
#include <cstdint>
#include <cstring>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "mech3/mech.h"
#include "mech3/mech_control.h"
#include "gamez/zmodel/gmod_cull.h"
#include "mech3/zai_attack.h"
#include "mech3/zai_brain.h"
#include "mech3/zai_node.h"
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
  ai::Behaviour* behaviour() const override {
    call("state.vt+0x18", {serial});
    return from;
  }
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

// The attack's test (test_ai_attack.py): behaviours 1..4 have a position
// (vt+0x18) when set.
bool g_step_pos_on[5] = {};
float g_step_pos[5][3] = {};
struct FakeBehaviour : ai::Behaviour {
  explicit FakeBehaviour(int32_t i) : id(i) {}
  ai::BehaviourState* make_state(ai::Order& o) override { return new FakeState(&o, this, id); }
  const float* position() const override { return id >= 1 && id <= 4 && g_step_pos_on[id] ? g_step_pos[id] : nullptr; }
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

bool g_pilot = false;
zgame::Handle g_player_vehicle = 0;

// The node state's test (test_ai_node.py): the owner's flat forward, its
// speeds (a Mech's +0x518 and +0x534 through the ported vt+0x118 / +0x120),
// its gate name and jump jets, the net's elements.
float g_owner_fwd[3] = {};
float g_vy = 0;  // the vehicle's +0x40
mech3::Mech g_mech;
bool g_owner_gate = false;
int32_t g_jump_jets = 0;

int32_t elem_id(const ai::Behaviour* b);
ai::Behaviour* elem_at(uint32_t id);

// The firing's test (test_ai_firing.py): plans 1..3 and sources 1..2 by
// number, their fields from the test; the weapon manager and weapons by
// number; six contacts, the vehicle's list and the plans' target slots.
struct PlanDef {
  int32_t slot = 0;  // its target slot (0..2)
  bool quick = false, line = false;
  int32_t kind = 0;
};
PlanDef g_plan_def[4];
ai::Contact g_fire_contact[6];
std::vector<ai::Contact*> g_fire_list;
ai::Contact* g_target_slot[3] = {};
uint32_t g_wm = 0;
bool g_fire_world = false, g_has_mech = false;
float g_reload[3] = {};
// The plan pick's test (test_ai_plan.py): entities 1..4 at fixed handles
// (the emulator's addresses), each contact's entity.
constexpr uint32_t kPlanEnt = kOwner + 0x4000;  // + 0x400 each
ai::AiTarget g_ent_target[5];
bool g_ent_active[5] = {};
int32_t g_contact_ent[6] = {};
bool g_bearer = true;
uint32_t g_group_weapon = 0;  // the selected group's weapon (1, 2; 0 none)
float g_muzzle[3][3] = {}, g_wspeed[3] = {}, g_vel[5][3] = {}, g_owner_m[9] = {};
uint32_t g_wflags[3] = {};
// The aim's test (test_ai_aim.py): the difficulty's error, the targeting
// aid, the manager's +0x88.
float g_aim_intensity = 1.0f;
bool g_aim_aid = false;
uint8_t g_wm_x88 = 0;
// The clear line's test (test_ai_clearline.py): two entities' nodes,
// centroids and zone lists, the nodes' flags, the centroids' positions and
// stale flags, by handle (the emulator's addresses).
struct LineEnt {
  uint32_t self = 0, node = 0, centroid = 0;
  uint8_t zone[4] = {};
  bool flag = false, stale = false;
  float pos[3] = {};
};
LineEnt g_line[2];
bool g_line_world = false;
const LineEnt* line_by(uint32_t h, uint32_t LineEnt::*f) {
  for (const LineEnt& l : g_line)
    if (h && l.*f == h) return &l;
  return nullptr;
}
uint32_t ent_handle(int32_t k) { return k ? kPlanEnt + 0x400 * static_cast<uint32_t>(k) : 0; }
int32_t ent_index(uint32_t h) { return h >= kPlanEnt + 0x400 && h <= kPlanEnt + 0x1000 ? static_cast<int32_t>((h - kPlanEnt) / 0x400) : 0; }
int32_t fire_contact_index(const ai::Contact* c) {
  for (int i = 0; i < 6; ++i)
    if (c == &g_fire_contact[i]) return i + 1;
  return 0;
}
class FakePlan : public ai::FirePlan {
 public:
  explicit FakePlan(int32_t i) : id(i) {
    const PlanDef& d = g_plan_def[i];
    target = &g_target_slot[d.slot];
    quick = d.quick, line_check = d.line, kind = d.kind;
  }
  ~FakePlan() override { call("fire.delete", {static_cast<uint32_t>(id)}); }
  uint8_t in_reach(float a) override { return call("plan.vt+0x4", {static_cast<uint32_t>(id), bits(a)}) & 0xff; }
  uint8_t track(float a) override { return call("plan.vt+0x10", {static_cast<uint32_t>(id), bits(a)}) & 0xff; }
  uint8_t ready() override { return call("plan.vt+0x18", {static_cast<uint32_t>(id)}) & 0xff; }
  int32_t id;
};
ai::FirePlan* make_plan(uint32_t id) { return id >= 1 && id <= 3 ? new FakePlan(static_cast<int32_t>(id)) : nullptr; }
class FakeSource : public ai::FireSource {
 public:
  explicit FakeSource(int32_t i) : id(i) {}
  ~FakeSource() override { call("fire.delete", {static_cast<uint32_t>(0x100 + id)}); }
  ai::FirePlan* take(bool* spent) override {
    const uint32_t r = call("source.vt+0x0", {static_cast<uint32_t>(id)});
    *spent = (r >> 8) & 1;
    return make_plan(r & 0xff);
  }
  int32_t id;
};

mech3::Mech* const kPlayerMech = reinterpret_cast<mech3::Mech*>(&g_now);  // the player controls' owner: kOwner (any address)

struct Hooks : ai::BrainHooks {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override { return call(key, args); }
  zgame::Handle vehicle_handle(void* v) override { return v == kPlayerMech ? kOwner : low(v); }
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
  ai::AiTarget* targetable(zgame::Handle v) override {
    if (v == kOwner) return &g_target;
    const int32_t k = ent_index(v);
    return k ? &g_ent_target[k] : nullptr;
  }
  bool entity_active(zgame::Handle e) override { return g_ent_active[ent_index(e)]; }
  zgame::Handle bearer(zgame::Handle v) override { return g_bearer ? v : 0; }
  bool select_target(zgame::Handle, ai::Contact* c) override {  // taken: the vehicle's target, as 0x004d5e40 does
    const bool ok = call("vehicle.vt+0xc0", {static_cast<uint32_t>(fire_contact_index(c))}) & 0xff;
    if (ok) g_target.target = c;
    return ok;
  }
  int32_t volley_pick(zgame::Handle, ai::Contact* c) override {
    return static_cast<int32_t>(call("0x004b1860", {static_cast<uint32_t>(fire_contact_index(c))}));
  }
  // The track's and lead point's world (test_ai_plan.py).
  void mech_aim_at_last_hit(zgame::Handle e) override { call("0x0041d130", {static_cast<uint32_t>(ent_index(e))}); }
  zgame::Handle wm_group_weapon(zgame::Handle) override { return g_group_weapon; }
  void weapon_node_position(zgame::Handle w, float out[3]) override {
    call("0x004ed530", {w});
    std::memcpy(out, g_muzzle[w < 3 ? w : 0], 12);
  }
  float weapon_speed(zgame::Handle w) override { return g_wspeed[w < 3 ? w : 0]; }
  uint32_t weapon_flags(zgame::Handle w) override { return g_wflags[w < 3 ? w : 0]; }
  void entity_lock_position(zgame::Handle e, float out[3]) override {
    out[0] = out[1] = out[2] = 0;
    for (const Entity& en : g_entity)
      if (en.handle && en.handle == e) std::memcpy(out, en.pos, 12);
  }
  // The tests over the brain answer the clear line from the trace (their
  // emulators stub 0x004d5980); test_ai_clearline.py checks the port's.
  bool clear_line_at(zgame::Handle v, zgame::Handle e) override { return call("0x004d5980", {v, e, 1u}) & 0xff; }
  void zone_list(zgame::Handle e, uint8_t out[4]) override {
    const LineEnt* l = g_line_world ? line_by(e, &LineEnt::self) : nullptr;
    if (l) std::memcpy(out, l->zone, 4);
    else ai::BrainHooks::zone_list(e, out);
  }
  zgame::Handle entity_node(zgame::Handle e) override {
    const LineEnt* l = line_by(e, &LineEnt::self);
    return l ? l->node : 0;
  }
  zgame::Handle entity_centroid(zgame::Handle e) override {
    const LineEnt* l = line_by(e, &LineEnt::self);
    return l ? l->centroid : 0;
  }
  void node_world_cached(zgame::Handle n, float out[3]) override {
    const LineEnt* l = line_by(n, &LineEnt::centroid);
    out[0] = out[1] = out[2] = 0;
    if (!l) return;
    if (l->stale) call("0x004ed280", {n, 1u});
    std::memcpy(out, l->pos, 12);
  }
  bool node_intersect(zgame::Handle n) override {
    const LineEnt* l = line_by(n, &LineEnt::node);
    return l && l->flag;
  }
  int32_t segment_first(const float a[3], const float b[3], uint32_t exclude, uint8_t* count) override {
    const int32_t r = static_cast<int32_t>(call("0x004eed90", {bits(a[0]), bits(a[1]), bits(a[2]), bits(b[0]),
                                                               bits(b[1]), bits(b[2]), 1u, exclude}));
    *count = static_cast<uint8_t>(call("segment.count", {}));
    return r;
  }
  float aim_intensity() override { return g_aim_intensity; }
  bool targeting_aid(zgame::Handle) override { return g_aim_aid; }
  void wm_aim_at(zgame::Handle wm, const float at[3]) override {
    g_wm_x88 = 1;
    call("0x004b0bc0", {wm, bits(at[0]), bits(at[1]), bits(at[2])});
  }
  void velocity(zgame::Handle v, float out[3]) override { std::memcpy(out, g_vel[v == kOwner ? 0 : ent_index(v)], 12); }
  void vehicle_matrix(zgame::Handle, float m[9]) override { std::memcpy(m, g_owner_m, 36); }
  bool bearer_aim_clamp(zgame::Handle, const float d[3], float out[3]) override {
    std::memcpy(out, d, 12);
    return call("vehicle.vt+0x90", {bits(d[0]), bits(d[1]), bits(d[2])}) & 0xff;
  }
  void bearer_set_aim(zgame::Handle, const float d[3]) override {
    call("vehicle.vt+0x154", {bits(d[0]), bits(d[1]), bits(d[2])});
  }
  ai::Behaviour* manoeuvre_behaviour(int32_t which) override { return beh_of(which == 0 ? 98 : 88); }
  ai::Behaviour* nearest(ai::AiNet* n, const float*) override {
    return beh_of(static_cast<int32_t>(call("0x004d51d0", {n ? n->get_id() : 0})));
  }
  uint8_t vehicle_flags(zgame::Handle v) override { return v == kOwner ? g_target.xbc : 0; }
  int32_t player_lance_state() override {
    if (!g_pilot) return -1;
    call("lance.vt+0x48", {});
    return static_cast<int32_t>(call("lance.vt+0x2c", {}));
  }
  ai::AiNet* formation_net() override { return net_of(200); }
  zgame::Handle player_vehicle() override { return g_pilot ? g_player_vehicle : 0; }
  ai::Behaviour* follow_behaviour(ai::AiNet* n) override {
    return beh_of(static_cast<int32_t>(call("0x004c0de0", {n ? n->get_id() : 0})));
  }
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
  const std::vector<ai::Contact*>* contacts(zgame::Handle v) override {
    if (v != kOwner) return nullptr;
    return g_fire_world ? &g_fire_list : &g_steer_contacts;
  }
  zgame::Handle contact_entity(const ai::Contact* c) override {
    if (g_fire_world) {
      const int32_t i = fire_contact_index(c);
      return i ? ent_handle(g_contact_ent[i - 1]) : 0;
    }
    for (size_t i = 0; i < g_steer_contacts.size(); ++i)
      if (g_steer_contacts[i] == c) return g_entity[g_contact_entity[i]].handle;
    return 0;
  }
  void forward(zgame::Handle e, float* x, float* z) override {
    if (e == kOwner) *x = g_owner_fwd[0], *z = g_owner_fwd[2];
    for (const Entity& en : g_entity)
      if (en.handle && en.handle == e) *x = en.fwd[0], *z = en.fwd[1];
  }
  float speed_at(zgame::Handle, float share) override { return g_mech.speed_at(share); }
  float stopping_distance(zgame::Handle, float speed) override { return g_mech.stopping_distance(speed); }
  float stop_share(zgame::Handle, float d) override { return fl(call("vehicle.vt+0x12c", {bits(d)})); }
  // The attack's test: the vehicle net's unported states as fakes (the
  // emulator's makers stubbed the same), by its behaviour's +0x04 (0).
  ai::BehaviourState* vehicle_step_state(ai::Behaviour& b, ai::Order& o) override { return new FakeState(&o, &b, 0); }
  float move_speed(zgame::Handle v) override { return fl(call("vehicle.vt+0x114", {v})); }
  zgame::Handle weapons(zgame::Handle) override { return g_wm; }
  zgame::Handle wm_current(zgame::Handle wm) override { return call("0x004b04c0", {wm}); }
  void wm_clear_aim(zgame::Handle wm) override { call("0x004b0bc0", {wm, 0u}); }
  void wm_alpha(zgame::Handle wm, float s) override { call("0x004b0a70", {wm, bits(s)}); }
  bool weapon_type_ready(zgame::Handle w) override { return call("0x004b4ba0", {w}) & 0xff; }
  float weapon_reload(zgame::Handle w) override { return w < 3 ? g_reload[w] : 0.0f; }
  int32_t weapon_ammo(zgame::Handle w) override { return static_cast<int32_t>(call("weapon.vt+0x14", {w})); }
  void weapon_hold_for_flight(zgame::Handle w) override { call("0x004b4c90", {w}); }
  void torso_centre(zgame::Handle) override {
    if (g_has_mech) call("0x00465a00", {1u});
  }
  ai::FirePlan* fire_plan_pick(ai::Brain&) override { return make_plan(call("0x004b6790", {})); }
  void aim_track(ai::Brain&, ai::Contact* t, zgame::Handle w) override {
    call("0x004b72a0", {static_cast<uint32_t>(fire_contact_index(t)), w});
  }
  void aim_lead(ai::Brain&, float dt, ai::Contact* t, zgame::Handle w, zgame::Handle wm) override {
    call("0x004b77e0", {bits(dt), static_cast<uint32_t>(fire_contact_index(t)), w, wm});
  }
  ai::Behaviour* path_next(ai::Brain&, ai::Behaviour* at) override {
    return elem_at(call("0x004c7cb0", {static_cast<uint32_t>(elem_id(at))}));
  }
  bool inout_clamp(zgame::Handle v, float p[3]) override {
    if (!(call("0x004c3000", {v}) & 0xff)) return false;
    p[0] *= 0.5f, p[2] *= 0.5f;  // the test's boundary: halfway to the origin
    return true;
  }
  float vertical_speed(zgame::Handle) override { return g_vy; }
  void pilot_of(zgame::Handle, ai::Pilot* p) override {
    p->name = g_owner_gate ? "g1" : "g2";
    p->jump_jets = g_jump_jets;
  }
  void radio_line(ai::Brain&, int32_t event) override { call("0x004e6d50", {event}); }
  // The hit test's (test_ai_hits.py): the plan entities are their own mechs.
  zgame::Handle vehicle_mech(zgame::Handle v) override { return v; }
  void ally_hit(zgame::Handle attacker, zgame::Handle victim) override {
    call("attacker.ictl.vt+0x98", {static_cast<uint32_t>(ent_index(attacker)), victim});
  }
  void allies_turn() override { call("0x004d5dc0", {}); }
  void firing_reset(ai::Brain&) override { call("0x004e9590", {}); }
  void firing_run(ai::Brain&, float dt) override { call("0x004e9fb0", {bits(dt)}); }
  ai::AiNet* vehicle_net(zgame::Handle v) override {
    if (v && v == ent_handle(1)) return ai::vehicle_net_of(g_ent_target[1], v);  // the attack test's target: its own net
    return net_of(static_cast<int32_t>(call("vehicle.vt+0xd8", {v})));
  }
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
  zgame::Handle veh = kOwner;
  zgame::Handle vehicle() override { return veh; }
  void stop() override { call("ictl.vt+0x14", {}); }
  void smooth(float dt) override { call("brain.vt+0x4", {bits(dt)}); }
  bool hit_lately() override { return call("ictl.vt+0xa0", {}) & 1; }
  void fire(bool on) override { call("ictl.vt+0x10", {on ? 1u : 0u}); }
  bool reversing() override { return call("ictl.vt+0x18", {}) & 1; }
  float throttle_now() override { return fl(call("ictl.vt+0x1c", {})); }
  void set_throttle(float v) override { call("ictl.vt+0x20", {bits(v)}); }
  void set_turn(float v) override { call("ictl.vt+0x28", {bits(v)}); }
  float turn_goal() override { return fl(call("ictl.vt+0x30", {})); }
  void aim_started() override { call("ictl.vt+0x80", {}); }
};
FakeControls g_controls;
Hooks g_hooks;

// The non-node steps as the node test builds them: the real steps (their
// weights and passable tests), their states fakes the trace numbers.
template <class S>
struct TestStep : S {
  template <class... A>
  explicit TestStep(int32_t i, A&&... a) : S(std::forward<A>(a)...), id(i) {}
  ai::BehaviourState* make_state(ai::Order& o) override { return new FakeState(&o, this, id); }
  int32_t id;
};
std::vector<std::unique_ptr<ai::Behaviour>> g_elems;
std::vector<int32_t> g_elem_id;
std::vector<std::unique_ptr<ai::Valve>> g_node_valves;
ai::Behaviour* elem_at(uint32_t id) {
  return id >= 1 && id <= g_elems.size() ? g_elems[id - 1].get() : nullptr;
}
int32_t elem_id(const ai::Behaviour* b) {
  if (b == ai::null_step()) return -2;
  for (size_t i = 0; i < g_elems.size(); ++i)
    if (g_elems[i].get() == b) return static_cast<int32_t>(i) + 1;
  return b ? -1 : 0;
}

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
  g_nets.push_back(std::make_unique<FakeNet>(200, true));  // the formation net 0x00603fb8
  for (int32_t i : {1, 2, 3, 4, 100, 88, 98}) g_behs.push_back(std::make_unique<FakeBehaviour>(i));
  ai::AiParams rec;
  g_brain = std::make_unique<ai::Brain>(net_of(1), rec);
  g_brain->orders.clear();
  g_serial = 0;
  ai::attack_part_unstacked() = 0;
  for (ai::AiTarget& t : g_ent_target) {  // the attack test's target nets, made again each case
    delete t.net;
    t.net = nullptr;
  }
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

// 0x004d8990: the player's controls' tick, their settings' vlist the list
// above, their hit record (IControls +0x04: last, run start, attacker);
// out: the four valves' values.
void probe_ao_player_vlist(const uint32_t* hit, int32_t* out) {
  mech3::PlayerControls pc;
  pc.owner = kPlayerMech;
  pc.hit.last = fl(hit[0]), pc.hit.run_start = fl(hit[1]), pc.hit.by = hit[2];
  pc.settings.vlist = g_brain->params.vlist;
  pc.pilot_tick();
  pc.settings.vlist = nullptr;  // the probe's
  for (int i = 0; i < 4; ++i) out[i] = g_valves[static_cast<size_t>(i)]->get();
}

// The hit handlers (test_ai_hits.py): kind 0 the base 0x004aec90, 1 the
// AI's 0x004ea520 on the brain. w: the owner's team and allies, a contact on
// the player (0/1) and its dist2, the attacker (0 none, 1..4 an entity, 5
// the owner), the entities' teams and allies (4 x 2), the player's vehicle
// (0 none, 1..4), a radio (0/1), the record (last, run start, attacker), the
// time. out: the record after.
void probe_ao_hit(int32_t kind, const uint32_t* w, uint32_t* out) {
  static ai::Radio radio;
  static ai::Contact player;
  g_target.team = w[0], g_target.allies = w[1];
  player.dist2 = fl(w[3]);
  g_target.player_contact = w[2] ? &player : nullptr;
  const uint32_t attacker = w[4] == 5 ? kOwner : w[4] ? kPlanEnt + 0x400 * w[4] : 0;
  for (int k = 1; k <= 4; ++k) g_ent_target[k].team = w[5 + 2 * (k - 1)], g_ent_target[k].allies = w[6 + 2 * (k - 1)];
  g_pilot = true;
  g_player_vehicle = w[13] ? kPlanEnt + 0x400 * w[13] : 0;
  g_brain->radio = w[14] ? &radio : nullptr;
  ai::HitRecord r;
  r.last = fl(w[15]), r.run_start = fl(w[16]), r.by = w[17];
  g_now = fl(w[18]);
  if (kind) g_brain->on_hit(r, kOwner, attacker);
  else ai::controls_hit(r, kOwner, attacker);
  g_brain->radio = nullptr;
  g_target.player_contact = nullptr;
  out[0] = bits(r.last), out[1] = bits(r.run_start), out[2] = r.by;
}

// 0x004ea500 on the brain: the victim (an entity 1..4); out: the firing's
// obstacle (+0x28) after.
uint32_t probe_ao_hit_ally(uint32_t victim) {
  g_brain->controls = &g_controls;  // the stand-down's (its weapon manager's aim)
  g_brain->hit_an_ally(victim);
  return g_brain->firing.obstacle;
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

// The plain states' test (test_ai_states.py): the vehicle's +0xbc, the
// player's pilot and vehicle; then the current order's state made by one
// of the port's own behaviours (0 default, 1 shared, 2 null step, 3 stand,
// 4 hold) and run, or asked: op 0 make, 1 run(dt), 2 may break in, 3 may
// fire, 4 its behaviour (0 none, 1..5 those, -1 another).
void probe_ao_state_world(uint32_t xbc, int32_t pilot, uint32_t player_vehicle) {
  g_target.xbc = static_cast<uint8_t>(xbc);
  g_pilot = pilot != 0;
  g_player_vehicle = player_vehicle;
}
int32_t probe_ao_state(int32_t op, int32_t which, uint32_t dt) {
  ai::Brain& b = *g_brain;
  b.controls = &g_controls;
  ai::Behaviour* bs[5] = {ai::default_behaviour(), ai::shared_behaviour(), ai::null_step(), ai::stand_behaviour(),
                          ai::hold_behaviour()};
  ai::Order& o = b.current();
  switch (op) {
    case 0: return o.set_behaviour(bs[which]);
    case 1: o.state->run(fl(dt)); return 0;
    case 2: return o.state->may_break_in();
    case 3: return o.state->may_fire();
    case 4: {
      const ai::Behaviour* x = o.state->behaviour();
      if (!x) return 0;
      for (int i = 0; i < 5; ++i)
        if (x == bs[i]) return i + 1;
      return -1;
    }
  }
  return -2;
}

// The stack as test_ai_states.py compares it: per order its part's serial
// and net id, its state (a fake's serial, else the exe's vtable of the real
// one), +0x24..+0x2f; then the brain's +0x104 and +0x298. 8 words an order.
int32_t probe_ao_state_snapshot(uint32_t* out, int32_t cap) {
  ai::Brain& b = *g_brain;
  int32_t n = 0;
  for (const ai::Order& o : b.orders) {
    if ((n + 1) * 8 + 4 > cap) return -1;
    uint32_t* w = out + 8 * n++;
    const auto* p = dynamic_cast<const FakePart*>(o.net.get());
    w[0] = p ? p->serial : 0, w[1] = p ? static_cast<uint32_t>(p->net_id) : 0;
    const ai::BehaviourState* s = o.state.get();
    uint32_t code = 0;
    if (const auto* f = dynamic_cast<const FakeState*>(s)) code = f->serial;
    else if (dynamic_cast<const ai::SharedState*>(s)) code = 0x5cbed0;
    else if (s) {
      const ai::Behaviour* x = s->behaviour();
      code = x == ai::default_behaviour() ? 0x5cb330 : x == ai::null_step() ? 0x5cb258
             : x == ai::stand_behaviour() ? 0x5cb370 : x == ai::hold_behaviour() ? 0x5cb350 : 0x5cb238;
    }
    w[2] = code;
    w[3] = o.x04;
    std::memcpy(w + 4, o.x24, 12);
    w[7] = o.owner == &b;
  }
  uint32_t* t = out + 8 * n;
  t[0] = b.x104;
  std::memcpy(t + 1, b.x298, 12);
  return n;
}

// The node state's test (test_ai_node.py). The net: n rows of 13 words,
// element i+1 each: kind (0 node, 1 speed, 2 valve, 3 jump, 4 gate, 5
// backup node, 6 deploy node), a, b, c, four links, then five more. A
// node's a..c its position and its links (-1 none, -2 the null step, else
// an element's index); a deploy node's five its first valve (0, 1) and
// value, its second and value, its target's index; a step's l0 what
// follows, a the speed or jump's value, a valve's a its valve and b its
// value. The deploy valve "mcbdeploy" is the probe's third.
void probe_ao_node_net(const uint32_t* rows, int32_t n) {
  g_elems.clear();
  g_node_valves.clear();
  for (const char* nm : {"v1", "v2", "mcbdeploy"}) g_node_valves.push_back(std::make_unique<ai::Valve>(nm));
  ai::deploy_valve() = ai::DeployValve{g_node_valves[2].get(), 1};
  for (int32_t i = 0; i < n; ++i) {
    const uint32_t* r = rows + 13 * i;
    const int32_t id = i + 1;
    switch (r[0]) {
      case 0:
      case 5: {
        const float p[3] = {fl(r[1]), fl(r[2]), fl(r[3])};
        g_elems.push_back(
            std::make_unique<ai::NodeStep>(r[0] ? ai::StepKind::BackupNode : ai::StepKind::Node, p, nullptr, 0));
        break;
      }
      case 6: {
        const float p[3] = {fl(r[1]), fl(r[2]), fl(r[3])};
        g_elems.push_back(std::make_unique<ai::DeployNode>(p, nullptr, 0, nullptr, g_node_valves[r[8] & 1].get(),
                                                           static_cast<uint8_t>(r[9]), g_node_valves[r[10] & 1].get(),
                                                           static_cast<uint8_t>(r[11]), -1));
        break;
      }
      case 1: g_elems.push_back(std::make_unique<ai::ValueStep>(ai::StepKind::Speed, r[1])); break;
      case 2:
        g_elems.push_back(std::make_unique<ai::ValveStep>(g_node_valves[r[1] & 1].get(), static_cast<int32_t>(r[2])));
        break;
      case 3: g_elems.push_back(std::make_unique<TestStep<ai::ValueStep>>(id, ai::StepKind::Jump, r[1])); break;
      case 7: {  // a scan (a 0) or canleave (1 on, 2 not) step; b the scan's tests, two bits each
        static const ai::StepKind kinds[3] = {ai::StepKind::Scan, ai::StepKind::CanLeave, ai::StepKind::CanLeaveNot};
        auto st = std::make_unique<ai::ScanStep>(kinds[r[1] % 3], 3);
        if (r[1] % 3 == 1) st->tests.emplace_back(ai::ScanTest::CanLeave, 0);
        else if (r[1] % 3 == 2) st->tests.emplace_back(ai::ScanTest::CanLeaveNot, 0);
        else {
          const ai::ScanTest on[3] = {ai::ScanTest::AimOn, ai::ScanTest::HitOn, ai::ScanTest::TimeOn};
          const ai::ScanTest off[3] = {ai::ScanTest::AimOff, ai::ScanTest::HitOff, ai::ScanTest::TimeOff};
          for (int k = 0; k < 3; ++k)
            if (r[2] >> (2 * k) & 1) st->tests.emplace_back((r[2] >> (2 * k + 1) & 1) ? on[k] : off[k], 0);
        }
        g_elems.push_back(std::move(st));
        break;
      }
      default: g_elems.push_back(std::make_unique<TestStep<ai::GateStep>>(id, "g1")); break;
    }
  }
  auto at = [](int32_t k) -> ai::Behaviour* { return k == -2 ? ai::null_step() : g_elems[static_cast<size_t>(k)].get(); };
  for (int32_t i = 0; i < n; ++i) {
    const uint32_t* r = rows + 13 * i;
    ai::Behaviour* e = g_elems[static_cast<size_t>(i)].get();
    if (r[0] == 0 || r[0] == 5 || r[0] == 6) {
      auto* nd = static_cast<ai::NodeStep*>(e);
      if (r[0] == 6) static_cast<ai::DeployNode*>(e)->target_step = at(static_cast<int32_t>(r[12]));
      for (int k = 0; k < 4; ++k)
        if (static_cast<int32_t>(r[4 + k]) != -1) nd->links.push_back(at(static_cast<int32_t>(r[4 + k])));
      nd->attached = true;
    } else {
      static_cast<ai::Step*>(e)->next = at(static_cast<int32_t>(r[4]));
    }
  }
}

// The world: w = forward[3], the brain's +0xec, the mech's +0x518 and
// +0x534, the game time, the gate name matching, the jump jets, the two
// valves' values, the owner's position[3]; then +0x298[3], "mcbdeploy"'s
// value, the vehicle's vertical speed, whether the brain has a way finder.
void probe_ao_node_world(const uint32_t* w) {
  for (int i = 0; i < 3; ++i) g_owner_fwd[i] = fl(w[i]);
  g_brain->xec = fl(w[3]);
  g_mech.max_speed = fl(w[4]), g_mech.throttle_down = fl(w[5]);
  g_now = fl(w[6]);
  g_owner_gate = w[7] != 0;
  g_jump_jets = static_cast<int32_t>(w[8]);
  g_node_valves[0]->value->value = static_cast<int32_t>(w[9]);
  g_node_valves[1]->value->value = static_cast<int32_t>(w[10]);
  for (int i = 0; i < 3; ++i) g_pos[i] = fl(w[11 + i]), g_brain->x298[i] = fl(w[14 + i]);
  g_node_valves[2]->value->value = static_cast<int32_t>(w[17]);
  g_vy = fl(w[18]);
  if (w[19] && !g_brain->path.finder) g_brain->path.finder = new ai::PathFinder;  // its answers the trace's
  if (!w[19]) g_brain->path.reset();
  for (int k = 0; k < 4; ++k) g_brain->params.put<uint32_t>(k < 3 ? 0x98 + 4 * k : 0x3c, w[20 + k]);
  g_brain->xdc = fl(w[24]);
}

// op 0: the current order given element `which` (0x004e8120); 1 its state
// run(dt) (a node state's only); 2 its resume. out: the state (its
// element's id for a node state, minus a fake's serial, else 0x10000 and
// its behaviour's id), the node state's +0x0c..+0x24, the steering block,
// +0x298, +0xec, +0x104 (then cleared), +0x80, a deploy state's +0x2c..
// once set (phase 4 on), the three valves; +0x98, +0x9c, +0xa0, +0x3c, +0xdc
// at 40.
void probe_ao_node(int32_t op, int32_t which, uint32_t dt, uint32_t* out) {
  ai::Brain& b = *g_brain;
  b.controls = &g_controls;
  ai::Order& o = b.current();
  if (op == 0) {
    o.set_behaviour(g_elems[static_cast<size_t>(which)].get());
  } else if (auto* ns = dynamic_cast<ai::NodeState*>(o.state.get())) {
    if (op == 1) ns->run(fl(dt));
    else ns->resume();
  } else if (op == 1 && o.state && !dynamic_cast<FakeState*>(o.state.get())) {
    const ai::Behaviour* x = o.state->behaviour();
    if (x && (x->kind == ai::StepKind::Speed || x->kind == ai::StepKind::Valve || x->kind == ai::StepKind::Scan ||
              x->kind == ai::StepKind::CanLeave || x->kind == ai::StepKind::CanLeaveNot))
      o.state->run(fl(dt));
  }
  std::memset(out, 0, 4 * 46);
  ai::Order& cur = b.current();
  if (auto* ns = dynamic_cast<ai::NodeState*>(cur.state.get())) {
    out[0] = static_cast<uint32_t>(elem_id(ns->behaviour()));
    out[1] = static_cast<uint32_t>(ns->phase);
    const float f[6] = {ns->turn, ns->x14, ns->throttle, ns->slow, ns->inv_slow, ns->retry_at};
    std::memcpy(out + 2, f, 24);
  } else if (auto* fs = dynamic_cast<FakeState*>(cur.state.get())) {
    out[0] = static_cast<uint32_t>(-static_cast<int32_t>(fs->serial));
  } else if (cur.state) {  // another real state: its behaviour
    out[0] = 0x10000u | (static_cast<uint32_t>(elem_id(cur.state->behaviour())) & 0xffff);
  }
  std::memcpy(out + 8, &b.steer, 0x48);
  std::memcpy(out + 26, b.x298, 12);
  std::memcpy(out + 29, &b.xec, 4);
  out[30] = b.x104;
  b.x104 = 0;  // as the brain's frame clears it
  out[31] = b.params.get<uint32_t>(0x80);
  if (auto* ds = dynamic_cast<ai::DeployState*>(cur.state.get()); ds && ds->phase >= 4) std::memcpy(out + 32, ds->face, 12);
  for (int i = 0; i < 3; ++i) out[35 + i] = static_cast<uint32_t>(g_node_valves[static_cast<size_t>(i)]->get());
  for (int k = 0; k < 4; ++k) out[40 + k] = b.params.get<uint32_t>(k < 3 ? 0x98 + 4 * k : 0x3c);
  std::memcpy(out + 44, &b.xdc, 4);
}

// The link pick alone (0x004ccf20) at element `which` for heading h[3]:
// out[0] the step picked (an element's id, -2 the null step, 0 none),
// out[1] the links, then their running sums.
void probe_ao_pick(int32_t which, const uint32_t* hd, uint32_t* out) {
  ai::Brain& b = *g_brain;
  b.controls = &g_controls;
  const float heading[3] = {fl(hd[0]), fl(hd[1]), fl(hd[2])};
  std::vector<float> sums;
  ai::Behaviour* r = ai::pick_link(b, g_elems[static_cast<size_t>(which)].get(), heading, &sums);
  out[0] = static_cast<uint32_t>(elem_id(r));
  out[1] = static_cast<uint32_t>(sums.size());
  for (size_t i = 0; i < sums.size() && i < 8; ++i) out[2 + i] = bits(sums[i]);
}

// [0x0060b478] as the port has it.
void probe_ao_turn_params(uint32_t* out) { std::memcpy(out, &ai::turn_params(), 16); }

// The walking pieces alone: 0 turn_toward, 1 turn_or_back (f: fwd[3],
// to[3], keep); out the turn, dot, cross, reverse.
void probe_ao_turn(int32_t op, const uint32_t* f, uint32_t* out) {
  float a[7];
  for (int i = 0; i < 7; ++i) a[i] = fl(f[i]);
  float dot = 0, cross = 0;
  bool rev = false;
  const float t = op == 0 ? ai::turn_toward(a, a + 3, &dot, &cross, a[6], ai::turn_params())
                          : ai::turn_or_back(a, a + 3, &dot, &cross, a[6], &rev, ai::turn_params());
  out[0] = bits(t), out[1] = bits(dot), out[2] = bits(cross), out[3] = rev;
}

// The firing's test (test_ai_firing.py). w: the firing's +0x00..+0x28 (11
// words; +0x14 the plan's number), then its first and second sources'
// numbers, the look-around's until, dir[3], phase; the brain's +0xf0,
// +0xf8, +0xfc and aiv +0x34, +0x38, +0xa8, +0xac, +0x98; the vehicle's
// target contact (1..6, 0 none), range, +0xbc, the manager's number, the
// mech; the reloads of weapons 1, 2; the plans' slots, quick, line, kind
// (4 words each, plans 1..3); the target slots (3 contact numbers); then
// the contacts: n, and per contact its delta[3].
void probe_ao_fire_set(const uint32_t* w) {
  probe_app::Mute mute;  // the old plan and sources go unlogged
  ai::Brain& b = *g_brain;
  b.controls = &g_controls;
  ai::Firing& f = b.firing;
  g_fire_world = true;
  f.free_for_test();
  delete f.first;
  delete f.second;
  f.first = f.second = nullptr;
  f.phase = static_cast<int32_t>(w[0]);
  f.time = fl(w[1]), f.quick_delay = fl(w[2]), f.delay = fl(w[3]), f.rest = fl(w[4]);
  f.free_for_test();
  f.reach_arg = fl(w[6]), f.track_arg = fl(w[7]), f.fire_arg = fl(w[8]), f.range = fl(w[9]);
  f.obstacle = w[10];
  f.first = w[11] ? new FakeSource(static_cast<int32_t>(w[11])) : nullptr;
  f.second = w[12] ? new FakeSource(static_cast<int32_t>(w[12])) : nullptr;
  f.look.owner = &f;
  f.look.until = fl(w[13]);
  for (int i = 0; i < 3; ++i) f.look.dir[i] = fl(w[14 + i]);
  f.look.phase = static_cast<int32_t>(w[17]);
  b.xf0 = static_cast<int32_t>(w[18]), b.xf8 = static_cast<uint8_t>(w[19]), b.xfc = fl(w[20]);
  const int offs[5] = {0x34, 0x38, 0xa8, 0xac, 0x98};
  for (int i = 0; i < 5; ++i) b.params.put<uint32_t>(offs[i], w[21 + i]);
  const uint32_t* c = w + 48;  // the contacts
  const uint32_t n = c[0];
  g_fire_list.clear();
  for (uint32_t i = 0; i < 6; ++i) {
    ai::Contact& k = g_fire_contact[i];
    k = ai::Contact{};
    if (i < n) {
      for (int j = 0; j < 3; ++j) k.delta[j] = fl(c[1 + 3 * i + j]);
      g_fire_list.push_back(&k);
    }
  }
  g_target.target = w[26] ? &g_fire_contact[w[26] - 1] : nullptr;
  g_target.range = fl(w[27]);
  g_target.xbc = static_cast<uint8_t>(w[28]);
  g_wm = w[29];
  g_has_mech = w[30] != 0;
  g_reload[1] = fl(w[31]), g_reload[2] = fl(w[32]);
  for (int i = 0; i < 3; ++i) {
    const uint32_t* d = w + 33 + 4 * i;
    g_plan_def[i + 1] = PlanDef{static_cast<int32_t>(d[0]), d[1] != 0, d[2] != 0, static_cast<int32_t>(d[3])};
  }
  for (int i = 0; i < 3; ++i) g_target_slot[i] = w[45 + i] ? &g_fire_contact[w[45 + i] - 1] : nullptr;
  f.plan = make_plan(w[5]);
}

// op 0 the run(dt), 1 the stand-down (0x004b6700), 2 scan_aim (0x004d8930).
// out: the firing's 11 words (+0x14 its plan's number), its sources'
// numbers, the look-around's until, dir, phase (and whether its owner is
// the firing), the brain's +0xf8, +0xfc, scan_aim's answer, the contacts'
// distances.
void probe_ao_fire(int32_t op, uint32_t dt, uint32_t* out) {
  ai::Brain& b = *g_brain;
  ai::Firing& f = b.firing;
  uint32_t answer = 0;
  if (op == 0) f.run(fl(dt));
  else if (op == 1) f.stand_down();
  else answer = b.scan_aim();
  std::memset(out, 0, 4 * 32);
  out[0] = static_cast<uint32_t>(f.phase);
  const float fl5[4] = {f.time, f.quick_delay, f.delay, f.rest};
  std::memcpy(out + 1, fl5, 16);
  out[5] = f.plan ? static_cast<uint32_t>(static_cast<FakePlan*>(f.plan)->id) : 0;
  const float a3[4] = {f.reach_arg, f.track_arg, f.fire_arg, f.range};
  std::memcpy(out + 6, a3, 16);
  out[10] = f.obstacle;
  out[11] = f.first ? static_cast<uint32_t>(static_cast<FakeSource*>(f.first)->id) : 0;
  out[12] = f.second ? static_cast<uint32_t>(static_cast<FakeSource*>(f.second)->id) : 0;
  std::memcpy(out + 13, &f.look.until, 4);
  std::memcpy(out + 14, f.look.dir, 12);
  out[17] = static_cast<uint32_t>(f.look.phase);
  out[18] = f.look.owner == &f;
  out[19] = b.xf8;
  std::memcpy(out + 20, &b.xfc, 4);
  out[21] = answer;
  for (int i = 0; i < 6; ++i) std::memcpy(out + 22 + i, &g_fire_contact[i].dist, 4);
}

// The game time [0x0060991c] alone.
void probe_ao_set_now(uint32_t t) { g_now = fl(t); }

void probe_ao_fire_done() {
  probe_app::Mute mute;
  ai::Firing& f = g_brain->firing;
  f.free_for_test();
  delete f.first;
  delete f.second;
  f.first = f.second = nullptr;
  g_fire_world = false;
}

// The plan pick's test (test_ai_plan.py). w: per entity 1..4 (5 words):
// active, team, pos[3]; the vehicle's allies, forward x, z, position[3],
// the manager, the bearer, the firing's range and obstacle (an entity's
// number), aiv +0x30, the vehicle's target (a contact's number); then n
// and per contact (5 words) dist2, delta[3], its entity's number.
void probe_ao_plan_set(const uint32_t* w) {
  ai::Brain& b = *g_brain;
  b.controls = &g_controls;
  g_fire_world = true;
  for (int k = 1; k <= 4; ++k) {
    const uint32_t* e = w + 5 * (k - 1);
    g_ent_active[k] = e[0] != 0;
    g_ent_target[k].team = e[1];
    Entity& en = g_entity[static_cast<size_t>(k)];
    en.handle = ent_handle(k);
    for (int j = 0; j < 3; ++j) en.pos[j] = fl(e[2 + j]);
  }
  const uint32_t* v = w + 20;
  g_target.allies = v[0];
  g_owner_fwd[0] = fl(v[1]), g_owner_fwd[2] = fl(v[2]);
  for (int j = 0; j < 3; ++j) g_pos[j] = fl(v[3 + j]);
  g_wm = v[6];
  g_bearer = v[7] != 0;
  b.firing.range = fl(v[8]);
  b.firing.obstacle = ent_handle(static_cast<int32_t>(v[9]));
  b.params.put<uint32_t>(0x30, v[10]);
  const uint32_t* c = w + 32;
  const uint32_t n = c[0];
  g_fire_list.clear();
  for (uint32_t i = 0; i < 6; ++i) {
    ai::Contact& k = g_fire_contact[i];
    k = ai::Contact{};
    g_contact_ent[i] = 0;
    if (i < n) {
      const uint32_t* r = c + 1 + 5 * i;
      k.dist2 = fl(r[0]);
      for (int j = 0; j < 3; ++j) k.delta[j] = fl(r[1 + j]);
      g_contact_ent[i] = static_cast<int32_t>(r[4]);
      k.entity = ent_handle(g_contact_ent[i]);
      g_fire_list.push_back(&k);
    }
  }
  g_target.target = v[11] ? &g_fire_contact[v[11] - 1] : nullptr;
  // The track's: the group's weapon, per weapon 1, 2 its muzzle[3], speed,
  // flags; velocities (the vehicle's, entities 1..4); the vehicle's matrix.
  const uint32_t* t = c + 1 + 5 * 6;
  g_group_weapon = t[0];
  for (int k = 1; k <= 2; ++k) {
    const uint32_t* q = t + 1 + 5 * (k - 1);
    for (int j = 0; j < 3; ++j) g_muzzle[k][j] = fl(q[j]);
    g_wspeed[k] = fl(q[3]), g_wflags[k] = q[4];
  }
  for (int k = 0; k < 5; ++k)
    for (int j = 0; j < 3; ++j) g_vel[k][j] = fl(t[11 + 3 * k + j]);
  for (int j = 0; j < 9; ++j) g_owner_m[j] = fl(t[26 + j]);
  g_target.range = fl(v[8]);
  g_target.range2 = 0;  // the test's emulator side writes 0 too
}

ai::VolleyPlan* g_plan = nullptr;
int32_t slot_code(ai::Contact* const* slot) {
  if (slot == &g_target.target) return 0xff;
  for (size_t i = 0; i < g_fire_list.size(); ++i)
    if (slot == &g_fire_list[i]) return static_cast<int32_t>(i) + 1;
  return slot ? -1 : 0;
}

// op 0 the pick (0x004b6790), 1 the plan's in_reach(f), 2 its ready, 3 its
// take, 4 its track. out: the answer, the plan (or 0: 1), its slot (0xff the vehicle's
// target, else a contact's number), score, quick, line, kind, bearer, the
// firing's range and obstacle, the vehicle's range and range2 and target,
// the contacts' distances.
void probe_ao_plan(int32_t op, uint32_t f, uint32_t* out) {
  ai::Brain& b = *g_brain;
  std::memset(out, 0, 4 * 24);
  if (op == 0) {
    {
      probe_app::Mute mute;
      delete g_plan;
    }
    g_plan = static_cast<ai::VolleyPlan*>(ai::volley_plan_pick(b.firing));
    out[0] = g_plan != nullptr;
  } else if (g_plan) {
    if (op == 1) out[0] = g_plan->in_reach(fl(f));
    else if (op == 2) out[0] = g_plan->ready();
    else if (op == 4) out[0] = g_plan->track(fl(f));
    else {
      bool spent = false;
      out[0] = g_plan->take(&spent) == g_plan ? 1u : 0u;
      out[0] |= spent ? 0x100u : 0u;
    }
  }
  if (g_plan) {
    out[1] = 1;
    out[2] = static_cast<uint32_t>(slot_code(g_plan->target));
    std::memcpy(out + 3, &g_plan->score, 4);
    out[4] = g_plan->quick, out[5] = g_plan->line_check;
    out[6] = static_cast<uint32_t>(g_plan->kind), out[7] = g_plan->bearer;
    std::memcpy(out + 19, g_plan->aim, 12);
  }
  std::memcpy(out + 8, &b.firing.range, 4);
  out[9] = static_cast<uint32_t>(ent_index(b.firing.obstacle));
  std::memcpy(out + 10, &g_target.range, 4);
  std::memcpy(out + 11, &g_target.range2, 4);
  out[12] = static_cast<uint32_t>(fire_contact_index(g_target.target));
  for (int i = 0; i < 6; ++i) std::memcpy(out + 13 + i, &g_fire_contact[i].dist, 4);
}

// An entity made active or not between the plan's calls.
void probe_ao_plan_entity(int32_t k, int32_t active) { g_ent_active[k] = active != 0; }

void probe_ao_plan_done() {
  probe_app::Mute mute;
  delete g_plan;
  g_plan = nullptr;
  g_fire_world = false;
  for (int k = 1; k <= 4; ++k) g_entity[static_cast<size_t>(k)] = Entity{};
}

// The aim's test (test_ai_aim.py), over the plan pick's world. w: the
// difficulty's error, the aid, the pilot's +0xb8 and +0xc0, the manager's
// +0x88, then the aim's 9 words.
ai::Aim g_aim;
void probe_ao_aim_set(const uint32_t* w) {
  g_aim_intensity = fl(w[0]);
  g_aim_aid = w[1] != 0;
  g_brain->params.put<uint32_t>(0xb8, w[2]);
  g_brain->params.put<uint32_t>(0xc0, w[3]);
  g_wm_x88 = static_cast<uint8_t>(w[4]);
  const uint32_t* a = w + 5;
  for (int j = 0; j < 3; ++j) g_aim.offset[j] = fl(a[j]), g_aim.drift[j] = fl(a[3 + j]);
  g_aim.active = static_cast<uint8_t>(a[6]);
  g_aim.until = fl(a[7]);
  g_aim.limit2 = fl(a[8]);
}

// op 0 the track (0x004b72a0), 1 the lead (0x004b77e0) at contact `ct`
// (1..6) with weapon `w` (1, 2) and the manager 1. out: the aim's 9 words
// and the manager's +0x88.
void probe_ao_aim(int32_t op, int32_t ct, uint32_t w, uint32_t dt, uint32_t* out) {
  ai::Brain& b = *g_brain;
  ai::Contact* c = &g_fire_contact[ct - 1];
  if (op == 0) g_aim.track(b, c, w);
  else g_aim.lead(b, fl(dt), c, w, 1);
  std::memcpy(out, g_aim.offset, 12);
  std::memcpy(out + 3, g_aim.drift, 12);
  out[6] = g_aim.active;
  std::memcpy(out + 7, &g_aim.until, 4);
  std::memcpy(out + 8, &g_aim.limit2, 4);
  out[9] = g_wm_x88;
}

// The clear line (test_ai_clearline.py). w per entity (7 words): itself,
// its node, centroid, zone list (4 bytes), node flag, stale, then the
// position (3); then the zone mode. at: the point, or null.
void probe_ao_line(const uint32_t* w, const float* at, int32_t* out) {
  for (int k = 0; k < 2; ++k) {
    const uint32_t* e = w + 9 * k;
    LineEnt& l = g_line[k];
    l.self = e[0], l.node = e[1], l.centroid = e[2];
    std::memcpy(l.zone, &e[3], 4);
    l.flag = e[4] != 0, l.stale = e[5] != 0;
    for (int j = 0; j < 3; ++j) l.pos[j] = fl(e[6 + j]);
  }
  zmod::zones().mode = static_cast<int32_t>(w[18]);
  g_line_world = true;
  out[0] = ai::clear_line(g_line[0].self, g_line[1].self, at);
  g_line_world = false;
  zmod::zones().mode = 1;
}

// The vehicle's own net (test_ai_vehnet.py): made twice on one AiTarget,
// its steps and getters; then its part in an order of the brain with
// `mode` (+0x40), a vehicle when made or not, run (with one by then when
// `run`), then destroyed. prev: the brain's home point before (0 none).
float g_prev_home[3] = {};
int32_t vehnet_slot(const ai::VehicleNet& n, const ai::Behaviour* b) {
  for (size_t i = 0; i < n.elems.size(); ++i)
    if (b && n.elems[i] == b) return static_cast<int32_t>(i);
  return -1;
}
void probe_ao_vehnet(int32_t mode, int32_t vehicle_at_make, int32_t run, int32_t prev, const float* pos,
                     uint32_t* out) {
  ai::Brain& b = *g_brain;
  std::memset(out, 0, 4 * 26);
  ai::AiTarget t;
  ai::VehicleNet* n = ai::vehicle_net_of(t, kOwner);
  out[0] = ai::vehicle_net_of(t, kOwner) == n;
  out[1] = static_cast<uint32_t>(n->get_id());
  for (int i = 0; i < 7; ++i) out[2 + i] = static_cast<uint32_t>(static_cast<ai::Behaviour*>(n->elems[size_t(i)])->kind);
  out[9] = static_cast<uint32_t>(vehnet_slot(*n, n->follow()));
  out[10] = static_cast<uint32_t>(vehnet_slot(*n, n->attack()));
  out[11] = static_cast<uint32_t>(vehnet_slot(*n, n->own_order()));
  out[23] = n->vehicle == kOwner;
  b.controls = &g_controls;
  b.params.put<int32_t>(0x40, mode);
  b.xe4 = prev ? g_prev_home : nullptr;
  std::memcpy(g_pos, pos, 12);
  auto home_code = [&](const ai::VehiclePart& p) { return b.xe4 == p.home ? 2u : b.xe4 == g_prev_home ? 1u : 0u; };
  g_controls.veh = vehicle_at_make ? kOwner : 0;
  {
    ai::Order o;
    o.owner = &b;
    auto* part = static_cast<ai::VehiclePart*>(n->make_part(o));
    out[12] = part->pending;
    out[13] = static_cast<uint32_t>(vehnet_slot(*n, part->behaviour));
    std::memcpy(out + 14, part->home, 12);
    out[17] = home_code(*part);
    if (run) {
      g_controls.veh = kOwner;
      part->run(0.1f);
      out[18] = part->pending;
      out[19] = home_code(*part);
      std::memcpy(out + 20, part->home, 12);
    }
    delete part;
    out[24] = b.xe4 == g_prev_home ? 1u : b.xe4 ? 2u : 0u;
  }
  g_controls.veh = kOwner;
  b.xe4 = nullptr;
  delete n;
}

// The attack's world (test_ai_attack.py). w: the brain's +0x28, +0x2c,
// +0x3c, +0x58, +0x5c, +0x60, +0x84, +0xec, +0xdc, +0xe8, +0x230; the
// position, the forward x and z; per behaviour 1..4 a position (on, x, y, z).
float g_attack_home_mark[3] = {};
void probe_ao_attack_world(const uint32_t* w) {
  ai::Brain& b = *g_brain;
  b.controls = &g_controls;
  b.params.put<uint32_t>(0x28, w[0]);
  b.params.put<uint32_t>(0x2c, w[1]);
  b.params.put<uint32_t>(0x3c, w[2]);
  b.params.put<uint32_t>(0x58, w[3]);
  b.params.put<uint32_t>(0x5c, w[4]);
  b.params.put<uint32_t>(0x60, w[5]);
  b.params.put<uint32_t>(0x84, w[6]);
  b.xec = fl(w[7]), b.xdc = fl(w[8]), b.xe8 = fl(w[9]), b.x230 = fl(w[10]);
  for (int j = 0; j < 3; ++j) g_pos[j] = fl(w[11 + j]);
  g_owner_fwd[0] = fl(w[14]), g_owner_fwd[2] = fl(w[15]);
  for (int k = 1; k <= 4; ++k) {
    const uint32_t* q = w + 16 + 4 * (k - 1);
    g_step_pos_on[k] = q[0] != 0;
    for (int j = 0; j < 3; ++j) g_step_pos[k][j] = fl(q[1 + j]);
  }
}
// The home point's stack top: 0 none, 1 the test's own, 100 + the order of
// an attack part's, 2 else.
uint32_t home_code(const float* h) {
  if (!h) return 0;
  if (h == g_attack_home_mark) return 1;
  int32_t i = 0;
  for (const ai::Order& o : g_brain->orders) {
    if (const auto* p = dynamic_cast<const ai::AttackPart*>(o.net.get()); p && h == p->home) return 100u + uint32_t(i);
    if (const auto* p = dynamic_cast<const ai::VehiclePart*>(o.net.get()); p && h == p->home) return 100u + uint32_t(i);
    ++i;
  }
  return 2;
}
void probe_ao_attack_home(int32_t on) { g_brain->xe4 = on ? g_attack_home_mark : nullptr; }
int32_t probe_ao_attack(int32_t net) { return ai::attack(*g_brain, net_of(net)); }
// The stack, 22 words an order: test_ai_orders' 13, then for an attack
// part its net's id (else 0), its home point and the one under it; for an
// attack state its kind (1 end, 2 rearm, 3 return), the return's net id,
// waypoint and turn.
int32_t probe_ao_attack_stack(uint32_t* out, int32_t cap) {
  int32_t n = 0;
  for (const ai::Order& o : g_brain->orders) {
    if ((n + 1) * 22 > cap) return -1;
    uint32_t* w = out + 22 * n++;
    std::memset(w, 0, 4 * 22);
    const auto* p = dynamic_cast<const FakePart*>(o.net.get());
    const auto* s = dynamic_cast<const FakeState*>(o.state.get());
    w[0] = o.x04;
    w[1] = o.owner == g_brain.get();
    w[2] = p ? p->serial : 0, w[3] = p ? static_cast<uint32_t>(p->net_id) : 0;
    w[4] = s ? s->serial : 0, w[5] = s ? static_cast<uint32_t>(s->beh_id) : 0;
    w[6] = static_cast<uint32_t>(o.x14);
    std::memcpy(w + 7, o.x18, 12);
    std::memcpy(w + 10, o.x24, 12);
    if (const auto* ap = dynamic_cast<const ai::AttackPart*>(o.net.get())) {
      w[13] = static_cast<uint32_t>(ap->target->get_id());
      std::memcpy(w + 14, ap->home, 12);
      w[17] = home_code(ap->prev);
    }
    if (const auto* vp = dynamic_cast<const ai::VehiclePart*>(o.net.get())) {
      w[13] = 0x10000u + static_cast<uint32_t>(vp->target->get_id());
      std::memcpy(w + 14, vp->home, 12);
      w[17] = home_code(vp->prev);
    }
    if (const auto* fs = dynamic_cast<const ai::FightState*>(o.state.get())) {
      w[18] = 4;
      std::memcpy(w + 20, &fs->turn, 4);
    }
    if (dynamic_cast<const ai::AttackEndState*>(o.state.get())) w[18] = 1;
    if (dynamic_cast<const ai::AttackRearmState*>(o.state.get())) w[18] = 2;
    if (const auto* rs = dynamic_cast<const ai::AttackReturnState*>(o.state.get())) {
      w[18] = 3;
      w[19] = static_cast<uint32_t>(rs->net ? rs->net->get_id() : 0);
      std::memcpy(w + 20, rs->way, 8);  // x, y (z and the turn below)
    }
  }
  return n;
}
// The brain after: the home point's top, +0xe8, +0x230, +0xdc, +0x3c, the
// unstacked count, and each return state's waypoint z and turn.
void probe_ao_attack_brain(uint32_t* out) {
  ai::Brain& b = *g_brain;
  out[0] = home_code(b.xe4);
  std::memcpy(out + 1, &b.xe8, 4);
  std::memcpy(out + 2, &b.x230, 4);
  std::memcpy(out + 3, &b.xdc, 4);
  out[4] = b.params.get<uint32_t>(0x3c);
  out[5] = static_cast<uint32_t>(ai::attack_part_unstacked());
  int32_t k = 0;
  for (const ai::Order& o : g_brain->orders)
    if (const auto* rs = dynamic_cast<const ai::AttackReturnState*>(o.state.get()); rs && k < 4) {
      std::memcpy(out + 6 + 2 * k, &rs->way[2], 4);
      std::memcpy(out + 7 + 2 * k, &rs->turn, 4);
      ++k;
    }
}

// The fight's world (test_ai_attack.py): the target vehicle (entity 1 of
// the plan's world) at `pos`, active or not; the brain's +0xcc, +0x24 and
// +0x22c, the time.
void probe_ao_fight_world(const float* pos, int32_t active, uint32_t cc, uint32_t r24, uint32_t now, int32_t mode) {
  g_brain->params.put<int32_t>(0x40, mode);
  Entity& en = g_entity[1];
  en.handle = ent_handle(1);
  std::memcpy(en.pos, pos, 12);
  g_ent_active[1] = active != 0;
  g_brain->params.raw[0xcc] = static_cast<uint8_t>(cc);
  g_brain->params.put<uint32_t>(0x24, r24);
  g_brain->x22c = 0;
  g_now = fl(now);
}
// The fight pushed as the brain's frame does: on the target's own net, its
// attack behaviour (slot 1).
int32_t probe_ao_fight_push() {
  ai::VehicleNet* vn = ai::vehicle_net_of(g_ent_target[1], ent_handle(1));
  return g_brain->push(vn, vn->attack());
}

// The brain's own order (test_ai_attack.py): contacts 1..3 at a squared
// distance, on the target vehicle (entity 1) or none; the brain's +0x74 and
// +0x80; then its run.
void probe_ao_own_world(const uint32_t* w) {
  for (int k = 0; k < 3; ++k) {
    g_seen[k].dist2 = fl(w[2 * k]);
    g_seen_vehicle[k] = w[2 * k + 1] ? ent_handle(1) : 0;
  }
  g_brain->params.put<uint32_t>(0x74, w[6]);
  g_brain->params.put<uint32_t>(0x78, w[7]);
}
// The station keeping's world: the pilot's and the target's velocities
// (vt+0x08), the target active or not.
void probe_ao_station_world(const float* own_vel, const float* target_vel, int32_t active, const float* target_pos) {
  Entity& en = g_entity[1];
  en.handle = ent_handle(1);
  std::memcpy(en.pos, target_pos, 12);
  std::memcpy(g_vel[0], own_vel, 12);
  std::memcpy(g_vel[1], target_vel, 12);
  g_ent_active[1] = active != 0;
}
void probe_ao_own(uint32_t dt) { g_brain->run_own_order(fl(dt)); }
// The own order: its part (0 none, 1 the vehicle net's) with its net id,
// home point and the one under it, its state's serial (a fake's).
void probe_ao_own_row(uint32_t* out) {
  const ai::Order& o = g_brain->order;
  std::memset(out, 0, 4 * 14);
  if (const auto* vp = dynamic_cast<const ai::VehiclePart*>(o.net.get())) {
    out[0] = 1;
    out[1] = static_cast<uint32_t>(vp->target->get_id());
    std::memcpy(out + 2, vp->home, 12);
    out[5] = home_code(vp->prev);
    out[6] = vp->pending;
  } else if (o.net) {
    out[0] = 2;
  }
  if (const auto* fs = dynamic_cast<const FakeState*>(o.state.get())) out[7] = fs->serial;
  else if (o.state) out[7] = 0xffffffff;
  // The station keeping (10d2): its kind (1, 2 the hold), way, speed,
  // range, phase.
  if (const auto* st = dynamic_cast<const ai::StationState*>(o.state.get())) {
    out[7] = dynamic_cast<const ai::StationHoldState*>(st) ? 2u : 1u;
    std::memcpy(out + 8, &st->way, 4);
    std::memcpy(out + 9, &st->close, 4);
    std::memcpy(out + 10, &st->range, 4);
    out[11] = static_cast<uint16_t>(st->phase);
    std::memcpy(out + 12, &st->turn, 4);
    out[13] = static_cast<uint16_t>(st->count);
  }
}

}  // extern "C"
