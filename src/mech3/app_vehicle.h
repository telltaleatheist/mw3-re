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
void* program_vehicle_centroid(void* v);  // +0x14
void program_vehicle_place(void* v, const float pos[3], float yaw);          // vt+0xdc
void program_vehicle_setup(void* v, void* def, const char* name, int32_t kind);  // vt+0x158
void program_vehicle_motion(void* v, void* mode, const char* name, const char* type);  // 0x0049ec90 -> 0x0049ef40
void program_vehicle_points(void* v);     // 0x0049f660
void program_vehicle_touch(void* v);      // 0x005833b0(v, node, 1, 0x004af550)
void program_vehicle_refresh(void* v);    // 0x005a85c0
// The team masks at +0xc0 / +0xc4 / +0xc8 (set_team_masks' writes and reads).
bool program_vehicle_mask(void* v, uint32_t off, uint32_t* get, const uint32_t* set);
void program_vehicles_forget();           // the mission's end
// Tests: a weapons.zrd shot of `weapon` from `killer` (an entity; its node
// zWeapon's attacker) for more than its health, through the damage 0x004ad3f0.
bool program_vehicle_kill(void* v, void* killer, const char* weapon);
bool program_vehicle_free(void* v);       // vt+0x00 0x004ac630(1); false when not a program vehicle
// What the AI's brain asks of a vehicle (docs/vehicles.md, In the program),
// by the basic vehicle's slots; the rest (position, forward, matrix, weapons,
// active, velocity) is zEntity's, the mech helpers' (app_mech.h).
float program_vehicle_health(void* v);                      // vt+0x98 0x004ad820
float program_vehicle_speed_at(void* v, float share);       // vt+0x118
float program_vehicle_stopping_distance(void* v, float s);  // vt+0x120
float program_vehicle_stop_share(void* v, float d);         // vt+0x12c
float program_vehicle_move_speed(void* v);                  // vt+0x114
bool program_vehicle_can_face(void* v, const float d[3]);   // vt+0x8c
bool program_vehicle_in_reach(void* v, float cosine);       // vt+0x150
bool program_vehicle_aim_clamp(void* v, const float d[3], float out[3]);  // vt+0x90
void program_vehicle_set_aim(void* v, const float d[3]);    // vt+0x154
void program_vehicle_velocity(void* v, float out[3]);       // vt+0x0c 0x005a82d0: +0x30
const char* program_vehicle_name(void* v);                  // its node's
void* program_vehicle_target(void* v);                      // the AiTarget base (ai::AiTarget*)
void program_vehicle_hit_an_ally(void* v, void* victim);    // vt+0x140's controls, vt+0x98
void program_vehicle_frame(void* v, float dt);              // vt+0x34 0x004a1040, when +0x09 is set
// The sensors' (app_sensors.cpp): vt+0xcc 0x004aee00 (active, and the
// brain's vt+0x0c), vt+0xd0 0x004aee40 (the brain's vt+0x10, true without
// one), vt+0xc8 0x004add90 (the brain's empty vt+0x08, the engine sound
// released) and the ping's 0x004adce0 (its engine sound at `pos`).
bool program_vehicle_awake(void* v);
bool program_vehicle_sees(void* v, uint32_t self);
void program_vehicle_sense_refresh(void* v);
void program_vehicle_engine_play(void* v, const float pos[3]);
void program_vehicle_set_sensed(void* v, bool on);  // ZEntity's +0x09: its frame runs
// valves.zrd's (app_mission.cpp's WiredValveHooks).
void* program_vehicle_named(const char* name);  // zEntity's by name (0x005a8210): its node's
void program_vehicle_destroy(void* v);          // vt+0x70 0x004ad2d0
void* program_vehicle_brain(void* controls);    // a program vehicle's AI controls' brain (ai::Brain*), else null
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
