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
  virtual uint32_t entity_pilot_kind(zgame::Handle e) = 0;              // vt+0x58, its +0x6a8's +0x0c: the roster id; 0 none
  virtual float difficulty_error() = 0;                                 // [0x0062cc64]
  virtual float pilot_error(Brain* b) = 0;                              // +0xb8
  virtual float pilot_aid(Brain* b) = 0;                                // +0xc0
  virtual bool targeting_aid(Brain* b) = 0;                             // the vehicle's +0x3f4 or +0x3c8 above 0
  virtual void vehicle_position(Brain* b, float out[3]) = 0;            // 0x005cb260 on the brain's vehicle
  virtual bool weapon_line(zgame::Handle w, const float at[3], float muzzle[3], float dir[3]) = 0;  // 0x004b7800: false, nothing written
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
  float dir[3] = {0, 0, 0};   // +0x18  to the aim, in the vehicle's frame
  float place[3] = {0, 0, 0}; // +0x28  the part's
  bool seen = false;          // +0x0d
  bool fired = false;         // +0x10
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
  virtual double dps(zgame::Handle w) = 0;                              // vt+0x1c: not damage, a shot's heat
  virtual int32_t ammo(zgame::Handle w) = 0;                            // vt+0x20
  virtual bool bin(zgame::Handle w, int32_t* full) = 0;                 // +0x5c, 0x004b8d30
  virtual void group_clear(zgame::Handle wm) = 0;                       // +0x30, 0x004b5dc0 each
  virtual void group_add(zgame::Handle wm, zgame::Handle w, float rate) = 0;  // 0x004b5d80, 0x004b7a90
  virtual void set_mode(zgame::Handle wm, int32_t mode) = 0;            // +0x1c
};
ShotPmHooks& shot_pm_hooks();
void set_shot_pm_hooks(ShotPmHooks* h);
// 0x004bcd40, PM's for MW3's volley_pick 0x004b1860: which weapons fire. `hold` is the brain's +0x34,
// the burst's seconds; `rate` its +0x124's +0x54.
bool shot_pick(zgame::Handle wm, const Contact* c, const float* at, float hold, float rate);

// What the plan's virtuals and the weapon checks ask outside.
class PlanPmHooks {
 public:
  virtual ~PlanPmHooks() = default;
  virtual zgame::Handle contact_entity(const Contact* c) = 0;           // 0x004f8d70
  virtual zgame::Handle mech(zgame::Handle e) = 0;                      // vt+0x58
  virtual void refresh(zgame::Handle m) = 0;                            // 0x0041df20
  virtual zgame::Handle wm(zgame::Handle v) = 0;                        // +0xd4
  virtual zgame::Handle group_weapon(zgame::Handle wm) = 0;             // the group at +0x2c: its +0x10
  virtual bool aim_point(zgame::Handle v, Contact* c, zgame::Handle weapon, float out[3]) = 0;  // vt+0x90
  virtual void weapon_place(zgame::Handle weapon, float out[3]) = 0;    // 0x004b76d0
  virtual const float* position(zgame::Handle e) = 0;                   // 0x005cb260
  virtual const float* matrix(zgame::Handle v) = 0;                     // +0x0c
  virtual bool turn(zgame::Handle v, const float dir[3], float out[3]) = 0;  // vt+0x9c
  virtual void face(zgame::Handle v, const float dir[3]) = 0;           // vt+0x160
  virtual void wm_aim_at(zgame::Handle wm, const float at[3]) = 0;      // 0x004b4a20
  virtual const std::vector<zgame::Handle>& weapons(zgame::Handle wm) = 0;   // +0x44
  virtual const std::vector<zgame::Handle>& group(zgame::Handle g) = 0; // +0x14
  virtual bool weapon_cos(zgame::Handle w, zgame::Handle wm, float* cos) = 0;  // 0x004b7800 on the manager's +0x0c
  virtual bool in_sight(zgame::Handle v, zgame::Handle e) = 0;          // 0x004e5d00
  virtual void part_place(uint32_t part, float out[3]) = 0;             // 0x004ff250 on its +0x08
  virtual const float* entity_place(zgame::Handle e) = 0;               // 0x005cb2b0
  // A weapon's fields for 0x004b8f80 and 0x004b9d80.
  virtual bool has_type(zgame::Handle w) = 0;                           // +0x30
  virtual int32_t ammo(zgame::Handle w) = 0;                            // vt+0x20
  virtual float blocked(zgame::Handle w) = 0;                           // +0x74
  virtual double reach(zgame::Handle w) = 0;                            // 0x004b7b90
  virtual zgame::Handle holder_mech(zgame::Handle w) = 0;               // +0x9c's +0x88, vt+0x2c, vt+0x48
  virtual bool mounted(zgame::Handle w) = 0;                            // +0x28 and +0x84
  virtual zgame::Handle muzzle_node(zgame::Handle w) = 0;               // the mount's +0x08, else +0x24
  virtual const float* mount_place(zgame::Handle w) = 0;                // +0x84's +0x70
  virtual bool spreads(zgame::Handle w) = 0;                            // +0x6c and +0x84
  virtual float spread(zgame::Handle w) = 0;                            // +0x84's +0x60
  virtual const float* offset(zgame::Handle w) = 0;                     // +0x88
  virtual bool buried(zgame::Handle node, float at[3]) = 0;             // 0x004ff3c0: `at` turned to the world's, true without a node
  virtual bool has_mount(zgame::Handle w) = 0;                          // +0x84
  virtual bool mount_reaches(zgame::Handle w, const float dir[3]) = 0;  // 0x004a7000
  virtual bool mech_reaches(zgame::Handle m, const float dir[3]) = 0;   // vt+0x94
  virtual int32_t rand() = 0;
};
PlanPmHooks& plan_pm_hooks();
void set_plan_pm_hooks(PlanPmHooks* h);

void cone_table(float out[2]);               // the start-up's +0x18, +0x20, for the test
bool in_line(Contact& target, Contact& c);   // 0x004b9f00: `c` in the cone round the target
bool plan_turn(PlanPm& p, Contact* c, bool grouped, float cone);        // 0x004baae0
bool plan_cone(const PlanPm& p, Contact* c, zgame::Handle wm, bool grouped, zgame::Handle g, float cone);  // 0x004ba9e0
const float* plan_place(PlanPm& p, const Contact* c);                   // 0x004ba0d0
void plan_sight(PlanPm& p, const Contact* c);                           // 0x004baac0
zgame::Handle best_weapon(zgame::Handle wm);                            // 0x004b9de0
bool can_hit(zgame::Handle w, const float at[3]);                       // 0x004b8f80
bool any_can_hit(zgame::Handle wm, const float at[3]);                  // 0x004bd1c0
// A weapon's line, 0x004b7800: where it fires from, the way to a point, and how far off its part's facing.
struct WeaponLinePm {
  float damage = 0;                   // +0x74
  zgame::Handle part_node = 0;        // +0x28's +0x08, none without a part
  zgame::Handle node = 0;             // +0x24
  const float* fp_place = nullptr;    // the firepoint's +0x70, none without one
  float fp_radius = 0;                // its +0x60
  const float* fp_tolerance = nullptr;  // its +0x00, the cosine it must face within
  int32_t salvo_left = 0;             // +0x6c
  float spread[3] = {0, 0, 0};        // +0x88
};
class WeaponLinePmHooks {
 public:
  virtual ~WeaponLinePmHooks() = default;
  virtual bool node_place(zgame::Handle node, float io[3]) = 0;  // 0x004ff3c0: false when placed
  virtual const float* node_axis(zgame::Handle node) = 0;         // its +0x38's +0x78
  virtual int32_t rand() = 0;
  virtual bool quiet() = 0;                                       // [0x0062f86c]: no facing asked
};
bool weapon_muzzle(WeaponLinePmHooks& h, const WeaponLinePm& w, float muzzle[3]);  // 0x004b76d0

bool weapon_line(WeaponLinePmHooks& h, const WeaponLinePm& w, const float* at, float muzzle[3], float dir[3],
                 float* cos, float* cos_kept);  // 0x004b7800
bool firepoint_reaches(const float cone[18], const float dir[3]);       // 0x004a7000, a firepoint's aim cone
bool torso_reaches(float min_pitch, float max_pitch, float max_yaw, const float dir[3]);  // 0x00467490's answer

// What the step asks outside. A plan and its virtuals are handles here until the step is wired to
// PlanPm: vt+0x14 turn, vt+0x04 in the cone, vt+0x1c sight, vt+0x20 place.
class StepPmHooks {
 public:
  virtual ~StepPmHooks() = default;
  virtual zgame::Handle wm(zgame::Handle v) = 0;                         // +0xd4
  virtual zgame::Handle mech(zgame::Handle v) = 0;                       // vt+0x58
  virtual Contact* target(zgame::Handle v) = 0;                          // +0x6c
  virtual double heat(zgame::Handle m) = 0;                              // vt+0xa8
  virtual float coolant(zgame::Handle m) = 0;                            // +0x590
  virtual void flush(zgame::Handle m) = 0;                               // 0x0041ad50
  virtual float now() = 0;
  virtual float hold(Brain* b) = 0;                                      // +0x34
  virtual float rest(Brain* b) = 0;                                      // +0x38
  virtual float range(Brain* b) = 0;                                     // +0x30
  virtual float rate(Brain* b) = 0;                                      // +0x124's +0x54
  virtual uint32_t xd0(Brain* b) = 0;                                    // +0xd0
  // The attacker list (the brain's +0x104, through the base): its drop limit (0x005bfdc0, +0x10) and fade rate
  // (0x00434980, +0x0c), both one-line setters the linker shares with other classes.
  virtual void seed(Brain* b, uint32_t v) = 0;
  virtual void start_x50(Brain* b, uint32_t v) = 0;
  virtual void set_range(zgame::Handle v, float r) = 0;                  // 0x004e7150
  virtual void look_start(Brain* b) = 0;  // the firing's +0x70 (vtable 0x5eb970) vt+0x00, 0x004ba630: on (+0x18), the firing its owner (+0x04)
  virtual zgame::Handle best_weapon(zgame::Handle wm) = 0;               // 0x004b9de0
  virtual float weapon_reach(zgame::Handle w) = 0;                       // 0x004b7b50
  virtual zgame::Handle pick(zgame::Handle v) = 0;                       // 0x004bc2e0
  virtual void plan_free(zgame::Handle plan) = 0;
  virtual bool plan_turn(zgame::Handle plan, float cone) = 0;            // vt+0x14
  virtual bool plan_cone(zgame::Handle plan, float cone) = 0;            // vt+0x04
  virtual bool plan_sight(zgame::Handle plan) = 0;                       // vt+0x1c, +0x0d
  virtual bool plan_quick(zgame::Handle plan) = 0;                       // +0x0c
  virtual bool plan_seen(zgame::Handle plan) = 0;                        // +0x0d
  virtual Contact* plan_contact(zgame::Handle plan) = 0;                 // [[+4]]
  virtual const float* plan_place(zgame::Handle plan, float hold, float rate) = 0;  // vt+0x20
  virtual bool plan_fired(zgame::Handle plan) = 0;                       // +0x10
  virtual void plan_set_fired(zgame::Handle plan, bool on) = 0;
  virtual bool shot(zgame::Handle wm, Contact* c, const float* at, float hold, float rate) = 0;  // 0x004bcd40
  virtual bool any_can_hit(zgame::Handle plan) = 0;                      // 0x004bd1c0
  virtual void too_far(Brain* b) = 0;                                    // the IControls' vt+0x78
  virtual void trigger(Brain* b, bool on) = 0;                           // the IControls' vt+0x10
  virtual zgame::Handle group(zgame::Handle wm) = 0;                     // +0x2c, +0x30: none at -1
  virtual double group_soonest(zgame::Handle g) = 0;                     // 0x004b5bf0
  virtual bool clear_of_friends(zgame::Handle plan) = 0;                 // the contacts by 0x004b9f00: none in the way, or the target first
  virtual void aim_track(zgame::Handle plan, Brain* b) = 0;              // AimPm::track(plan, 0, b)
  virtual void aim_lead(float dt, zgame::Handle plan, zgame::Handle wm, Brain* b) = 0;  // AimPm::lead
  virtual void stand_down(zgame::Handle v) = 0;                          // vt+0x164(0, &o, 1), the speech 0x004677e0 when o's +0x3ac
  virtual void brain_x20(Brain* b) = 0;                                  // vt+0x20
};
StepPmHooks& step_pm_hooks();
void set_step_pm_hooks(StepPmHooks* h);

// The firing's state 0x004bc930's owner: the brain's +0x108.
struct FiringPm {
  zgame::Handle vehicle = 0;
  Brain* brain = nullptr;     // +0x6c
  int32_t phase = 8;          // +0x00
  float time = 0;             // +0x04
  float quick = 0, delay = 0; // +0x08, +0x0c
  float rest = 0;             // +0x10
  zgame::Handle plan = 0;     // +0x14
  float reach_arg = 0, track_arg = 0, fire_arg = 0;  // +0x18, +0x1c, +0x20
  float x24 = 0;              // +0x24
  float range = 0;            // +0x2c
  float wait = 0;             // +0x30
  bool busy = false;          // +0x34
  float too_hot = 0;          // +0x38
  float x3c = 0;              // +0x3c
  float x40 = 0;              // +0x40
  float x44 = 0, x48 = 0;     // +0x44, +0x48
  uint32_t x50 = 0;           // +0x50
  float x54 = 0;              // +0x54  what `rate` answers
  float cool_for = 0, cool_until = 0, cool_below = 0;  // +0x58, +0x5c, +0x60
  float shot_every = 0, shot_next = 0;  // +0x64, +0x68
  // +0x94: a one-byte object made with the firing (0x004ba750, 0x004be1a0) from [0x0060b70c], 1 in the exe's
  // data; nothing writes either, so an ally hit always keeps the plan (0x004ba8c0).
  bool keeps_plan = true;
  void seed();                // 0x004ba750's copy of the start-up's numbers (0x004ba660)
  void step(float dt);        // 0x004bb700
};

}  // namespace mech3::ai

#endif
