#pragma once
#include <cstdint>

#include "mech3/app_mech.h"

// The sensors' hooks (mech3/sensors, docs/ai.md section 15) wired to the
// program's objects: the mechs on zEntity's list (the port's) and
// targets.zrd's objects. Not original code; app_sensors.cpp.
namespace mech3 {

// The engine as the mech build has it, and Mech3Msg.dll's text (0x00576040).
void wire_sensors(const MechWiring* w, const char* (*message)(void* e, int32_t id));

// What a vehicle senses, for the program's tests: its contacts nearest first.
struct SensedContact {
  uint32_t seen = 0;       // the handle of what it saw
  float distance = 0;      // metres
  bool in_sight = false;
};
int32_t sensed_contacts(const void* mech, SensedContact* out, int32_t cap);
// The contact the mech has selected (its +0x6c), -1 none or not among its contacts.
int32_t selected_contact(const void* mech);

}  // namespace mech3
