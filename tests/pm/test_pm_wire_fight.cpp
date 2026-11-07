// The firing's and the attack's hooks on the program: what's answered reaches BrainHooks or the port's own pieces,
// the rest is counted, and the hook tables are left as found.
#include "check.h"
#include "mech3/zai_steps.h"
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
  bool mech_none = false;
  Handle vehicle_mech(Handle v) override { return mech_none ? 0 : v; }
  Handle weapons(Handle) override { return 0x55; }
  int32_t rand() override { return 99; }
  float heat(Handle) override { return 2.5f; }
  WeaponManager* wm = nullptr;
  int refreshed = 0;
  zgame::Handle sighted = 0;
  float at[3] = {};
  void mech_aim_at_last_hit(Handle) override { ++refreshed; }
  bool clear_line(Handle, Handle e) override { sighted = e; return true; }
  bool can_face(Handle, const float d[3]) override { return d[0] > 0; }
  bool bearer_aim_clamp(Handle, const float d[3], float o[3]) override { o[0] = d[0], o[1] = d[1], o[2] = -d[2]; return true; }
  void bearer_set_aim(Handle, const float d[3]) override { at[0] = d[0]; }
  void wm_clear_aim(Handle) override { at[1] = -1.0f; }
  void velocity(Handle e, float o[3]) override { o[0] = e == 5 ? 10.0f : 0.0f, o[1] = 0, o[2] = 0; }
  void entity_lock_position(Handle, float o[3]) override { o[0] = 100, o[1] = 0, o[2] = 0; }
  Handle contact_entity(const Contact* c) override { return c->seen; }
  void vehicle_matrix(Handle, float m[9]) override {
    for (int i = 0; i < 9; ++i) m[i] = i % 4 == 0 ? 1.0f : 0.0f;
  }
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
  EXPECT(pick_pm_hooks().part_child(7, 0) == 0);  // no mech object
  step_pm_hooks().seed(1);
  EXPECT(pm_hooks_missing().at("WireStep.seed") == 1);
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
  EXPECT(pm_hooks_missing().size() == 1 && pm_hooks_missing().at("fired: trigger") == 1);  // the run's count of shots
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

TEST(pm_wire_firing_aims_through_the_vehicle_slots) {
  Fake f;
  Installed in(&f);
  plan_pm_hooks().refresh(7);
  EXPECT(f.refreshed == 1);
  EXPECT(plan_pm_hooks().in_sight(7, 9) && f.sighted == 9);
  const float d[3] = {1, 0, 2};
  float o[3];
  EXPECT(pick_pm_hooks().faces(7, d));
  EXPECT(plan_pm_hooks().turn(7, d, o) && o[2] == -2.0f);
  plan_pm_hooks().face(7, d);
  EXPECT(f.at[0] == 1.0f);
  step_pm_hooks().stand_down(7);
  EXPECT(f.at[1] == -1.0f);  // the manager's aim cleared
}

TEST(pm_wire_firing_places_are_never_null) {
  Fake f;
  Installed in(&f);
  EXPECT(plan_pm_hooks().matrix(7)[8] == 1.0f);
}

TEST(pm_wire_firing_aim_point_is_the_lead_point) {
  Fake f;
  Installed in(&f);
  Contact c;
  c.seen = 5;
  float o[3];
  // No weapon: nothing to lead, and with no mech's smoothed view the point stays.
  f.mech_none = true;
  EXPECT(plan_pm_hooks().aim_point(7, &c, 0, o));
  EXPECT(o[0] == 100.0f);
}

TEST(pm_wire_firing_threat_is_the_attacker_list) {
  Fake f;
  Installed in(&f);
  AiParams rec;
  Brain b(nullptr, rec);
  EXPECT(pick_pm_hooks().threat(&b, 0) == 0 && pick_pm_hooks().threat(&b, 1) == 0);  // none yet
  EXPECT(pm_hooks_missing().count("WirePick.threat") == 0);
}

TEST(pm_wire_firing_soonest_and_friends) {
  Fake f;
  Installed in(&f);
  EXPECT(step_pm_hooks().group_soonest(0) == 100000.0);  // no group
  Contact t, friend_;
  t.delta[0] = 20, friend_.delta[0] = 10;  // the friend in line, nearer
  t.dist = friend_.dist = -1;
  f.self.contacts = {&friend_, &t};
  auto* p = new PlanPm;
  Contact* slot = &t;
  p->slot = &slot, p->vehicle = 7;
  const zgame::Handle h = pm_id(p);
  EXPECT(!step_pm_hooks().clear_of_friends(h));
  friend_.delta[0] = -10, friend_.dist = -1;  // behind
  EXPECT(step_pm_hooks().clear_of_friends(h));
  f.self.contacts.clear();
  pm_ids_forget(p);
  delete p;
}

TEST(pm_wire_firing_stand_down_clears_the_fought_flag) {
  Fake f;
  Installed in(&f);
  AiParams rec;
  Brain b(nullptr, rec);
  Controls c;
  b.controls = &c;
  WireWalk& w = pm_walk(b, c);
  w.set_fought(true);
  step_pm_hooks().brain_x20(&b);
  EXPECT(!w.fought());
  pm_forget(b, 7);
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
  EXPECT(!pm_run_pending(b));
  mech3::ai::VehicleNet vn(9);
  EXPECT(b.set_net(&vn) && b.push_net(&vn) && b.push_net(&vn));
  a.return_to(0);  // back: the order popped, one a frame
  a.return_to(0);
  const size_t n = b.orders.size();
  EXPECT(pm_run_pending(b) && b.orders.size() == n - 1);
  EXPECT(pm_run_pending(b) && !pm_run_pending(b));
  pm_pending_forget(b);
  b.set_net(nullptr);
}
