#ifndef MECH3_ZAI_STEER_PM_H
#define MECH3_ZAI_STEER_PM_H

// Pirate's Moon's steering (the brain's +0x210): MW3's `Steer` (+0x250) with its constants made
// fields and its contact push rewritten; docs/pm-map.md, step 3. Nothing runs it until the
// orders' switch.
#include <cstdint>
#include <vector>

#include "gamez/zgame/unported.h"

namespace mech3::ai {

struct Contact;
class Brain;

// What it asks outside.
class SteerPmHooks {
 public:
  virtual ~SteerPmHooks() = default;
  virtual void position(zgame::Handle e, float out[3]) = 0;       // 0x005cb260
  virtual void forward(zgame::Handle v, float* x, float* z) = 0;  // +0x24, +0x2c
  virtual bool halted(zgame::Handle v) = 0;                       // vt+0x150
  virtual bool order_steers(Brain* b) = 0;                        // 0x004fa470
  virtual const std::vector<const Contact*>& contacts(zgame::Handle v) = 0;  // +0xac
  virtual zgame::Handle contact_entity(const Contact* c) = 0;     // 0x004f8d70
  virtual bool targetable(zgame::Handle e) = 0;                   // vt+0x48
  virtual float contact_dist2(const Contact* c) = 0;              // +0x0c
};

struct SteerPm {
  float start[3];       // +0x04  0x0066f5e0
  float dir[2];         // +0x10  x, z
  float along_min = 0;  // +0x18
  float length = 0;     // +0x1c
  float along_at = 0;   // +0x20  the carrot's distance along
  float across = 0;     // +0x24
  float across_to = 0;  // +0x28
  float rate = 0;       // +0x2c
  float ease;           // +0x30  0x0060bb84
  float along;          // +0x34
  float side;           // +0x38
  float near_dist;      // +0x3c  0x0060bb78
  float x40;            // +0x40  0x0062fb5c
  float keep_near;      // +0x44  0x0060bb88
  uint8_t keep = 1;     // +0x48  0x0060bb90
  float x4c;            // +0x4c  0x0060bb94
  float keep_slope;     // +0x50  0x0060bb98
  float keep_scale;     // +0x54  0x0060bb9c
  uint8_t push = 1;     // +0x58  0x0060bba0
  float push_reach;     // +0x5c  0x0060bba4
  float push_band;      // +0x60  0x0060bba8
  Brain* brain = nullptr;  // +0xa0
  int32_t sets = 0;     // +0x9c

  SteerPm();                                                                 // 0x004dc0f0
  bool near(SteerPmHooks& h) const;                                          // 0x004dc210
  bool at_end() const { return along_at <= along_min; }                      // 0x004dc270
  void carrot(float out[3]) const;                                           // 0x004dc290
  void on_line(const float p[3], float out[2]) const;                        // 0x004dc320
  void project(const float p[3], float out[2]) const;                        // 0x004dc980
  void advance(float d);                                                     // 0x004dc400
  void shift(float da, float ds);                                            // 0x004dc450
  bool push_amount(SteerPmHooks& h, const Contact* c, float* da, float* ds);  // 0x004dc480
  void set(SteerPmHooks& h, const float to[3], const float from[3], float rate, zgame::Handle v);  // 0x004dc7a0
  void measure(SteerPmHooks& h, zgame::Handle v);                            // 0x004dc9d0
  void update(SteerPmHooks& h, float dt, zgame::Handle v);                   // 0x004dcb30
};

}  // namespace mech3::ai

#endif
