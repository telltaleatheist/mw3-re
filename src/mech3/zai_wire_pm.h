#ifndef MECH3_ZAI_WIRE_PM_H
#define MECH3_ZAI_WIRE_PM_H

// Pirate's Moon's AI hooks on the program: the zai_*_pm classes' calls out, answered through
// BrainHooks, which already holds the mission's vehicles. A call with no answer yet is logged once
// and counted. Nothing runs these without PM_AI=1 (docs/pm-tests.md, The switch's plan).
#include <map>
#include <string>
#include <vector>

#include "mech3/zai_brain.h"
#include "mech3/zai_brain_pm.h"
#include "mech3/zai_steer_pm.h"
#include "mech3/zai_target.h"

namespace mech3::ai {

bool pm_ai_on();                                      // PM_AI=1, read once
void pm_hook_missing(const char* key);                // the log's line, the first time
const std::map<std::string, int>& pm_hooks_missing();  // what the first run reads
void pm_hooks_missing_clear();                        // not original: the tests start empty

// A pilot's PM state, beside its MW3 Brain, which stays as the mission built it. This record owns
// what PM's frame writes (`mode`, `wait_until`...); nothing copies them back into the Brain.
struct PmBrain {
  BrainTickPm tick;
  OrdersPm orders;
};
void pm_seed(BrainTickPm& t, const Brain& b);  // the params and the fields with an MW3 twin
PmBrain& pm_brain(Brain& b);                   // made and seeded at the first ask
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
  bool order_steers(Brain*) override { return missing<false>("0x004fa470"); }
  const std::vector<const Contact*>& contacts(zgame::Handle v) override {
    seen_.clear();
    if (const std::vector<Contact*>* c = brain_hooks().contacts(v)) seen_.assign(c->begin(), c->end());
    return seen_;
  }
  zgame::Handle contact_entity(const Contact* c) override { return brain_hooks().contact_entity(c); }
  bool targetable(zgame::Handle) override { return missing<false>("entity.vt+0x48"); }
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

}  // namespace mech3::ai

#endif
