// PM's target setter (0x004e6330, on every AiTarget vtable in PM's place of MW3's 0x004d5e40) on the port's
// AiTarget. Its +0x78 is the saved target PM's save_target (0x004e7680) keeps: a lancemate the player
// commands, picked while none is held, is saved as it is picked.
#include "mech3/zai_brain_pm.h"
#include "mech3/zai_target.h"
#include "mech3/zai_wire_pm.h"

namespace mech3::ai {

namespace {
// 0x004f8d70 then vt+0x28's +0xc0 / +0xbc: the contact's vehicle's AiTarget, found as MW3's set_target does.
struct OnTarget : TargetsPmHooks {
  OnTarget(uint32_t (*team)(const AiTarget&, const Contact*), const AiTarget& t) : team_fn(team), t(t) {}
  uint32_t team_of(zgame::Handle c) override { return team_fn(t, contact(c)); }
  uint32_t flags_of(zgame::Handle c) override {
    const AiTarget* a = target_of(c);
    return a ? a->xbc : 0;
  }
  static const Contact* contact(zgame::Handle c) { return static_cast<const Contact*>(pm_ptr(c)); }
  static const AiTarget* target_of(zgame::Handle c) {
    const Contact* k = contact(c);
    const zgame::Handle v = k && k->seen ? target_hooks().seen_vehicle(k->seen) : 0;
    return v ? brain_hooks().targetable(v) : nullptr;
  }
  uint32_t (*team_fn)(const AiTarget&, const Contact*);
  const AiTarget& t;
};
}  // namespace

bool pm_set_target(AiTarget& t, Contact* c, bool keep_sides) {
  TargetsPm s;
  s.target = pm_id(t.target), s.ally = pm_id(t.ally_target), s.enemy = pm_id(t.enemy_target);
  s.marked = pm_id(t.saved_target);
  s.flags = t.xbc, s.allies = t.allies, s.enemies = t.enemies;
  OnTarget h([](const AiTarget& a, const Contact* k) { return k ? a.team_of(a.vehicle_of(k)) : 0u; }, t);
  set_target_pm(h, s, pm_id(c), keep_sides);
  t.target = static_cast<Contact*>(pm_ptr(s.target));
  t.ally_target = static_cast<Contact*>(pm_ptr(s.ally));
  t.enemy_target = static_cast<Contact*>(pm_ptr(s.enemy));
  t.saved_target = static_cast<Contact*>(pm_ptr(s.marked));
  return true;
}

}  // namespace mech3::ai
