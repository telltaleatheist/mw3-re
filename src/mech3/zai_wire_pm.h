#ifndef MECH3_ZAI_WIRE_PM_H
#define MECH3_ZAI_WIRE_PM_H

// Pirate's Moon's AI hooks on the program: the zai_*_pm classes' calls out, answered through
// BrainHooks, which already holds the mission's vehicles. A call with no answer yet is logged once
// and counted. Nothing runs these without PM_AI=1 (docs/pm-tests.md, The switch's plan).
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "mech3/zai_attackers_pm.h"
#include "mech3/zai_brain.h"
#include "mech3/zai_brain_pm.h"
#include "mech3/zai_steer_pm.h"
#include "mech3/zai_target.h"
#include "mech3/zai_wire_orders_pm.h"

namespace mech3::ai {

class WalkPmHooks;
template <class Base> class WireWalkT;
using WireWalk = WireWalkT<WalkPmHooks>;

bool pm_ai_on();                                      // PM_AI=1, read once
void pm_hook_missing(const char* key);                // the log's line, the first time
const std::map<std::string, int>& pm_hooks_missing();  // what the first run reads
void pm_hooks_missing_clear();                        // not original: the tests start empty

// A pilot's PM state, beside its MW3 Brain, which stays as the mission built it. This record owns
// what PM's frame writes (`mode`, `wait_until`...); nothing copies them back into the Brain.
struct PmBrain {
  PmBrain();
  ~PmBrain();
  BrainTickPm tick;
  OrdersPm orders;
  SteerPm steer;
  AttackersPm attackers = AttackersPm::for_pilot(0.0f);  // the controls' +0x04
  std::unique_ptr<WireWalk> walk;  // its `fought` is state
  zgame::Handle vehicle = 0;       // from the first frame
  float home_kept[3] = {};         // FIX (bug): the attack's hold point, kept past its state
};
void pm_seed(BrainTickPm& t, const Brain& b);  // the params and the fields with an MW3 twin
PmBrain& pm_brain(Brain& b);                   // made and seeded at the first ask
WireWalk& pm_walk(Brain& b, BrainControls& c);  // the brain's one, made at the first ask
TargetsPm& pm_targets(zgame::Handle vehicle);  // by the vehicle: 0x004e6330's `this`
void pm_forget(Brain& b, zgame::Handle vehicle);

// Handles are 32 bits and the port's pointers are not: nets, states and contacts get an id here.
uint32_t pm_id(const void* p);                 // 0 for null
void* pm_ptr(uint32_t id);                     // null for 0 or an id no longer known
void pm_ids_forget(const void* p);
void pm_wire_clear();                          // the mission's end: records and ids

// SteerPmHooks over BrainHooks. A template because WalkPmHooks and JumpPmHooks derive from it.
template <class Base>
class WireSteer : public Base {
 public:
  void position(zgame::Handle e, float out[3]) override { brain_hooks().position(e, out); }
  void forward(zgame::Handle v, float* x, float* z) override { brain_hooks().forward(v, x, z); }
  bool halted(zgame::Handle v) override { return brain_hooks().airborne(v); }  // vt+0x150, PM's +0x5a8: MW3's vt+0x144
  // 0x004fa470 on the brain's orders: the order's net (vt+0x14) and state (vt+0x20) both idle. The state's slot
  // answers 1 on 49 of PM's state vtables, the attack's among them (0x004eb000); both are counted assumptions.
  bool order_steers(Brain* b) override {
    if (!b || b->orders.empty()) return missing<true>("assumed: the order's net idle (vt+0x14)");  // the exe's always has one
    return pm_orders_idle(*b);
  }
  const std::vector<const Contact*>& contacts(zgame::Handle v) override {
    seen_.clear();
    if (const std::vector<Contact*>* c = brain_hooks().contacts(v)) seen_.assign(c->begin(), c->end());
    return seen_;
  }
  zgame::Handle contact_entity(const Contact* c) override { return brain_hooks().contact_entity(c); }
  // vt+0x48: a mech answers itself (0x00402250 on 0x005dfaac); other classes are unread.
  bool targetable(zgame::Handle e) override { return brain_hooks().vehicle_mech(e) != 0; }
  float contact_dist2(const Contact* c) override { return c->dist2; }

 protected:
  template <bool V>
  static bool missing(const char* key) {
    pm_hook_missing(key);
    return V;
  }

 private:
  std::vector<const Contact*> seen_;  // the exe's list by reference; ours holds them unconst
};

using WiredSteerPm = WireSteer<SteerPmHooks>;

// PM's frame in place of Brain::frame under PM_AI=1 (0x004fab90 for 0x004e9710). Until PM's orders
// and states are wired they are MW3's, which keep `mode` and `busy` on the Brain, so those two
// cross at the frame's ends.
bool pm_frame(Brain& b, BrainControls& c, float dt);
// A hit on the pilot, for the list. The port's hit carries no damage yet, so it counts 0: `latest`
// is right, `hardest` isn't. The clock is the game's until [0x008c1b50] is mapped.
void pm_note_hit(Brain& b, zgame::Handle attacker, float damage = 0.0f);
// 0x004fbc20 and 0x004fbdd0 for the brain's on_hit and died under PM_AI=1.
void pm_on_hit(Brain& b, HitRecord& r, zgame::Handle owner, zgame::Handle attacker, float damage);
void pm_died(Brain& b);
zgame::Handle pm_hardest_attacker(Brain& b);  // 0x004b2890 on the controls' +0x04
zgame::Handle pm_latest_attacker(Brain& b);   // 0x004b2820

}  // namespace mech3::ai

#endif
