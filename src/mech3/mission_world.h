#pragma once
#include <cstdint>

#include "gamez/zclass/cls_data.h"

// mech3\mission.cpp: the mission's world parameters, the `world_params`
// block of the mission's world.zrd (mpinfo.zrd for multiplayer maps 3 and
// up): fog, sky, sun, wind, weather, temperature, jump-jet ceiling and the
// cloud pattern (docs/drafts/world-params.md).
namespace mech3 {

using zcls::Node;

// The small node handle the mission keeps for the sun (new(8), vtable 0x005c6108).
struct NodeHandle {
  Node* node = nullptr;            // +0x04
};

// The rain or snow particle system (0x98 bytes, built by 0x005a60a0 / 0x005a5ad0).
// Only the fields the mission writes are named.
struct Weather : zcls::Raw<0x98> {
  // +0x44 u16 packed colour, +0x48 / +0x4c alpha gradient, +0x68 wind dir,
  // +0x6c wind vel, +0x70 gravity
  Node* camera = nullptr;          // +0x50  the player view's camera node
};

// The first 0x4c bytes of the mission object (its ctor 0x0048f290 starts with
// 0x00491d70). Only +0x00..+0x09 are set by that ctor; the rest is whatever
// the mission's memory held until a read sets it (here zero).
struct MissionWorld {
  Weather* weather = nullptr;      // +0x00  owned by the particle manager once added
  NodeHandle* sun = nullptr;       // +0x04  the "sunlight" node, if the world has one
  uint8_t fluctuate = 0;           // +0x08  the cloud pattern was read
  uint8_t fog = 0;                 // +0x09  a fog block was read
  float fog_range[2] = {};         // +0x0c  near, far
  float fog_altitude[2] = {};      // +0x14
  float fog_color[3] = {};         // +0x1c
  Node* sky = nullptr;             // +0x28  the horizon child named by fog "node"
  int32_t fog_state = 0;           // +0x2c  fog "state"; no shipped file has it, nothing read here uses it
  float water_range[2] = {};       // +0x30  underwater fog
  float water_altitude[2] = {};    // +0x38
  float water_color[3] = {};       // +0x40
};

// Globals the reader sets (heat.md, interp.md `ceiling`).
struct Environment {
  float gravity = 30.0f;              // 0x005e5d34  WorldSetGravity, the console's "gravity"
  float ambient_temperature = 20.0f;  // 0x005e5d3c
  float inv_temperature = 0.05f;      // 0x005e5d40  the heat code's dissipation factor
  float jet_ceiling = 150.0f;         // 0x005e5d44
};
Environment& environment();

// The calls into engine parts not ported yet; the front end or a test sets them.
struct WorldHooks {
  int32_t (*game_type)();                                  // [0x0060861c]+0x6c vtable +0x2c (3 multiplayer)
  int32_t (*mission_number)();                             // [0x0060861c]+0x6c vtable +0x4c
  int32_t (*hw_card_flag)();                               // 0x0040d4d0, the HWCardFlag option
  Node* world;                                             // [0x00608c84]+4, null when there is no world object
  Node* camera;                                            // [0x005f4c8c]+4, the player view's camera
  void (*puffer_tick_reset)();                             // 0x00549410, gmod_puff
  void (*wind_velocity)(const float v[3]);                 // 0x0054b630
  void (*wind_max_speed)(float v);                         // 0x0054b650
  void (*wind_random_accel)(float v);                      // 0x0054b660
  void (*wind_random_angvel)(float radians);               // 0x0054b670
  void (*light_get_orientation)(Node* light, float* x, float* y, float* z);  // 0x004fb060
  void (*light_set_orientation)(Node* light, float x, float y, float z);     // 0x004faff0
  void (*light_set_ambient)(Node* light, float v);         // 0x004fa940
  void (*light_set_diffuse)(Node* light, float v);         // 0x004fa9b0
  Weather* (*create_snow)(int32_t particles);              // new(0x98) + 0x005a5ad0, null if out of memory
  Weather* (*create_rain)(int32_t particles);              // new(0x98) + 0x005a60a0
  void (*weather_add)(Weather* w);                         // 0x0059fbe0 on the manager 0x00736328
  void (*weather_remove)(Weather* w);                      // 0x0059fc80 on the manager
  void (*weather_destroy)(Weather* w);                     // vtable +0x00 (1)
  void (*weather_start)(Weather* w, float v);              // vtable +0x60
  void (*weather_stop)(Weather* w);                        // vtable +0x64
  void (*fluctuate_on)(int32_t on);                        // 0x00543910, gmod_light
  void (*fluctuate_period)(float v);                       // 0x00543920
  void (*fluctuate_wavelength)(float v);                   // 0x00543940
  void (*fluctuate_base_amp)(float base, float amplitude); // 0x00543960
  void (*fluctuate_heading)(float radians);                // 0x005439f0
  void (*software_color)(const float rgb[3]);              // 0x00566250, software renderer
  // The frame's (0x004926d0).
  const float* (*camera_angles)();                         // [[0x005f4c8c]+4] data +0x20, the camera's rotation
  float (*frame_dt)();                                     // [0x0072c52c]
  void (*clouds_advance)(const float dir[2], float dt);    // 0x00543a50, gmod_light
};
WorldHooks& world_hooks();

void world_init(MissionWorld& m);                          // 0x00491d70
bool world_clear(MissionWorld& m);                         // 0x00491d80
// 0x004926d0, from the missions' frame: the cloud pattern drifts the way the
// camera looks (its dt argument is not used: the frame's [0x0072c52c] is).
void world_update(const MissionWorld& m);
void world_set_enabled(MissionWorld& m, bool on);          // 0x00492660
// Reads `key` of `file` (world.zrd and the mission's name); multiplayer maps
// past 2 read mpinfo.zrd's world_params instead. Always true.
bool world_read_params(MissionWorld& m, const char* file, const char* key);  // 0x00491de0

}  // namespace mech3
