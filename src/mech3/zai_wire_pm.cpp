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
#include <typeinfo>
#include <string>

namespace mech3::ai {

namespace {
std::map<std::string, int>& missing_counts() {
  static std::map<std::string, int> m;
  return m;
}
}  // namespace

// PM's own AI is the game's (Owen, 2026-11-08); PM_AI=0 runs MW3's in its place, for comparing.
bool pm_ai_on() {
  static const bool on = [] {
    const char* v = std::getenv("PM_AI");
    return !(v && std::strcmp(v, "0") == 0);
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
// The pilot's numbers the exe's tick reads where they live, each frame: the scan steps (scan_hit, scan_time, canleave)
// and the scripts change them as it goes.
void pm_params(BrainTickPm& t, const AiParams& p) {
  t.sight = p.get<float>(0x1c), t.engage = p.get<float>(0x20), t.leash = p.get<float>(0x24);
  t.mode = p.get<int32_t>(0x3c), t.health_floor = p.get<float>(0x50);
  t.delay_on = p.get<int32_t>(0x5c), t.wait_low = p.get<float>(0x64), t.wait_high = p.get<float>(0x68);
  t.awake = p.get<int32_t>(0x88), t.by_player = p.get<int32_t>(0x8c), t.aims = p.get<int32_t>(0x98), t.reacts = p.get<int32_t>(0x9c);
  t.wanders = p.get<int32_t>(0xa0), t.manoeuvre_kind = p.get<int32_t>(0xa4);
  t.aim_gap = p.get<float>(0xa8), t.react_gap = p.get<float>(0xb0), t.engage_flags = p.get<uint8_t>(0xcc);
}

void pm_seed(BrainTickPm& t, const Brain& b) {
  pm_params(t, b.params);
  t.delay_until = b.xdc, t.busy = b.xf0, t.wait_until = b.xf4, t.home = b.xe4;
  t.attack_until = b.xe8, t.aim_at = b.xfc, t.react_at = b.x100, t.give_up = b.x230, t.runs_down = b.x2a4;
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
  if (fresh) {
    pm_seed(it->second.tick, b);
    it->second.steer.brain = &b;  // the steering's +0xa0: whose orders say whether it steers (0x004fa470)
  }
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
  // vt+0x0c: a compiled net's link yes, the built-in net's (0x005ed5b0) and a vehicle net's part no. MW3's built-in
  // net answers as a compiled one.
  bool net_allows(zgame::Handle net) override {
    AiNet* n = static_cast<AiNet*>(pm_ptr(net));
    return !dynamic_cast<BuiltinNet*>(n) && n->allows_attack();
  }
  bool state_allows(zgame::Handle state) override {
    return static_cast<BehaviourState*>(pm_ptr(state))->may_break_in();
  }
  float clock() override { return h.now(); }  // [0x008c1b50], another clock than [0x0062da90]: the attackers' own clock here
  void push_behaviour(int32_t which) override {  // 0x004fa170 of [0x00627cf8] (0) or [0x00627ce8] (1), as MW3's pair
    pm_push_behaviour(b, brain_hooks().manoeuvre_behaviour(which));
  }
  uint32_t allies(zgame::Handle v) override {
    const AiTarget* t = h.targetable(v);
    return t ? t->allies : 0;
  }
  uint32_t team(zgame::Handle a) override {
    const AiTarget* t = h.targetable(a);
    return t ? t->team : 0;
  }
  zgame::Handle attacker_vehicle(zgame::Handle a) override { return h.vehicle_mech(a); }  // as MW3's 0x004ea520 asks
  zgame::Handle player_vehicle() override { return h.player_vehicle(); }
  zgame::Handle player_contact(zgame::Handle v) override {
    const AiTarget* t = h.targetable(v);
    return t ? pm_id(t->player_contact) : 0;
  }
  float dist2(zgame::Handle c) override { return static_cast<Contact*>(pm_ptr(c))->dist2; }
  void radio_line(int32_t slot) override { h.radio_line(b, slot / 4); }
  // 0x004b2900(v, 1): the attacker's damage, last and first hit, three floats in a row.
  const float* hit_record(zgame::Handle v) override {
    AttackerPm* a = pm_brain(b).attackers.find(v, true, h.now());
    return a ? &a->sum : nullptr;
  }
  void allies_turn() override { h.allies_turn(); }
  // 0x004b2a00: the attacker list's add (0x004b2740, the damage its fourth argument), then an ally's hit told to
  // it, as MW3's 0x004aec90 (which also keeps MW3's one-attacker record, still read by hit_lately).
  void hit_base(int32_t, int32_t, int32_t, zgame::Handle attacker) override {
    pm_note_hit(b, attacker, damage);
    if (hit) controls_hit(*hit, owner, attacker);
  }
  // 0x004b2ab0 (MW3's 0x004aed50): the controls' tick once more; AiVehicleControls::died runs it before Brain::died
  // reaches here, in the exe's order (the tick, then the radio's line).
  void died_base() override {}
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

 public:
  HitRecord* hit = nullptr;  // a hit's, for hit_base
  zgame::Handle owner = 0;
  float damage = 0.0f;
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
  void steer_update(float dt, zgame::Handle v) override {  // +0x210's vt+0x04, whatever state runs
    pm_brain(b).steer.update(pm_walk(b, c), dt, v);
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
  // vt+0x28: a mech pilot's brain (0x005efdb0, made by 0x004e8c00) hands the attacker to its firing record's +0x90
  // (0x004ba5d0 -> 0x004ba5c0), which the next pick tries once (docs/pm-tests.md, One hit turns a lancemate); the
  // other brains' slot (0x005afca0) does nothing.
  void take_attacker(zgame::Handle a) override {
    if (!h.vehicle_mech(c.vehicle())) return;
    pm_hook_missing(a ? "took an attacker" : "took no attacker");  // for the first runs: the tick's reaction to a hit
    pm_firing(b, c.vehicle()).pick.prefer_next = a;
  }
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
    if (!pm_state_running(b)) {  // for the first runs: which of MW3's states still runs
      const BehaviourState* s = b.orders.empty() ? nullptr : b.current().state.get();
      pm_hook_missing((std::string("stand-in: MW3's orders, ") + (s ? typeid(*s).name() : "none")).c_str());
    }
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

void pm_note_hit(Brain& b, zgame::Handle attacker, float damage) {
  if (std::getenv("PM_TRACE")) std::printf("pm trace: %a hit by %u\n", brain_hooks().now(), attacker);
  pm_brain(b).attackers.add(attacker, damage, brain_hooks().now());
}

void pm_on_hit(Brain& b, HitRecord& r, zgame::Handle owner, zgame::Handle attacker, float damage) {
  WiredSidePm side(b);
  side.hit = &r, side.owner = owner, side.damage = damage;
  on_hit_pm(side, pm_brain(b).tick, owner, 0, 0, 0, attacker);
}

bool pm_scan_aim(Brain& b) {
  WiredSidePm side(b);
  return scan_aim_pm(side, pm_brain(b).tick, brain_hooks().now());
}

void pm_died(Brain& b) {
  WiredSidePm side(b);
  died_pm(side, pm_brain(b).tick);
}
zgame::Handle pm_hardest_attacker(Brain& b) { return pm_brain(b).attackers.hardest(brain_hooks().now()); }
zgame::Handle pm_latest_attacker(Brain& b) { return pm_brain(b).attackers.latest(brain_hooks().now()); }

bool pm_frame(Brain& b, BrainControls& c, float dt) {
  static const bool installed = (pm_firing_install(), true);
  (void)installed;
  b.controls = &c;
  PmBrain& r = pm_brain(b);
  r.vehicle = c.vehicle();
  pm_params(r.tick, b.params);
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
