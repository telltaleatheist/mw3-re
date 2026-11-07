#ifndef MECH3_ZAI_ATTACK_PM_H
#define MECH3_ZAI_ATTACK_PM_H

// Pirate's Moon's attack state, 0x004dedf0: the fight's state machine over the fighting state's walk, the jump
// and the return to the net. docs/pm-map.md, step 3. Nothing runs it until the orders' switch.
#include <cstdint>

#include "mech3/zai_walk_pm.h"

namespace mech3::ai {

// The brain's fields it keeps.
struct AttackBrainPm {
  int32_t mode = 0;           // +0x3c
  int32_t kind = 0;           // +0x40  the order kind it answers a lost target with (0x004c7800)
  float delay = 0;            // +0x4c
  float x58 = 0;              // +0x58
  int32_t x5c = 0, x60 = 0;   // +0x5c, +0x60
  float xb4 = 0;              // +0xb4
  float xfc = 0;              // +0xfc
  const float* place = nullptr;  // +0x104
  float x108 = 0;             // +0x108
  float x1f0 = 0;             // +0x1f0
};

struct AttackStatePm;

class AttackPmHooks {
 public:
  virtual ~AttackPmHooks() = default;
  virtual bool context(AttackStatePm& s) = 0;   // 0x004debe0: the walking context made, its no_target
  virtual void fight_step() = 0;                // 0x004d9d00
  virtual void fight_enter() = 0;               // 0x004d9be0
  virtual double speed() = 0;                   // the vehicle's vt+0xa4, unrounded in the register
  virtual float now() = 0;                      // [0x0062da90]
  virtual float intensity() = 0;                // [0x0062cc60]
  virtual const float* position() = 0;          // the context's +0x0c
  virtual uint32_t orders() = 0;                // the brain's +0x154
  virtual zgame::Handle prior_net() = 0;        // the order two from the back: its +0x0c, vt+0x18
  virtual zgame::Handle nearest(zgame::Handle net, const float* at) = 0;  // 0x004e5580, then vt+0x74
  virtual const float* place_of(zgame::Handle behaviour) = 0;  // vt+0x1c
  virtual zgame::Handle order_target() = 0;     // the order's +0x0c, vt+0x1c, vt+0x18, vt+0x50
  virtual zgame::Handle kind_behaviour(int32_t kind) = 0;  // 0x004c7800
  virtual void queue_after(zgame::Handle target, zgame::Handle behaviour) = 0;  // 0x004ea0f0, a 0x005eeca4
  virtual void return_to(zgame::Handle behaviour) = 0;  // 0x004ea180, a 0x005ef254, or with none a 0x005ef250
};

// What the attack leaves on the brain's list: run with the brain, between frames.
class PendingPmHooks {
 public:
  virtual ~PendingPmHooks() = default;
  virtual void pop_order() = 0;                       // 0x004ea220
  virtual void set_behaviour(zgame::Handle b) = 0;    // 0x004f9f00 on the order now last
  virtual void follow(zgame::Handle target, zgame::Handle b) = 0;  // 0x004ea740
};
struct PendingPm {
  enum Kind { kFollow, kBack, kBackTo } kind = kBack;  // vtables 0x005eeca4, 0x005ef250, 0x005ef254
  zgame::Handle target = 0;     // +0x04
  zgame::Handle behaviour = 0;  // +0x08, 0x005ef254's +0x04
  void run(PendingPmHooks& h);  // 0x004dedb0, 0x004e1fc0, 0x004eab40
};

// Past FightStatePm's.
struct AttackStatePm : FightStatePm {
  int32_t x38 = 0;                 // +0x38  the first argument, kept
  const float* hold_at = nullptr;  // +0x3c
  zgame::Handle hold_net = 0;      // +0x40
  float hold[3] = {};              // +0x44
  void start(WalkPmHooks& h, zgame::Handle vehicle, int32_t arg);  // 0x004decb0
  void step(AttackPmHooks& h, AttackBrainPm& b);  // 0x004dedf0
  void find_hold(AttackPmHooks& h);
};

}  // namespace mech3::ai

#endif
