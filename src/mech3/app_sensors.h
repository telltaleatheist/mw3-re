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

// The HUD's contacts and their objects of any class (entity_classes.h): a
// contact's vehicle (0x004e6fe0), entity (+8) and size (0x004e7070); an
// object's team (vt+0x28 +0xc0), blip (vt+0x3c), hidden (vt+0xa8), mech
// (vt+0x50), name (vt+0xa4), place (0x005a85e0), centre (vt+0xbc) and its
// centroid node's place (+0x14).
void* hud_contact_vehicle(const void* contact);
void* hud_contact_entity(const void* contact);
float hud_contact_size(const void* contact);
uint32_t hud_object_team(const void* o);
bool hud_object_classed(const void* o);  // an entity class the sensors know (its vt+0x28 +0xc0 a team)
void* hud_object_blip(const void* o, int32_t kind);
bool hud_object_hidden(const void* o);
void* hud_object_mech(const void* o);
const char* hud_object_name(const void* o);
void hud_object_place(const void* o, float out[3]);
void hud_object_centre(const void* o, float out[3]);
void hud_object_centroid(const void* o, float out[3]);
void hud_object_aim_place(const void* o, float out[3]);  // 0x005a8630 (the lead point's)
void hud_object_velocity(const void* o, float out[3]);   // vt+0xc
const float* weapon_lock_point(const void* o);           // 0x005a8630, a live pointer
const float* weapon_lock_velocity(void* o);              // vt+0xc, a pointer
void* hud_target_marker(int32_t kind);                   // [0x0060b4f0 + 4 kind]
void hud_object_view_tint(const void* o, bool on);      // vt+0x68: TARGET's view tint, by class

}  // namespace mech3
