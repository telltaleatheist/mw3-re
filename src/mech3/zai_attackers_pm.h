#ifndef MECH3_ZAI_ATTACKERS_PM_H
#define MECH3_ZAI_ATTACKERS_PM_H

// Pirate's Moon's list of who hit a pilot (the AI controls' +0x04, made by 0x004b2950), where MW3's
// HitRecord holds one attacker. Each attacker's damage fades with time; one not heard from in
// `limit` seconds drops off.
#include <cstdint>
#include <list>

#include "gamez/zgame/unported.h"

namespace mech3::ai {

struct AttackerPm {
  float sum;                // +0x08  the damage, fading
  float last;               // +0x0c
  float first;              // +0x10
  zgame::Handle attacker;   // +0x14
};

class AttackersPm {
 public:
  AttackersPm(float rate, float limit, float clock);  // 0x004b2580
  static AttackersPm for_pilot(float clock);          // 0x004b2950: 0.3 a second, 5 s

  bool age(zgame::Handle attacker, float amount, float clock);    // 0x004b25c0: true when it found them
  void add(zgame::Handle attacker, float amount, float clock);    // 0x004b2740
  bool hit_within(float secs, float clock);                       // 0x004b27d0
  zgame::Handle hardest(float clock);                             // 0x004b2890: the most damage
  zgame::Handle latest(float clock);                              // 0x004b2820: the last to hit
  AttackerPm* find(zgame::Handle attacker, bool aged, float clock);  // 0x004b2900

  std::list<AttackerPm> list;  // +0x04, +0x08
  void set_rate(float r) { rate_ = r; }    // 0x00434980 on it (the firing step's start)
  void set_limit(float l) { limit_ = l; }  // 0x005bfdc0 on it
  float rate() const { return rate_; }
  float limit() const { return limit_; }

 private:
  float rate_;   // +0x0c
  float limit_;  // +0x10
  float stamp_;  // +0x14
};

}  // namespace mech3::ai

#endif
