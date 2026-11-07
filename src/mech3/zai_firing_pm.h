#ifndef MECH3_ZAI_FIRING_PM_H
#define MECH3_ZAI_FIRING_PM_H

// Pirate's Moon's firing (docs/pm-map.md): the step 0x004bb700 keeps about a fifth of MW3's
// 0x004b5b80, so this is its own port. Nothing runs it until the orders' switch.
#include <cstdint>
#include <vector>

#include "gamez/zgame/unported.h"

namespace mech3::ai {

struct Contact;
class Brain;

// What the aim asks outside.
class AimPmHooks {
 public:
  virtual ~AimPmHooks() = default;
  virtual Contact* plan_contact(zgame::Handle plan) = 0;                // [[+4]]
  virtual const float* plan_position(zgame::Handle plan) = 0;           // vt+0x20
  virtual zgame::Handle contact_entity(const Contact* c) = 0;           // 0x004f8d70
  virtual bool entity_is_vehicle(zgame::Handle e) = 0;                  // vt+0x48
  virtual uint32_t entity_pilot_kind(zgame::Handle e) = 0;              // vt+0x58, its +0x6a8's +0x0c; 0 none
  virtual float difficulty_error() = 0;                                 // [0x0062cc64]
  virtual float pilot_error(Brain* b) = 0;                              // +0xb8
  virtual float pilot_aid(Brain* b) = 0;                                // +0xc0
  virtual bool targeting_aid(Brain* b) = 0;                             // the vehicle's +0x3f4 or +0x3c8 above 0
  virtual void vehicle_position(Brain* b, float out[3]) = 0;            // 0x005cb260 on the brain's vehicle
  virtual void weapon_line(zgame::Handle w, const float at[3], float muzzle[3], float dir[3]) = 0;  // 0x004b7800
  virtual int32_t rand() = 0;
  virtual float now() = 0;                                              // [0x0062da90]
  virtual void wm_aim_at(zgame::Handle wm, const float* at) = 0;        // 0x004b4a20
  virtual void wm_has_target(zgame::Handle wm, bool on) = 0;            // +0x8c, MW3's +0x88
};
AimPmHooks& aim_pm_hooks();
void set_aim_pm_hooks(AimPmHooks* h);

// 0x004013d0: made a unit in place, its length as stored, the exe's evaluation at 53 bits.
float unit3(float v[3]);
// 0x004f8dc0: the contact's distance and unit delta, worked out once.
float contact_distance(Contact& c);

// The aim's error (0x24 bytes), MW3's Aim with the error's line turned and the range held at 100
// (docs/pm-map.md, The firing's aim).
struct AimPm {
  float offset[3] = {0, 0, 0};       // +0x00
  float drift[3] = {1.0f, 0, 0};     // +0x0c
  bool active = false;               // +0x18
  float until = 0;                   // +0x1c
  float limit2 = 1.0f;               // +0x20

  bool frame(zgame::Handle plan, zgame::Handle w, Brain* b, float f[9], float muzzle[3], double* spread, float* dist);
  void pick_drift(const float f[9], float spread, float dist);      // 0x004baf40
  void track(zgame::Handle plan, zgame::Handle w, Brain* b);        // 0x004bb040
  void repick(zgame::Handle plan, zgame::Handle w, Brain* b, float at[3]);  // 0x004bb290
  void lead(float dt, zgame::Handle plan, zgame::Handle w, zgame::Handle wm, Brain* b);  // 0x004bb580
};

// The pilot's numbers the plan's pick reads.
struct PickPilot {
  int32_t pick_order = 0;   // +0xd4  whose threat first: 0, 1 the first list, 2, 3 the second, 4, 5 none, else no target
  int32_t part_mode = 0;    // +0xd8  0, 3 by the weights, 1 the weakest part, 2 the strongest
  float weights[4] = {};    // +0xdc  the torso; +0xe0 each leg; +0xe4 each arm; +0xe8 the head
  float repick = 0;         // +0xcc
  float range = 0;          // +0x30
  uint8_t filter[2] = {};   // +0xed, +0xee  what mustn't be left out
  uint32_t marker = 0;      // +0x2cc  the brain's own marker
};

// What the plan's pick asks outside.
class PickPmHooks {
 public:
  virtual ~PickPmHooks() = default;
  virtual std::vector<Contact*>& contacts(zgame::Handle v) = 0;         // +0xac
  virtual Contact** target_slot(zgame::Handle v) = 0;                   // +0x6c
  virtual uint32_t allies(zgame::Handle v) = 0;                         // +0xc4
  virtual const float* forward(zgame::Handle v) = 0;                    // +0x24
  virtual bool faces(zgame::Handle v, const float dir[3]) = 0;          // vt+0x98
  virtual bool in_sight(zgame::Handle v, zgame::Handle e) = 0;          // 0x004e5d00
  virtual bool select(zgame::Handle v, Contact* c) = 0;                 // vt+0xcc(c, 0)
  virtual void select_none(zgame::Handle v) = 0;                        // vt+0xcc(0, 0)
  virtual zgame::Handle contact_entity(const Contact* c) = 0;           // 0x004f8d70
  virtual zgame::Handle contact_mech(const Contact* c) = 0;             // 0x004f8d80
  virtual bool active(zgame::Handle e) = 0;                             // +0x08
  virtual uint32_t team(zgame::Handle e) = 0;                           // vt+0x28's +0xc0
  virtual uint8_t flags(zgame::Handle e) = 0;                           // +0xbc
  virtual zgame::Handle group(zgame::Handle e) = 0;                     // vt+0x68
  virtual uint32_t group_marker(zgame::Handle g) = 0;                   // vt+0xf8
  virtual uint32_t group_xe8(zgame::Handle g) = 0;                      // vt+0xe8
  virtual zgame::Handle vehicle(zgame::Handle e) = 0;                   // vt+0x48
  virtual zgame::Handle vehicle_x58(zgame::Handle m) = 0;               // vt+0x58
  virtual uint32_t vehicle_x100(zgame::Handle m) = 0;                   // vt+0x100
  virtual uint32_t entity_x50(zgame::Handle e) = 0;                     // vt+0x50
  virtual float health(zgame::Handle e) = 0;                            // vt+0xa4
  virtual zgame::Handle mech(zgame::Handle e) = 0;                      // vt+0x58
  virtual float heat(zgame::Handle m) = 0;                              // +0x578
  virtual zgame::Handle part(zgame::Handle m, int32_t offset) = 0;      // legs +0x39c, arms +0x3a4, torso +0x3ac, head +0x3b4
  virtual uint32_t part_child(zgame::Handle m, int32_t leg) = 0;        // 0x0041e290: the leg's calf, +0x74
  virtual float part_health(zgame::Handle p) = 0;                       // vt+0x48(0)
  virtual const float* position(zgame::Handle e) = 0;                   // 0x005cb260
  virtual zgame::Handle player() = 0;                                   // [0x0062c40c]+0x14
  virtual zgame::Handle threat(Brain* b, int32_t list) = 0;             // 0x004b2820, 0x004b2890 on +0x104
  virtual int32_t rand() = 0;
  virtual float now() = 0;
};
PickPmHooks& pick_pm_hooks();
void set_pick_pm_hooks(PickPmHooks* h);

// A contact offered (0x10 bytes). Lowest score first.
struct PickRec {
  Contact** slot = nullptr;
  float score = 0;
  uint8_t faces = 0, in_sight = 0;
};

// The plan 0x005eb990 (0x34 bytes): what the pick hands the step.
struct PlanPm {
  Contact** slot = nullptr;   // +0x04
  float score = 0;            // +0x08
  uint8_t faces = 0, in_sight = 0;  // +0x0c
  zgame::Handle vehicle = 0;  // +0x14
  uint32_t part = 0;          // +0x24
};

// The firing's fields the pick reads and keeps.
struct PickFiring {
  zgame::Handle vehicle = 0;   // [param]
  Brain* brain = nullptr;      // +0x6c
  PickPilot pilot;
  float reach = 0;             // +0x2c
  zgame::Handle aimed = 0;     // +0x28  the last aimed-at object: a candidate on its line is left alone
  bool aimed_up = false;       // its +0x08
  PlanPm* plan = nullptr;      // +0x14
  float until = 0;             // +0x4c
  zgame::Handle prefer = 0;    // +0x8c
  zgame::Handle prefer_next = 0;  // +0x90
  Contact* seen = nullptr;     // param's +0x0c
};
PlanPm* plan_pick(PickFiring& f);  // 0x004bc2e0

// What the shot asks outside. The shooter's numbers: 0x004bc930 reads them from the contact's owner.
struct ShotShooter {
  float heat = 0, health = 0;      // vt+0xa8, vt+0xa4
  bool vehicle = false;            // vt+0x58
  bool pilot = false;              // the brain's +0x124
  float heat_most = 0, rate_most = 0, cost_least = 0, health_past = 0, ammo_keep = 0;  // +0x38, +0x3c, +0x40, +0x44, +0x48
};
class ShotPmHooks {
 public:
  virtual ~ShotPmHooks() = default;
  virtual ShotShooter shooter(const Contact* c) = 0;
  virtual const std::vector<zgame::Handle>& weapons(zgame::Handle wm) = 0;  // +0x44, +0x28 of them
  virtual bool ready(zgame::Handle w) = 0;                              // 0x004b7bd0
  virtual bool can_hit(zgame::Handle w, const float* at) = 0;           // 0x004b8f80
  virtual double reach_least(zgame::Handle w) = 0;                      // 0x004b7b40
  virtual double reach_most(zgame::Handle w) = 0;                       // 0x004b7b90
  virtual float cost(zgame::Handle w) = 0;                              // +0x1c
  virtual float value(zgame::Handle w) = 0;                             // +0x18, MW3's rating
  virtual double dps(zgame::Handle w) = 0;                              // vt+0x1c, MW3's vt+0x10
  virtual int32_t ammo(zgame::Handle w) = 0;                            // vt+0x20
  virtual bool bin(zgame::Handle w, int32_t* full) = 0;                 // +0x5c, 0x004b8d30
  virtual void group_clear(zgame::Handle wm) = 0;                       // +0x30, 0x004b5dc0 each
  virtual void group_add(zgame::Handle wm, zgame::Handle w, float rate) = 0;  // 0x004b5d80, 0x004b7a90
  virtual void set_mode(zgame::Handle wm, int32_t mode) = 0;            // +0x1c
};
ShotPmHooks& shot_pm_hooks();
void set_shot_pm_hooks(ShotPmHooks* h);
// 0x004bcd40: which weapons fire. `k` is the brain's +0x34, `rate` its +0x124's +0x54.
bool shot_pick(zgame::Handle wm, const Contact* c, const float* at, float k, float rate);

}  // namespace mech3::ai

#endif
