#pragma once
#include <cstdint>

#include "mech3/mech_parts.h"

// A mech's gait animations (docs/movement.md, The animation objects).
// Original file: mech3\mech_anim.cpp. MechAnim (vtable 0x005bd1f0, 0xdc
// bytes) sits over a zSIScript motion; only the fields and methods the gait
// and its timing use are here so far. The blends (vtable 0x005bd240) and the
// one-shot actions (0x005bd290) override the virtuals.
namespace mech3 {

class MechAnim {
 public:
  float rate = 1.0f;          // +0x04  playback rate (script +0x04)
  float mult = 1.0f;          // +0x08  stride multiplier: rate = speed * mult
  float stride = 0.0f;        // +0x0c  hip travel per loop
  float loop = 0.0f;          // +0x54  loop time (script)
  float t = 0.0f;             // +0x88  time, before the rate
  float stride_speed = 0.0f;  // +0x8c
  float events[3] = {};       // +0xd0  left and right footfall, a quarter cycle

  virtual ~MechAnim() = default;
  virtual void set_speed(float speed);   // +0x08, 0x0041ee80
  virtual void advance(float dt);        // +0x10, 0x0041ef60
  virtual void set_blend(float) {}       // +0x14, 0x004a5bc0 (blends only)
  virtual void velocity(Vec3* v) { (void)v; }  // +0x20, 0x0041f8d0: the hip's speed into v->z (sampling not ported)
  virtual void seek(int event);          // +0x44, 0x0041fce0

  void copy_phase(const MechAnim& from);  // 0x0041ef00
  void set_stride_speed(float s);         // 0x0041ef90
};

// A one-shot leg action (vtable 0x005bd290, 0xf0 bytes): fallf, fallb,
// landf, landb, getupf, getupb, collapsel, collapser (docs/movement-jump.md).
// Its pose sampling (vt +0x24..+0x3c) is not ported; root() stands in.
class MechAction : public MechAnim {
 public:
  bool done = false;      // +0xec
  bool reversed = false;  // +0xed  running backwards (Stop)

  void advance(float dt) override;  // +0x10, 0x00420f80: clamps to 0..loop and sets done
  virtual void root(Vec3* pos, float q[4]) { (void)pos, (void)q; }  // +0x30, the hip's sample
  void reset();                     // 0x00420f40
  bool started() const;             // 0x00421000
  void stop();                      // 0x00421060
};

}  // namespace mech3
