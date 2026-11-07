#include "mech3/zai_wire_walk_pm.h"

#include "mech3/zai_brain_pm.h"
#include "mech3/zainet_inout.h"

namespace mech3::ai {

namespace {
struct Engage : EngagePmHooks {
  float now() override { return brain_hooks().now(); }
  void position(zgame::Handle v, float o[3]) override { brain_hooks().position(v, o); }
  double health(zgame::Handle v) override { return brain_hooks().health(v); }
};
}  // namespace

zgame::Handle WireWalk::target_entity(zgame::Handle v) {
  const AiTarget* t = brain_hooks().targetable(v);
  return t && t->target ? brain_hooks().contact_entity(t->target) : 0;
}

bool WireWalk::has_target_contact(zgame::Handle v) {
  const AiTarget* t = brain_hooks().targetable(v);
  return t && t->target;
}

bool WireWalk::pull(zgame::Handle v, const float from[3], const float to[3], float out[3]) {
  Pilot p;
  brain_hooks().pilot_of(v, &p);
  return inout_pull(v, from, to, out, p.jump_jets != 0);
}

float WireWalk::pilot_x544(zgame::Handle) {
  pm_hook_missing("mech+0x544");
  return 0.0f;
}

double WireWalk::stopping_at(zgame::Handle, float) {
  pm_hook_missing("vehicle.vt+0x134");
  return 0.0;
}

zgame::Handle WireWalk::order_target() {
  pm_hook_missing("order+0x0c.target");
  return 0;
}

const float* WireWalk::target_place(zgame::Handle) {
  pm_hook_missing("target.vt+0x04");
  return nullptr;
}

bool WireWalk::target_live(zgame::Handle) {
  pm_hook_missing("target+0x1c+0x08");
  return false;
}

bool WireWalk::must_leave(zgame::Handle v) {
  Engage h;
  return must_leave_pm(h, pm_brain(b_).tick, v);
}

const InOutVia* WireWalk::at_via(zgame::Handle v) {
  const InOutLast last = inout_last(v);
  return last.at_via ? last.via : nullptr;
}

void WireWalk::queue_via(const InOutVia*) { pm_hook_missing("0x004ea0f0.via"); }

zgame::Handle WireWalk::next_along(const float*) {
  pm_hook_missing("0x004d60d0");
  return 0;
}

void WireWalk::set_behaviour(zgame::Handle) { pm_hook_missing("0x004f9f00"); }

void WireWalk::order_pop() { pm_hook_missing("order.vt+0x0c"); }

zgame::Handle WireWalk::contact_pick(zgame::Handle v, int32_t kind) {
  const Contact* c = brain_hooks().pick(v, kind);
  return c ? brain_hooks().contact_entity(c) : 0;
}

const float* WireWalk::entity_place(zgame::Handle t) {
  brain_hooks().position(t, place_);
  return place_;
}

}  // namespace mech3::ai
