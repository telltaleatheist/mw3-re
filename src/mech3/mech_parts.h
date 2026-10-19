#pragma once
#include <cstdint>

// A mech's parts and how they take damage (docs/damage.md). Original file:
// mech3\mech_parts.cpp (the base class sits just before its string, the
// subclasses just after).
namespace mech3 {

struct Mech;

struct Vec3 {
  float x, y, z;
};

using Handle = void*;  // an object of a system not ported yet (node, effect, entity)

// The fields a hit reads from a weapon (docs/types.json ZWeaponDef, WeaponType).
struct WeaponType {
  int32_t id = -1;            // +0x00  mech-lab index
  float heat_2 = 0;           // +0x0c  HEAT's second value, added to the target
  float zaps_hud = 0;         // +0x14  ZAPS_HUD: static on the target's HUD
  float zaps_hud_2 = 0;       // +0x18
  float impact_force = 0;     // +0x48
  bool is_narc = false;       // +0x4c
};

struct WeaponDef {
  const char* military_name = "";  // +0x08
  uint32_t flags = 0;              // +0x6c  bit 1: BEAM
  WeaponType* type = nullptr;      // +0x1f0
};

struct Collision {
  Handle x20 = nullptr;   // +0x20
  Handle node = nullptr;  // +0x24  the node hit
};

enum PartId : int32_t {
  kTorso = 1, kLeftArm, kRightArm, kLeftThigh, kRightThigh, kHead, kLeftTorso, kRightTorso,
  kLeftHand, kRightHand, kLeftCalf, kRightCalf, kLeftFoot, kRightFoot, kHip,
};

enum CriticalKind : int32_t { kCritSink = 0, kCritWeapon, kCritAmmo, kCritEquipment, kCritSpecial };

// Base part, vtable 0x005c2840. Armor and internal structure are fractions,
// 1 = intact; the scales are 100 / points.
class MechPart {
 public:
  Mech* mech = nullptr;          // +0x04  (looked up lazily in the exe, 0x460000)
  Handle node = nullptr;         // +0x08
  Vec3* translation = nullptr;   // +0x18  the node's local translation (object3d data +0x54, 0x00460190)
  int32_t id = -1;               // node +0x28, -1 without a node
  float internal = 1.0f;         // +0x38
  float internal_scale = 1.0f;   // +0x3c
  float armor = 1.0f;            // +0x40
  float armor_scale = 100.0f;    // +0x44
  bool destroyed = false;        // +0x48
  Handle x68 = nullptr;          // +0x68
  Handle debris_effect = nullptr;  // +0x6c
  Handle trail_effect = nullptr;   // +0x70

  virtual ~MechPart() = default;
  virtual int take_hit(WeaponDef* def, Collision* coll, float damage, Vec3* impulse);  // +0x18
  virtual void apply_impact(Vec3*, float) {}                  // +0x1c, 0x0040baf0
  virtual void destroy(int variant, int fly);                 // +0x20
  virtual bool critical(int kind, int section);               // +0x28
  virtual void apply_damage(float d, int section);            // +0x38
  virtual void set_internal(float v, int section);            // +0x3c
  virtual void set_armor(float v, int section);               // +0x40
  virtual float get_armor(int) const { return armor; }        // +0x44, 0x00467340
  virtual float get_internal(int) const { return internal; }  // +0x48, 0x00467350
  virtual float get_armor_scale(int) const { return armor_scale; }        // +0x4c, 0x00467360
  virtual float get_internal_scale(int) const { return internal_scale; }  // +0x50, 0x00467370

  void detach();                 // vtable +0x0c with no node, 0x00460190
  void lose_contents();          // 0x004617f0
  bool has_equipment(int kind);  // 0x00460cc0
  Handle fly_off(Vec3* from, Vec3* x, Vec3* velocity, int flag);  // vtable +0x60
  int base_take_hit(WeaponDef* def, Collision* coll, float damage, Vec3* impulse) {
    return MechPart::take_hit(def, coll, damage, impulse);
  }
};

class Hand;
class Calf;
class Foot;

// Torso, vtable 0x005c29e0, 0x1b4 bytes: also the side torsos, as sections
// 0-2 front (centre, left, right) and 3-5 rear.
class Torso : public MechPart {
 public:
  Handle death_effect = nullptr;   // +0x74
  Handle death_effect2 = nullptr;  // +0x78
  float half_width = 0;            // +0x7c
  float half_depth = 0;            // +0x80
  Vec3 jolt_spin{};                // +0xd8
  Vec3 jolt{};                     // +0x100
  float jolt_scale[3] = {};        // +0x14c
  float sec_armor[6] = {};         // +0x174 + 4s, sections 1-5
  float sec_armor_scale[6] = {};   // +0x188 + 4s, sections 1-5
  float side_internal[2] = {1, 1};        // +0x1a0, +0x1a4
  float side_internal_scale[2] = {1, 1};  // +0x1a8, +0x1ac
  int32_t centring = 0;            // +0x1b0  CMD_CENTER_TORSO 1, CMD_CENTER_LEGS 2 (docs/movement-ground.md)

  int take_hit(WeaponDef* def, Collision* coll, float damage, Vec3* impulse) override;  // 0x00464720
  void apply_impact(Vec3* impulse, float force) override;  // 0x00464af0
  void destroy(int explode, int fly) override;             // 0x00465f40
  bool critical(int kind, int section) override { return MechPart::critical(kind, section); }  // 0x004647d0
  void set_internal(float v, int section) override;        // 0x00464830
  void set_armor(float v, int section) override;           // 0x00464950
  float get_armor(int s) const override;                   // 0x004649c0
  float get_internal(int s) const override;                // 0x00464a20
  float get_armor_scale(int s) const override;             // 0x004649f0
  float get_internal_scale(int s) const override;          // 0x00464a70
  int hit_section(const Vec3* shot) const;                 // 0x00460350

 private:
  float* internal_slot(int s);
};

// Thigh, vtable 0x005c2978, 0xd4 bytes; the calf and foot hang off it.
class Leg : public MechPart {
 public:
  Calf* calf = nullptr;          // +0x74
  Foot* foot = nullptr;          // +0x78

  int take_hit(WeaponDef* def, Collision* coll, float damage, Vec3* impulse) override;  // 0x00463b40
  void destroy(int variant, int fly) override;              // 0x00463f70
  bool critical(int kind, int section) override;            // 0x00463ef0
};

class Arm : public MechPart {    // vtable 0x005c2a48, 0xd0 bytes
 public:
  Hand* hand = nullptr;          // +0x74
  int take_hit(WeaponDef* def, Collision* coll, float damage, Vec3* impulse) override;  // 0x00466ba0
  void apply_impact(Vec3* impulse, float force) override;   // 0x00466c30
  void destroy(int variant, int fly) override;              // 0x00466c50
};

class Head : public MechPart {   // vtable 0x005c2b80, 0x74 bytes
 public:
  int take_hit(WeaponDef* def, Collision* coll, float damage, Vec3* impulse) override;  // 0x00467200
  void apply_impact(Vec3* impulse, float force) override;   // 0x00466c30
  void destroy(int variant, int fly) override;              // 0x004671e0
};

class Hip : public MechPart {    // vtable 0x005c28a8
 public:
  int take_hit(WeaponDef* def, Collision* coll, float damage, Vec3* impulse) override;  // 0x00462560
  void apply_impact(Vec3* impulse, float force) override;   // 0x00466c30
  void destroy(int variant, int fly) override;              // 0x00462490
};

// The parts below have no armor or structure: hits go to their parent.
class Foot : public MechPart {   // vtable 0x005c2910, 0xec bytes
 public:
  float clearance = 0;           // +0x8c  height above the ground
  int32_t ground_state = 0;      // +0x94  0 air, 1 touching, 2, 3 planted
  Handle shadow = nullptr;       // +0x78  always 0: the constructor hides <c>_shadow and clears it
  bool in_water = false;         // +0xac
  bool pivot = false;            // +0xc0  counter-spins while the mech turns in place
  float pivot_rate = 0;          // +0xd4
  float water_depth = 0;         // +0xdc  water surface above the foot
  float water_surface = 0;       // +0xe0
  Handle effect = nullptr;       // +0xe4
  int32_t soil = 0;              // +0xe8  docs/structs.md: 8 is lava

  Leg* leg() const;              // 0x00462d80
  int take_hit(WeaponDef* def, Collision* coll, float damage, Vec3* impulse) override;  // 0x00462980
  void destroy(int variant, int fly) override;              // 0x004629f0
  void set_internal(float v, int section) override;         // 0x00462890
  void stop_effect();                                       // 0x00463100
  void set_pivot(float rate);                               // 0x00462dc0
  void show_shadow(bool on);                                // 0x0041eca0
};

class Calf : public MechPart {   // vtable 0x005c2b18
 public:
  Leg* leg() const;              // 0x00466f80
  int take_hit(WeaponDef* def, Collision* coll, float damage, Vec3* impulse) override;  // 0x00466f50
  void destroy(int variant, int fly) override;              // 0x00466f30
  bool critical(int kind, int section) override;            // 0x00466fc0
  void set_internal(float v, int section) override;         // 0x00466f00
};

class Hand : public MechPart {   // vtable 0x005c2ab0
 public:
  Arm* arm() const;              // 0x00466e40
  int take_hit(WeaponDef* def, Collision* coll, float damage, Vec3* impulse) override;  // 0x00467440
  void apply_impact(Vec3* impulse, float force) override;   // 0x00466c30
  void destroy(int variant, int fly) override;              // 0x00466e10
  void set_internal(float v, int section) override;         // 0x00466eb0
};

}  // namespace mech3
