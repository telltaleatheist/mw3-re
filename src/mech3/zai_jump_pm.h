#ifndef MECH3_ZAI_JUMP_PM_H
#define MECH3_ZAI_JUMP_PM_H

// Pirate's Moon's jump state (vtable 0x005eedbc): MW3's `JumpState` with a turn on the spot first, the
// glide held at the climb's height and the steering kept up in the air; docs/pm-map.md, step 3.
// Nothing runs it until the orders' switch.
#include "mech3/zai_drive_pm.h"

namespace mech3::ai {

class JumpPmHooks : public WalkPmHooks {
 public:
  virtual zgame::Handle mech(zgame::Handle v) = 0;         // vt+0x58
  virtual bool has_jets(zgame::Handle m) = 0;              // +0x3e8 above 0
  virtual void set_x588(zgame::Handle m) = 0;              // +0x588 = 1, a float read as MW3's jet fuel +0x564 is
  virtual bool in_air(zgame::Handle m) = 0;                // vt+0x150
  virtual float climb_speed(zgame::Handle m) = 0;          // +0x40
  virtual float along_speed(zgame::Handle m) = 0;          // +0x44
  virtual const float* way_point() = 0;                    // the next step's vt+0x1c
  virtual float margin() = 0;                              // the step's +0x10
  virtual float jet_climb() = 0;                           // [0x00629314]
  virtual float gravity() = 0;                             // [0x00607ec0]
  virtual void set_throttle(float v) = 0;                  // the controls' vt+0x20
  virtual void set_turn(float v) = 0;                      // vt+0x28
  virtual void set_jet_forward(float v) = 0;               // vt+0x3c
  virtual void set_jump(float v) = 0;                      // vt+0x4c
  virtual void go_next() = 0;                              // 0x004f9f00 on the next step
};

// 0x004dac40 with 0x004dad70: MW3's JumpPlan and the climb's top height.
struct JumpPlanPm {
  float t[5] = {};   // +0x00 burn up, +0x04 coast up, +0x08 glide, +0x0c coast down, +0x10 burn down
  float speed = 0;   // +0x14
  float climb = 0, fall = 0;  // +0x18, +0x1c
  float peak = 0;    // +0x20
  float need = 0;    // +0x24
  float dist = 0;    // +0x28
  float rise = 0;    // +0x2c
  float ratio = 0;   // +0x30
  float k = 0;       // +0x34
  float total = 0;   // +0x38
};
JumpPlanPm jump_plan_pm(const float from[3], const float to[3], float up, float down, float margin, float speed);

class JumpPm {
 public:
  JumpPm(JumpPmHooks& h, SteerPm& steer, zgame::Handle vehicle);  // 0x004daf40
  void run(float dt);                                             // 0x004db0e0, vt+0x14
  static bool can(JumpPmHooks& h, zgame::Handle vehicle);         // 0x004dade0

  JumpPmHooks& h;
  SteerPm& steer;
  zgame::Handle vehicle;
  int32_t phase = 0;      // +0x08  0, 1 turning; 2 burn up, 3 coast up, 4 glide, 5 coast down, 6 burn down, 7 landing
  float t[5] = {};        // +0x0c..+0x1c  seconds, each made a game time as its phase comes
  float speed = 0;        // +0x20
  float x24 = 0;          // +0x24
  float far = 0;          // +0x28  how far out the descent starts
  uint8_t x2c = 1;        // +0x2c
  float peak = 0;         // +0x30
};

}  // namespace mech3::ai

#endif
