#pragma once
#include <cstdint>

// The mech object and the parts of it ported so far. Layout: docs/structs.md.
// Original file unknown: these functions sit between mech.cpp and mech_anim.cpp.
namespace mech3 {

using Sound = void*;  // a playing sound (zSound voice), 0 when none

struct Foot {
  bool in_water;       // +0x0ac
  float water_depth;   // +0x0dc  water surface above the foot
  int32_t soil;        // +0x0e8  docs/structs.md: 8 is lava
};

struct Leg {
  Foot* foot;          // +0x078
};

struct HeatSinks {     // embedded at mech +0x658
  int32_t count;       // +0x004  10 by default
  bool doubles;        // +0x008
  float rate() const;
};

enum : uint32_t {
  kOverride = 0x1000000,
  kPoweredDown = 0x2000000,
};

struct Vec3 {
  float x, y, z;
};

struct Mech {
  Vec3 vec_3c;               // +0x03c  zeroed on power-down (ZEntity3d)
  float height;              // +0x068  from the model's bounding box, 12 if none
  void* ammo;                // +0x0d4  weapons and ammo bins
  bool power_down_pending;   // +0x0dc
  bool heat_shutdown;        // +0x0dd
  bool powered_down;         // +0x0de
  void* controls;            // +0x37c
  Leg* legs[2];              // +0x38c, +0x390
  float heat;                // +0x554  0..2
  float heat_added;          // +0x558  this frame
  float heat_dissipation;    // +0x55c  this frame
  float masc_time;           // +0x574  non-zero while MASC is on
  int32_t anim_state;        // +0x578  1, 12, 13 let a power-down finish; 10 cancels it
  bool no_power_down;        // +0x584  likely airborne
  int32_t jump_jets_firing;  // +0x580
  uint8_t hud_saved;         // +0x587  the screen state kept while powered down
  Sound loops[3];            // +0x5b8  stopped on power-down
  Sound heat_beep;           // +0x5cc
  Sound heat_alarm;          // +0x5d0
  Sound shutdown_warning;    // +0x608
  Sound power_up_sound;      // +0x5d4
  Sound power_down_sound;    // +0x5d8
  Sound shutdown_voice;      // +0x60c
  bool heat_death_disabled;  // +0x630
  HeatSinks heat_sinks;      // +0x658
  void* owner;               // +0x684  pilot / HUD side
  uint32_t sound_flags;      // +0x688  bit 0: power sounds
  uint32_t heat_state;       // +0x69c  bits 0-2 beep, 3-5 shutdown, 24 override, 25 powered down
  float shutdown_time;       // +0x6a0
  float beep_time;           // +0x6a4
  float voice_time;          // +0x6a8

  bool is_powered_down() const;
  void power(int power_down);   // 0x0041cc30
  void finish_power_down();     // 0x0041cac0
  Foot* foot(int leg) const;
};

// Globals of other modules that the heat update reads.
struct World {
  Mech* player = nullptr;           // 0x005f4c8c +0x64
  bool full_heat_all_mechs = true;  // 0x005e1fd0
  bool no_heat_haze = false;        // 0x00608e4d
  float time = 0.0f;                // 0x0060991c  game time, seconds
  float dt = 0.0f;                  // 0x0072c52c
};
World& world();

// Calls into code that is not ported yet. Unset ones do nothing and return 0.
struct HeatCalls {
  bool (*heat_option)() = nullptr;                          // 0x0040cfc0
  int (*game_type)() = nullptr;                             // pilot 0x0060861c +0x6c, vtable +0x2c; 3 = multiplayer
  float (*throttle)(Mech&) = nullptr;                       // +0x37c GetThrottle (docs/controls.md)
  void* (*screen)(Mech&) = nullptr;                         // 0x00430aa0 on +0x684
  void (*heat_haze)(void* screen, float amount, int kind, float dt) = nullptr;  // 0x0043c1f0
  Sound (*play_sound)(Mech&, int id) = nullptr;             // 0x00432990 on +0x684
  void (*stop_sound)(Sound) = nullptr;                      // 0x0056e9f0
  void (*play_mech_sound)(int index) = nullptr;             // 0x0041e770
  void (*power)(Mech&, int on) = nullptr;                   // test hook; unset calls Mech::power
  void (*torso_hit)(Mech&, int explode) = nullptr;          // torso vtable +0x20 (explode, 1)
  bool (*ammo_explosion)(Mech&) = nullptr;                  // 0x004b0de0 on +0x0d4
  void (*hud_message)(float seconds, int message) = nullptr;  // 0x0040fe70
  void (*stop_controls)(Mech&, int arg) = nullptr;          // +0x37c vtable +0x4c (Stop)
  uint8_t (*screen_power_down)(void* screen) = nullptr;     // 0x0043bb20
  void (*screen_power_up)(void* screen, uint8_t saved) = nullptr;  // 0x0043bb50
  Sound (*play_sound_at)(Mech&, int id) = nullptr;          // 0x00432990 at the mech's position
};
HeatCalls& heat_calls();

void update_heat(Mech& m, float dt);  // 0x004192f0

}  // namespace mech3
