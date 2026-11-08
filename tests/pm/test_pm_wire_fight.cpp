// The firing's and the attack's hooks on the program: what's answered reaches BrainHooks or the port's own pieces,
// the rest is counted, and the hook tables are left as found.
#include "check.h"
#include "mech3/zai_steps.h"
#include "mech3/weapon.h"
#include "mech3/zai_target.h"
#include "mech3/zai_wire_attack_pm.h"
#include "mech3/zai_wire_firing_pm.h"
#include "mech3/zai_wire_nets_pm.h"
#include "mech3/zai_wire_orders_pm.h"

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

namespace {
// A behaviour whose states count their resumes.
int g_resumes = 0;
struct Counted : Behaviour {
  struct State : BehaviourState {
    using BehaviourState::BehaviourState;
    void resume() override { ++g_resumes; }
  };
  BehaviourState* make_state(Order& o) override { return new State(&o, this); }
};
}  // namespace

TEST(pm_wire_orders_give_pop_and_run_on_the_brain) {
  Fake f;
  Installed in(&f);
  AiParams rec;
  Brain b(nullptr, rec);
  mech3::ai::VehicleNet vn(9);
  EXPECT(b.set_net(&vn));
  const size_t n = b.orders.size();
  pm_give_order(b, &vn, nullptr);  // 0x004ea740: an order over what runs
  EXPECT(b.orders.size() == n + 1);
  pm_push_behaviour(b, nullptr);  // 0x004ea7f0: on the last order's own net
  EXPECT(b.orders.size() == n + 2 && b.current().net && b.current().net->net() == &vn);
  EXPECT(pm_may_fire(b) == b.may_fire());  // the last order's state, as MW3's 0x004e9070
  pm_pop_order(b);
  pm_pop_order(b);
  EXPECT(b.orders.size() == n);
  // 0x004ea220: the order under the one popped resumes once (Brain::pop's resume stands for PM's).
  Counted under;
  pm_give_order(b, &vn, &under);
  pm_push_behaviour(b, nullptr);
  g_resumes = 0;
  pm_pop_order(b);
  EXPECT(g_resumes == 1 && b.current().state->from == &under);
  pm_pop_order(b);
  EXPECT(b.orders.size() == n);
  b.set_net(nullptr);
}

TEST(pm_wire_nets_ten_slots_and_the_kind_lookup) {
  Fake f;
  Installed in(&f);
  VehicleNet* n = pm_vehicle_net(7);
  EXPECT(n == pm_vehicle_net(7) && n == f.self.net && n->elems.size() == 10);  // on the vehicle, as MW3's
  EXPECT(pm_vehicle_net(9) == nullptr);  // no AiTarget
  EXPECT(n->follow() != nullptr && n->attack() != nullptr);  // MW3's asks still answer
  AiParams rec;
  Brain b(nullptr, rec);
  EXPECT(b.set_net(n));
  // 0x004c7800: kind 1 is slot 2, 7 is slot 9, anything else slot 0.
  EXPECT(pm_kind_behaviour(b, 1)->kind == static_cast<StepKind>(kVehicleSlotsPm[2]));
  EXPECT(pm_kind_behaviour(b, 7)->kind == static_cast<StepKind>(kVehicleSlotsPm[9]));
  EXPECT(pm_kind_behaviour(b, 0)->kind == StepKind::VehFollow);
  b.set_net(nullptr);
  delete f.self.net;  // znet's to free in the program
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

namespace mech3::ai {
bool pm_set_target(AiTarget& t, Contact* c, bool keep_sides);
}

namespace {
// Vehicles 21 (an ally, a lancemate the player commands) and 22 (an enemy); a contact's seen is its vehicle.
struct Teams : TargetHooks {
  Handle seen_vehicle(Handle s) override { return s; }
  Handle ai_target(Handle v) override { return v; }
  uint32_t team_of(Handle v) override { return v == 21 ? 2u : v == 22 ? 4u : 0u; }
};
struct Lancemate : BrainHooks {
  AiTarget mate;
  AiTarget* targetable(Handle v) override { return v == 21 ? &mate : nullptr; }
};
}  // namespace

TEST(pm_wire_set_target_keeps_the_sides_and_saves_a_lancemate) {
  Teams teams;
  TargetHooks& old_t = target_hooks();
  set_target_hooks(&teams);
  Lancemate f;
  f.mate.xbc = 0x10;
  BrainHooks& old_b = brain_hooks();
  set_brain_hooks(&f);
  AiTarget t;
  t.allies = 2, t.enemies = 4;
  Contact ally, enemy;
  ally.seen = 21, enemy.seen = 22;
  pm_set_target(t, &enemy, false);
  EXPECT(t.target == &enemy && t.enemy_target == &enemy && !t.ally_target && !t.saved_target);
  pm_set_target(t, &ally, false);  // 0x004e6330: a lancemate picked while none is held is saved
  EXPECT(t.target == &ally && t.ally_target == &ally && t.saved_target == &ally);
  t.saved_target = nullptr, t.xbc = 0x20;  // one held: none saved
  pm_set_target(t, &ally, false);
  EXPECT(!t.saved_target);
  pm_set_target(t, nullptr, false);  // none: the sides it was are cleared
  EXPECT(!t.target && !t.ally_target && t.enemy_target == &enemy);
  pm_set_target(t, &enemy, true);  // keep_sides: only the target
  EXPECT(t.target == &enemy && !t.ally_target);
  set_brain_hooks(&old_b);
  set_target_hooks(&old_t);
  pm_wire_clear();
}

TEST(pm_wire_scan_state_runs_its_tests_then_the_next) {
  Fake f;
  Installed in(&f);
  AiParams rec;
  Brain b(nullptr, rec);
  b.orders.emplace_back().owner = &b;
  ScanStep scan(StepKind::Scan, 2);
  scan.tests = {{ScanTest::AimOn, 0}, {ScanTest::TimeOn, 0}};
  Behaviour after;
  scan.next = &after;
  b.params.put<int32_t>(0x98, 0), b.params.put<int32_t>(0xa0, 0);
  b.current().state.reset(pm_make_scan_state(b.current(), scan));
  b.x104 = 0;
  b.current().state->run(0.1f);  // 0x004d5a10: this state goes
  EXPECT(b.params.get<int32_t>(0x98) == 1 && b.params.get<int32_t>(0xa0) == 1);
  EXPECT(b.x104 == 1);  // the brain's +0x158: run the order again
  EXPECT(b.current().state && b.current().state->from == &after);
}

namespace {
struct OneWeapon : BrainHooks {
  void* w = nullptr;
  void* object_of(Handle h) override { return h == 0x77 ? w : nullptr; }
};
}  // namespace

// A beam's heat by the second (0x004b8f10): its cycle is a period's recharge plus its firing, and the shot's heat
// (vt+0x1c, 0x004bc910) an eighth of that.
TEST(pm_wire_firing_a_beams_heat_rate) {
  mech3::WeaponType type;
  type.heat = 4.0f, type.fire_time = 0.5f, type.recharge_rate = 1.5f;
  zwep::WeaponDef def;
  def.type = &type, def.period = 2.0f;
  mech3::BeamWeapon beam(&def, nullptr, nullptr);
  OneWeapon f;
  f.w = &beam;
  Installed in(&f);
  const double cycle = 2.0 * 0.5 / 1.5 + 0.5;
  const float rate = static_cast<float>(1.0 / cycle * 4.0);
  EXPECT(shot_pm_hooks().cost(0x77) == rate);
  EXPECT(shot_pm_hooks().dps(0x77) == 0.125 * rate);
}

// The step's quick plan is one that faces its contact; any_can_hit (0x004bd1c0) asks for the others.
TEST(pm_wire_firing_a_quick_plan_faces) {
  Fake f;
  Installed in(&f);
  PlanPm p;
  p.faces = 1;
  EXPECT(step_pm_hooks().plan_quick(pm_id(&p)));
  p.faces = 0;
  EXPECT(!step_pm_hooks().plan_quick(pm_id(&p)));
  pm_ids_forget(&p);
}

// A queued follow (0x004ea0f0 with a 0x005eeca4) gives its order on the net it names.
TEST(pm_wire_a_queued_follow_runs_on_its_net) {
  Fake f;
  Installed in(&f);
  AiParams rec;
  Brain b(nullptr, rec);
  mech3::ai::VehicleNet vn(9);
  EXPECT(b.set_net(&vn));
  mech3::ai::VehicleNet other(10);
  pm_queue_follow(b, &other, nullptr);
  EXPECT(pm_run_pending(b));
  EXPECT(b.current().net && b.current().net->net() == &other);
  pm_pop_order(b);
  b.set_net(nullptr);
}

// 0x004fa470's net half (vt+0x14): idle on a vehicle net's or a compiled net's part, not on the built-in net's.
TEST(pm_wire_orders_idle_but_on_the_built_in_net) {
  Fake f;
  Installed in(&f);
  AiParams rec;
  Brain b(nullptr, rec);
  mech3::ai::VehicleNet vn(9);
  EXPECT(b.set_net(&vn));
  pm_give_order(b, &vn, nullptr);
  EXPECT(pm_orders_idle(b));
  mech3::ai::BuiltinNet bn(7, 0);
  pm_give_order(b, &bn, nullptr);
  EXPECT(!pm_orders_idle(b));
  pm_pop_order(b);
  pm_pop_order(b);
  b.set_net(nullptr);
}
