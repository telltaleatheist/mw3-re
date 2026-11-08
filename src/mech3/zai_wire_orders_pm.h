#ifndef MECH3_ZAI_WIRE_ORDERS_PM_H
#define MECH3_ZAI_WIRE_ORDERS_PM_H

// PM's orders (zai_brain_pm: 0x004fb260, 0x004f9e70, 0x004ea220, 0x004ea5f0, 0x004ea740, 0x004ea7f0,
// 0x004fa470, 0x004fa6d0) on the brain's MW3 orders. The deque of MW3 `Order`s stays the storage, as the states
// keep their `Order` (docs/worklogs: the boundary with dev 2); PM's functions run on a mirror of it, and their
// hooks do the work on the brain.
#include "mech3/zai_brain.h"

namespace mech3::ai {

bool pm_orders_making();  // a state being made by PM's orders, not MW3's Brain::push (for the first runs' counts)
BehaviourState* pm_make_scan_state(Order& o, Behaviour& b);  // a scan or canleave step's, running 0x004d5a10
void pm_run_orders(Brain& b, float dt);                    // 0x004fb260
bool pm_may_fire(Brain& b);                                // 0x004fa6d0
bool pm_orders_idle(Brain& b);                             // 0x004fa470
void pm_give_order(Brain& b, AiNet* net, Behaviour* beh);  // 0x004ea740 (0x004fa150)
void pm_push_behaviour(Brain& b, Behaviour* beh);          // 0x004ea7f0 (0x004fa170)
void pm_pop_order(Brain& b);                               // 0x004ea220

}  // namespace mech3::ai

#endif
