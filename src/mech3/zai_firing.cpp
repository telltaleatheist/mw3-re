#include "mech3/zai_firing.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "crt/msvc_math.h"

#include "mech3/zai_brain.h"
#include "gamez/zmath/zmth_vector.h"
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

namespace {
// A candidate (16 bytes in the exe): where its contact is kept, its score
// (lower is better), whether the vehicle could face it and had a clear line.
struct Candidate {
  Contact* const* slot;
  float score = 0;
  uint8_t turn = 0, clear = 0;
};
// [0x0060b2b8]: the record a rejected candidate becomes (0x004b6aa0's
// template, its slot 0x0060b2f4).
Contact* const kNoTarget = nullptr;

// 0x004b6b30, the scorer over the candidates, nearest first: `on` goes off
// once one is out of range, rejecting the rest.
struct Scorer {
  zgame::Handle v;
  Firing& f;
  float range2;
  bool on = true;

  void operator()(Candidate& r) {
    BrainHooks& h = brain_hooks();
    if (on && score(h, r)) return;
    r = Candidate{&kNoTarget};
  }

  bool score(BrainHooks& h, Candidate& r) {
    Contact* c = *r.slot;
    const zgame::Handle e = h.contact_entity(c);  // 0x004e6fe0
    if (!e || !h.entity_active(e)) return false;
    float s = c->dist2;
    if (unordered_or_less(s, 1.0f)) return false;  // 0x005c9360
    if (!(s <= range2)) {
      on = false;
      return false;
    }
    const AiTarget* mine = h.targetable(v);
    const AiTarget* theirs = h.targetable(e);  // its vt+0x28
    if (mine && theirs && (mine->allies & theirs->team)) return false;
    if (unordered_or_less(s, 1600.0f)) s = 100.0f;  // 0x005c9368, 0x005c9364
    c->distance();  // 0x004e7030: the direction
    const float* dir = c->dir;
    if (unordered_or_less(0.5f, std::fabs(dir[1]))) return false;  // 0x005c936c: too steep
    if (f.obstacle) {
      if (h.entity_active(f.obstacle)) {
        float po[3], pv[3];
        h.position(f.obstacle, po);
        h.position(v, pv);
        zmth::Vec3 to{0, 0, 0};
        const float d = zmth::direction(zmth::Vec3{pv[0], pv[1], pv[2]}, zmth::Vec3{po[0], po[1], po[2]}, to);
        const float dot = (to.x * dir[0] + to.y * dir[1]) + to.z * dir[2];
        if (dot > 0) {
          if (unordered_or_less(d, 10.0f)) return false;  // 0x005c93f4
          if (unordered_or_less((1.0f - dot * dot) / dot * d * d, 400.0f)) return false;  // 0x005c93f8
        }
      }
      f.obstacle = 0;
    }
    float fwd[3] = {0, 0, 0};
    h.forward(v, &fwd[0], &fwd[2]);
    const float flat = dir[0] * fwd[0] + dir[2] * fwd[2];
    const float k = 0.25f / (0.25f - dir[1] * dir[1]);  // [0x0060b2b4] 0.5 squared (0x004b5b00)
    const float m = crt::x87_add_f24(2.0, -static_cast<double>(flat)) * k;  // the double 0x005c9400
    s = m * m * s;
    r.turn = h.can_face(v, dir);  // vt+0x8c
    if (!r.turn) s = s + s;
    s = (h.health(e) - -0.5f) * s * 0.6666667f;  // vt+0x98; 0x005c9408, 0x005c940c
    s = (1.5f - h.heat(e)) * s * 0.6666667f;     // vt+0x9c; 0x005c9410
    r.clear = h.clear_line(v, e);                // 0x004d5980(e, 0)
    if (!r.clear) s = crt::x87_mul_f24(s, 10.0);  // the double 0x005c9418
    r.score = s;
    return true;
  }
};

// 0x004b5b60: the better (lower) score; a NaN is better than anything.
bool better(const Candidate& a, const Candidate& b) { return unordered_or_less(a.score, b.score); }

// 0x004b5a20 on a candidate's slot: the bearer's weapon manager picks the
// volley at its contact (0x004b1860, item 13) into `kind` (0 for none, set
// whenever it is asked); whether one was picked.
bool volley_kind(Contact* const* slot, zgame::Handle v, int32_t* kind) {
  BrainHooks& h = brain_hooks();
  if (!slot || !*slot || !v) return false;
  const zgame::Handle b = h.bearer(v);  // vt+0x40
  const zgame::Handle wm = b ? h.weapons(b) : 0;
  if (!wm) return false;
  *kind = h.volley_pick(wm, *slot);
  return *kind != 0;
}
}  // namespace

// Mech3.exe 1.2 @ 0x004b6790. The candidates are the first 256 contacts;
// the best by score (0x004b79e0, a partial sort to one place: each better
// one swapped to the front) and then the next ones in that order, until the
// vehicle takes one as its target (0x004b5a80: vt+0xc0 unless it already
// is, then its slot the vehicle's +0x6c). Found, the sensor range is halved
// but kept past the target (its distance + 128, at most +0x30); none,
// doubled up to +0x30.
FirePlan* volley_plan_pick(Firing& f) {
  BrainHooks& h = brain_hooks();
  Brain& b = *f.brain;
  const zgame::Handle v = b.controls->vehicle();
  AiTarget* t = h.targetable(v);
  const float most = b.params.get<float>(0x30);
  auto widen = [&] {
    const float r = f.range + f.range;
    if (!(r > most)) {
      f.range = r;
      if (t) t->set_range(r);  // 0x004d6d00
    }
    return nullptr;
  };
  const std::vector<Contact*>* cs = h.contacts(v);
  const size_t n = cs ? std::min<size_t>(cs->size(), 256) : 0;
  if (n == 0) return widen();
  std::vector<Candidate> recs;
  recs.reserve(n);
  for (size_t i = 0; i < n; ++i) recs.push_back(Candidate{&(*cs)[i]});
  Scorer score{v, f, most * most};
  for (Candidate& r : recs) score(r);
  recs.erase(std::remove_if(recs.begin(), recs.end(), [](const Candidate& r) { return r.slot == &kNoTarget; }),
             recs.end());
  for (size_t i = 1; i < recs.size(); ++i)
    if (better(recs[i], recs[0])) std::swap(recs[i], recs[0]);
  Candidate* pick = nullptr;
  for (Candidate& r : recs) {
    bool ok = true;
    if (t && r.slot != &t->target) ok = h.select_target(v, *r.slot);  // vt+0xc0(contact, 0)
    if (ok) {
      if (t) r.slot = &t->target;
      pick = &r;
      break;
    }
  }
  if (!pick) return widen();
  float d = (*pick->slot)->distance() + 128.0f;  // 0x005c9370
  if (d > most) d = most;
  const float half = f.range * 0.5f;  // 0x005c93ec
  f.range = unordered_or_less(half, d) ? d : half;
  if (t) t->set_range(f.range);
  const zgame::Handle bearer = h.bearer(v);
  if (!bearer) return nullptr;
  int32_t kind = 0;
  if (!volley_kind(pick->slot, v, &kind)) return nullptr;
  auto* p = new VolleyPlan;
  p->target = pick->slot;
  p->score = pick->score;
  p->quick = pick->turn != 0;
  p->line_check = pick->clear != 0;
  p->kind = kind;
  p->bearer = bearer;
  return p;
}

// Mech3.exe 1.2 @ 0x004b6af0
uint8_t VolleyPlan::in_reach(float arg) { return brain_hooks().bearer_in_reach(bearer, arg) == 1; }

uint8_t VolleyPlan::track(float arg) { return brain_hooks().plan_track(*this, arg); }

// Mech3.exe 1.2 @ 0x004b6e10
uint8_t VolleyPlan::ready() {
  BrainHooks& h = brain_hooks();
  const uint8_t clear = h.clear_line(bearer, *target ? h.contact_entity(*target) : 0);  // FIX (bug): as take's
  line_check = clear != 0;
  return clear;
}

// Mech3.exe 1.2 @ 0x004b6fa0: spent when its target's entity is gone or not
// active; else itself while the line to it is clear (0x004d5980 at its
// position, 0x005a8630), the volley still picks one (0x004b5a20, its kind
// renewed) and it tracks (slot +0x0c: +0x08(1.0): +0x10(1.0)).
FirePlan* VolleyPlan::take(bool* spent) {
  BrainHooks& h = brain_hooks();
  // FIX (bug): a target slot whose contact is gone is read through null in
  // the exe; it counts as spent.
  const zgame::Handle e = *target ? h.contact_entity(*target) : 0;
  *spent = !e || !h.entity_active(e);
  if (*spent) return nullptr;
  if (!h.clear_line_at(bearer, e)) return nullptr;
  if (!volley_kind(target, bearer, &kind)) return nullptr;
  return track(1.0f) ? this : nullptr;
}

}  // namespace mech3::ai
