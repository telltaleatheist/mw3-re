#pragma once
#include <cstdint>
#include <vector>

#include "gamez/zclass/cls_di.h"
#include "mech3/collision_hit.h"
#include "mech3/mech_parts.h"

// Mech collision (docs/movement-collision.md): eleven probes on the mech's box
// swept from last frame to this one through the world's segment query, the
// crossings sorted into walls, ceilings, touchable objects, vehicles and other
// mechs, and each kind's response. Original file unknown: the code sits
// between mech_anim.cpp and mech_data.cpp (named mech_collide here).
namespace mech3 {

struct Mech;

// The collider embedded in the mech at +0xfc (ctor 0x004222e0). Each list is
// prepended in the exe; the responses take its head, here the back.
struct Collider {
  int32_t clear = 0;               // +0xfc  1: the last query crossed nothing
  std::vector<CollisionHit> vehicles, mechs, ceilings, walls, touches;  // +0x100, +0x104, +0x108, +0x10c, +0x114
  float restitution = 0.1f;        // +0x118
  float yaw_kick = 0.01f;          // +0x11c
  int32_t count = 11;              // +0x120
  Vec3 local[15] = {};             // +0x124
  Vec3 world[15] = {};             // +0x1d8  this frame
  Vec3 last[15] = {};              // +0x28c  where last frame's resolution left them
  float matrix[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};  // +0x340  a 3x3 and translation (0x004abaf0)
  uint32_t x370 = 0;               // +0x370
  bool any() const { return !vehicles.empty() || !mechs.empty() || !ceilings.empty() || !walls.empty() ||
                            !touches.empty(); }
  void clear_lists();              // 0x00422c60
  void place(const float m[12]);   // 0x00422500: world and last = m * local
};

// The network's collision record a remote side sends (mech +0xd8).
struct NetCollision {
  uint32_t frame = 0;              // +0x00
  uint32_t sent = 0;               // +0x04
  uint8_t on = 0;                  // +0x08
  uint8_t active = 0;              // +0x0b
  Vec3 normal{};                   // +0x24
  Vec3 point{};                    // +0x30
};

// What collision calls outside the mech code. Unset ones do nothing or
// report none.
struct CollideCalls {
  // test hook; unset: zcls::segment_query on world_hooks().world
  void (*segments)(float* segs, int32_t points, zcls::SegmentHits* out) = nullptr;
  void (*node_intersect)(Handle node, int on) = nullptr;     // 0x004eb6d0; unset: the zcls node's flag 0x10
  void (*refresh_box)(Handle node) = nullptr;                // 0x004ecf70
  const float* (*node_box)(Handle node) = nullptr;           // node +0xa4: its box, min xyz then max xyz
  const float* (*hit_box)(Handle node) = nullptr;            // 0x004ec9c0: +0x74, null without a box
  Mech* (*node_mech)(Handle node) = nullptr;                 // 0x0040ef10: the mech a node belongs to
  Handle (*node_entity)(Handle node) = nullptr;              // 0x0040ef00: the vehicle or other entity
  bool (*node_touchable)(Handle node) = nullptr;             // node +0xbc: a damage handler with a method
  float (*touch)(Handle node, const Vec3* from, const CollisionHit& h, float damage) = nullptr;  // 0x005836b0
  void (*set_position)(Mech&, const Vec3* p) = nullptr;      // vtable +0x18
  // The AiTarget contacts (+0xac..+0xb0) the bump looks through.
  int32_t (*contact_count)(Mech&) = nullptr;
  Handle (*contact)(Mech&, int32_t i) = nullptr;
  Mech* (*contact_mech)(Handle c) = nullptr;                 // 0x004e6ff0
  bool (*contact_target)(Handle c) = nullptr;                // 0x004e6fe0, its vt +0x40, then +0xf4
  float (*contact_dist2)(Handle c) = nullptr;                // c +0x0c
  float (*contact_dist)(Handle c) = nullptr;                 // 0x004e7030
  const Vec3* (*contact_dir)(Handle c) = nullptr;            // c +0x20
  int32_t (*ai_reverses)(Mech&) = nullptr;                   // vtable +0x140, then its +0x70
  // A vehicle hit (0x004237c0).
  bool (*entity_dead)(Handle e) = nullptr;                   // e +0x08 clear
  Handle (*entity_target)(Handle e) = nullptr;               // vt +0xec: what takes the damage
  Handle (*entity_body)(Handle e) = nullptr;                 // vt +0x140, then its +0x5c
  uint32_t (*entity_team)(Handle e) = nullptr;               // vt +0xf4, its vt +0x28, +0xc0 (0: none)
  int32_t (*entity_damage)(Handle target, float damage) = nullptr;  // vt +0x160(0, 0, damage): killed
  float* (*body_velocity)(Handle body) = nullptr;            // +0x64
  float* (*body_spin)(Handle body) = nullptr;                // +0x70
  float (*body_mass)(Handle body) = nullptr;                 // +0x9b0
  const float* (*entity_matrix)(Handle e) = nullptr;         // e +0x0c
  // Another mech.
  bool (*remote)(Mech& other) = nullptr;                     // vt +0x58
  void (*net_shove)(Mech& self, Mech& other, float damage, float vx, float vz) = nullptr;  // 0x00470320
  void (*ff_jolt)(const float n[3], float amount) = nullptr; // 0x004335d0 on [0x00608620]
  void (*crouch)(Hip& hip, float amount) = nullptr;          // test hook (0x004622a0): unset runs Hip::duck
};
CollideCalls& collide_calls();

}  // namespace mech3
