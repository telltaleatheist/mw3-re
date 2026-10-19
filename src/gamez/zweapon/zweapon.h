#pragma once
#include <cstdint>

#include "gamez/zweapon/zwep_init.h"

// zWeapon's run time: the code from 0x00581230 to zwep_init.c (0x00583880)
// and after it to 0x00586b70 (docs/weapons.md, "zWeapon run time"). The file
// names are not in the exe; this is named after the module. Only the setters
// the game half calls before a shot are ported so far.
namespace zwep {

// What the game half hands the next shot or beam (globals 0x009a0ee0..,
// 0x009a17cc..; reset to these values by the startup 0x00583750 and by every
// spawn, docs/weapons.md "Fire modifiers").
struct FireState {
  int32_t* lock_flag = nullptr;        // 0x009a0ee0  the firing weapon's lock flag
  Handle* lock_target = nullptr;       // 0x009a0ee4  and its target
  int32_t beam_ground_fx = 1;          // 0x009a17cc  0: the beam plays no ground impact
  float damage_scale = 1;              // 0x009a17d0
  float beam_range_scale = 1;          // 0x009a17d4  0.5 under water
  float track_scale[2] = {1, 1};       // 0x009a17d8, 0x009a17dc  FCS: 2, 2 (read by nothing)
  int32_t terrain_follow = 0;          // 0x009a17e0  FCS: 1
};
FireState& fire_state();

// What a hit leaves for the damage code (globals 0x009a0ec4.., 0x009a1788..).
struct HitState {
  Vec3f3 shot_from{}, shot_to{};       // 0x009a0ec4, 0x009a0ed0  the shot's line (Mech::TakeDamage reads it)
  Handle attacker = nullptr;           // 0x009a0edc  the shooter's node
  int32_t record_shot = 1;             // 0x009a0eec  0 while a hit's own impact and splash run
  int32_t splash_enabled = 1;          // 0x009a0ee8  air bursts splash only with it
  int32_t beam_ground_fx = 1;          // 0x009a0ef0  the beam's copy of FireState::beam_ground_fx
  Handle killed = nullptr;             // 0x009a1788  set by the handler (0x00583710): the hit killed
  HitRecord* killed_at = nullptr;      // 0x009a1790  and where, for the craters
  float health_after = 1;              // 0x009a178c  set by the handler (0x00583730)
  void (*pre_hit)(Handle handler, Handle node, float damage, WeaponDef* def) = nullptr;  // 0x009a17a4
  int32_t hits = 0;                    // 0x009a17c0  hits by `counted` (network "WeaponData")
  Handle counted = nullptr;            // 0x009a17c4
  float dt = 0;                        // 0x0072c52c  the frame's, set by the game loop
  float tether_ceiling = 30;           // 0x009a17a0  a tethered shot's height limit
};
HitState& hit_state();

// A beam in the air, 0x150 bytes (0x00584a70). The fields its damage reads.
struct Beam {
  float length = 0;                    // +0x28
  float damage_scale = 1;              // +0x34  FireState::damage_scale at the start
  float range_scale = 1;               // +0x38  FireState::beam_range_scale (0.5 under water)
  uint32_t flags = 0;                  // +0x14c  bit 0 ground effect, bit 1 damage dealt
};

// What a splash query found: the exe's 0x500-byte buffer, a count byte then
// 0x28-byte records (31 fit).
struct HitList {
  uint8_t count = 0;
  HitRecord rec[31];
};

// The scene, effects and sounds a hit touches; unset ones do nothing.
struct HitHooks {
  void (*surface_mark)(HitRecord* hit) = nullptr;                   // 0x00542060 (not for target-heat weapons)
  uintptr_t (*node_handler)(Handle node) = nullptr;                 // node +0xbc: 0 none, 1 a projectile, else a handler
  Projectile* (*node_projectile)(Handle node) = nullptr;            // node +0x40
  bool (*handler_live)(uintptr_t handler) = nullptr;                // handler +4 (its function) is set
  uint32_t (*call_handler)(uintptr_t handler, WeaponDef* def, HitRecord* hit, float damage) = nullptr;  // handler +4 on +0
  int32_t (*soil)(Handle material) = nullptr;                       // material +0x20
  void (*effect)(Handle anim, const float args[9]) = nullptr;       // 0x00508aa0
  int32_t (*crater)(WeaponDef* def, HitRecord* hit, int32_t soil, Projectile* p) = nullptr;     // 0x005825b0
  int32_t (*quicksand)(WeaponDef* def, HitRecord* hit, int32_t soil, Projectile* p) = nullptr;  // 0x005824b0
  void (*sound)(WeaponDef* def, HitRecord* hit, int32_t soil, float volume) = nullptr;         // 0x00582f60
  void (*impact_effect)(int32_t effect, const Vec3f3* at) = nullptr;                           // 0x0050d650
  void (*surface_angles)(const Vec3f3* normal, float out[3]) = nullptr;  // 0x00525a50 from 0x005eda70, then 0x00525f80
  bool (*node_intersect)(Handle node) = nullptr;                    // node +0x24 bit 4
  void (*set_intersect)(Handle node, int32_t on) = nullptr;         // 0x004eb6d0
  // 0x004f0ac0 on the world: what lies within `radius` of `at`, into `out`;
  // 0 when it found something.
  int32_t (*query)(Handle node, const Vec3f3* at, float radius, HitList* out) = nullptr;
};
HitHooks& hit_hooks();

float beam_damage(WeaponDef* def, Beam* b);                                        // 0x00583020
uint32_t apply_damage(WeaponDef* def, Handle owner, const Vec3f3* from, HitRecord* hit, float damage,
                      Handle anim);                                                // 0x00583480
uint32_t impact(WeaponDef* def, HitRecord* hit, Projectile* p, Handle anim);        // 0x005826b0
int32_t splash_query(WeaponDef* def, Projectile* p, int32_t flag, HitList* out);   // 0x00582920
int32_t splash(WeaponDef* def, Projectile* p, HitList* list, uintptr_t exclude);   // 0x005829b0
void hit(WeaponDef* def, Projectile* p, HitRecord* rec);                           // 0x00586350
void air_burst(WeaponDef* def, Projectile* p);                                     // 0x00582320
void note_kill(Handle killed, HitRecord* at);                                      // 0x00583710
void step(WeaponDef* def, Projectile* p, Vec3f3* move);                            // 0x00585cc0
inline void set_health_after(float h) { hit_state().health_after = h; }           // 0x00583730

void set_lock(int32_t* flag, Handle* target);                       // 0x00581f60
bool set_fire_sound(WeaponDef* def, int32_t choice);                 // 0x00581f70
bool set_fire_animation(WeaponDef* def, int32_t choice);             // 0x00581f90
void attach_fire(WeaponDef* def, Handle node);                       // 0x00581fb0
void attach_fire(WeaponDef* def, Handle node, const void* offset);   // 0x00581fd0

}  // namespace zwep
