#include "mech3/zai_wire_pm.h"

#include "mech3/zai_wire_attack_pm.h"
#include "mech3/zai_wire_firing_pm.h"
#include "mech3/zai_wire_nets_pm.h"
#include "mech3/zai_wire_orders_pm.h"
#include "mech3/zai_wire_walk_pm.h"
#include "mech3/zai_wire_states_pm.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace mech3::ai {

namespace {
std::map<std::string, int>& missing_counts() {
  static std::map<std::string, int> m;
  return m;
}
}  // namespace

bool pm_ai_on() {
  static const bool on = [] {
    const char* v = std::getenv("PM_AI");
    return v && std::strcmp(v, "1") == 0;
  }();
  return on;
}

// The first run's report: every call with no answer, with its count.
static void report_missing() {
  for (const auto& [key, n] : missing_counts()) std::printf("mw3: pm ai: %8d %s\n", n, key.c_str());
}

void pm_hook_missing(const char* key) {
  static const bool reports = (missing_counts(), std::atexit(report_missing) == 0);  // the map outlives it
  (void)reports;
  if (++missing_counts()[key] == 1) std::printf("mw3: pm ai: no answer yet for %s\n", key);
}

// The params keep MW3's offsets (zai_params.h); +0xcc is PM's +0xec. The brain's own fields are
// the twins docs/pm-tests.md names; `home` and the times start as the Brain's ctor left them.
void pm_seed(BrainTickPm& t, const Brain& b) {
  const AiParams& p = b.params;
  t.sight = p.get<float>(0x1c), t.engage = p.get<float>(0x20), t.leash = p.get<float>(0x24);
  t.mode = p.get<int32_t>(0x3c), t.health_floor = p.get<float>(0x50);
  t.delay_on = p.get<int32_t>(0x5c), t.wait_low = p.get<float>(0x64), t.wait_high = p.get<float>(0x68);
  t.awake = p.get<int32_t>(0x88), t.by_player = p.get<int32_t>(0x8c), t.reacts = p.get<int32_t>(0x9c);
  t.wanders = p.get<int32_t>(0xa0), t.manoeuvre_kind = p.get<int32_t>(0xa4);
  t.react_gap = p.get<float>(0xb0), t.engage_flags = p.get<uint8_t>(0xcc);
  t.delay_until = b.xdc, t.busy = b.xf0, t.wait_until = b.xf4, t.home = b.xe4;
  t.attack_until = b.xe8, t.react_at = b.x100, t.give_up = b.x230, t.runs_down = b.x2a4;
  t.radio = b.radio != nullptr;
}

namespace {
std::map<const Brain*, PmBrain>& brains() {
  static std::map<const Brain*, PmBrain> m;
  return m;
}
std::map<zgame::Handle, TargetsPm>& targets() {
  static std::map<zgame::Handle, TargetsPm> m;
  return m;
}
std::map<const void*, uint32_t>& ids() {
  static std::map<const void*, uint32_t> m;
  return m;
}
std::map<uint32_t, void*>& ptrs() {
  static std::map<uint32_t, void*> m;
  return m;
}
uint32_t g_next_id = 1;
}  // namespace

PmBrain::PmBrain() = default;
PmBrain::~PmBrain() = default;

WireWalk& pm_walk(Brain& b, BrainControls& c) {
  PmBrain& r = pm_brain(b);
  if (!r.walk) r.walk = std::make_unique<WireWalk>(b, c);
  return *r.walk;
}

PmBrain& pm_brain(Brain& b) {
  auto [it, fresh] = brains().try_emplace(&b);
  if (fresh) pm_seed(it->second.tick, b);
  return it->second;
}
TargetsPm& pm_targets(zgame::Handle vehicle) { return targets()[vehicle]; }
void pm_forget(Brain& b, zgame::Handle vehicle) {
  if (const auto it = brains().find(&b); it != brains().end()) {
    if (!vehicle) vehicle = it->second.vehicle;
    brains().erase(it);
  }
  targets().erase(vehicle);
  pm_firing_forget(vehicle);
  pm_pending_forget(b);
}

uint32_t pm_id(const void* p) {
  if (!p) return 0;
  auto [it, fresh] = ids().try_emplace(p, g_next_id);
  if (fresh) ptrs()[g_next_id++] = const_cast<void*>(p);
  return it->second;
}
void* pm_ptr(uint32_t id) {
  const auto it = ptrs().find(id);
  return it == ptrs().end() ? nullptr : it->second;
}
void pm_ids_forget(const void* p) {
  const auto it = ids().find(p);
  if (it == ids().end()) return;
  ptrs().erase(it->second);
  ids().erase(it);
}
void pm_wire_clear() {
  brains().clear(), targets().clear(), ids().clear(), ptrs().clear();
  g_next_id = 1;
}

namespace {
// BrainSidePmHooks: what may_engage_pm asks first; the hit side waits for PM's states.
class WiredSidePm final : public BrainSidePmHooks {
 public:
  explicit WiredSidePm(Brain& brain) : b(brain), h(brain_hooks()) {}
  float now() override { return h.now(); }
  void position(zgame::Handle v, float out[3]) override { h.position(v, out); }
  double health(zgame::Handle v) override { return h.health(v); }
  bool net_allows(zgame::Handle net) override { return static_cast<AiNet*>(pm_ptr(net))->allows_attack(); }
  bool state_allows(zgame::Handle state) override {
    return static_cast<BehaviourState*>(pm_ptr(state))->may_break_in();
  }
  float clock() override { return none<float>("[0x008c1b50]"); }
  void push_behaviour(int32_t which) override {  // 0x004fa170 of [0x00627cf8] (0) or [0x00627ce8] (1), as MW3's pair
    pm_push_behaviour(b, brain_hooks().manoeuvre_behaviour(which));
  }
  uint32_t allies(zgame::Handle) override { return none<uint32_t>("vehicle+0xc4"); }
  uint32_t team(zgame::Handle) override { return none<uint32_t>("attacker.vt+0x28"); }
  zgame::Handle attacker_vehicle(zgame::Handle) override { return none<zgame::Handle>("attacker.vt+0x58"); }
  zgame::Handle player_vehicle() override { return h.player_vehicle(); }
  zgame::Handle player_contact(zgame::Handle) override { return none<zgame::Handle>("vehicle+0x98"); }
  float dist2(zgame::Handle c) override { return static_cast<Contact*>(pm_ptr(c))->dist2; }
  void radio_line(int32_t slot) override { h.radio_line(b, slot / 4); }
  const float* hit_record(zgame::Handle) override { return none<const float*>("0x004b2900"); }
  void allies_turn() override { h.allies_turn(); }
  void hit_base(int32_t, int32_t, int32_t, zgame::Handle) override { pm_hook_missing("0x004b2a00"); }
  void died_base() override { pm_hook_missing("0x004b2ab0"); }
  double speed_at(zgame::Handle, float) override { return none<double>("vehicle.vt+0x124"); }
  bool in_world(zgame::Handle) override { return none<bool>("vehicle+0x04"); }
  void set_way(const float*, const float*, float, zgame::Handle) override { pm_hook_missing("steer.set_way"); }

 private:
  template <class T>
  static T none(const char* key) {
    pm_hook_missing(key);
    return T{};
  }
  Brain& b;
  BrainHooks& h;
};

// TickPmHooks on a Brain. Where PM's piece isn't wired, MW3's runs and says so once.
class WiredTickPm final : public TickPmHooks {
 public:
  WiredTickPm(Brain& brain, BrainControls& controls) : b(brain), c(controls), h(brain_hooks()) {}
  bool picked = false;

  bool ai_on() override { return h.ai_on(); }
  float now() override { return h.now(); }
  void stop() override { c.stop(); }
  void smooth(float dt) override { c.smooth(dt); }
  int32_t power_state(zgame::Handle v) override { return h.powered_down(v); }
  void steer_update(float dt, zgame::Handle v) override {
    if (pm_state_running(b)) return pm_brain(b).steer.update(pm_walk(b, c), dt, v);
    pm_hook_missing("stand-in: MW3's steering");
    b.steer.update(dt, v);
  }
  zgame::Handle pick(zgame::Handle v) override { return pm_id(h.pick(v, kEnemy)); }
  zgame::Handle player_contact(zgame::Handle v) override {
    const AiTarget* t = h.targetable(v);
    return t ? pm_id(t->player_contact) : 0;
  }
  zgame::Handle target_contact(zgame::Handle v) override {
    const AiTarget* t = h.targetable(v);
    return t ? pm_id(t->target) : 0;
  }
  float dist2(zgame::Handle c2) override { return contact(c2)->dist2; }
  zgame::Handle contact_vehicle(zgame::Handle c2) override { return h.contact_vehicle(contact(c2)); }
  float range2(zgame::Handle v) override { return h.contact_range2(v); }
  void count_awake() override { h.count_awake(); }
  bool has_manager(zgame::Handle v) override {
    float a;
    return h.ammo_value(v, &a);
  }
  double ammo(zgame::Handle v) override {
    float a = 1.0f;
    h.ammo_value(v, &a);
    return a;
  }
  void radio_line(int32_t slot) override { h.radio_line(b, slot / 4); }
  void vlist(zgame::Handle v, bool* was_picked, zgame::Handle* enemy) override {
    Contact* t = contact(*enemy);
    b.vlist(c, v, was_picked, &t);
    *enemy = pm_id(t);
  }
  bool hit_lately() override { return c.hit_lately(); }
  bool may_fire() override { return pm_may_fire(b); }  // 0x004fa6d0
  void firing_reset() override { h.firing_reset(b); }
  zgame::Handle worst_attacker() override { return pm_latest_attacker(b); }
  void take_attacker(zgame::Handle) override { pm_hook_missing("brain.vt+0x28"); }
  void manoeuvre() override {  // 0x004fab30
    WiredSidePm side(b);
    manoeuvre_pm(side, pm_brain(b).tick);
  }
  void current(zgame::Handle* net, zgame::Handle* state) override {
    Order& o = b.current();
    *net = pm_id(o.net->net());
    *state = pm_id(o.state.get());
  }
  bool may_engage(zgame::Handle v, zgame::Handle net, zgame::Handle state) override {
    WiredSidePm side(b);
    const BrainTickPm& t = pm_brain(b).tick;
    const bool yes = may_engage_pm(side, t, v, net, state);
    if (!yes) {  // which check refused, for the first run
      if (t.mode != 1) pm_hook_missing(("why no engage: mode " + std::to_string(t.mode)).c_str());
      else if (net && !side.net_allows(net)) pm_hook_missing("why no engage: net");
      else if (state && !side.state_allows(state)) pm_hook_missing("why no engage: state");
      else pm_hook_missing("why no engage: leash, health or time");
    }
    return yes;
  }
  zgame::Handle target_vehicle(zgame::Handle c2) override { return h.contact_vehicle(contact(c2)); }
  zgame::Handle vehicle_net(zgame::Handle v) override { return pm_id(pm_vehicle_net(v)); }  // PM's ten slots
  zgame::Handle attack_behaviour(zgame::Handle net) override {
    return pm_id(h.attack_behaviour(static_cast<AiNet*>(pm_ptr(net))));
  }
  // 0x004fa150, 0x004ea740: PM's attack order over what runs. MW3's checks (0x004e9ca0) and its x230, xe8 aren't
  // PM's: the attack state keeps its own (+0x1f0, +0x108).
  void attack(zgame::Handle net, zgame::Handle behaviour) override {
    pm_give_order(b, static_cast<AiNet*>(pm_ptr(net)), static_cast<Behaviour*>(pm_ptr(behaviour)));
  }
  int32_t rand_int() override { return h.rand(); }
  void run_orders(float dt) override {
    if (!pm_state_running(b)) pm_hook_missing("stand-in: MW3's orders");
    pm_run_orders(b, dt);  // 0x004fb260: one pending item, then the last order, again while it asks
    if (!h.powered_down(c.vehicle()) && b.params.get<int32_t>(0x80) == 1) h.own_order_run(b, dt);
  }
  void firing(float dt) override { pm_firing(b, c.vehicle()).fire.step(dt); }

 private:
  static Contact* contact(zgame::Handle id) { return static_cast<Contact*>(pm_ptr(id)); }
  Brain& b;
  BrainControls& c;
  BrainHooks& h;
};
}  // namespace

void pm_note_hit(Brain& b, zgame::Handle attacker) {
  pm_hook_missing("the hit's damage");
  if (std::getenv("PM_TRACE")) std::printf("pm trace: %a hit by %u\n", brain_hooks().now(), attacker);
  pm_brain(b).attackers.add(attacker, 0.0f, brain_hooks().now());
}
zgame::Handle pm_hardest_attacker(Brain& b) { return pm_brain(b).attackers.hardest(brain_hooks().now()); }
zgame::Handle pm_latest_attacker(Brain& b) { return pm_brain(b).attackers.latest(brain_hooks().now()); }

bool pm_frame(Brain& b, BrainControls& c, float dt) {
  static const bool installed = (pm_firing_install(), true);
  (void)installed;
  b.controls = &c;
  PmBrain& r = pm_brain(b);
  r.vehicle = c.vehicle();
  r.tick.mode = b.params.get<int32_t>(0x3c);
  r.tick.busy = b.xf0;
  if (!pm_state_running(b))  // MW3's states keep these on the Brain; PM's attack keeps them in the record
    r.tick.give_up = b.x230, r.tick.attack_until = b.xe8, r.tick.home = b.xe4;
  WiredTickPm hooks(b, c);
  // For the first run: how a pilot comes to be awake (params +0x88 set, or the sight check).
  pm_hook_missing(r.tick.awake ? "awake: params +0x88" : r.tick.by_player ? "sight check: by +0x98" : "sight check: by the pick");
  const bool answer = tick_pm(hooks, r.tick, c.vehicle(), dt);
  b.params.put<int32_t>(0x3c, r.tick.mode);
  static const bool trace = std::getenv("PM_TRACE") != nullptr;  // for finding where two runs part
  if (trace)
    std::printf("pm trace: %a v%u mode %d busy %d orders %zu answer %d x104 %d\n", brain_hooks().now(), r.vehicle,
                r.tick.mode, int(r.tick.busy), b.orders.size(), int(answer), int(b.x104));
  return answer;
}

const std::map<std::string, int>& pm_hooks_missing() { return missing_counts(); }
void pm_hooks_missing_clear() { missing_counts().clear(); }

}  // namespace mech3::ai
