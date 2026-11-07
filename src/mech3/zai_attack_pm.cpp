// Pirate's Moon's attack state (zai_attack_pm.h). The original file is unknown; it sits with the fighting states.
#include "mech3/zai_attack_pm.h"

#include <cfloat>

#pragma STDC FP_CONTRACT OFF

namespace mech3::ai {

namespace {
bool odd_step(uint32_t flags) {  // the fighting state's steps 1, 3, 5: it can't be walked from
  const uint32_t step = flags & 0xf;
  return step == 1 || step == 3 || step == 5;
}
}  // namespace

// The nearest behaviour of the order two from the back, when there is one.
void AttackStatePm::find_hold(AttackPmHooks& h) {
  hold_net = 0;
  hold_at = nullptr;
  if (h.orders() <= 1) return;
  if (const zgame::Handle net = h.prior_net()) hold_net = h.nearest(net, h.position());
  if (hold_net) hold_at = h.place_of(hold_net);
}

// Pirate's Moon 1.0 @ 0x004dedf0. The high nibble of the flags is the state's own step, the low one the fighting
// state's. (The exe's masks keep only bits it then sets, so each replaces the nibble.)
void AttackStatePm::step(AttackPmHooks& h, AttackBrainPm& b) {
  const bool no_target = h.context(*this);
  bool lost = false;
  switch (flags & 0xf0) {
    case 0x10:
      break;
    case 0x20:
      lost = true;
      break;
    case 0x30:
    case 0x40:
      if (!no_target && !odd_step(flags)) {
        h.fight_step();
        flags = (flags & ~0xf0u) | 0x30;
      } else {
        flags = (flags & ~0xf0u) | 0x50;
      }
      return;
    case 0x50:
      if (b.mode != 1) {
        b.x108 = FLT_MAX;
        b.x1f0 = 0.0f;
        b.place = nullptr;
        if (b.x60 == 0 && b.x5c == 0) {
          b.mode = 1;
          b.xfc = FLT_MAX;
        }
        h.return_to(hold_net);
        return;
      }
      [[fallthrough]];
    case 0: {
      flags = 0;
      h.fight_enter();
      b.x1f0 = static_cast<float>(h.speed() - static_cast<double>(b.xb4));
      b.x108 = static_cast<float>(static_cast<double>(h.intensity()) * b.delay + h.now());  // rounded once
      const float* at = h.position();
      find_hold(h);
      if (hold_net) at = hold_at;
      hold[0] = at[0], hold[1] = at[1], hold[2] = at[2];
      b.place = hold;
      hold_net = 0;
      hold_at = nullptr;
      break;
    }
    default:
      return;
  }
  if (!lost && !no_target) {
    if (!odd_step(flags)) {
      h.fight_step();
      flags = (flags & ~0xf0u) | 0x10;
      return;
    }
    const zgame::Handle target = h.order_target();
    if (const zgame::Handle to = h.kind_behaviour(b.kind)) {
      h.queue_after(target, to);
      flags = (flags & ~0xf0u) | 0x20;
      return;
    }
  }
  find_hold(h);
  range_near = 0.0f;
  range_mid = 0.0f;
  range_far = 20.0f;
  b.mode = 0;
  if (b.x5c != 0) b.xfc = h.now() + b.x58;
  flags = (flags & ~0xf0u) | 0x40;
}

}  // namespace mech3::ai
