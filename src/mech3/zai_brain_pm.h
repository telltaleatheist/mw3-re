#ifndef MECH3_ZAI_BRAIN_PM_H
#define MECH3_ZAI_BRAIN_PM_H

// Pirate's Moon's brain: its frame and what it calls, MW3's Brain::frame rewritten (docs/pm-tests.md, The brain
// and the tick). Nothing runs it until the orders' switch.
#include <cstdint>
#include <deque>
#include <list>

#include "gamez/zgame/unported.h"

namespace mech3::ai {

// What the frame reads and writes on the brain.
struct BrainTickPm {
  float sight = 0;           // +0x1c  squared
  float engage = 0;          // +0x20  squared
  float leash = 0;           // +0x24  squared, from `home`
  int32_t mode = 0;          // +0x3c
  float health_floor = 0;    // +0x50
  int32_t delay_on = 0;      // +0x5c
  float wait_low = 0;        // +0x64
  float wait_high = 0;       // +0x68
  int32_t awake = 0;         // +0x88  needs no one in sight
  int32_t by_player = 0;     // +0x8c  sees by its +0x98 contact, not the nearest enemy
  int32_t aims = 0;          // +0x98  scan_aim ( 1 1 ): out of reach, the pilot manoeuvres
  int32_t reacts = 0;        // +0x9c
  int32_t wanders = 0;       // +0xa0
  int32_t manoeuvre_kind = 0;  // +0xa4
  float aim_gap = 0;         // +0xa8
  float react_gap = 0;       // +0xb0
  float delay_until = 0;     // +0xfc
  int32_t busy = 0;          // +0x110
  float wait_until = 0;      // +0x114
  uint8_t engage_flags = 0;  // +0xec  1 the leash, 2 the attack's time, 4 the health floor, 8 the give-up health
  const float* home = nullptr;  // +0x104
  float attack_until = 0;    // +0x108
  float aim_at = 0;          // +0x11c
  float react_at = 0;        // +0x120
  float give_up = 0;         // +0x1f0
  uint8_t runs_down = 0;     // +0x2c4  keeps going powered down
  bool radio = false;        // +0x2c8
};

class TickPmHooks {
 public:
  virtual ~TickPmHooks() = default;
  virtual bool ai_on() = 0;                                  // [0x00630055]
  virtual float now() = 0;                                   // [0x0062da90]
  virtual void stop() = 0;                                   // the controls' vt+0x14
  virtual void smooth(float dt) = 0;                         // the brain's vt+0x10
  virtual int32_t power_state(zgame::Handle v) = 0;          // vt+0xac
  virtual void steer_update(float dt, zgame::Handle v) = 0;  // +0x210's vt+0x04
  virtual zgame::Handle pick(zgame::Handle v) = 0;           // 0x004e7010(1)
  virtual zgame::Handle player_contact(zgame::Handle v) = 0;  // +0x98
  virtual zgame::Handle target_contact(zgame::Handle v) = 0;  // +0x6c
  virtual float dist2(zgame::Handle c) = 0;                   // the contact's +0x0c
  virtual zgame::Handle contact_vehicle(zgame::Handle c) = 0;  // +0x04, vt+0x2c, vt+0x48
  virtual float range2(zgame::Handle v) = 0;                  // +0xa4
  virtual void count_awake() = 0;                             // [0x0062db48]
  virtual bool has_manager(zgame::Handle v) = 0;              // +0xd4
  virtual double ammo(zgame::Handle v) = 0;                   // 0x004b8f50, unrounded
  virtual void radio_line(int32_t slot) = 0;                  // 0x004f8ae0(radio's +slot, 1.0)
  virtual void vlist(zgame::Handle v, bool* picked, zgame::Handle* enemy) = 0;  // 0x004fa9e0
  virtual bool hit_lately() = 0;                              // the controls' vt+0x98
  virtual bool may_fire() = 0;                                // 0x004fa6d0
  virtual void firing_reset() = 0;                            // 0x004faa20
  virtual zgame::Handle worst_attacker() = 0;                 // 0x004b2820 on the controls' +0x04
  virtual void take_attacker(zgame::Handle a) = 0;            // the brain's vt+0x28
  virtual void manoeuvre() = 0;                               // 0x004fab30
  virtual void current(zgame::Handle* net, zgame::Handle* state) = 0;  // 0x004fbf30: its +0x0c, +0x10
  virtual bool may_engage(zgame::Handle v, zgame::Handle net, zgame::Handle state) = 0;  // 0x004fa5a0
  virtual zgame::Handle target_vehicle(zgame::Handle c) = 0;  // 0x004f8d70, vt+0x48
  virtual zgame::Handle vehicle_net(zgame::Handle v) = 0;     // vt+0xe4
  virtual zgame::Handle attack_behaviour(zgame::Handle net) = 0;  // 0x004c7950
  virtual void attack(zgame::Handle net, zgame::Handle behaviour) = 0;  // 0x004fa150
  virtual int32_t rand_int() = 0;
  virtual void run_orders(float dt) = 0;                      // 0x004fb260
  virtual void firing(float dt) = 0;                          // 0x004fb3d0
};

bool tick_pm(TickPmHooks& h, BrainTickPm& b, zgame::Handle vehicle, float dt);  // 0x004fab90

// What the brain's smaller pieces call.
class EngagePmHooks {
 public:
  virtual ~EngagePmHooks() = default;
  virtual float now() = 0;                                     // [0x0062da90]
  virtual void position(zgame::Handle v, float out[3]) = 0;    // 0x005cb260
  virtual double health(zgame::Handle v) = 0;                  // vt+0xa4
};

class BrainSidePmHooks : public EngagePmHooks {
 public:
  virtual float clock() = 0;                                   // [0x008c1b50]
  virtual bool net_allows(zgame::Handle net) = 0;              // vt+0x0c
  virtual bool state_allows(zgame::Handle state) = 0;          // vt+0x04
  virtual void push_behaviour(int32_t which) = 0;              // 0x004fa170 of [0x00627cf8], [0x00627ce8]
  virtual uint32_t allies(zgame::Handle v) = 0;                // +0xc4
  virtual uint32_t team(zgame::Handle attacker) = 0;           // vt+0x28's +0xc0
  virtual zgame::Handle attacker_vehicle(zgame::Handle a) = 0;  // vt+0x58
  virtual zgame::Handle player_vehicle() = 0;                  // [0x0062c40c]'s +0x14
  virtual zgame::Handle player_contact(zgame::Handle v) = 0;   // +0x98
  virtual float dist2(zgame::Handle c) = 0;                    // +0x0c
  virtual void radio_line(int32_t slot) = 0;                   // 0x004f8ae0(radio's +slot, 1.0), radio or not
  virtual const float* hit_record(zgame::Handle v) = 0;        // 0x004b2900(v, 1) on the controls' +0x04
  virtual void allies_turn() = 0;                              // 0x004e6240
  virtual void hit_base(int32_t a, int32_t b, int32_t c, zgame::Handle attacker) = 0;  // 0x004b2a00
  virtual void died_base() = 0;                                // 0x004b2ab0
  virtual double speed_at(zgame::Handle v, float t) = 0;       // vt+0x124
  virtual bool in_world(zgame::Handle v) = 0;                  // +0x04
  virtual void set_way(const float* from, const float* to, float speed, zgame::Handle v) = 0;  // +0x210's vt+0x00
};

bool may_engage_pm(BrainSidePmHooks& h, const BrainTickPm& b, zgame::Handle v, zgame::Handle net, zgame::Handle state);  // 0x004fa5a0
bool must_leave_pm(EngagePmHooks& h, const BrainTickPm& b, zgame::Handle v);  // 0x004fa210
void manoeuvre_pm(BrainSidePmHooks& h, BrainTickPm& b);  // 0x004fab30
// 0x004e8d40, the controls' vt+0x78 (MW3's 0x004d8930): the step can't hit from here. Busy: true. With +0x98 on and the
// time past +0x11c, the next try +0xa8 seconds on and a manoeuvre: true. `now` is [0x0062da90].
bool scan_aim_pm(BrainSidePmHooks& h, BrainTickPm& b, float now);
void on_hit_pm(BrainSidePmHooks& h, const BrainTickPm& b, zgame::Handle owner, int32_t x, int32_t y, int32_t z,
               zgame::Handle attacker);  // 0x004fbc20
void died_pm(BrainSidePmHooks& h, const BrainTickPm& b);  // 0x004fbdd0
// 0x004fbe90: the way laid afresh from where it stands; `spot` the brain's +0x08, `from` its +0x2b8.
void way_reset_pm(BrainSidePmHooks& h, zgame::Handle v, float throttle, const float spot[3], float from[3]);

// An order (vtable 0x005f05b8, 0x84 bytes): what it runs is its net and state. The exe's deque, blocks of 31, is
// the STL's: 0x004fbf30 back(), 0x004faff0 push_back, 0x004ea220 pop_back.
struct OrderPm {
  uint8_t ready = 1;        // +0x04  the pending list may run on it
  uint8_t x5 = 0;           // +0x05
  zgame::Handle net = 0;    // +0x0c
  zgame::Handle state = 0;  // +0x10
};

// The brain's +0x128.
struct OrdersPm {
  std::deque<OrderPm> orders;      // +0x14..+0x34
  uint8_t again = 0;               // +0x30 of the object, the brain's +0x158: run the last order once more
  std::list<zgame::Handle> pending;  // +0x38, +0x3c
};

class OrdersPmHooks {
 public:
  virtual ~OrdersPmHooks() = default;
  virtual bool may_fire(zgame::Handle state) = 0;      // vt+0x08
  virtual bool net_idle(zgame::Handle net) = 0;        // vt+0x14
  virtual bool state_idle(zgame::Handle state) = 0;    // vt+0x20
  virtual zgame::Handle as_net(zgame::Handle obj) = 0;  // vt+0x34
  virtual zgame::Handle make_net(zgame::Handle net) = 0;      // vt+0x54 on the order's +0x14
  virtual zgame::Handle make_state(zgame::Handle state) = 0;  // vt+0x8c on the order's +0x34
  virtual zgame::Handle default_net() = 0;             // 0x00627c68
  virtual zgame::Handle default_state() = 0;           // 0x00627cc8
  virtual zgame::Handle stand_state() = 0;             // 0x0062faf8
  virtual void free_part(zgame::Handle part) = 0;      // vt+0x00(0): they sit in the order
  virtual void resume(zgame::Handle state) = 0;        // vt+0x10
  virtual void stop_state(zgame::Handle state) = 0;    // vt+0x0c
  virtual zgame::Handle net_of(zgame::Handle net) = 0;  // vt+0x18
  virtual void run_order(OrderPm& o, float dt) = 0;    // vt+0x04 0x004f9f30
  virtual void run_pending(zgame::Handle item) = 0;    // 0x004ea0e0, then 0x004ea0b0
};

// The scan state (vtable 0x005ee28c, made by 0x004d59c0, MW3's ScanState): each test, then on to the next step at once.
class ScanPmHooks {
 public:
  virtual ~ScanPmHooks() = default;
  virtual void scan_test(int32_t test) = 0;              // the entry's function on the brain plus its +0x04
  virtual void set_behaviour(zgame::Handle next) = 0;    // 0x004f9f00: this state goes
};
void scan_run_pm(ScanPmHooks& h, OrdersPm& o, int32_t tests, zgame::Handle next);  // 0x004d5a10

// The scripted steps' states, MW3's with PM's offsets: speed (vtable 0x005eddac, MW3's SpeedState), the two waits
// (0x005ede8c wait_interval, 0x005edf6c wait_time; WaitState), and valve, shunt and valve union (0x005ee04c,
// 0x005ee86c, 0x005eea2c; PassState) and valve_assign (0x005eeb0c; AssignState). All yes to vt+0x04, +0x08, +0x20.
class OrderStepPmHooks {
 public:
  virtual ~OrderStepPmHooks() = default;
  virtual void set_max_throttle(uint32_t bits) = 0;               // the brain's +0x10c (MW3's +0xec)
  virtual zgame::Handle path_next(zgame::Handle step) = 0;        // 0x004d3120 on the brain's +0x1f8
  virtual void valve_set(zgame::Handle valve, int32_t value) = 0;  // 0x004e40d0
  virtual void set_behaviour(zgame::Handle next) = 0;             // 0x004f9f00: this state goes
};
void speed_run_pm(OrderStepPmHooks& h, OrdersPm& o, uint32_t value, zgame::Handle next);  // 0x004d48b0
// 0x004d4a20 (wait_interval: the step's seconds after the state's start) and 0x004d4b80 (wait_time: the game time past
// the step's), both on only strictly past it (a NaN never), the sum at the x87's precision.
bool wait_due_pm(float start, float value, float now, bool absolute);
void wait_run_pm(OrderStepPmHooks& h, float start, float value, float now, bool absolute, zgame::Handle next);
void pass_run_pm(OrderStepPmHooks& h, OrdersPm& o, zgame::Handle step, zgame::Handle next);  // 0x004d6ec0
void assign_run_pm(OrderStepPmHooks& h, OrdersPm& o, zgame::Handle step, zgame::Handle valve, int32_t value,
                   zgame::Handle next);  // 0x004d7010

// A vehicle's net (0x004c6f90, vtable 0x005ed448): ten slots in its +0x0c, each a behaviour's object. The kinds an
// order asks by, and each slot's class by its vtable, are the exe's.
inline constexpr uint32_t kVehicleSlotsPm[10] = {0x005ed38c, 0x005ed2d0, 0x005ed214, 0x005ed158, 0x005df820,
                                                 0x005ed09c, 0x005ecfe0, 0x005ecf24, 0x005ece68, 0x005ecdac};
class NetSlotsPmHooks {
 public:
  virtual ~NetSlotsPmHooks() = default;
  virtual zgame::Handle slot(int32_t i) = 0;                      // the net's +0x0c, [i]
  virtual zgame::Handle step_of(zgame::Handle s, int32_t off) = 0;  // vt+0x60, vt+0x74, then vt+off
};
zgame::Handle kind_behaviour_pm(NetSlotsPmHooks& h, int32_t kind);  // 0x004c7800
zgame::Handle follow_pm(NetSlotsPmHooks& h);      // 0x004c7920
zgame::Handle attack_pm(NetSlotsPmHooks& h);      // 0x004c7950
zgame::Handle net_slot5_pm(NetSlotsPmHooks& h);       // 0x004c7980

// Where to aim at a contact, 0x004e7210 (MW3's 0x004d6de0, `lead_point`): a mech shooting leads by its smoothed view
// of the target's velocity (Mech's +0x100, aim_part), and with no lead to work out the point moves by that view's
// error, where MW3's stays put.
class LeadPmHooks {
 public:
  virtual ~LeadPmHooks() = default;
  virtual const float* velocity(zgame::Handle e) = 0;        // vt+0x0c
  virtual const float* smoothed_velocity(zgame::Handle v) = 0;  // vt+0x58's +0x100, none without a mech
  virtual void aim_point(zgame::Handle e, float out[3]) = 0;  // 0x005cb2b0
  virtual uint32_t weapon_flags(zgame::Handle w) = 0;        // its +0x30's +0x6c
  virtual float weapon_speed(zgame::Handle w) = 0;           // its +0x30's +0x2c
  virtual const float* muzzle(zgame::Handle w) = 0;          // 0x004b76d0
};
bool lead_point_pm(LeadPmHooks& h, zgame::Handle self, zgame::Handle entity, zgame::Handle weapon, float out[3]);

// What's set on a target change, 0x004e6330 (MW3's 0x004d5e40, `AiTarget::set_target`).
struct TargetsPm {
  zgame::Handle target = 0;     // +0x6c
  zgame::Handle ally = 0;       // +0x70
  zgame::Handle enemy = 0;      // +0x74
  zgame::Handle marked = 0;     // +0x78  new in PM: the last ally with +0xbc bit 4, while it has no bit 5 itself
  uint32_t flags = 0;           // +0xbc
  uint32_t allies = 0;          // +0xc4
  uint32_t enemies = 0;         // +0xc8
};
class TargetsPmHooks {
 public:
  virtual ~TargetsPmHooks() = default;
  virtual uint32_t team_of(zgame::Handle contact) = 0;   // 0x004f8d70, vt+0x28's +0xc0
  virtual uint32_t flags_of(zgame::Handle contact) = 0;  // 0x004f8d70's +0xbc
};
void set_target_pm(TargetsPmHooks& h, TargetsPm& t, zgame::Handle contact, bool keep_sides);

bool may_fire_pm(OrdersPmHooks& h, OrdersPm& o);     // 0x004fa6d0
bool orders_idle_pm(OrdersPmHooks& h, OrdersPm& o);  // 0x004fa470
void order_parts_pm(OrdersPmHooks& h, OrderPm& o, zgame::Handle net, zgame::Handle state);  // 0x004f9e70
void pop_order_pm(OrdersPmHooks& h, OrdersPm& o);    // 0x004ea220 with the order's destructor 0x004f9da0
bool replace_orders_pm(OrdersPmHooks& h, OrdersPm& o, zgame::Handle obj);  // 0x004fa860
void run_orders_pm(OrdersPmHooks& h, OrdersPm& o, float dt);  // 0x004fb260
void stop_orders_pm(OrdersPmHooks& h, OrdersPm& o);           // 0x004ea5f0
void give_order_pm(OrdersPmHooks& h, OrdersPm& o, zgame::Handle net, zgame::Handle behaviour);  // 0x004ea740
void push_behaviour_pm(OrdersPmHooks& h, OrdersPm& o, zgame::Handle behaviour);                 // 0x004ea7f0

// PM's lance orders (the orders the dispatcher 0x0046b4d0 picks, 0x004fb420..0x004fb7f0, on the brain's slots
// vt+0x2c..+0x54): what each calls, for the exe's order of calls. A contact is the order's object (its handle).
class LancePmHooks {
 public:
  virtual ~LancePmHooks() = default;
  virtual zgame::Handle own_vehicle() = 0;                        // the brain's +0x11c (through the virtual base)
  virtual zgame::Handle contact_vehicle(zgame::Handle c) = 0;     // 0x004f8d70
  virtual bool leavable() = 0;                                    // 0x004fa320
  virtual zgame::Handle vehicle_net(zgame::Handle v) = 0;         // vt+0xe4
  virtual zgame::Handle follow_behaviour(zgame::Handle net) = 0;  // 0x004c7920
  virtual void give(zgame::Handle net, zgame::Handle beh) = 0;    // 0x004fa150
  virtual zgame::Handle first_order() = 0;                        // 0x004fa1d0 (0x004eab10)
  virtual void pop_to(zgame::Handle first) = 0;                   // 0x004fa1b0 (0x004ea8d0)
  virtual int32_t give_net(zgame::Handle net) = 0;                // 0x004fa130 (0x004ea690), its low byte
  virtual zgame::Handle formation() = 0;                          // 0x00627c18
  virtual uint32_t team(zgame::Handle v) = 0;                     // vt+0x28's +0xc0
  virtual uint32_t allies(zgame::Handle own) = 0;                 // +0xc4
  virtual uint32_t enemies(zgame::Handle own) = 0;                // +0xc8
  virtual void keep(zgame::Handle target) = 0;                    // the brain's +0x124's +0x8c
  virtual int32_t refit() = 0;                                    // vt+0x48 (0x004fb850)
  virtual bool has_radio() = 0;                                   // +0x2c8
  virtual void radio_line(int32_t line) = 0;                      // 0x004f8ae0 on the radio's word `line`
};
int32_t lance_order_pm(LancePmHooks& h, int32_t order, zgame::Handle contact);

}  // namespace mech3::ai

#endif
