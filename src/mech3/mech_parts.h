#pragma once
#include <cstdint>

#include "gamez/zweapon/zwep_init.h"

// A mech's parts and how they take damage (docs/damage.md). Original file:
// mech3\mech_parts.cpp (the base class sits just before its string, the
// subclasses just after).
namespace mech3 {

struct Mech;

struct Vec3 {
  float x, y, z;
};

using Handle = void*;  // an object of a system not ported yet (node, effect, entity)

// The scene-node calls the parts' pose makes (zClass/Object3d.c). Unset: nothing.
struct PoseCalls {
  void (*node_set)(Handle node, const float* m) = nullptr;  // 0x004fc300: m (3x4) is the node's new matrix
  float* (*node_matrix)(Handle node) = nullptr;             // 0x004fc3f0: the object3d's local 3x4
};
PoseCalls& pose_calls();

// The weapon a hit comes from: the zWeapon definition and its MW3 half
// (mech3/weapon.h, docs/weapons.md).
struct WeaponType;
using WeaponDef = zwep::WeaponDef;

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
  float* rotation = nullptr;     // +0x14  the node's local 3x3 (object3d data +0x30); the translation follows it
  Vec3* translation = nullptr;   // +0x18  the node's local translation (object3d data +0x54, 0x00460190)
  int32_t id = -1;               // node +0x28, -1 without a node
  float internal = 1.0f;         // +0x38
  float internal_scale = 1.0f;   // +0x3c
  float armor = 1.0f;            // +0x40
  float armor_scale = 100.0f;    // +0x44
  bool destroyed = false;        // +0x48
  bool pending = false;          // +0x4a  a pose to apply instead of the next one (+0x4c, +0x58)
  Vec3 pending_pos{};            // +0x4c
  float pending_q[4] = {1, 0, 0, 0};  // +0x58  (a foot's target rotation, set_pose 0x004629b0)
  Handle x68 = nullptr;          // +0x68
  Handle debris_effect = nullptr;  // +0x6c
  Handle trail_effect = nullptr;   // +0x70

  virtual ~MechPart() = default;
  virtual void update(float) {}  // +0x10 (base 0x00461730; torso 0x00464e80, hip 0x004623a0, leg 0x00463ee0, arm 0x00466510)
  virtual void node_changed();   // +0x14, 0x00460180: the node's matrix set (0x004fc300); torso 0x004659c0
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
  virtual void set_pose(const Vec3* pos, const float* q);  // +0x2c, 0x00460280 (hip 0x004624b0, torso 0x00464600, foot 0x004629b0)

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
  int32_t aim = 0;                 // +0x09c  the AI's aim, cancelled by any player input
  float twist_in = 0, pitch_in = 0;    // +0x0bc, +0x0c0  the controls' twist and pitch
  float look_yaw = 0, look_pitch = 0;  // +0x0c4, +0x0c8  the free look
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
  bool set_twist(float twist, float look);                 // 0x00466200
  bool set_pitch(float pitch, float look);                 // 0x00466240

 private:
  float* internal_slot(int s);
};

// Thigh, vtable 0x005c2978, 0xd4 bytes; the calf and foot hang off it.
class Leg : public MechPart {
 public:
  Calf* calf = nullptr;          // +0x74
  Foot* foot = nullptr;          // +0x78
  bool knee_back = false;        // +0x7c  mech +0x585 at setup (0x004637b0): the knee bends the other way
  float thigh_angle = 0;         // +0x80  the IK's pitch at the hip
  float knee_angle = 0;          // +0x84
  float thigh_q[4] = {1, 0, 0, 0};  // +0x88
  float calf_q[4] = {1, 0, 0, 0};   // +0x98
  // Set up by 0x004637b0 from the rest pose (not ported): the leg's lengths and offsets.
  float ik_a8 = 0;               // +0xa8
  float thigh_len = 0;           // +0xac  hip joint to knee
  float ik_b0 = 0;               // +0xb0
  float hip_x = 0;               // +0xb4  the foot's rest x, taken off the target
  float calf_len = 0;            // +0xb8  knee to ankle
  float ik_bc = 0, ik_c0 = 0, ik_c4 = 0, ik_c8 = 0;  // +0xbc..+0xc8
  float ik_k = 0;                // +0xcc  -(thigh^2 + calf^2)
  float ik_m = 0;                // +0xd0  1 / (2 thigh calf)

  bool solve(const Vec3& p);     // 0x00463c90: the angles and joint rotations for an ankle at p
  bool reach(const Vec3& p, const float q[4]);  // 0x00463e40: the IK onto the thigh, calf and foot

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
  float rot[4] = {1, 0, 0, 0};    // +0x74  the pose's hip rotation
  float inv[4] = {1, 0, 0, 0};    // +0x84  its inverse
  Vec3 offset{};                 // +0x98  added to the pose (y: the crouch, toward crouch_target)
  float crouch_target = 0;       // +0xa8
  void set_pose(const Vec3* pos, const float* q) override;  // 0x004624b0
  int take_hit(WeaponDef* def, Collision* coll, float damage, Vec3* impulse) override;  // 0x00462560
  void apply_impact(Vec3* impulse, float force) override;   // 0x00466c30
  void destroy(int variant, int fly) override;              // 0x00462490
};

// The parts below have no armor or structure: hits go to their parent.
class Foot : public MechPart {   // vtable 0x005c2910, 0xec bytes
 public:
  float clearance = 0;           // +0x8c  height above the ground
  int32_t ground_state = 0;      // +0x94  0 air, 1 touching, 2, 3 planted
  uint32_t toe_count = 0;        // +0xa0
  Handle* toe_nodes = nullptr;   // +0xa4
  float (*toe_q)[4] = nullptr;   // +0xa8  each toe's last rotation
  float last_q[4] = {1, 0, 0, 0};  // +0xc4  the rotation set last (the gait's blend-in starts from it)
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
  void set_pose(const Vec3* pos, const float* q) override;  // 0x004629b0: the target, applied by the foot's update
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
