#pragma once
// The mission's landmines (docs/landmines.md): landmines.zrd's block for the
// mission (a weapon, a radius, the places) read at the load, checked by every
// mech's update, a mine within the radius setting off the weapon where it
// lies. Original file unknown: the code sits between mech3\briefing.cpp and
// mech3\mech.cpp (0x00413470..0x004137c0), beside the mechs' set-up.
#include <cstdint>
#include <vector>

#include "gamez/zweapon/zwep_init.h"

namespace zcls { struct GroundHits; }
namespace zrdr { struct Node; }

namespace mech3 {

struct Landmine {     // 0x10 bytes
  uint8_t armed = 1;  // +0x00
  float at[3] = {};   // +0x04
};

// The engine calls; unset ones are zReader's, zWeapon's and zClass's own.
struct LandmineHooks {
  zrdr::Node* (*load)(const char* file) = nullptr;                       // 0x00556740
  void (*free)(zrdr::Node* root) = nullptr;                              // 0x00556820
  zwep::WeaponDef* (*find_def)(const char* name) = nullptr;              // 0x00581ed0
  // 0x004edab0 on the world ([0x00608c84]+4): 0 when it found ground.
  int32_t (*ground)(float x, float y, float z, zcls::GroundHits* out) = nullptr;
  uint32_t (*impact)(zwep::WeaponDef*, zwep::HitRecord*, zwep::Projectile*, zwep::Handle) = nullptr;     // 0x005826b0
  int32_t (*splash_query)(zwep::WeaponDef*, zwep::Projectile*, int32_t, zwep::HitList*) = nullptr;     // 0x00582920
  int32_t (*splash)(zwep::WeaponDef*, zwep::Projectile*, zwep::HitList*, uintptr_t) = nullptr;         // 0x005829b0
};
LandmineHooks& landmine_hooks();

struct Landmines {
  std::vector<Landmine> list;          // 0x00604e48 (+4..+8)
  zwep::WeaponDef* weapon = nullptr;   // 0x00604e5c
  float radius = 20.0f;                // 0x005e1f58
  float radius_sq = 400.0f;            // 0x00604e40
  // 0x00604e60: the stand-in shot a mine fires. The exe gives it the node
  // 0x00605438, memory nothing else touches (zero: no INTERSECT flag for the
  // splash query to clear); the port leaves it none, which reads the same.
  zwep::Projectile shot;
};
Landmines& landmines();

bool landmines_load(const char* file, const char* block);  // 0x004135c0
void landmines_free();                                     // 0x00413770
bool landmines_check(const float at[3]);                   // 0x00413560
Landmine* landmine_detonate(Landmine& mine);               // 0x00413470

}  // namespace mech3
