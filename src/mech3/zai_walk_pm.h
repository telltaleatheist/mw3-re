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
  virtual double speed_at(zgame::Handle v, float t) = 0;    // vt+0x124, unrounded in the register
  virtual double stop_distance(zgame::Handle v, float s) = 0;  // vt+0x12c
  virtual double stopping_at(zgame::Handle v, float t) { (void)v; (void)t; return 0.0; }  // vt+0x134
  virtual float x94() { return 0.0f; }                      // the brain's +0x94
  virtual void clear_mode() {}                              // the brain's +0x3c to 0
  virtual float range_far() = 0;                            // the brain's +0x28
  virtual float range_near() = 0;                           // the brain's +0x2c
  // What the order is about: the order's +0x0c, vt+0x1c, vt+0x18, vt+0x50 (the exe has no answer for none).
  virtual zgame::Handle order_target() = 0;
  virtual const float* target_place(zgame::Handle t) = 0;   // vt+0x04
  virtual const float* target_facing(zgame::Handle t) { (void)t; return nullptr; }  // +0x1c's vt+0x24, its +0x24; none: +x
  virtual bool target_live(zgame::Handle t) = 0;            // +0x1c's +0x08
  virtual bool must_leave(zgame::Handle v) = 0;             // 0x004fa210
  virtual void set_throttle(float v) = 0;                   // the controls' vt+0x20
  virtual void set_turn(float v) = 0;                       // vt+0x28
  // The boundary record's +0x50 and +0x4c: the via node the last pull ended on, or none.
  virtual const InOutVia* at_via(zgame::Handle v) = 0;
  virtual void queue_via(const InOutVia* via) = 0;          // 0x004ea0f0 on the brain's +0x15c, a 0x005eeca4 item
  // The move state 0x004dcfe0 and the guarding state's pick 0x004dfec0.
  virtual zgame::Handle next_along(const float dir[3]) { (void)dir; return 0; }  // 0x004d60d0, then vt+0x74
  virtual void set_behaviour(zgame::Handle b) { (void)b; }                       // 0x004f9f00
  // The terminus node's end (0x004e1fd0).
  virtual void set_again() {}    // 0x004fbe60(1): the brain's +0x158, the last order run once more
  virtual void order_ready() {}  // the order's +0x04
  virtual void return_back() {}  // 0x004ea180 on the brain's +0x15c with a new 0x005ef250: back to the order before
  virtual int32_t rand_int() { return 0; }                                       // rand
  virtual float reach() { return 0.0f; }                                         // the brain's +0x44
  // The manoeuvre 0x004d5040.
  virtual double jump_fuel(zgame::Handle v) { (void)v; return 0.0; }             // vt+0x154
  virtual bool airborne(zgame::Handle v) { (void)v; return false; }              // vt+0x150
  virtual float forward_speed(zgame::Handle v) { (void)v; return 0.0f; }         // +0x44
  virtual double move_speed(zgame::Handle v) { (void)v; return 0.0; }            // vt+0x120
  virtual void order_pop() {}                                                    // the order's vt+0x0c
  virtual zgame::Handle contact_pick(zgame::Handle v, int32_t kind) { (void)v, (void)kind; return 0; }  // 0x004e7010, its +4
  virtual bool hit_lately(zgame::Handle v) { (void)v; return false; }            // 0x004b27d0(1)
  virtual zgame::Handle attacker(zgame::Handle v) { (void)v; return 0; }         // 0x004b2820
  virtual bool lead_point(zgame::Handle v, zgame::Handle t, float out[3]) { (void)v, (void)t, (void)out; return false; }  // a pilot, +0x6c's +4, 0x0041e8b0
  virtual const float* entity_place(zgame::Handle t) { (void)t; return nullptr; }  // 0x005cb2b0
  virtual void set_fought(bool on) { (void)on; }                                 // the brain's +0x118
  virtual bool fought() { return false; }
  virtual uint8_t params_ec() { return 0; }                                      // the brain's +0xec
  virtual float ceiling() { return 0.0f; }                                       // the brain's +0x7c
  virtual void set_jump(float v) { (void)v; }                                    // the controls' vt+0x34
  virtual void set_jet_forward(float v) { (void)v; }                             // vt+0x3c
  virtual void stop_controls() {}                                                // vt+0x14
  virtual void manoeuvre_over() {}                                               // the brain's +0x110 down one
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

// The state run by 0x004dcfe0 (vtable 0x005eee40): driving down the steering's way, then on to the next behaviour.
struct MoveStatePm {
  int32_t step = 0;   // +0x0c  0 just begun, 1 driving, 2 waiting
  float turn = 0;     // +0x10
  float throttle = 0; // +0x18
  float dist = 0;     // +0x1c
  float slow = 0;     // +0x20
  float wake = 0;     // +0x24
  void start(WalkPmHooks& h, SteerPm& s, zgame::Handle v, const float* to);    // 0x004dcbe0
  void run(WalkPmHooks& h, SteerPm& s, zgame::Handle v, const float* place);  // 0x004dcfe0; `place` its +0x08's vt+0x1c
  // The backup node's run (0x004ddab0, vtable 0x005ef0dc; MW3's 0x004cfad0): 0 turns its back to the way, 1 backs
  // up it, 2 turns its front, then on as the run's end (3 the wait).
  void run_backup(WalkPmHooks& h, SteerPm& s, zgame::Handle v, const float* place);
  // The terminus node's run (0x004e1fd0, vtable 0x005ef634): driven there, then back to the order before.
  void run_terminus(WalkPmHooks& h, SteerPm& s, zgame::Handle v, const float* place);
};

// The manoeuvre state (0x004d5040, vtable 0x005ee150's twin of MW3's 0x004ca600): the brakes on, then a target to
// turn to, the jets while they last.
struct ManoeuvrePm {
  int32_t no_jets = 0;        // +0x08
  float jump = 0;             // +0x0c
  int32_t phase = 0;          // +0x10
  float turn = 0;             // +0x14
  float until = 0;            // +0x18
  zgame::Handle target = 0;   // +0x2c
  void run(WalkPmHooks& h, zgame::Handle v);  // 0x004d5040
  void leave(WalkPmHooks& h);                 // 0x004d4fa0, the destructor's work
};

// The guarding state's own part (0x004dfec0).
struct GuardPickPm {
  float at[3] = {};   // +0x10
  float rate = 0;     // +0x1c
  float reach = 0;    // +0x28
};

struct GuardStatePm : GuardPickPm {
  int32_t x08 = 0;          // +0x08  the first argument, kept
  int32_t step = 0;         // +0x0c  0 just begun, 1 back from the side state (see run), 2 walking, 3 pick again
  float far_way = 10.0f;    // +0x20  still going while further than this from the way's end
  float most = 0;           // +0x24  1.3 of the brain's +0x44
  void start(WalkPmHooks& h, zgame::Handle vehicle, int32_t arg);    // 0x004dfd00, 0x004dfe40
  void run(WalkPmHooks& h, SteerPm& s, zgame::Handle vehicle);       // 0x004dffb0
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

// The state 0x004d8690 (vtable 0x005eebc0): a place behind the order's target, by the way it faces, 0.9
// of the brain's +0x44 back; driven to while it stays within 20 m of the place picked.
struct BehindPm {
  int32_t step = 0;           // +0x0c  0 just begun, 1, 2 driving, 3 pick again
  float point[3] = {0, 0, 0}; // +0x10
  float turn = 0;             // +0x1c
  float throttle = 0;         // +0x20  the brain's +0x10c at first
  float speed = 0;            // +0x24
  float stop = 0;             // +0x28
  float inv_stop = 0;         // +0x2c
  float x34 = 0;              // +0x34  1.3 of the brain's +0x44
  float x38 = 0;              // +0x38  the brain's +0x2c
  void start(WalkPmHooks& h, zgame::Handle vehicle);                  // 0x004d8690, 0x004d8580
  void run(WalkPmHooks& h, SteerPm& s, zgame::Handle vehicle);        // 0x004d89d0
};

// The state 0x004d8dc0 (vtable 0x005eebe4): round the order's target at the brain's +0x44, on the side of +0x20's
// sign, turning back when the boundary stands in the way (docs/pm-tests.md, The states).
struct CirclePm {
  int32_t step = 0;         // +0x08
  float turn = 0;           // +0x0c
  int32_t x18 = 0;          // +0x18  the first argument, kept
  float throttle = 1.0f;    // +0x1c  the brain's +0x94, at most its +0x10c
  float side = 1.0f;        // +0x20  the sign the side, the size the time of the last change
  void start(WalkPmHooks& h, int32_t arg);                     // 0x004d8ca0
  void change_side(float now);                                 // 0x004d8d30, vt+0x10
  void run(WalkPmHooks& h, zgame::Handle vehicle);             // 0x004d8dc0
};

// PM's follow, the vehicle net's slot 0 (vtable 0x005ed38c, make_state 0x004d91e0): not MW3's straight
// line but a fighting state on the walk, vtable 0x005eecd4 (docs/pm-tests.md, PM's follow, found).
void follow_start(WalkPmHooks& h, SteerPm& s, zgame::Handle vehicle, FightStatePm& st);  // 0x004daaf0
void follow_run(WalkPmHooks& h, SteerPm& s, zgame::Handle vehicle, FightStatePm& st);    // 0x004dabf0

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

// The context of the attack state 0x004dedf0 (vtable 0x005ef1fc): the target is the state's held place, else the
// order's.
class WalkAttackPm : public WalkFightPm {
 public:
  WalkAttackPm(WalkPmHooks& h, SteerPm& steer, zgame::Handle vehicle, FightStatePm& state, const float* held, int32_t mode);  // 0x004debe0
};

// The context of the guarding state 0x004dffb0 (vtables 0x005ef2a0, 0x005ef30c): the order's target
// and how far it is from here and from the way's start.
class WalkGuardPm : public WalkPm {
 public:
  void pick(GuardPickPm& g);  // 0x004dfec0
  WalkGuardPm(WalkPmHooks& h, SteerPm& steer, zgame::Handle vehicle);  // 0x004dfb50
  void measure() override;            // vt+0x10  0x004dfc30

  zgame::Handle target = 0;           // +0x50
  const float* target_pos = nullptr;  // +0x54
  uint8_t gone = 0;                   // +0x58  no target, or it should leave it
  float to_target[3] = {};            // +0x5c
  float target_dist = 0;              // +0x68
  float from_way = 0;                 // +0x6c
  uint8_t x70 = 0, x71 = 0, x72 = 0;  // +0x70 the way is still good, +0x71 out from the target, +0x72 both
};

}  // namespace mech3::ai

#endif
