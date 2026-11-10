// The walk's hooks on the program (zai_wire_walk_pm): each asks the controls, the brain or BrainHooks,
// and what has no answer yet is counted.
#include <vector>

#include "check.h"
#include "mech3/zai_node.h"
#include "mech3/zai_steps.h"
#include "mech3/zai_wire_attack_pm.h"
#include "mech3/zainet_inout.h"
#include "mech3/zai_wire_walk_pm.h"

using namespace mech3::ai;
using zgame::Handle;

namespace {
struct Fake : BrainHooks {
  float clock = 50.0f, life = 0.5f, spot[3] = {1, 2, 3};
  AiTarget self;
  Contact c;
  bool in_air = false;
  float now() override { return clock; }
  float health(Handle) override { return life; }
  void position(Handle, float o[3]) override { o[0] = spot[0], o[1] = spot[1], o[2] = spot[2]; }
  AiTarget* targetable(Handle v) override { return v == 7 ? &self : nullptr; }
  Handle contact_entity(const Contact* k) override { return k->seen; }
  Contact* pick(Handle, int32_t side) override { return side == 1 ? &c : nullptr; }
  Handle vehicle_mech(Handle v) override { return v == 7 ? v : 0; }
  float speed_at(Handle, float t) override { return t * 2.0f; }
  double stopping_distance(Handle, float s) override { return s * 3.0f; }
  float jump_fuel(Handle) override { return 0.25f; }
  bool airborne(Handle) override { return in_air; }
  float forward_speed(Handle) override { return 4.0f; }
  float move_speed(Handle) override { return 5.0f; }
  int32_t rand() override { return 1234; }
  bool entity_active(Handle e) override { return e == 9; }
  void forward(Handle, float* x, float* z) override { *x = 0.6f, *z = 0.8f; }
};
struct Controls : BrainControls {
  float sent[4] = {};
  int stops = 0;
  Handle vehicle() override { return 7; }
  void stop() override { ++stops; }
  void smooth(float) override {}
  bool hit_lately() override { return true; }
  void fire(bool) override {}
  bool reversing() override { return false; }
  float throttle_now() override { return 0.75f; }
  void set_throttle(float v) override { sent[0] = v; }
  void set_turn(float v) override { sent[1] = v; }
  void set_jump(float v) override { sent[2] = v; }
  void set_jet_forward(float v) override { sent[3] = v; }
  float turn_goal() override { return -0.5f; }
  void aim_started() override {}
  Handle attacker() override { return 33; }
};
struct Installed {
  BrainHooks* old = &brain_hooks();
  explicit Installed(BrainHooks* h) { set_brain_hooks(h); }
  ~Installed() {
    set_brain_hooks(old);
    pm_wire_clear();
    pm_hooks_missing_clear();
  }
};
}  // namespace

TEST(pm_wire_walk_reaches_controls_and_vehicle) {
  Fake f;
  Installed in(&f);
  AiParams rec;
  Brain b(nullptr, rec);
  b.xec = 0.625f;
  Controls c;
  WireWalk w(b, c);
  EXPECT(w.turn_goal() == -0.5f && w.throttle_goal() == 0.75f && w.max_throttle() == 0.625f);
  w.set_throttle(0.1f), w.set_turn(0.2f), w.set_jump(0.3f), w.set_jet_forward(0.4f), w.stop_controls();
  EXPECT(c.sent[0] == 0.1f && c.sent[1] == 0.2f && c.sent[2] == 0.3f && c.sent[3] == 0.4f && c.stops == 1);
  EXPECT(w.speed_at(7, 2.0f) == 4.0 && w.stop_distance(7, 2.0f) == 6.0);
  EXPECT(w.jump_fuel(7) == 0.25 && w.forward_speed(7) == 4.0f && w.move_speed(7) == 5.0);
  EXPECT(w.now() == 50.0f && w.rand_int() == 1234 && w.hit_lately(7) && w.attacker(7) == 33);
  EXPECT(w.has_pilot(7) && !w.has_pilot(8));
  f.in_air = true;
  EXPECT(w.airborne(7));
  EXPECT(w.way_from() == b.x298);
}

TEST(pm_wire_walk_reads_the_vehicles_target) {
  Fake f;
  Installed in(&f);
  AiParams rec;
  Brain b(nullptr, rec);
  Controls c;
  WireWalk w(b, c);
  f.c.seen = 9;
  EXPECT(!w.has_target_contact(7) && w.target_entity(7) == 0);
  f.self.target = &f.c;
  EXPECT(w.has_target_contact(7) && w.target_entity(7) == 9);
  EXPECT(!w.has_target_contact(8));
  EXPECT(w.contact_pick(7, 1) == 9 && w.contact_pick(7, 3) == 0);
  EXPECT(w.entity_place(9)[2] == 3.0f);
  EXPECT(!w.fought());
  w.set_fought(true);
  EXPECT(w.fought());
}

TEST(pm_wire_walk_mode_and_leave_share_the_record) {
  Fake f;
  Installed in(&f);
  AiParams rec;
  Brain b(nullptr, rec);
  Controls c;
  WireWalk w(b, c);
  PmBrain& p = pm_brain(b);
  p.tick.mode = 1, p.tick.engage_flags = 4, p.tick.health_floor = 0.25f;
  EXPECT(!w.must_leave(7));
  f.life = 0.125f;
  EXPECT(w.must_leave(7));
  f.life = 0.5f;
  w.clear_mode();
  EXPECT(p.tick.mode == 0 && w.must_leave(7));
  p.tick.busy = 2;
  w.manoeuvre_over();
  EXPECT(p.tick.busy == 1);
}

TEST(pm_wire_walk_order_target_is_the_vehicle_nets) {
  Fake f;
  Installed in(&f);
  AiParams rec;
  Brain b(nullptr, rec);
  Controls c;
  WireWalk w(b, c);
  EXPECT(w.order_target() == 0);
  VehicleNet vn(9);
  EXPECT(b.set_net(&vn));
  EXPECT(w.order_target() == 9);
  EXPECT(w.target_place(9)[2] == 3.0f && w.target_live(9) && !w.target_live(8));
  EXPECT(w.target_facing(9)[2] == 0.8f);
  b.set_net(nullptr);
}

TEST(pm_wire_walk_counts_what_has_no_answer) {
  Fake f;
  Installed in(&f);
  AiParams rec;
  Brain b(nullptr, rec);
  Controls c;
  WireWalk w(b, c);
  pm_hooks_missing_clear();
  EXPECT(w.pilot_x544(8) == 0.0f);  // no mech
  EXPECT(!w.target_place(0) && !w.target_live(0) && !w.target_facing(0));
  EXPECT(w.stopping_at(7, 1.5f) == 9.0);  // 1.5 x 2, then x 3
  EXPECT(pm_hooks_missing().empty());
}

namespace {
// A node with two links that each weigh `w`, and a lone link.
struct Node : Behaviour {
  std::vector<Behaviour*> to;
  float w = 1.0f;
  float at[3] = {0, 0, 0};
  int32_t link_count() const override { return int32_t(to.size()); }
  Behaviour* link(int32_t i) const override { return to[size_t(i)]; }
  const float* way_point() const override { return at; }
  float weight(Brain&, const Behaviour*, LinkWay* way) override {
    way->dir[0] = 0, way->dir[2] = 1;
    return w;
  }
};
}  // namespace

TEST(pm_wire_walk_next_along_picks_from_the_states_node) {
  Fake f;
  Installed in(&f);
  AiParams rec;
  Brain b(nullptr, rec);
  Controls c;
  WireWalk w(b, c);
  Node here, a, z;
  here.to = {&a};
  const float dir[3] = {0, 0, 1};
  EXPECT(w.next_along(dir) == 0);  // no order
  b.orders.emplace_back().owner = &b;
  b.current().state = std::make_unique<BehaviourState>(&b.current(), &here);
  EXPECT(w.next_along(dir) == pm_id(&a));  // the lone link
  a.w = 0;
  EXPECT(w.next_along(dir) == 0);  // weighs nothing
  a.w = 1;
  here.to = {&a, &z};
  // both straight on: the sums 1 and 2, the roll 1234 x 2 / 32767 below the first
  EXPECT(w.next_along(dir) == pm_id(&a));
  b.current().state = std::make_unique<BehaviourState>(&b.current(), nullptr);
  EXPECT(w.next_along(dir) == 0);  // a state on no node
  EXPECT(pm_hooks_missing().empty());
}

TEST(pm_wire_walk_queue_via_leaves_a_pending_item) {
  Fake f;
  Installed in(&f);
  AiParams rec;
  Brain b(nullptr, rec);
  Controls c;
  WireWalk w(b, c);
  Node net, node;
  InOutVia via;
  via.net = &net, via.node = &node;
  w.queue_via(&via);
  w.queue_via(&via);
  EXPECT(pm_pending_count(b) == 2);
  pm_queue_back(b, nullptr);  // the terminus's (0x004e1fd0)
  pm_queue_back(b, &node);
  EXPECT(pm_pending_count(b) == 4);
  EXPECT(pm_hooks_missing().empty());
  pm_pending_forget(b);
}
