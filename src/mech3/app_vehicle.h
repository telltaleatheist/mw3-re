#pragma once
#include <cstdint>
#include <vector>

#include "mech3/app_mech.h"

// The vehicles in the program (app_vehicle.cpp; docs/vehicles.md, In the
// program). Pointers stay void* here: mech.h's Handle is not app_mission's.
namespace mech3 {

void wire_vehicles(const MechWiring* w);  // from wire_mech_build; null: the defaults again
bool is_program_vehicle(const void* p);   // made by the mission start, still alive
void* program_vehicle_create(void* ctl, const char* type, const char* name);  // 0x004ac320
void* program_vehicle_controls(void* v);  // +0xe4
void* program_vehicle_node(void* v);      // +0x04
void program_vehicle_place(void* v, const float pos[3], float yaw);          // vt+0xdc
void program_vehicle_setup(void* v, void* def, const char* name, int32_t kind);  // vt+0x158
void program_vehicle_motion(void* v, void* mode, const char* name, const char* type);  // 0x0049ec90 -> 0x0049ef40
void program_vehicle_points(void* v);     // 0x0049f660
void program_vehicle_touch(void* v);      // 0x005833b0(v, node, 1, 0x004af550)
void program_vehicle_refresh(void* v);    // 0x005a85c0
// The team masks at +0xc0 / +0xc4 / +0xc8 (set_team_masks' writes and reads).
bool program_vehicle_mask(void* v, uint32_t off, uint32_t* get, const uint32_t* set);
void program_vehicles_forget();           // the mission's end
std::vector<void*> program_vehicles();    // this mission's, as made
// What the headless tests check of one.
struct VehicleSummary {
  const char* name = nullptr;  // its node's
  bool in_world = false, ai_controls = false, owned = false, active = false;
  float health = 0, health_max = 0;
  int32_t mode = -1, supports = 0, points = 0;
  uint32_t team = 0;
  float pos[3] = {};
};
bool describe_vehicle(const void* v, VehicleSummary* out);

}  // namespace mech3
