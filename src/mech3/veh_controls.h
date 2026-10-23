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
  virtual ~TurretLimits() = default;
  // vt+0x08 0x004a4180: the direction's y not past either sine (NaN within).
  virtual bool within(const float d[3]) const;
  // vt+0x04 0x004a41b0: within, the direction itself (true); else the
  // limit it passed, at the direction's heading (false). FIX (bug), behind
  // zfix bugs: the exe takes the heading's length as hypot(x, y), not
  // hypot(x, z); its only vehicle caller drops the answer (docs/vehicles.md).
  virtual bool clamp(const float d[3], float out[3]) const;
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
  // The motion block's reader: zReader's floats, its report.
  virtual bool reader_float(zrdr::Node* node, const char* key, float* out);  // 0x00556a60
  virtual void report(int32_t level, const char* file, int32_t line, const char* message);  // 0x0049bfd0
  // The setup's: zReader, the animations, the owner's own 0x004af2a0 and its
  // weapons (0x004b4660, the port's build_vehicle_weapons).
  virtual zrdr::Node* reader_find(zrdr::Node* node, const char* key);   // 0x005569e0
  virtual Handle anim_find(const char* name);                           // 0x0050b450
  virtual void anim_start(Handle def, Handle node);                     // 0x00508c10(def, node, 0, 0, 0)
  virtual void weapon_slots(struct VehicleControls& c, void* owner, zrdr::Node* def);  // the owner's 0x004af2a0
  virtual void owner_weapons(void* owner, zrdr::Node* def);             // 0x004b4660
  // 0x004af2a0's: zWeapon's definitions and beams, a node shown or hidden.
  virtual Handle weapon_def(const char* name);                          // 0x00581ed0: zwep::find_def
  virtual const char* weapon_def_name(Handle def);                      // its +0x04
  virtual uint32_t weapon_def_flags(Handle def);                        // its +0x6c (bit 1 a beam)
  virtual void node_set_active(Handle node, int32_t on);                // 0x004eb170
  // 0x00584a70(def, the owner's node; 0, the slot's node, muzzle, dir, segments).
  virtual Handle beam_make(Handle def, Handle owner_node, Handle node, const Vec3c* muzzle, const Vec3c* dir,
                           int32_t segments);
  // The turret's aim (2b1): a node's cached world rotation (its object3d
  // data +0x60, 3x3 rows, a read; render.md), its turn (0x004fbef0), the
  // owner's world matrix (+0x0c, a read) and its +0x18 (vt+0x10, 0x005a8710).
  virtual const float* node_world_matrix(Handle node);
  virtual void node_add_rotation(Handle node, float x, float y, float z);
  virtual const float* owner_matrix(void* owner);
  virtual void owner_x18(void* owner, float out[3]);
};
void set_veh_controls_hooks(VehControlsHooks* h);  // null: the defaults
int32_t& vehicle_serial();                         // [0x0060b148], the next +0x14

// The block at +0x398 (0x005869d0 clears it).
// The freeze and heat glow (zWeapon's 0x00586a30 / 0x00586af0, weapons.md):
// the hit adds to +0x0c (heat up, freeze down, -1..1) and puts a glow node
// at +0x10; +0x08 is what the damage reads (below -0.5: frozen solid).
struct Block398 {
  uint32_t flags = 0;  // +0x00  bits 0, 1 cleared
  uint32_t x04 = 0;    // +0x04  the last weapon
  float x08 = 0;       // +0x08  (the frame's, step 2c)
  uint32_t x0c = 0, x10 = 0, x14 = 0;
  void clear();  // 0x005869d0
};

// The vehicle's motion definition, the controls' +0x764 block (0x2a0 bytes,
// to the base's end at +0xa04), read from vehicle.zrd's mode by 0x0049ef40
// (through 0x0049ec90). docs/vehicles.md, The motion block.
struct VehicleMotion {
  using Handle = void*;
  enum Mode : int32_t { kBasic = 0, kFly = 1, kSub = 2, kTrack = 3, kHover = 4, kAmphib = 5 };
  int32_t mode = 0;            // +0x000  by the first two letters; track without the key
  float rates[2] = {};         // +0x004  10, 30
  float turns[2] = {};         // +0x00c  0.6, 2
  float turn_damping = 0;      // +0x014  30
  float rate_damping[2] = {};  // +0x018  30, 30
  float gun_hi = 0, gun_lo = 0;  // +0x020  gun_pitch (lo, hi): -0.2588 (-15 degrees), 0.5
  // +0x028: the pointer to the limiter at +0x02c (VehicleControls::turret)
  float friction[3] = {};      // +0x058  10000, 10, 0; [1] held under 0.9 [0]
  float stopping = 0;          // +0x064  8
  float chas_smooth = 0;       // +0x068  |v|
  Vec3c chas_pitch;            // +0x06c  z as |z|
  Vec3c chas_roll;             // +0x078  likewise
  int32_t support_count = 0;   // +0x084  4 from the model's support00..03, else the platform list's
  float mode_alt = 0;          // +0x088  2
  Vec3c support[16];           // +0x08c
  uint32_t support_word[16] = {};  // +0x14c  zeroed per support (the placement too)
  float quicksand_slowdown = 0;  // +0x18c  0.9
  float lava_slowdown = 0;     // +0x190  0.8
  Vec3c collision[12];         // +0x194  the model's collide00..11 (in the order 0-2, 6-8, 3-5, 9-11), else the list's
  float mass = 0;              // +0x248  1
  float inv_mass = 0;          // +0x24c
  float a_damping = 0;         // +0x250  8
  uint32_t x258 = 0;           // +0x258
  uint32_t x260 = 0;           // +0x260
  float wave[7] = {};          // +0x264  amphib_wave, then hover_wave, then sub_wave: the last given
  float collision_d[2] = {};   // +0x280  0.5, 0.15
  Handle right_morphs = nullptr;  // +0x288
  Handle left_morphs = nullptr;   // +0x28c
  Handle chassis = nullptr;       // +0x290
  Handle rtracks = nullptr;       // +0x294
  Handle ltracks = nullptr;       // +0x298
  Handle destroy_anim = nullptr;  // +0x29c  zEffect's by the vehicle's name, else its type
};

// A weapon slot (0x38 bytes; ten from +0x3b8, the owner's 0x004af2a0 fills
// them from the definition's `weapons`, items as build_vehicle_weapons reads
// them: (name n n ammo ...)).
struct WeaponSlot {
  using Handle = void*;
  Handle def = nullptr;      // +0x00  zWeapon's definition by the item's name
  int32_t index = 0;         // +0x04  0..9
  Handle node = nullptr;     // +0x08  its firepoint under the gun: the weapon's name, or <name>_L
  Handle node_r = nullptr;   // +0x0c  <name>_R when the item's third value has bit 0
  uint32_t flags = 0;        // +0x14  bit 0 the item's 8th value's bit 0, bit 1 paired, bit 2 counted
  float x18 = 0;             // +0x18  the item's 4th value (an int) while bit 2
  uint32_t x24 = 0;          // +0x24
  uint32_t x28 = 0, x2c = 0, x30 = 0;  // +0x28  the item's 5th to 7th values as they are
  Handle beam = nullptr;     // +0x34  zWeapon's beam for a beam weapon (0x00584a70)
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

  virtual void clear_commands();        // IControls vt+0x14 0x004ad640 (SetThrottle(0), SetTurn(0))
  // vt+0x24 / vt+0x28, the aim (the base's 0x004d6dc0: no, 0x0048b940:
  // nothing; the AI's turn the turret, zai_brain).
  virtual bool can_aim(float cos_limit) { (void)cos_limit; return false; }
  virtual void aim_at(const float dir[3]) { (void)dir; }
  // What the vehicle and its debug readout ask (2b2), the base's answers:
  // vt+0x1c destroyed (0x004a5270: yes), vt+0x20 (0x004d72a0: 0), vt+0x04
  // the path mode (0x0040bae0: 0), vt+0x08 its goal node (0x004a5260: -1),
  // IControls vt+0x70 on a path (0x0040bae0: no), IControls vt+0xa8 the death
  // (0x004aed50: the pilot's tick, the base's _purecall: nothing here).
  // vt+0x14 the turn rate's step (the base's 0x004a2a00, with the motion;
  // the AI's 0x004a2ae0: the turn x top_turn).
  virtual void turn_step(const VehicleMotion& m, float dt);
  virtual bool destroyed() { return true; }
  virtual int32_t dynamics() { return 0; }
  virtual int32_t path_mode() { return 0; }
  virtual int32_t path_goal() { return -1; }
  virtual bool on_path() { return false; }
  virtual void died() {}

  float throttle = 0;         // +0x008  IControls SetThrottle (vt+0x20, 0x004a5b70)
  float turn = 0;             // +0x00c  IControls SetTurn (vt+0x28, 0x004a5bb0)
  int32_t serial = 0;         // +0x014  [0x0060b148], counted up
  float x1c = 0;              // +0x01c  the gravity, [0x00609a14] (0x004accd0)
  uint32_t x20 = 0, x24 = 0;  // +0x020  (the placement zeroes them)
  uint32_t frozen = 0;        // +0x028  a freeze hit froze it (the damage's; no shipped weapon freezes)
  uint32_t x2c = 0;           // +0x02c
  uint32_t x30 = 0;           // +0x030
  uint32_t sliding = 0;       // +0x034  sliding sideways (the movement's, 0x004a2c90)
  int32_t state = 0;          // +0x038  4: destroyed (the death's; the basic controls' vt+0x1c asks it)
  uint32_t x3c = 0;           // +0x03c
  Vec3c x50;                  // +0x050  0x0063d870; the turn rates, y the turn (vt+0x14)
  Vec3c velocity;             // +0x064  the world's (the owner's +0x30 copies it)
  Vec3c x70;                  // +0x070  0x0063d870 (the placement); the local velocity: z ahead, x the slide
  float top_speed = 0;        // +0x088  the motion's rates[1] in the movement, slowed in quicksand and lava
  float top_turn = 0;         // +0x08c  turns[1] likewise
  uint32_t x94[4] = {};       // +0x094
  Vec3c points[15];           // +0x160  each the place, when placed after the init (count +0x214)
  int32_t point_count = 0;    // +0x214
  Vec3c up;                   // +0x308  the owner's matrix's second row, once rotated (and each movement, 0x004a2890)
  Vec3c x314;                 // +0x314  (the side force reads x and z)
  Vec3c angles0;              // +0x320  the node's rotation as first read
  Vec3c angles;               // +0x32c  (0x004fbe90)
  uint32_t x350 = 0;          // +0x350
  uint32_t fire_on = 0;       // +0x390  IControls vt+0x10 (0x004ad720)
  uint32_t beam_on = 0;       // +0x394  the first slot's beam firing (the death stops it)
  Block398 x398;              // +0x398
  Handle node = nullptr;      // +0x3b0  the owner's node (the setup's)
  WeaponSlot* first = nullptr;  // +0x3b4  the first slot filled
  WeaponSlot slots[10];       // +0x3b8
  uint32_t x5e8 = 0;          // +0x5e8
  int32_t x5ec = 0;           // +0x5ec
  Vec3c x5fc;                 // +0x5fc  [0x005eda80]
  Vec3c aim_dir;              // +0x5f0  a beam's direction, read live (0x00584a70 keeps the pointer)
  // The firepoints under the gun: fpnt_c's position and the inverse of its
  // length when the gun has it, else the muzzle starts at (0, 0, -1) and the
  // scale 1; fpnt_r and fpnt_l likewise (removed and freed once read), else
  // fixed offsets. The muzzle (+0x608) is what a beam reads live; the found
  // positions are likely what the aiming works from (docs/vehicles.md).
  Vec3c muzzle;               // +0x608
  Vec3c fire_l;               // +0x614  (1.51, 0.396, -2.46) without fpnt_l
  Vec3c fire_r;               // +0x620  (-1.51, 0.396, -2.46) without fpnt_r
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
  float health_max = 0;       // +0x740  100, or the definition's
  float health = 0;           // +0x744  likewise; what the damage takes from
  Vec3c heading;              // +0x748  a heading to turn to while x760 (0x004a28c0)
  uint32_t x760 = 0;          // +0x760  turning to `heading`
  VehicleMotion motion;       // +0x764
  TurretLimits turret;        // +0x790 (the pointer to it at +0x78c), the gun's pitch (0x004a4b30)
  void* owner = nullptr;      // IControls +0x10: the vehicle (SetOwner, vt+0x08)
};

// Mech3.exe 1.2 @ 0x0049ef40 (0x0049ec90 on the controls' +0x764): the
// motion block from vehicle.zrd's `mode` (node the vehicle's, `name` and
// `type` its own; the collision count into *collision_count, the controls'
// +0x214, only from the list).
void vehicle_motion_read(VehicleControls& c, void* node, zrdr::Node* mode, const char* name, const char* type,
                         int32_t* collision_count);

// Mech3.exe 1.2 @ 0x004af2a0 (a method of the owner, on its controls): the
// ten slots emptied, then one per `weapons` item: its definition, flags and
// values, its firepoint nodes under the gun hidden, a beam weapon's beam; the
// first slot's nodes shown. A beam gets 8 segments on the controls whose
// serial is 1, 1 on every other (as written: docs/vehicles.md).
void vehicle_weapon_slots(VehicleControls& c, void* owner, zrdr::Node* def);

// The AI controls' turret aim (their +0xcb0 and +0xcbc; zai_brain's
// AiVehicleControls holds it): docs/vehicles.md, The gun's aim.
struct TurretAim {
  float dir[3] = {};   // +0xcb0  the aim, in the world
  uint8_t set = 0;     // +0xcbc
};
// Mech3.exe 1.2 @ 0x004e7570 (vt+0x28): `dir` (the vehicle's frame) into the world.
void turret_aim_at(VehicleControls& c, TurretAim& a, const float dir[3]);
// Mech3.exe 1.2 @ 0x004e7490 (vt+0x24): the aim in the turret's frame,
// flattened, more ahead than `cos_limit`; true without a turret.
bool turret_can_aim(const VehicleControls& c, const TurretAim& a, float cos_limit);
// Mech3.exe 1.2 @ 0x004e72f0 (+0xa04 vt+0x04, IControls vt+0xbc): the
// turret turned toward the aim (else the owner's +0x18) by 1 - e^(-5.545 dt)
// of the angle, through the exe's exp2 bit trick; answers 1.
int32_t turret_turn(VehicleControls& c, const TurretAim& a, float dt);

// Mech3.exe 1.2 @ 0x004ab990: the 3x3 rotation of the angles (x, y, z), as
// zMath's euler (0x00521670) builds it, into `out`.
void euler_rotation(const float angles[3], float out[9]);

}  // namespace mech3
