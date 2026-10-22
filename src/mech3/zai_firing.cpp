#include "mech3/zai_firing.h"

#include <cmath>

#include "mech3/zai_brain.h"
#include "mech3/zai_target.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3::ai {

namespace {
bool unordered_or_less(float a, float b) { return !(a >= b); }  // x87 C0 after fcomp a, b

// [0x0060b2c8] (0x004b5860(10, 30, 10, 200) at start-up): the cone a friend
// must be in to block a shot: past 30 m it narrows from cos(atan(10/30)) to
// cos(atan(10/200)) over 170 m.
struct LineCone {
  float near_x, far_x, near_d, far_d, span, inv_span, near_cos, far_cos, cos_span;
};
const LineCone& line_cone() {
  static const LineCone c = [] {
    LineCone k{};
    k.near_x = 10, k.far_x = 10, k.near_d = 30, k.far_d = 200;
    k.span = k.far_d - k.near_d;
    k.inv_span = 1.0f / k.span;
    k.near_cos = k.near_d / static_cast<float>(std::hypot(static_cast<double>(k.near_x), static_cast<double>(k.near_d)));
    k.far_cos = k.far_d / static_cast<float>(std::hypot(static_cast<double>(k.far_x), static_cast<double>(k.far_d)));
    k.cos_span = k.far_cos - k.near_cos;
    return k;
  }();
  return c;
}
}  // namespace

// Mech3.exe 1.2 @ 0x004b6570, with the look-around 0x004b6390 and the aim
// 0x004b7060 (`new(0x24)`).
Firing::Firing() {
  aim = new Aim;
  brain_hooks().on_new(0x24, aim);
}

// Mech3.exe 1.2 @ 0x004b65f0. The brain's IControls vt+0x10(0) it calls
// first is 0x004a5bc0 by then (does nothing); the plan is freed when it is
// not +0x30, then +0x30, +0x50 and the aim.
Firing::~Firing() {
  free_plan();
  delete first;
  delete second;
  first = second = nullptr;
  if (aim) {
    brain_hooks().on_delete(aim);
    delete aim;
    aim = nullptr;
  }
}

// Mech3.exe 1.2 @ 0x004b63f0: behind, then left for 2.5 s, then right, 5 s
// each, then again.
void LookAround::sweep() {
  const float now = brain_hooks().now();
  switch (phase) {
    case 1:
      until = now - -5.0f;  // 0x005c93b8
      dir[0] = 0, dir[1] = 0, dir[2] = -1.0f;  // 0x005c9378
      break;
    case 2:
      if (unordered_or_less(now, until)) return;
      until = now - -2.5f;  // 0x005c93bc
      dir[0] = -1.0f, dir[1] = 0, dir[2] = 0;  // 0x005c9388
      break;
    case 3:
      if (unordered_or_less(now, until)) return;
      until = now - -5.0f;
      dir[0] = 1.0f, dir[1] = 0, dir[2] = 0;  // 0x005eda60
      break;
    case 4:
      if (!unordered_or_less(now, until)) phase = 1;
      return;
    default: return;
  }
  ++phase;  // after its vt+0x04 on the way (0x004d6dc0: nothing)
}

// Mech3.exe 1.2 @ 0x004b6690: the trigger let go, the rest until now + wait,
// the plan freed (unless it is the first source's), the aim cleared.
void Firing::drop(float wait) {
  BrainHooks& h = brain_hooks();
  brain->controls->fire(false);
  time = h.now() + wait;
  free_plan();
  h.wm_clear_aim(h.weapons(brain->controls->vehicle()));
}

// A first source may hand itself out as the plan (+0x14 == +0x30): kept.
void Firing::free_plan() {
  if (plan && (!first || dynamic_cast<void*>(plan) != dynamic_cast<void*>(first))) delete plan;
  plan = nullptr;
}

// Mech3.exe 1.2 @ 0x004b6700 (from the brain, 0x004e9590)
void Firing::stand_down() {
  drop(rest);
  brain->xf8 = 0;  // its vt+0x14, 0x004e95a0
  phase = 5;
}

// Mech3.exe 1.2 @ 0x004b5940 over the vehicle's contacts (0x004b5fc6): the
// nearest in a cone around the line to the plan's target, past 30 m a
// narrower one. A clear line: none, or the target itself.
bool Firing::clear_line(const FirePlan& p) {
  BrainHooks& h = brain_hooks();
  Contact* target = *p.target;
  const std::vector<Contact*>* cs = h.contacts(brain->controls->vehicle());
  if (!cs) return true;
  const LineCone& k = line_cone();
  for (Contact* c : *cs) {
    if (c == target) return true;
    const float d = c->distance() - k.near_d;  // 0x004e7030
    float t;
    if (0.0f < d && d < k.span) t = d * k.inv_span;
    else t = 0.0f < d ? 0.0f : 1.0f;  // 0x005c93b0 / 0x005c93b4
    const float cone = t * k.cos_span + k.near_cos;
    c->distance();
    target->distance();
    const float dot = (target->dir[0] * c->dir[0] + target->dir[1] * c->dir[1]) + target->dir[2] * c->dir[2];
    if (cone < dot) return false;
  }
  return true;
}

// Mech3.exe 1.2 @ 0x004b5b80 (docs/ai.md 17.7). Run while a step asks for
// another; nothing without a weapon manager.
void Firing::run(float dt) {
  BrainHooks& h = brain_hooks();
  Brain& b = *brain;
  BrainControls& c = *b.controls;
  const zgame::Handle v = c.vehicle();
  const zgame::Handle wm = h.weapons(v);
  if (!wm) return;
  AiTarget* t = h.targetable(v);
  auto target = [&]() -> Contact* { return t ? t->target : nullptr; };
  const float fire_wait = b.params.get<float>(0x38);  // the rest after a burst
  bool again;
  do {
    again = false;
    switch (phase) {
      case 8:
        range = t ? t->range : 0;
        look.start(this);
        phase = 1, again = true;
        break;
      case 1:
      case 6: {
        FirePlan* p = nullptr;
        bool spent = false;
        if (first) {
          p = first->take(&spent);
          if (spent) {
            delete first;
            first = nullptr;
          }
        }
        if (!p && second) p = second->take(&spent);
        if (!p) p = h.fire_plan_pick(b);  // 0x004b6790 (item 12b)
        delete second;
        second = nullptr;
        plan = p;
        if (!p) {
          drop(rest);
          b.xf8 = 0;
          phase = 5;
          break;
        }
        p->track(track_arg);
        phase = 2;
        const float now = h.now();
        if (p->quick) {
          time = quick_delay + now;
        } else {
          time = delay + now;
          c.aim_started();  // IControls vt+0x80
        }
        break;
      }
      case 2:
      case 3: {
        if (!target()) {
          drop(rest);
          b.xf8 = 0;
          phase = 5;
          break;
        }
        const bool on = plan->track(track_arg) && plan->in_reach(reach_arg) == 1 && (phase == 2 || plan->ready());
        if (on && phase == 2) {
          phase = 3;
          break;
        }
        if (on) {
          const zgame::Handle w = h.wm_current(wm);
          h.aim_track(b, target(), w);  // 0x004b72a0 (item 12b)
          if (plan->kind == 3) {
            h.aim_lead(b, dt, target(), w, wm);  // 0x004b77e0
            h.wm_alpha(wm, 0.2f);                 // 0x004b0a70
            drop(b.params.get<float>(0x38) + b.params.get<float>(0x34));
            b.xf8 = 0;
            phase = 5;
            break;
          }
          phase = 4;
          time = b.params.get<float>(0x34) + h.now();
          again = true;
          break;
        }
        if (unordered_or_less(h.now(), time) || h.now() == time) break;
        if (plan->in_reach(fire_arg) == 1 && !(h.vehicle_flags(v) & 0x10) && (phase == 2 || plan->ready())) {
          const zgame::Handle w = h.wm_current(wm);
          h.aim_track(b, target(), w);
          phase = 7;
          time = h.now() + b.params.get<float>(0xac);
          again = true;
          break;
        }
        drop(rest);
        b.xf8 = 0;
        phase = 5, again = true;
        break;
      }
      case 4:
      case 7: {
        // The burst over (0x004b6060, 0x004b61c8): the trigger let go, the
        // rest from now, the plan freed, the aim cleared.
        auto end_burst = [&](float wait) {
          c.fire(false);
          time = h.now() + wait;
          free_plan();
          h.wm_clear_aim(h.weapons(v));  // 0x004b0bc0(0)
          b.xf8 = 0;
          phase = 5;
        };
        if (!target()) {
          if (phase == 4) {
            drop(rest);
            b.xf8 = 0;
            phase = 5;
          } else {
            end_burst(rest);
          }
          break;
        }
        const zgame::Handle w = h.wm_current(wm);
        bool firing_on;
        if (phase == 4) {
          firing_on = h.weapon_type_ready(w) && unordered_or_less(h.now(), time - h.weapon_reload(w)) &&
                      h.weapon_ammo(w) != 0 && plan->track(fire_arg) == 1;
        } else {
          plan->track(track_arg);
          firing_on = !unordered_or_less(time, h.weapon_reload(w) + h.now());
        }
        if (!firing_on) {
          end_burst(fire_wait);
          break;
        }
        if (plan->in_reach(fire_arg) == 1 && plan->line_check && clear_line(*plan)) {
          h.aim_lead(b, dt, target(), w, wm);  // 0x004b77e0
          h.weapon_hold_for_flight(w);          // the weapon's type's 0x004b4c90
          c.fire(true);
        } else {
          c.fire(false);
        }
        break;
      }
      case 5: {
        const bool resting = unordered_or_less(h.now(), time);
        h.torso_centre(v);  // the mech's torso 0x00465a00(1)
        if (resting) {
          look.sweep();
          break;
        }
        phase = 6, again = true;
        break;
      }
      default: break;
    }
  } while (again);
}

}  // namespace mech3::ai
