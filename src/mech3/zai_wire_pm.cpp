#include "mech3/zai_wire_pm.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

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
  static const bool reports = std::atexit(report_missing) == 0;
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

PmBrain& pm_brain(Brain& b) {
  auto [it, fresh] = brains().try_emplace(&b);
  if (fresh) pm_seed(it->second.tick, b);
  return it->second;
}
TargetsPm& pm_targets(zgame::Handle vehicle) { return targets()[vehicle]; }
void pm_forget(Brain& b, zgame::Handle vehicle) {
  brains().erase(&b);
  targets().erase(vehicle);
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
  bool may_fire() override { return b.may_fire(); }
  void firing_reset() override { h.firing_reset(b); }
  zgame::Handle worst_attacker() override {
    pm_hook_missing("0x004b2820");
    return 0;
  }
  void take_attacker(zgame::Handle) override { pm_hook_missing("brain.vt+0x28"); }
  void manoeuvre() override { b.manoeuvre(); }
  void current(zgame::Handle* net, zgame::Handle* state) override {
    Order& o = b.current();
    *net = pm_id(o.net->net());
    *state = pm_id(o.state.get());
  }
  bool may_engage(zgame::Handle v, zgame::Handle net, zgame::Handle state) override {
    pm_hook_missing("stand-in: MW3's may_engage");
    return b.may_engage(v, static_cast<AiNet*>(pm_ptr(net)), static_cast<BehaviourState*>(pm_ptr(state)));
  }
  zgame::Handle target_vehicle(zgame::Handle c2) override { return h.contact_vehicle(contact(c2)); }
  zgame::Handle vehicle_net(zgame::Handle v) override { return pm_id(h.vehicle_net(v)); }
  zgame::Handle attack_behaviour(zgame::Handle net) override {
    return pm_id(h.attack_behaviour(static_cast<AiNet*>(pm_ptr(net))));
  }
  void attack(zgame::Handle net, zgame::Handle behaviour) override {
    pm_hook_missing("stand-in: MW3's attack");
    if (h.attack(b, b.current().net->net())) {
      b.x230 = h.health(c.vehicle()) - b.params.get<float>(0xb4);
      b.xe8 = b.params.get<float>(0x4c) * h.intensity() + h.now();
      b.push(static_cast<AiNet*>(pm_ptr(net)), static_cast<Behaviour*>(pm_ptr(behaviour)));
    }
  }
  int32_t rand_int() override { return h.rand(); }
  void run_orders(float dt) override {
    pm_hook_missing("stand-in: MW3's orders");
    do {
      b.x104 = 0;
      b.current().run(dt);
    } while (b.x104);
    if (!h.powered_down(c.vehicle()) && b.params.get<int32_t>(0x80) == 1) h.own_order_run(b, dt);
  }
  void firing(float dt) override {
    pm_hook_missing("stand-in: MW3's firing");
    h.firing_run(b, dt);
  }

 private:
  static Contact* contact(zgame::Handle id) { return static_cast<Contact*>(pm_ptr(id)); }
  Brain& b;
  BrainControls& c;
  BrainHooks& h;
};
}  // namespace

bool pm_frame(Brain& b, BrainControls& c, float dt) {
  b.controls = &c;
  PmBrain& r = pm_brain(b);
  r.tick.mode = b.params.get<int32_t>(0x3c);
  r.tick.busy = b.xf0;
  r.tick.give_up = b.x230, r.tick.attack_until = b.xe8;
  WiredTickPm hooks(b, c);
  const bool answer = tick_pm(hooks, r.tick, c.vehicle(), dt);
  b.params.put<int32_t>(0x3c, r.tick.mode);
  return answer;
}

const std::map<std::string, int>& pm_hooks_missing() { return missing_counts(); }
void pm_hooks_missing_clear() { missing_counts().clear(); }

}  // namespace mech3::ai
