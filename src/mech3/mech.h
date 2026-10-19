#pragma once
#include <cstdint>
#include <list>

#include "mech3/mech_parts.h"

// The mech object and the parts of it ported so far. Layout: docs/structs.md.
// Original file: mech3\mech.cpp (these functions sit between it and
// mech_anim.cpp); heat in mech.cpp, damage in mech_damage.cpp.
namespace mech3 {

struct Config;
using Sound = void*;  // a playing sound (zSound voice), 0 when none

struct HeatSinks {     // embedded at mech +0x658
  int32_t count;       // +0x004  10 by default
  bool doubles;        // +0x008
  float rate() const;
  bool lose(int32_t n);  // 0x0045fbc0
};

enum : uint32_t {
  kOverride = 0x1000000,
  kPoweredDown = 0x2000000,
};

// The pilot side of a mech (+0x684): its roster entry and the effects its
// parts and death use (docs/damage.md).
struct Pilot {
  int32_t roster = 0;            // +0x00c  index in the mech roster; 7 is the Elemental
  Handle part_effects[6][3] = {};  // +0x1a4 + 0xc x id: two destruction effects, a trail
  Handle death_effects[4] = {};  // +0x1ec
  int32_t x200 = 0;              // +0x200
  Handle water_death = nullptr;  // +0x20c
  int32_t* x228 = nullptr;       // +0x228  a block whose [1] is read
};

struct Mech {
  Handle node = nullptr;     // +0x004  the world node
  bool active = true;        // +0x008
  Vec3 velocity{};           // +0x030
  Vec3 vec_3c{};             // +0x03c  zeroed on power-down (ZEntity3d); z is the forward speed
  Vec3 vec_58{};             // +0x058
  float height;              // +0x068  from the model's bounding box, 12 if none
  uint32_t team = 0;         // +0x0c0  AiTarget masks
  uint32_t allies = 0;       // +0x0c4
  void* ammo;                // +0x0d4  weapons and ammo bins
  bool power_down_pending;   // +0x0dc
  bool heat_shutdown;        // +0x0dd
  bool powered_down;         // +0x0de
  float last_hit_time = 0;   // +0x0e0
  MechPart* last_hit_part = nullptr;  // +0x0e4
  void* controls;            // +0x37c
  std::list<MechPart*> parts;  // +0x384, count +0x388
  Leg* legs[2];              // +0x38c, +0x390
  Arm* arms[2] = {};         // +0x394, +0x398
  Torso* torso = nullptr;    // +0x39c
  Hip* hip = nullptr;        // +0x3a0
  Head* head = nullptr;      // +0x3a4
  int32_t equipment[15] = {};  // +0x3ac  installed count per equipment kind (docs/weapons.md)
  Handle decals = nullptr;   // +0x3e8
  Config* config = nullptr;  // +0x3ec
  bool damaged = false;      // +0x410
  bool killed_by_nuke = false;  // +0x411
  Handle anims[13] = {};     // +0x424  [0], [1] limping legs, [4] the base, [11] the blend
  Handle action_objects[10] = {};  // +0x458
  std::list<int32_t> actions;  // +0x480, count +0x484
  float walk_speed = 0;      // +0x510
  float max_speed = 0;       // +0x518
  float knock = 0;           // +0x53c  length of knock_vec
  Vec3 knock_vec{};          // +0x540
  float heat;                // +0x554  0..2
  float heat_added;          // +0x558  this frame
  float heat_dissipation;    // +0x55c  this frame
  float narc_time = 0;       // +0x570
  float masc_time;           // +0x574  non-zero while MASC is on
  int32_t anim_state;        // +0x578  1, 12, 13 let a power-down finish; 10 dead
  int32_t prev_state = 0;    // +0x57c
  bool no_power_down;        // +0x584  airborne (vtable +0x144, 0x41ec70)
  int32_t jump_jets_firing;  // +0x580
  uint8_t hud_saved;         // +0x587  the screen state kept while powered down
  int32_t limp = -1;         // +0x5ac  the limping leg, -1 none
  Sound sounds[25] = {};     // +0x5b8  all stopped on death
  bool invincible;           // +0x630  the Invincible cheat (docs/damage.md)
  HeatSinks heat_sinks;      // +0x658
  Handle attacker = nullptr;  // +0x664  the last to hit it
  Pilot* pilot = nullptr;    // +0x684
  uint32_t sound_flags;      // +0x688  bit 0: power sounds
  uint32_t heat_state;       // +0x69c  bits 0-2 beep, 3-5 shutdown, 24 override, 25 powered down
  float shutdown_time;       // +0x6a0
  float beep_time;           // +0x6a4
  float voice_time;          // +0x6a8
  float knockdown[5] = {};   // +0x6b8  base, x 0.8, x 80, threshold, slope (0x414c71, 0x417a10)

  Sound* loops() { return sounds; }                 // +0x5b8, three: stopped on power-down
  Sound& heat_beep() { return sounds[5]; }          // +0x5cc
  Sound& heat_alarm() { return sounds[6]; }         // +0x5d0
  Sound& power_up_sound() { return sounds[7]; }     // +0x5d4
  Sound& power_down_sound() { return sounds[8]; }   // +0x5d8
  Sound& shutdown_warning() { return sounds[20]; }  // +0x608
  Sound& shutdown_voice() { return sounds[21]; }    // +0x60c

  bool is_powered_down() const;
  bool airborne() const { return no_power_down; }   // vtable +0x144, 0x0041ec70
  void add_heat(float heat) { heat_added += heat; }  // 0x00419e40
  void power(int power_down);   // 0x0041cc30
  void finish_power_down();     // 0x0041cac0
  Foot* foot(int leg) const;    // 0x0041d4f0
  Calf* calf(int leg) const;    // 0x0041d4d0

  // Damage (mech_damage.cpp).
  int take_damage(WeaponDef* def, Collision* coll, float damage);  // vtable +0x164, 0x0041ac60
  void die();                         // vtable +0x70, 0x0041b0d0
  void on_part_destroyed(MechPart* part);  // 0x0041b5f0
  void destroy_part(int id);          // 0x0041cfa0
  bool start_limp(int leg, bool fall);  // 0x0041d5f0
  void fall();                        // 0x0041b640
  bool knocked_down(int kind);        // 0x0041b660
  void set_max_speed(float v);        // 0x004143c0
  void scale_knockdown(float f);      // 0x00417a10
  float tonnage() const;              // 0x00416770
  void mark_narc(bool on);            // 0x0041ac10
  void note_hit();                    // 0x0041d170
};

// Globals of other modules that the heat and damage code reads.
struct World {
  Mech* player = nullptr;           // 0x005f4c8c +0x64
  Mech* mission_player = nullptr;   // 0x0060861c +0x14  (the same mech in play)
  bool stats = false;               // 0x00608620  the statistics object exists
  float difficulty = 1.0f;          // 0x00608e5c  damage taken by your side: 0.7, 1.0, 1.3
  Vec3 shot[2] = {};                // 0x009a0ec4  the hitting shot: from, to
  bool death_anim_toggle = false;   // 0x00605cc0
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
  void (*torso_hit)(Mech&, int explode) = nullptr;          // test hook; unset: torso destroy (explode, 1)
  bool (*ammo_explosion)(Mech&) = nullptr;                  // 0x004b0de0 on +0x0d4
  void (*hud_message)(float seconds, int message) = nullptr;  // 0x0040fe70
  void (*stop_controls)(Mech&, int arg) = nullptr;          // +0x37c vtable +0x4c (Stop)
  uint8_t (*screen_power_down)(void* screen) = nullptr;     // 0x0043bb20
  void (*screen_power_up)(void* screen, uint8_t saved) = nullptr;  // 0x0043bb50
  Sound (*play_sound_at)(Mech&, int id) = nullptr;          // 0x00432990 at the mech's position
};
HeatCalls& heat_calls();

// The calls damage makes into code that is not ported yet (scene nodes,
// effects, weapons, HUD, sound, network, statistics). Unset ones do nothing
// and return 0; the screen, throttle, sounds and game type come from HeatCalls.
struct DamageCalls {
  Handle (*attacker)() = nullptr;                    // 0x0040eee0 on the shot's owner [0x009a0edc]
  MechPart* (*node_part)(Handle node) = nullptr;     // 0x0040ef30: the part a node is, if any
  Handle (*node_owner)(Handle node) = nullptr;       // 0x0040ef10: the mech a node belongs to
  const char* (*message)(int id) = nullptr;          // 0x00576040, Mech3Msg.dll
  void (*controls_hit)(Mech&, WeaponDef*, Collision*, float damage, Handle attacker) = nullptr;  // +0x37c vtable +0x94
  void (*controls_stop)(Mech&) = nullptr;            // +0x37c vtable +0xa8
  void (*net_event)(Mech&, int kind, intptr_t arg) = nullptr;  // vtable +0x168, 0x0046f160
  void (*killer_notify)(Handle killer, Mech& victim) = nullptr;  // the killer's vtable +0x80
  void (*stats)(uint32_t fn, int arg) = nullptr;     // [0x00608620] statistics, method fn
  void (*stats_hit)(int weapon, const Vec3* impulse, float force) = nullptr;  // 0x00433720
  bool (*networked)() = nullptr;                     // 0x00479480 on [0x0060861c]
  int (*view_mode)() = nullptr;                      // [0x005f4c8c] +0x58 +4, -1 without a view
  void (*mission_failed)(int why) = nullptr;         // 0x0040ae70
  void (*mission_player_died)() = nullptr;           // 0x0048f9e0 on 0x006098d0
  void (*player_died_hud)(void* screen) = nullptr;   // 0x0041b263..0x0041b30c: the cockpit HUD shuts off
  Handle (*roster_x1c)(Mech&) = nullptr;             // 0x00414370
  void (*damage_decal)(Handle x1c, Handle decals, Collision*, float damage) = nullptr;  // 0x00474d90
  void (*last_hit)(Mech&) = nullptr;                 // 0x0041d130
  void (*zaps_hud)(void* screen, float a, int kind, float b) = nullptr;  // 0x0043be70
  void (*head_hit_hud)() = nullptr;                  // 0x00508da0(0, 0, 0)
  void (*hud_weapons)(void* screen) = nullptr;       // 0x00446520 on screen +0x6520
  void (*equip_voice)(int kind) = nullptr;           // 0x00485390: the callout for an equipment kind
  void (*voice)(int index) = nullptr;                // 0x00485370
  bool (*force_variant0)() = nullptr;                // 0x00586ce0
  void (*weapons_lost)(Mech&, int part) = nullptr;   // 0x004b1040 on +0xd4: the part's weapons are destroyed
  bool (*weapons_critical)(Mech&, int part, int ammo) = nullptr;  // 0x004b0de0 on +0xd4
  void (*weapons_stop)(Mech&) = nullptr;             // 0x004b1110 on +0xd4
  Handle (*fly_off)(MechPart&, Vec3* from, Vec3* x, Vec3* velocity, int flag) = nullptr;  // part vtable +0x60
  Handle (*spawn_effect)(Handle effect, Handle node, const float args[9]) = nullptr;  // 0x00508aa0
  Handle (*spawn_effect_at)(Handle effect, Handle node, float x, float y, float z) = nullptr;  // 0x00508c10
  void (*on_effect_end)(Handle effect, MechPart&) = nullptr;  // 0x00508f00 with 0x00461c90
  void (*node_unlink)(Handle x68, Handle node) = nullptr;     // 0x004e66a0
  void (*node_set_active)(Handle node, int on) = nullptr;     // 0x004eb170
  void (*node_set_name)(Handle node, const char* name) = nullptr;  // 0x004eb300
  void (*stop_foot_effect)(Handle effect) = nullptr;          // 0x0045fb50
  void (*free_part)(MechPart*) = nullptr;                     // vtable +0 (1); unset deletes
  const float* (*node_matrix)(MechPart&) = nullptr;          // node +0x38 +0x60, 3x3 + translation
  void (*node_update)(MechPart&) = nullptr;                   // 0x004ed530
  bool (*anim_ok)(Mech&, int kind, int arg) = nullptr;        // 0x00416780
  void (*hip_reset)(MechPart& hip, const Vec3* v) = nullptr;  // 0x004621f0
  void (*death_prep)(Mech&) = nullptr;                        // 0x0041d070
  void (*death_anim)(Mech&, int anim) = nullptr;              // 0x0041b760
  void (*stop_action)(Handle obj) = nullptr;                  // 0x00421060
  Handle (*blend_anim)(Handle base, Handle limp) = nullptr;   // 0x0041ff30
  void (*anim_speed)(Handle anim, float v) = nullptr;         // 0x0041ef90
  void (*gait_speed)(Mech&, float v) = nullptr;               // 0x004329c0 on +0x684 with +0x4a0
  const Vec3* (*position)(Mech&) = nullptr;                   // 0x005a85e0, the world position
};
DamageCalls& damage_calls();

void update_heat(Mech& m, float dt);  // 0x004192f0

}  // namespace mech3
