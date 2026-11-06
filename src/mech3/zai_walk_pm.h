#ifndef MECH3_ZAI_WALK_PM_H
#define MECH3_ZAI_WALK_PM_H

// Pirate's Moon's walking context (base vtable 0x005eec2c): one frame's turn and throttle for a pilot
// following the steering's carrot, what MW3's node state and its kin do in line (zai_node).
// docs/pm-map.md, step 3. Nothing runs it until the orders' switch.
#include "mech3/zai_steer_pm.h"

namespace mech3::ai {

struct InOutVia;

class WalkPmHooks : public SteerPmHooks {
 public:
  virtual float turn_goal() = 0;      // the controls' vt+0x30
  virtual float throttle_goal() = 0;  // the controls' vt+0x1c
  virtual float max_throttle() = 0;   // the brain's +0x10c
  virtual zgame::Handle target_entity(zgame::Handle v) = 0;  // 0x004f8d70 on the vehicle's +0x6c contact, none without one
  virtual bool has_target_contact(zgame::Handle v) = 0;      // the vehicle's +0x6c
  // 0x004ccaf0: true with `out` the place the boundary pulls `to` to; `out` may be `to`. Its fifth
  // argument only lands in the area's +0xb8, which no query reads.
  virtual bool pull(zgame::Handle v, const float from[3], const float to[3], float out[3]) = 0;
  virtual float* way_from() = 0;                            // the brain's +0x2b8
  virtual bool has_pilot(zgame::Handle v) = 0;               // vt+0x58
  virtual float pilot_x544(zgame::Handle v) = 0;
  virtual float now() = 0;                                  // [0x0062da90]
  virtual float speed_at(zgame::Handle v, float t) = 0;     // vt+0x124
  virtual float stop_distance(zgame::Handle v, float s) = 0;  // vt+0x12c
  virtual float range_far() = 0;                            // the brain's +0x28
  virtual float range_near() = 0;                           // the brain's +0x2c
  // What the order is about: the order's +0x0c, vt+0x1c, vt+0x18, vt+0x50 (the exe has no answer for none).
  virtual zgame::Handle order_target() = 0;
  virtual const float* target_place(zgame::Handle t) = 0;   // vt+0x04
  virtual bool target_live(zgame::Handle t) = 0;            // +0x1c's +0x08
  virtual bool must_leave(zgame::Handle v) = 0;             // 0x004fa210
  virtual void set_throttle(float v) = 0;                   // the controls' vt+0x20
  virtual void set_turn(float v) = 0;                       // vt+0x28
  // The boundary record's +0x50 and +0x4c: the via node the last pull ended on, or none.
  virtual const InOutVia* at_via(zgame::Handle v) = 0;
  virtual void queue_via(const InOutVia* via) = 0;          // 0x004ea0f0 on the brain's +0x15c, a 0x005eeca4 item
};

float flat_dir_pm(const float from[3], const float to[3], float out[3]);  // 0x00537e60: its flat length

class WalkFightPm;

// What the fighting states keep and hand down to their walking context (vtable 0x005eec78).
struct StateOwnedPm {
  virtual ~StateOwnedPm() = default;
};
struct FightStatePm {
  FightStatePm();                                       // 0x004d9b10
  void enter(WalkPmHooks& h, zgame::Handle vehicle);    // 0x004d9be0
  void reset();                                         // 0x004d9ce0, vt+0x10
  void release();                                       // 0x004d9cc0
  void plan(WalkPmHooks& h, WalkFightPm& c);            // 0x004da1c0, vt+0x24
  void step(WalkPmHooks& h, WalkFightPm& c);            // 0x004d9d00

  uint32_t flags = 0;                 // +0x08  its low four bits the state's step
  float stop_dist;                    // +0x0c
  float range_far;                    // +0x10
  float range_near = 0, range_mid = 0, x1c = 0, speed = 0;  // +0x14, +0x18, +0x1c, +0x20
  float wake = 0;                     // +0x24  a time
  float until;                        // +0x28  a time
  uint8_t pulled = 0, x2d = 0;        // +0x2c  the last plan was pulled by the boundary; +0x2d
  StateOwnedPm* owned = nullptr;      // +0x30
  uint8_t x34 = 0;                    // +0x34  [0x0060bb64], nothing writes it
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
  virtual void measure();         // vt+0x10  0x004d92a0

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
  friend struct FightStatePm;
  void refresh();                 // what the exe reads through its pointers
  void go(bool by_near);
};

// The context of a state fighting a target (vtable 0x005eec50).
class WalkFightPm : public WalkPm {
 public:
  WalkFightPm(WalkPmHooks& h, SteerPm& steer, zgame::Handle vehicle, FightStatePm& state);  // 0x004d99a0

  void measure() override;            // vt+0x10  0x004d9a90
  void measure_target();              // 0x004d99e0
  void decide(const FightStatePm& s);  // 0x004d9b90
  void take_order_target();           // 0x004daa70's own part (vtable 0x005eecac)

  FightStatePm& state;                // +0x50
  const float* target_pos = nullptr;  // +0x54
  float to_target[3] = {};            // +0x58
  float target_dist = 0;              // +0x64
  uint8_t in_range = 0;               // +0x68
  uint8_t at_stop = 0;                // +0x69
  uint8_t no_target = 0;              // +0x6a
  float way_own[3] = {};              // +0x6c
  const float* way_point = nullptr;   // +0x78
  float way_dir[3] = {};              // +0x7c
  float way_dist = 0;                 // +0x88
  uint8_t may_go = 0;                 // +0x8c
  uint8_t off_way = 0;                // +0x8d
};

// The context of the guarding state 0x004dffb0 (vtables 0x005ef2a0, 0x005ef30c): the order's target
// and how far it is from here and from the way's start.
class WalkGuardPm : public WalkPm {
 public:
  WalkGuardPm(WalkPmHooks& h, SteerPm& steer, zgame::Handle vehicle);  // 0x004dfb50
  void measure() override;            // vt+0x10  0x004dfc30

  zgame::Handle target = 0;           // +0x50
  const float* target_pos = nullptr;  // +0x54
  uint8_t gone = 0;                   // +0x58  no target, or it should leave it
  float to_target[3] = {};            // +0x5c
  float target_dist = 0;              // +0x68
  float from_way = 0;                 // +0x6c
  uint8_t x70 = 0, x71 = 0, x72 = 0;  // +0x70..+0x72
};

}  // namespace mech3::ai

#endif
