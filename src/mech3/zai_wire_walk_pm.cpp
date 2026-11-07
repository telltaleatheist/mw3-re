#include "mech3/zai_wire_walk_pm.h"

#include "mech3/mech.h"
#include "mech3/zai_brain_pm.h"
#include "mech3/zai_steps.h"
#include "mech3/zainet_inout.h"

namespace mech3::ai {

namespace {
struct Engage : EngagePmHooks {
  float now() override { return brain_hooks().now(); }
  void position(zgame::Handle v, float o[3]) override { brain_hooks().position(v, o); }
  double health(zgame::Handle v) override { return brain_hooks().health(v); }
};
}  // namespace

zgame::Handle wire_walk::target_entity(Brain& b_, zgame::Handle v) {
  const AiTarget* t = brain_hooks().targetable(v);
  return t && t->target ? brain_hooks().contact_entity(t->target) : 0;
}

bool wire_walk::has_target_contact(Brain& b_, zgame::Handle v) {
  const AiTarget* t = brain_hooks().targetable(v);
  return t && t->target;
}

bool wire_walk::pull(Brain& b_, zgame::Handle v, const float from[3], const float to[3], float out[3]) {
  Pilot p;
  brain_hooks().pilot_of(v, &p);
  return inout_pull(v, from, to, out, p.jump_jets != 0);
}

// PM's mech +0x544 is the configuration's +0x7c (0x00414cb0), the port's throttle_up.
float wire_walk::pilot_x544(Brain& b_, zgame::Handle v) {
  const zgame::Handle m = brain_hooks().vehicle_mech(v);
  const Mech* k = m ? static_cast<const Mech*>(brain_hooks().object_of(m)) : nullptr;
  return k ? k->throttle_up : 0.0f;
}

// The mech's 0x0041e010: its stopping distance (vt+0x12c) from its top speed times `t`, rounded to a float between.
double wire_walk::stopping_at(Brain& b_, zgame::Handle v, float t) {
  return brain_hooks().stopping_distance(v, brain_hooks().speed_at(v, t));
}

// For the first run orders are MW3's (dev 2): what the order is about is its vehicle net's vehicle, as
// StationHoldState reads it.
zgame::Handle wire_walk::order_target(Brain& b_) {
  if (b_.orders.empty() || !b_.current().net) return 0;
  const auto* vn = dynamic_cast<VehicleNet*>(b_.current().net->net());
  return vn ? vn->vehicle : 0;
}

const float* wire_walk::target_place(Brain& b_, zgame::Handle t, float* place_) {
  if (!t) return nullptr;
  brain_hooks().position(t, place_);
  return place_;
}

const float* wire_walk::target_facing(Brain& b_, zgame::Handle t, float* facing_) {
  if (!t) return nullptr;
  brain_hooks().forward(t, &facing_[0], &facing_[2]);
  return facing_;
}

bool wire_walk::target_live(Brain& b_, zgame::Handle t) { return t && brain_hooks().entity_active(t); }

bool wire_walk::must_leave(Brain& b_, zgame::Handle v) {
  Engage h;
  return must_leave_pm(h, pm_brain(b_).tick, v);
}

const InOutVia* wire_walk::at_via(Brain& b_, zgame::Handle v) {
  const InOutLast last = inout_last(v);
  return last.at_via ? last.via : nullptr;
}

void wire_walk::queue_via(Brain& b_, const InOutVia*) { pm_hook_missing("0x004ea0f0.via"); }

zgame::Handle wire_walk::next_along(Brain& b_, const float*) {
  pm_hook_missing("0x004d60d0");
  return 0;
}

void wire_walk::set_behaviour(Brain& b_, zgame::Handle b) { b_.current().set_behaviour(static_cast<Behaviour*>(pm_ptr(b))); }

void wire_walk::order_pop(Brain& b_) { b_.current().pop(); }

zgame::Handle wire_walk::contact_pick(Brain& b_, zgame::Handle v, int32_t kind) {
  const Contact* c = brain_hooks().pick(v, kind);
  return c ? brain_hooks().contact_entity(c) : 0;
}

const float* wire_walk::entity_place(Brain& b_, zgame::Handle t, float* place_) {
  brain_hooks().position(t, place_);
  return place_;
}

}  // namespace mech3::ai
