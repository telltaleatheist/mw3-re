#include "mech3/zai_firing.h"

#include <algorithm>
#include <cmath>
#include <cstring>
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

namespace {
// The fast square root of the exe's lead (0x004d6fd5): the float's bits
// halved (arithmetic shift) plus 0x1fc00000.
float fast_sqrt(float x) {
  int32_t i;
  std::memcpy(&i, &x, 4);
  i = (i >> 1) + 0x1fc00000;
  float r;
  std::memcpy(&r, &i, 4);
  return r;
}
float dot3(const float a[3], const float b[3]) { return (a[0] * b[0] + a[1] * b[1]) + a[2] * b[2]; }
}  // namespace

// Mech3.exe 1.2 @ 0x004d6de0. A beam or a weapon whose def +0x6c has 0x800
// or bit 1 (instant) aims at the entity itself; so does one of no speed.
// Else the time t to meet solves (s^2 - v.v) t^2 + 2 d.v t + d.d = 0 for
// the relative velocity v and the way d from the muzzle: one root when the
// square term is 0 (t = d.d / (2 s d.v)), d / s when v is nearly still
// (under 0.001, the point then the entity's own), else the smaller positive
// root; t must come out above 0.
bool lead_point(zgame::Handle bearer, const Contact& c, zgame::Handle w, float out[3]) {
  BrainHooks& h = brain_hooks();
  h.entity_lock_position(c.entity, out);  // 0x005a8630
  float muzzle[3];
  h.weapon_node_position(w, muzzle);  // 0x004ed530 on its +0x20
  const uint32_t flags = h.weapon_flags(w);
  const bool instant = (flags & 0x800) || (flags & 2) == 2;
  const float speed = instant ? 3.4028235e38f : h.weapon_speed(w);
  if (instant) return true;
  if (speed == 0 || std::isnan(speed)) return true;  // the double 0x005ccfc8
  const float d[3] = {out[0] - muzzle[0], out[1] - muzzle[1], out[2] - muzzle[2]};
  float vs[3], vt[3];
  h.velocity(bearer, vs);    // its vt+0x0c
  h.velocity(c.entity, vt);
  const float v[3] = {vt[0] - vs[0], vt[1] - vs[1], vt[2] - vs[2]};
  const float vv = dot3(v, v), dd = dot3(d, d), dv = dot3(d, v);
  const float a = speed * speed - vv;
  const bool still = unordered_or_less(std::fabs(vv), 0.001f);  // the double 0x005ccfd0
  float t;
  bool solved = true;
  if (a == 0 || std::isnan(a)) {
    if (dv == 0 || std::isnan(dv)) solved = false;
    else t = dd / (dv * speed + dv * speed);
  } else if (still) {
    t = fast_sqrt(dd) / speed;
  } else {
    const float disc = a * dd + dv * dv;
    if (!(disc > 0)) {
      if (unordered_or_less(disc, 0)) solved = false;
      else t = -(dv / a);
    } else {
      const float root = fast_sqrt(disc);
      const float inv = 1.0f / a;  // 0x005ccfb8
      const float t1 = (root - dv) * inv;
      const float t2 = (-dv - root) * inv;
      t = t2 > 0 ? t2 : t1;
    }
  }
  if (!solved || !(t > 0)) return false;
  if (!still) out[0] = out[0] + t * v[0], out[1] = out[1] + t * v[1], out[2] = out[2] + t * v[2];
  return true;
}

// Mech3.exe 1.2 @ 0x004b6e30: the target's mech aims others at its last hit
// part (0x0041d130); the lead point for the bearer's selected group's weapon
// (vt+0x84), the way to it from the weapon's node (0x004ed530), unit,
// turned by the bearer's matrix (+0x0c) and given the torso to aim along
// (vt+0x154), clamped (vt+0x90). Its argument goes unread.
uint8_t VolleyPlan::track(float arg) {
  (void)arg;
  BrainHooks& h = brain_hooks();
  // FIX (bug): a target slot whose contact is gone, a bearer without a
  // weapon manager or without a selected group are read through null in
  // the exe; nothing is tracked.
  Contact* c = *target;
  const zgame::Handle e = c ? h.contact_entity(c) : 0;
  if (e) h.mech_aim_at_last_hit(e);
  const zgame::Handle wm = h.weapons(bearer);
  const zgame::Handle w = wm ? h.wm_group_weapon(wm) : 0;
  if (!c || !w) return 0;
  float at[3];
  const bool ok = lead_point(bearer, *c, w, at);
  if (!ok) return 0;
  h.weapon_node_position(w, aim);
  aim[0] = at[0] - aim[0], aim[1] = at[1] - aim[1], aim[2] = at[2] - aim[2];
  zmth::Vec3 n{aim[0], aim[1], aim[2]};
  zmth::normalize(n);  // 0x004020d0
  float m[9];
  h.vehicle_matrix(bearer, m);
  aim[0] = (n.x * m[0] + n.y * m[1]) + n.z * m[2];
  aim[1] = (n.x * m[3] + n.y * m[4]) + n.z * m[5];
  aim[2] = (n.x * m[6] + n.y * m[7]) + n.z * m[8];
  float clamped[3];
  h.bearer_aim_clamp(bearer, aim, clamped);  // its answer only feeds a dot the exe drops
  h.bearer_set_aim(bearer, aim);
  return 1;
}

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

namespace {
// The unit axis least along `n`, less its part along `n` (inlined three
// times in the aim): the first smallest |n[i]| wins, as the exe's strict <.
void across(const float n[3], float out[3]) {
  float best = 3.4028235e+38f, best_abs = 3.4028235e+38f;
  int k = -1;
  for (int i = 0; i < 3; ++i) {
    const float a = std::fabs(n[i]);
    if (a < best_abs) best = n[i], best_abs = a, k = i;
  }
  for (int i = 0; i < 3; ++i) {
    float x = -(n[i] * best);
    if (i == k) x = x - -1.0f;
    out[i] = x;
  }
}
// f's row 1 from rows 2 and 0: row2 x row0.
void cross_rows(float f[9]) {
  const float* a = f + 6;
  const float* b = f;
  f[3] = a[1] * b[2] - b[1] * a[2];
  f[4] = a[2] * b[0] - b[2] * a[0];
  f[5] = a[0] * b[1] - b[0] * a[1];
}
void normalize_row(float r[3]) {
  zmth::Vec3 n{r[0], r[1], r[2]};
  zmth::normalize(n);  // 0x004020d0
  r[0] = n.x, r[1] = n.y, r[2] = n.z;
}
// v across the rows of f: v0 row0 + v1 row1 + v2 row2.
void to_frame(float v[3], const float f[9]) {
  const float x = (v[0] * f[0] + v[1] * f[3]) + v[2] * f[6];
  const float y = (v[0] * f[1] + v[1] * f[4]) + v[2] * f[7];
  const float z = (v[0] * f[2] + v[1] * f[5]) + v[2] * f[8];
  v[0] = x, v[1] = y, v[2] = z;
}
// A random angle: (rand / 16383.5 - 1) * span, rounded as the 24-bit x87 does.
float random_angle(double span) {
  const float u = crt::x87_mul_f24(brain_hooks().rand(), 6.103701895199438e-05);  // 0x005c9428
  return crt::x87_mul_f24(crt::x87_add_f24(u, -1.0), span);                         // 0x005c93a8
}
const float kAimScale = static_cast<float>(2.0 / 100.0);  // [0x0060b2ec], 0x004b7030 at start-up
}  // namespace

// Mech3.exe 1.2 @ 0x004b70b0: the error's size (the pilot's +0xb8 times the
// difficulty's, times +0xc0 when the mech has a targeting computer or a
// fire control system); when not 0, the weapon's muzzle, the unit way from
// the target to it in f's row 2 and its length.
bool Aim::frame(Brain& b, Contact* target, zgame::Handle w, float f[9], float muzzle[3], float* spread,
                float* dist) {
  BrainHooks& h = brain_hooks();
  *spread = b.params.get<float>(0xb8) * h.aim_intensity();
  if (h.targeting_aid(b.controls->vehicle())) *spread = b.params.get<float>(0xc0) * *spread;
  active = *spread != 0;
  if (active) {
    h.weapon_node_position(w, muzzle);  // 0x004ed530 on +0x20
    float at[3];
    h.entity_lock_position(h.contact_entity(target), at);  // 0x005a8630
    f[6] = muzzle[0] - at[0], f[7] = muzzle[1] - at[1], f[8] = muzzle[2] - at[2];
    zmth::Vec3 n{f[6], f[7], f[8]};
    *dist = zmth::normalize(n);
    f[6] = n.x, f[7] = n.y, f[8] = n.z;
  }
  return active;
}

// Mech3.exe 1.2 @ 0x004b71a0: the drift, across the line of sight within 45
// degrees of row 0 (back over the target), at 2% of the error times the
// range per second; the offset's limit that far.
void Aim::pick_drift(const float f[9], float spread, float dist) {
  BrainHooks& h = brain_hooks();
  const float r = spread * dist * kAimScale;
  limit2 = r * r;
  const float a = random_angle(0.7853981633973);  // 0x005e88a0
  drift[2] = 0;
  drift[1] = static_cast<float>(std::sin(double{a}));
  drift[0] = static_cast<float>(std::cos(double{a}));
  drift[1] = r * drift[1];
  drift[0] = r * drift[0];
  to_frame(drift, f);
  until = h.now() + 0.5f;  // 0x005e8894
}

// Mech3.exe 1.2 @ 0x004b72a0 (from the firing, before a shot): a fresh
// offset at 0.6 to 1 of the limit in a random direction across the line of
// sight, and a drift back over the target.
void Aim::track(Brain& b, Contact* target, zgame::Handle w) {
  float f[9], muzzle[3], spread, dist;
  if (!frame(b, target, w, f, muzzle, &spread, &dist)) return;
  BrainHooks& h = brain_hooks();
  across(f + 6, f);
  normalize_row(f);
  cross_rows(f);
  const float u = crt::x87_add_f24(crt::x87_mul_f24(h.rand(), 1.2207403790398877e-05), 0.6);  // 0x005c9440, 0x005c9448
  const float r = u * spread * kAimScale * dist;
  const float a = random_angle(3.14159265359);  // 0x005c9450
  offset[2] = 0;
  offset[1] = static_cast<float>(std::sin(double{a}));
  offset[0] = static_cast<float>(std::cos(double{a}));
  offset[1] = r * offset[1];
  offset[0] = offset[0] * r;
  to_frame(offset, f);
  f[0] = -offset[0], f[1] = -offset[1], f[2] = -offset[2];
  normalize_row(f);
  cross_rows(f);
  pick_drift(f, spread, dist);
}

// Mech3.exe 1.2 @ 0x004b74f0 (the lead, once the offset strays past its
// limit): `at` is carried along its line from the muzzle back to the range
// of the target, the offset taken from there and a new drift picked; behind
// the muzzle the offset is dropped.
void Aim::repick(Brain& b, Contact* target, zgame::Handle w, float at[3]) {
  float f[9], muzzle[3], spread, dist;
  if (!frame(b, target, w, f, muzzle, &spread, &dist)) return;
  BrainHooks& h = brain_hooks();
  float d[3] = {at[0] - muzzle[0], at[1] - muzzle[1], at[2] - muzzle[2]};
  const float dot = (d[0] * f[6] + d[1] * f[7]) + d[2] * f[8];
  const float along = -dot;
  if (!(along > 0)) {
    offset[0] = offset[1] = offset[2] = 0;  // 0x0063d870
    across(f + 6, f);
  } else {
    if (!(along == dist) && !std::isnan(dist)) {  // x87 C3: equal or unordered
      const float k = dist / along;
      d[0] = k * d[0], d[1] = k * d[1], d[2] = k * d[2];
      at[0] = muzzle[0] + d[0], at[1] = muzzle[1] + d[1], at[2] = muzzle[2] + d[2];
      float pos[3];
      h.entity_lock_position(h.contact_entity(target), pos);
      offset[0] = at[0] - pos[0], offset[1] = at[1] - pos[1], offset[2] = at[2] - pos[2];
      const float o2 = (offset[0] * offset[0] + offset[1] * offset[1]) + offset[2] * offset[2];
      if (o2 == 0 || std::isnan(o2)) across(f + 6, offset);
    }
    f[0] = -offset[0], f[1] = -offset[1], f[2] = -offset[2];
  }
  normalize_row(f);
  cross_rows(f);
  pick_drift(f, spread, dist);
}

// Mech3.exe 1.2 @ 0x004b77e0 (from the firing, as it fires): the manager
// aimed at the target's position plus the drifted offset, a new offset first
// when it is past its limit; with no error at the position itself.
void Aim::lead(Brain& b, float dt, Contact* target, zgame::Handle w, zgame::Handle wm) {
  BrainHooks& h = brain_hooks();
  float pos[3];
  if (!active) {
    h.entity_lock_position(h.contact_entity(target), pos);
    h.wm_aim_at(wm, pos);
    return;
  }
  offset[0] = drift[0] * dt + offset[0];
  offset[1] = drift[1] * dt + offset[1];
  offset[2] = drift[2] * dt + offset[2];
  h.entity_lock_position(h.contact_entity(target), pos);
  float at[3] = {pos[0] + offset[0], pos[1] + offset[1], pos[2] + offset[2]};
  const float o2 = (offset[0] * offset[0] + offset[1] * offset[1]) + offset[2] * offset[2];
  if (limit2 < o2) repick(b, target, w, at);
  h.wm_aim_at(wm, at);
}

}  // namespace mech3::ai
