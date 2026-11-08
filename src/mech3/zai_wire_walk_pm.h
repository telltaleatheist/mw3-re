#ifndef MECH3_ZAI_WIRE_WALK_PM_H
#define MECH3_ZAI_WIRE_WALK_PM_H

// WalkPmHooks on the program, one per brain: the steering's through WireSteer, the rest on the brain, its
// controls and BrainHooks. A hook with no answer yet is counted (pm_hook_missing).
#include "mech3/zai_walk_pm.h"
#include "mech3/zai_wire_pm.h"

namespace mech3::ai {

// The answers that aren't one line, shared by every WireWalkT.
namespace wire_walk {
zgame::Handle target_entity(Brain& b, zgame::Handle v);
bool has_target_contact(Brain& b, zgame::Handle v);
bool pull(Brain& b, zgame::Handle v, const float from[3], const float to[3], float out[3]);
float pilot_x544(Brain& b, zgame::Handle v);
double stopping_at(Brain& b, zgame::Handle v, float t);
zgame::Handle order_target(Brain& b);
const float* target_place(Brain& b, zgame::Handle t, float* buf);
const float* target_facing(Brain& b, zgame::Handle t, float* buf);
bool target_live(Brain& b, zgame::Handle t);
bool must_leave(Brain& b, zgame::Handle v);
const InOutVia* at_via(Brain& b, zgame::Handle v);
void queue_via(Brain& b, const InOutVia* via);
zgame::Handle next_along(Brain& b, const float dir[3]);
void set_behaviour(Brain& b, zgame::Handle beh);
void return_back(Brain& b);
void order_pop(Brain& b);
zgame::Handle contact_pick(Brain& b, zgame::Handle v, int32_t kind);
const float* entity_place(Brain& b, zgame::Handle t, float* buf);
}  // namespace wire_walk

// A template over its base, as WireSteer is: JumpPmHooks derives from WalkPmHooks.
template <class Base = WalkPmHooks>
class WireWalkT : public WireSteer<Base> {
 public:
  WireWalkT(Brain& b, BrainControls& c) : b_(b), c_(c) {}

  float turn_goal() override { return c_.turn_goal(); }
  float throttle_goal() override { return c_.throttle_now(); }
  float max_throttle() override { return b_.xec; }
  zgame::Handle target_entity(zgame::Handle v) override { return wire_walk::target_entity(b_, v); }
  bool has_target_contact(zgame::Handle v) override { return wire_walk::has_target_contact(b_, v); }
  bool pull(zgame::Handle v, const float from[3], const float to[3], float out[3]) override { return wire_walk::pull(b_, v, from, to, out); }
  float* way_from() override { return b_.x298; }
  bool has_pilot(zgame::Handle v) override { return brain_hooks().vehicle_mech(v) != 0; }
  float pilot_x544(zgame::Handle v) override { return wire_walk::pilot_x544(b_, v); }
  float now() override { return brain_hooks().now(); }
  double speed_at(zgame::Handle v, float t) override { return brain_hooks().speed_at(v, t); }
  double stop_distance(zgame::Handle v, float s) override { return brain_hooks().stopping_distance(v, s); }
  double stopping_at(zgame::Handle v, float t) override { return wire_walk::stopping_at(b_, v, t); }
  float x94() override { return b_.params.get<float>(0x94); }
  void clear_mode() override { pm_brain(b_).tick.mode = 0; }
  float range_far() override { return b_.params.get<float>(0x28); }
  float range_near() override { return b_.params.get<float>(0x2c); }
  zgame::Handle order_target() override { return wire_walk::order_target(b_); }
  const float* target_place(zgame::Handle t) override { return wire_walk::target_place(b_, t, place_); }
  const float* target_facing(zgame::Handle t) override { return wire_walk::target_facing(b_, t, facing_); }
  bool target_live(zgame::Handle t) override { return wire_walk::target_live(b_, t); }
  bool must_leave(zgame::Handle v) override { return wire_walk::must_leave(b_, v); }
  void set_throttle(float v) override { c_.set_throttle(v); }
  void set_turn(float v) override { c_.set_turn(v); }
  const InOutVia* at_via(zgame::Handle v) override { return wire_walk::at_via(b_, v); }
  void queue_via(const InOutVia* via) override { wire_walk::queue_via(b_, via); }
  zgame::Handle next_along(const float dir[3]) override { return wire_walk::next_along(b_, dir); }
  void set_behaviour(zgame::Handle b) override { wire_walk::set_behaviour(b_, b); }
  void set_again() override { b_.x104 = 1; }  // the brain's +0x158 as the orders' wiring keeps it
  void order_ready() override {}             // MW3's orders have no such byte: always ready (pm_run_pending)
  void return_back() override { wire_walk::return_back(b_); }
  int32_t rand_int() override { return brain_hooks().rand(); }
  float reach() override { return b_.params.get<float>(0x44); }
  double jump_fuel(zgame::Handle v) override { return brain_hooks().jump_fuel(v); }
  bool airborne(zgame::Handle v) override { return brain_hooks().airborne(v); }
  float forward_speed(zgame::Handle v) override { return brain_hooks().forward_speed(v); }
  double move_speed(zgame::Handle v) override { return brain_hooks().move_speed(v); }
  void order_pop() override { wire_walk::order_pop(b_); }
  zgame::Handle contact_pick(zgame::Handle v, int32_t kind) override { return wire_walk::contact_pick(b_, v, kind); }
  bool hit_lately(zgame::Handle) override { return c_.hit_lately(); }
  zgame::Handle attacker(zgame::Handle) override { return c_.attacker(); }
  bool lead_point(zgame::Handle v, zgame::Handle t, float out[3]) override { return brain_hooks().lead_point(v, t, out); }
  const float* entity_place(zgame::Handle t) override { return wire_walk::entity_place(b_, t, place_); }
  void set_fought(bool on) override { fought_ = on; }
  bool fought() override { return fought_; }
  uint8_t params_ec() override { return pm_brain(b_).tick.engage_flags; }
  float ceiling() override { return b_.params.get<float>(0x7c); }
  void set_jump(float v) override { c_.set_jump(v); }
  void set_jet_forward(float v) override { c_.set_jet_forward(v); }
  void stop_controls() override { c_.stop(); }
  void manoeuvre_over() override { --pm_brain(b_).tick.busy; }

 private:
  Brain& b_;
  BrainControls& c_;
  bool fought_ = false;       // the brain's +0x118
  float place_[3] = {};       // what entity_place hands back; the exe's is the entity's own
  float facing_[3] = {};
};

}  // namespace mech3::ai

#endif
