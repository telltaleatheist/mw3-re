#include "mech3/zai_wire_nets_pm.h"

#include <string>

#include "mech3/zai_brain.h"
#include "mech3/zai_brain_pm.h"
#include "mech3/zai_target.h"
#include "mech3/zai_wire_pm.h"

namespace mech3::ai {

// Pirate's Moon 1.0 @ 0x004c6f90's table, on MW3's net: its seven become ten.
PmVehicleNet::PmVehicleNet(zgame::Handle vehicle) : VehicleNet(vehicle) {
  for (znet::Element*& e : elems) {
    brain_hooks().on_delete(e);
    delete e;
  }
  elems.assign(10, nullptr);
  for (size_t i = 0; i < 10; ++i) {
    const StepKind kind = i == 0 ? StepKind::VehFollow : i == 1 ? StepKind::VehAttack : i == 5 ? StepKind::VehOwn
                                                                                               : static_cast<StepKind>(kVehicleSlotsPm[i]);
    elems[i] = new Behaviour(kind);
    brain_hooks().on_new(0xc, elems[i]);
  }
}

NetPart* PmVehicleNet::make_part(Order& o) { return new PmVehiclePart(&o, this); }
AiNet* PmVehiclePart::net() const { return target; }

namespace {
struct Slots : NetSlotsPmHooks {
  explicit Slots(const VehicleNet& n) : n(n) {}
  zgame::Handle slot(int32_t i) override {
    return size_t(i) < n.elems.size() ? pm_id(n.elems[size_t(i)]) : 0;
  }
  // vt+0x60, vt+0x74, then +0x90/+0x94/+0x98: the slot's behaviour itself on the port's nets.
  zgame::Handle step_of(zgame::Handle s, int32_t) override {
    auto* e = static_cast<znet::Element*>(pm_ptr(s));
    Behaviour* b = e ? static_cast<AiElem*>(e)->as_behaviour() : nullptr;
    return pm_id(b);
  }
  const VehicleNet& n;
};
}  // namespace

// 0x004d70f0's place: the vehicle's +0xd0, given to znet as MW3's is. A net MW3's code made first stays.
VehicleNet* pm_vehicle_net(zgame::Handle vehicle) {
  AiTarget* t = brain_hooks().targetable(vehicle);
  if (!t) return nullptr;
  if (!t->net) {
    t->net = new PmVehicleNet(vehicle);
    brain_hooks().on_new(0x20, t->net);
  }
  return t->net;
}

Behaviour* pm_kind_behaviour(Brain& b, int32_t kind) {
  const auto* n = b.orders.empty() || !b.current().net ? nullptr : dynamic_cast<VehicleNet*>(b.current().net->net());
  if (!n) return nullptr;
  if (n->elems.size() < 10) pm_hook_missing("0x004c7800 on MW3's seven-slot net");
  Slots h(*n);
  auto* r = static_cast<Behaviour*>(pm_ptr(kind_behaviour_pm(h, kind)));
  if (r && r->kind == StepKind::VehFollow)  // for the first runs: who asks for the follow
    pm_hook_missing(("kind lookup: follow for kind " + std::to_string(kind)).c_str());
  return r;
}

}  // namespace mech3::ai
