#ifndef MECH3_ZAI_WIRE_WALK_PM_H
#define MECH3_ZAI_WIRE_WALK_PM_H

// WalkPmHooks on the program, one per brain: the steering's through WireSteer, the rest on the brain, its
// controls and BrainHooks. A hook with no answer yet is counted (pm_hook_missing).
#include "mech3/zai_walk_pm.h"
#include "mech3/zai_wire_pm.h"

namespace mech3::ai {

class WireWalk : public WireSteer<WalkPmHooks> {
 public:
  WireWalk(Brain& b, BrainControls& c) : b_(b), c_(c) {}

  float turn_goal() override { return c_.turn_goal(); }
  float throttle_goal() override { return c_.throttle_now(); }
  float max_throttle() override { return b_.xec; }
  zgame::Handle target_entity(zgame::Handle v) override;
  bool has_target_contact(zgame::Handle v) override;
  bool pull(zgame::Handle v, const float from[3], const float to[3], float out[3]) override;
  float* way_from() override { return b_.x298; }
  bool has_pilot(zgame::Handle v) override { return brain_hooks().vehicle_mech(v) != 0; }
  float pilot_x544(zgame::Handle v) override;
  float now() override { return brain_hooks().now(); }
  double speed_at(zgame::Handle v, float t) override { return brain_hooks().speed_at(v, t); }
  double stop_distance(zgame::Handle v, float s) override { return brain_hooks().stopping_distance(v, s); }
  double stopping_at(zgame::Handle v, float t) override;
  float x94() override { return b_.params.get<float>(0x94); }
  void clear_mode() override { pm_brain(b_).tick.mode = 0; }
  float range_far() override { return b_.params.get<float>(0x28); }
  float range_near() override { return b_.params.get<float>(0x2c); }
  zgame::Handle order_target() override;
  const float* target_place(zgame::Handle t) override;
  bool target_live(zgame::Handle t) override;
  bool must_leave(zgame::Handle v) override;
  void set_throttle(float v) override { c_.set_throttle(v); }
  void set_turn(float v) override { c_.set_turn(v); }
  const InOutVia* at_via(zgame::Handle v) override;
  void queue_via(const InOutVia* via) override;
  zgame::Handle next_along(const float dir[3]) override;
  void set_behaviour(zgame::Handle b) override;
  int32_t rand_int() override { return brain_hooks().rand(); }
  float reach() override { return b_.params.get<float>(0x44); }
  double jump_fuel(zgame::Handle v) override { return brain_hooks().jump_fuel(v); }
  bool airborne(zgame::Handle v) override { return brain_hooks().airborne(v); }
  float forward_speed(zgame::Handle v) override { return brain_hooks().forward_speed(v); }
  double move_speed(zgame::Handle v) override { return brain_hooks().move_speed(v); }
  void order_pop() override;
  zgame::Handle contact_pick(zgame::Handle v, int32_t kind) override;
  bool hit_lately(zgame::Handle) override { return c_.hit_lately(); }
  zgame::Handle attacker(zgame::Handle) override { return c_.attacker(); }
  bool lead_point(zgame::Handle v, zgame::Handle t, float out[3]) override { return brain_hooks().lead_point(v, t, out); }
  const float* entity_place(zgame::Handle t) override;
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
};

}  // namespace mech3::ai

#endif
