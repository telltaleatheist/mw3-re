#ifndef MECH3_ZAI_WIRE_NETS_PM_H
#define MECH3_ZAI_WIRE_NETS_PM_H

// PM's vehicle net (0x004c6f90): ten slots where MW3's has seven. Slots 0, 1 and 5 keep MW3's kinds (follow,
// attack, the station), which MW3's code asks by; the others carry PM's slot vtables as their kind
// (kVehicleSlotsPm), for pm_make_state to answer. Made under PM_AI=1 in place of MW3's net for the tick's attack.
#include "mech3/zai_brain.h"
#include "mech3/zai_steps.h"

namespace mech3::ai {

class PmVehicleNet : public VehicleNet {
 public:
  explicit PmVehicleNet(zgame::Handle vehicle);
  NetPart* make_part(Order& o) override;  // Pirate's Moon 0x004c72a0: a bare part, PmVehiclePart
};

// Pirate's Moon 1.0 @ 0x004c78c0 (vtable 0x005ed538; deleting 0x004c78f0, destructor 0x004c7910): the order, the net
// at +0x18, a byte at +0x1c cleared, an empty run (0x005c7a30). MW3's VehiclePart (0x004c0ba0) stacked a home
// point on the brain for the leash and picked the mode's behaviour; PM's leash goes from the attack state's own
// home (0x004dedf0, the brain's +0x104), none outside it.
class PmVehiclePart : public NetPart {
 public:
  PmVehiclePart(Order* o, VehicleNet* n) : NetPart(o), target(n) {}
  AiNet* net() const override;  // as MW3's 0x0040a890: +0x18
  VehicleNet* target = nullptr;  // +0x18
  bool x1c = false;              // +0x1c  (its reader not found)
};

// The vehicle's net (AiTarget +0xd0), a PmVehicleNet made at the first ask; null for a vehicle with no AiTarget.
VehicleNet* pm_vehicle_net(zgame::Handle vehicle);
// 0x004c7800 on the net of the brain's last order: the behaviour of an order kind (1..7), slot 0's for any other.
Behaviour* pm_kind_behaviour(Brain& b, int32_t kind);

}  // namespace mech3::ai

#endif
