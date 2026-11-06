#ifndef MECH3_ZAI_WALK_PM_H
#define MECH3_ZAI_WALK_PM_H

// Pirate's Moon's walking context (base vtable 0x005eec2c): one frame's turn and throttle for a pilot
// following the steering's carrot, what MW3's node state and its kin do in line (zai_node).
// docs/pm-map.md, step 3. Nothing runs it until the orders' switch.
#include "mech3/zai_steer_pm.h"

namespace mech3::ai {

class WalkPmHooks : public SteerPmHooks {
 public:
  virtual float turn_goal() = 0;      // the controls' vt+0x30
  virtual float throttle_goal() = 0;  // the controls' vt+0x1c
  virtual float max_throttle() = 0;   // the brain's +0x10c
  virtual zgame::Handle target_entity(zgame::Handle v) = 0;  // 0x004f8d70 on the vehicle's +0x6c contact, none without one
  virtual bool has_target_contact(zgame::Handle v) = 0;      // the vehicle's +0x6c
  virtual bool way_blocked(zgame::Handle v, const float from[3], const float to[3]) = 0;  // 0x004ccaf0
  virtual bool has_pilot(zgame::Handle v) = 0;               // vt+0x58
  virtual float pilot_x544(zgame::Handle v) = 0;
};

class WalkPm {
 public:
  WalkPm(WalkPmHooks& h, SteerPm& steer, zgame::Handle vehicle);  // 0x004d9870
  virtual ~WalkPm() = default;

  virtual void walk();            // vt+0x00  0x004d9680
  virtual void walk_on();         // vt+0x04  0x004d9740
  virtual void walk_straight();   // vt+0x08  0x004d9710
  virtual void hold();            // vt+0x0c  0x004d97a0
  virtual void aim_turnabout();   // vt+0x14  0x004d92c0
  virtual void aim();             // vt+0x18  0x004d9370
  virtual void turn();            // vt+0x1c  0x004d9390
  virtual void throttle();        // vt+0x20  0x004d9550
  void measure();                 // 0x004d92a0

  WalkPmHooks& h;
  SteerPm& steer;                 // +0x14
  zgame::Handle vehicle;          // +0x08
  float pos[3] = {};              // +0x0c  (the exe keeps a pointer)
  float fwd[2] = {};              // +0x10  x, z of the vehicle's +0x24
  float off_along = 0, off_side = 0, off_sum = 0;  // +0x18, +0x1c, +0x20
  float to[3] = {};               // +0x24
  float dir[3] = {};              // +0x30
  uint8_t backing = 0;            // +0x3c
  float dot = 0, cross = 0;       // +0x40, +0x44
  float turn_out = 0, throttle_out = 0;  // +0x48, +0x4c

 protected:
  void refresh();                 // what the exe reads through its pointers
  void go(bool blocked_test);
};

}  // namespace mech3::ai

#endif
