#ifndef MECH3_ZAI_WIRE_NETS_PM_H
#define MECH3_ZAI_WIRE_NETS_PM_H

// PM's vehicle net (0x004c6f90): ten slots where MW3's has seven. Slots 0, 1 and 5 keep MW3's kinds (follow,
// attack, the station), which MW3's code asks by; the others carry PM's slot vtables as their kind
// (kVehicleSlotsPm), for pm_make_state to answer. Made under PM_AI=1 in place of MW3's net for the tick's attack.
#include "mech3/zai_steps.h"

namespace mech3::ai {

class PmVehicleNet : public VehicleNet {
 public:
  explicit PmVehicleNet(zgame::Handle vehicle);
};

PmVehicleNet* pm_vehicle_net(zgame::Handle vehicle);  // one a vehicle, made at the first ask
void pm_vehicle_nets_clear();                          // the mission's end
// 0x004c7800 on the net of the brain's last order: the behaviour of an order kind (1..7), slot 0's for any other.
Behaviour* pm_kind_behaviour(Brain& b, int32_t kind);

}  // namespace mech3::ai

#endif
