#ifndef MECH3_ZAI_FIRING_PM_H
#define MECH3_ZAI_FIRING_PM_H

// Pirate's Moon's firing (docs/pm-map.md): the step 0x004bb700 keeps about a fifth of MW3's
// 0x004b5b80, so this is its own port. Nothing runs it until the orders' switch.
#include <cstdint>

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

}  // namespace mech3::ai

#endif
