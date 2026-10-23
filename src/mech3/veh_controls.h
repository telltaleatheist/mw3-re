#pragma once
#include <cstdint>

#include "gamez/zgame/unported.h"

namespace zrdr {
struct Node;
}

// The vehicles' controls, the base 0x004ac750 (vtable 0x005c8b58, the
// IControls virtual base after its 0xa04 bytes): the AI's vehicle controls
// (mech3/zai_brain.h) and the vehicle's own basic ones are made over it.
// docs/vehicles.md. Original file: before mech3\veh_basic.cpp, with the
// basic vehicle (mech3/veh_basic_object.h).
namespace mech3 {

// 0x004a4a70 over 0x004a4000 (0x14 bytes, vtable 0x005c8340): a turret's
// limits as sines and cosines (0x004a3fc0), made with -pi/2 and pi/2 (the
// other two arguments, -pi and pi, are not kept).
struct TurretLimits {
  explicit TurretLimits(float lo, float hi);
  float lo_sin, lo_cos;  // +0x04
  float hi_sin, hi_cos;  // +0x0c
};

struct Vec3c {
  float x = 0, y = 0, z = 0;
};

// The calls out, by the original's address. Handles are zClass nodes and
// models; the owner is the vehicle (mech3::Vehicle).
class VehControlsHooks : public zgame::Unported {
 public:
  using Handle = void*;
  static uint32_t low(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
  static Handle h(uint32_t v) { return reinterpret_cast<Handle>(static_cast<uintptr_t>(v)); }
  virtual uint32_t ref(const void* p) { return low(p); }

  virtual void node_rotation(Handle node, float* x, float* y, float* z);  // 0x004fbe90
  virtual void owner_refresh(void* owner) { unported("0x005a85c0", {ref(owner)}); }  // ZEntity3d's, from the node
  virtual Handle node_find(Handle root, const char* name);              // 0x004f9d60, the whole tree
  virtual Handle node_child(Handle node, const char* name);             // 0x004eb0d0
  virtual void node_position(Handle node, Vec3c& out);                  // 0x004fc3f0: its matrix's +0x24
  virtual void node_translation(Handle node, float* x, float* y, float* z);  // 0x004fc020
  virtual Handle node_model(Handle node);                               // 0x004eb420
  virtual void node_set_model(Handle node, Handle model);               // 0x004eb370
  virtual void model_release(Handle model);                             // 0x00539150
  virtual void node_remove_child(Handle parent, Handle child);          // 0x004ebb40
  virtual void node_free(Handle node);                                  // 0x004f9050
  // The setup's: zReader, the animations, the owner's own 0x004af2a0 and its
  // weapons (0x004b4660, the port's build_vehicle_weapons).
  virtual zrdr::Node* reader_find(zrdr::Node* node, const char* key);   // 0x005569e0
  virtual Handle anim_find(const char* name);                           // 0x0050b450
  virtual void anim_start(Handle def, Handle node);                     // 0x00508c10(def, node, 0, 0, 0)
  virtual void owner_4af2a0(void* owner, zrdr::Node* def) { unported("0x004af2a0", {ref(owner), low(def)}); }
  virtual void owner_weapons(void* owner, zrdr::Node* def);             // 0x004b4660
};
void set_veh_controls_hooks(VehControlsHooks* h);  // null: the defaults
int32_t& vehicle_serial();                         // [0x0060b148], the next +0x14

// The block at +0x398 (0x005869d0 clears it).
struct Block398 {
  uint32_t flags = 0;  // +0x00  bits 0, 1 cleared
  uint32_t x04 = 0;
  uint32_t x08 = 0, x0c = 0, x10 = 0, x14 = 0;
  void clear();  // 0x005869d0
};

// The base's fields its users have shown so far; the rest stays 0 from the
// zeroed block (0x004ac860).
struct VehicleControls {
  using Handle = void*;
  VehicleControls();  // 0x004ac750 (0x004ac740 on +0x764)
  virtual ~VehicleControls() = default;
  VehicleControls(const VehicleControls&) = delete;
  VehicleControls& operator=(const VehicleControls&) = delete;

  virtual void setup(zrdr::Node* def);  // vt+0x0c 0x004acbc0
  virtual void init();                  // vt+0x10 0x004ac8a0
  void reset_1c();                      // vt+0x18 0x004accd0

  int32_t serial = 0;         // +0x014  [0x0060b148], counted up
  float x1c = 0;              // +0x01c  the gravity, [0x00609a14] (0x004accd0)
  uint32_t x28 = 0, x2c = 0;  // +0x028
  uint32_t x30 = 0;           // +0x030
  uint32_t x3c = 0;           // +0x03c
  Vec3c x50;                  // +0x050  0x0063d870
  uint32_t x94[4] = {};       // +0x094
  Vec3c up;                   // +0x308  the owner's matrix's second row, once rotated
  Vec3c angles0;              // +0x320  the node's rotation as first read
  Vec3c angles;               // +0x32c  (0x004fbe90)
  uint32_t x350 = 0;          // +0x350
  Block398 x398;              // +0x398
  Handle node = nullptr;      // +0x3b0  the owner's node (the setup's)
  uint32_t x5e8 = 0;          // +0x5e8
  int32_t x5ec = 0;           // +0x5ec
  Vec3c x5fc;                 // +0x5fc  [0x005eda80]
  // The firepoints under the gun: fpnt_c's position and the inverse of its
  // length when the gun has it, else the direction (0, 0, -1) and 1; fpnt_r
  // and fpnt_l likewise (removed and freed once read), else fixed offsets.
  // The found and the default values land in different fields (as written;
  // which the firing reads: docs/vehicles.md, Open).
  Vec3c fire_c_default;       // +0x608
  Vec3c fire_l_default;       // +0x614  (1.51, 0.396, -2.46)
  Vec3c fire_r_default;       // +0x620  (-1.51, 0.396, -2.46)
  Vec3c fpnt_c;               // +0x62c
  Vec3c fpnt_r;               // +0x638
  Vec3c fpnt_l;               // +0x644
  int32_t x650 = 0;           // +0x650
  Vec3c gun_pos;              // +0x6d8  the gun's world position
  Handle body = nullptr;      // +0x700
  Handle turret_node = nullptr;  // +0x704
  Handle gun = nullptr;       // +0x708
  Handle shadow = nullptr;    // +0x70c
  float fire_c_scale = 0;     // +0x724
  float health = 0;           // +0x740  100, or the definition's
  float health_max = 0;       // +0x744  likewise
  uint32_t x760 = 0;          // +0x760
  TurretLimits turret;        // +0x790 (the pointer to it at +0x78c)
  void* owner = nullptr;      // IControls +0x10: the vehicle (SetOwner, vt+0x08)
};

// Mech3.exe 1.2 @ 0x004ab990: the 3x3 rotation of the angles (x, y, z), as
// zMath's euler (0x00521670) builds it, into `out`.
void euler_rotation(const float angles[3], float out[9]);

}  // namespace mech3
