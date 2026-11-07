// The firing's and the attack's hooks on the program: what's answered reaches BrainHooks or the port's own pieces,
// the rest is counted, and the hook tables are left as found.
#include "check.h"
#include "mech3/weapon.h"
#include "mech3/zai_target.h"
#include "mech3/zai_wire_attack_pm.h"
#include "mech3/zai_wire_firing_pm.h"

using namespace mech3::ai;
using mech3::Weapon;
using mech3::WeaponManager;
using zgame::Handle;

namespace {
struct Fake : BrainHooks {
  AiTarget self;
  float now() override { return 12.0f; }
  float health(Handle) override { return 0.5f; }
  void position(Handle, float o[3]) override { o[0] = 1, o[1] = 2, o[2] = 3; }
  void forward(Handle, float* x, float* z) override { *x = 0.6f, *z = 0.8f; }
  AiTarget* targetable(Handle v) override { return v == 7 ? &self : nullptr; }
  Handle vehicle_mech(Handle v) override { return v; }
  Handle weapons(Handle) override { return 0x55; }
  int32_t rand() override { return 99; }
  float heat(Handle) override { return 2.5f; }
  WeaponManager* wm = nullptr;
  void* object_of(Handle h) override { return h == 0x55 ? static_cast<void*>(wm) : nullptr; }
  float speed_at(Handle, float t) override { return t; }
  float stopping_distance(Handle, float s) override { return s; }
};
struct Controls : BrainControls {
  Handle vehicle() override { return 7; }
  void stop() override {}
  void smooth(float) override {}
  bool hit_lately() override { return false; }
  void fire(bool on) override { fired += on; }
  int fired = 0;
  bool reversing() override { return false; }
  float throttle_now() override { return 0; }
  void set_throttle(float) override {}
  void set_turn(float) override {}
  void set_jump(float) override {}
  void set_jet_forward(float) override {}
  float turn_goal() override { return 0; }
  void aim_started() override {}
};
struct Installed {
  BrainHooks* old = &brain_hooks();
  AimPmHooks* aim = &aim_pm_hooks();
  PickPmHooks* pick = &pick_pm_hooks();
  ShotPmHooks* shot = &shot_pm_hooks();
  PlanPmHooks* plan = &plan_pm_hooks();
  StepPmHooks* step = &step_pm_hooks();
  explicit Installed(BrainHooks* h) {
    set_brain_hooks(h);
    pm_firing_install();
  }
  ~Installed() {
    set_brain_hooks(old);
    set_aim_pm_hooks(aim), set_pick_pm_hooks(pick), set_shot_pm_hooks(shot), set_plan_pm_hooks(plan), set_step_pm_hooks(step);
    pm_firing_forget(7);
    pm_wire_clear();
    pm_hooks_missing_clear();
  }
};
}  // namespace

TEST(pm_wire_firing_reaches_the_vehicle_and_counts_the_rest) {
  Fake f;
  Installed in(&f);
  EXPECT(pick_pm_hooks().now() == 12.0f && pick_pm_hooks().rand() == 99 && pick_pm_hooks().health(7) == 0.5f);
  EXPECT(pick_pm_hooks().forward(7)[2] == 0.8f);
  EXPECT(pick_pm_hooks().position(7)[1] == 2.0f);
  EXPECT(step_pm_hooks().wm(7) == 0x55);
  EXPECT(step_pm_hooks().target(7) == nullptr && step_pm_hooks().target(8) == nullptr);
  Contact c;
  f.self.target = &c;
  EXPECT(step_pm_hooks().target(7) == &c);
  EXPECT(*pick_pm_hooks().target_slot(7) == &c);
  pm_hooks_missing_clear();
  EXPECT(shot_pm_hooks().cost(1) == 0.0f);
  EXPECT(pm_hooks_missing().at("WireShot.cost") == 1);
}

TEST(pm_wire_firing_answers_what_the_step_calls_most) {
  Fake f;
  Installed in(&f);
  AiParams rec;
  Brain b(nullptr, rec);
  Controls c;
  b.controls = &c;
  pm_hooks_missing_clear();
  EXPECT(pick_pm_hooks().group(7) == 0);  // a mech has none: nothing counted
  step_pm_hooks().set_range(7, 300.0f);
  EXPECT(f.self.range == 300.0f);
  step_pm_hooks().trigger(&b, true);
  EXPECT(c.fired == 1);
  EXPECT(step_pm_hooks().heat(7) == 2.5);
  EXPECT(pm_hooks_missing().empty());
}

TEST(pm_wire_firing_lists_a_managers_weapons) {
  Fake f;
  Installed in(&f);
  WeaponManager m(nullptr);
  f.wm = &m;
  m.count = 2;
  m.weapons[0] = reinterpret_cast<Weapon*>(uintptr_t{0x10});  // only their handles are read
  m.weapons[1] = reinterpret_cast<Weapon*>(uintptr_t{0x20});
  const std::vector<Handle>& l = shot_pm_hooks().weapons(0x55);
  EXPECT(l.size() == 2 && l[0] == 0x10 && l[1] == 0x20);
  m.weapons[0] = m.weapons[1] = nullptr;
  m.count = 0;
  EXPECT(shot_pm_hooks().weapons(0x55).empty());
  EXPECT(step_pm_hooks().best_weapon(0x55) == 0);
  EXPECT(step_pm_hooks().group(0x55) == f.handle_of(m.group_ptr()));
  m.group = -1;  // none selected
  EXPECT(step_pm_hooks().group(0x55) == 0);
}

TEST(pm_wire_firing_keeps_a_record_a_vehicle) {
  Fake f;
  Installed in(&f);
  AiParams rec;
  Brain b(nullptr, rec);
  FiringRecord& r = pm_firing(b, 7);
  EXPECT(r.fire.vehicle == 7 && r.pick.brain == &b && &pm_firing(b, 7) == &r);
  EXPECT(r.fire.quick == 3.0f && r.fire.phase == 8);  // seeded
  EXPECT(r.pick.pilot.repick == 10.0f && r.pick.pilot.weights[0] == 1.0f);
  r.fire.quick = 4.0f;
  EXPECT(pm_firing(b, 7).fire.quick == 4.0f);  // once
}

TEST(pm_wire_attack_makes_its_context_and_counts_the_order_side) {
  Fake f;
  Installed in(&f);
  AiParams rec;
  Brain b(nullptr, rec);
  Controls c;
  WireWalk walk(b, c);
  SteerPm steer;
  WireAttack a(b, walk, steer, 7);
  AttackStatePm s;
  pm_hooks_missing_clear();
  EXPECT(a.context(s));  // no order target wired: no target
  EXPECT(a.position()[1] == 2.0f);
  EXPECT(a.speed() == 0.5 && a.now() == 12.0f && a.orders() == b.orders.size());
  a.fight_enter();
  EXPECT(s.speed == 1.0f);  // the brain's +0x10c, by the fake's speed
  a.queue_after(1, 2);
  EXPECT(pm_hooks_missing().at("0x004ea0f0") == 1);
}
