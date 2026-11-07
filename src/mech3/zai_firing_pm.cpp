#include "mech3/zai_firing_pm.h"

#include <cfloat>
#include <cmath>
#include <cstring>
#include <utility>

#include "crt/msvc_sort.h"

#include "mech3/zai_target.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3::ai {
namespace {

AimPmHooks* g_hooks = nullptr;
PickPmHooks* g_pick = nullptr;
ShotPmHooks* g_shot = nullptr;

constexpr float kAimScale = 0.02f;           // [0x0062f8ec], 0x004bad40: 2 / 100
constexpr double kPi = 3.14159265359;        // 0x005e6e20
constexpr double kQuarterPi = 0.7853981633973;  // 0x0060b700
constexpr double kUnit = 6.103701895199438e-05;  // 0x005eb9b8

// The smallest component's unit axis, less its share of `row`: a line across it.
void across(const float row[3], float out[3]) {
  float a = FLT_MAX, least = FLT_MAX;
  int k = -1;
  for (int i = 0; i < 3; ++i) {
    const float v = std::fabs(row[i]);
    if (v < least) a = row[i], least = v, k = i;
  }
  for (int i = 0; i < 3; ++i) {
    double v = -(double{row[i]} * a);
    if (i == k) v += 1.0;
    out[i] = static_cast<float>(v);
  }
}

// f's row 1 from rows 0 and 2.
void cross_rows(float f[9]) {
  f[3] = static_cast<float>(double{f[2]} * f[7] - double{f[8]} * f[1]);
  f[4] = static_cast<float>(double{f[0]} * f[8] - double{f[6]} * f[2]);
  f[5] = static_cast<float>(double{f[6]} * f[1] - double{f[0]} * f[7]);
}

void to_frame(float v[3], const float f[9]) {
  const double a = double{v[0]} * f[0] + double{v[1]} * f[3];
  const double b = double{v[0]} * f[1] + double{v[1]} * f[4];
  const double c = double{v[0]} * f[2] + double{v[1]} * f[5];
  v[0] = static_cast<float>(a + double{v[2]} * f[6]);
  v[1] = static_cast<float>(b + double{v[2]} * f[7]);
  v[2] = static_cast<float>(c + double{v[2]} * f[8]);
}

float dot_stored(const float a[3], const float b[3]) {
  return static_cast<float>((double{a[0]} * b[0] + double{a[1]} * b[1]) + double{a[2]} * b[2]);
}

}  // namespace

AimPmHooks& aim_pm_hooks() { return *g_hooks; }
void set_aim_pm_hooks(AimPmHooks* h) { g_hooks = h; }
PickPmHooks& pick_pm_hooks() { return *g_pick; }
void set_pick_pm_hooks(PickPmHooks* h) { g_pick = h; }
ShotPmHooks& shot_pm_hooks() { return *g_shot; }
void set_shot_pm_hooks(ShotPmHooks* h) { g_shot = h; }

// Pirate's Moon 1.0 @ 0x004013d0
float unit3(float v[3]) {
  const double x = double{v[0]} * v[0], y = double{v[1]} * v[1], z = double{v[2]} * v[2];
  const double root = std::sqrt((x + z) + y);
  const float stored = static_cast<float>(root);
  if (stored == 0) return static_cast<float>(root);
  const double k = 1.0 / root;
  v[0] = static_cast<float>(k * v[0]);
  v[1] = static_cast<float>(k * v[1]);
  v[2] = static_cast<float>(k * v[2]);
  return stored;
}

// Pirate's Moon 1.0 @ 0x004f8dc0
float contact_distance(Contact& c) {
  if (c.dist < 0) {
    std::memcpy(c.dir, c.delta, sizeof c.dir);
    c.dist = unit3(c.dir);
  }
  return c.dist;
}

// Pirate's Moon 1.0 @ 0x004badc0: the error's size as a double, none on what isn't a vehicle, the
// muzzle (the vehicle's place without a weapon), f's row 2 from it to the target, and the range,
// held at 100 so a far target's error stops growing.
bool AimPm::frame(zgame::Handle plan, zgame::Handle w, Brain* b, float f[9], float muzzle[3], double* spread, float* dist) {
  AimPmHooks& h = aim_pm_hooks();
  Contact* c = h.plan_contact(plan);
  const zgame::Handle e = h.contact_entity(c);
  if (e && h.entity_is_vehicle(e)) {
    *spread = double{h.difficulty_error()} * h.pilot_error(b);
    if (h.targeting_aid(b)) *spread = double{h.pilot_aid(b)} * *spread;
  } else {
    *spread = 0;
  }
  active = *spread != 0;
  if (active) {
    const float* at = h.plan_position(plan);
    if (w) {
      h.weapon_line(w, at, muzzle, f + 6);
    } else {
      h.vehicle_position(b, muzzle);
      for (int i = 0; i < 3; ++i) f[6 + i] = at[i] - muzzle[i];
      unit3(f + 6);
    }
    const float d = contact_distance(*c);
    *dist = d > 100.0f ? 100.0f : d;
  }
  return active;
}

// Pirate's Moon 1.0 @ 0x004baf40: MW3's 0x004b71a0.
void AimPm::pick_drift(const float f[9], float spread, float dist) {
  AimPmHooks& h = aim_pm_hooks();
  const double wide = double{kAimScale} * spread * dist;
  const float r = static_cast<float>(wide);
  limit2 = static_cast<float>(wide * r);
  const double a = (h.rand() * kUnit - 1.0) * kQuarterPi;
  drift[2] = 0;
  drift[1] = static_cast<float>(std::sin(a));
  drift[0] = static_cast<float>(std::cos(a));
  drift[1] = r * drift[1];
  drift[0] = r * drift[0];
  to_frame(drift, f);
  until = h.now() + 0.5f;
}

// Pirate's Moon 1.0 @ 0x004bb040
void AimPm::track(zgame::Handle plan, zgame::Handle w, Brain* b) {
  float f[9], muzzle[3], dist;
  double spread;
  if (!frame(plan, w, b, f, muzzle, &spread, &dist)) return;
  AimPmHooks& h = aim_pm_hooks();
  across(f + 6, f);
  unit3(f);
  cross_rows(f);
  const double u = h.rand() * 1.2207403790398877e-05 + 0.6;  // 0x005eb9c8, 0x005eb9c0
  const float r = static_cast<float>(u * kAimScale * spread * dist);
  const double a = (h.rand() * kUnit - 1.0) * kPi;
  offset[2] = 0;
  offset[1] = static_cast<float>(std::sin(a));
  offset[0] = static_cast<float>(std::cos(a));
  offset[1] = r * offset[1];
  offset[0] = r * offset[0];
  to_frame(offset, f);
  f[0] = -offset[0], f[1] = -offset[1], f[2] = -offset[2];
  unit3(f);
  cross_rows(f);
  pick_drift(f, static_cast<float>(spread), dist);
}

// Pirate's Moon 1.0 @ 0x004bb290. The row is MW3's turned round, from the muzzle to the target, and
// the test not: a target in front takes MW3's "behind the muzzle" branch and drops the offset.
void AimPm::repick(zgame::Handle plan, zgame::Handle w, Brain* b, float at[3]) {
  float f[9], muzzle[3], dist;
  double spread;
  if (!frame(plan, w, b, f, muzzle, &spread, &dist)) return;
  AimPmHooks& h = aim_pm_hooks();
  float d[3] = {at[0] - muzzle[0], at[1] - muzzle[1], at[2] - muzzle[2]};
  const float along = -dot_stored(d, f + 6);
  if (!(along > 0)) {
    offset[0] = offset[1] = offset[2] = 0;  // 0x0066f5e0
    across(f + 6, f);
  } else {
    if (!(along == dist) && !std::isnan(along) && !std::isnan(dist)) {  // x87 C3: equal or unordered
      const double k = double{dist} / along;
      for (int i = 0; i < 3; ++i) d[i] = static_cast<float>(d[i] * k);
      for (int i = 0; i < 3; ++i) at[i] = muzzle[i] + d[i];
      const float* pos = h.plan_position(plan);
      for (int i = 0; i < 3; ++i) offset[i] = at[i] - pos[i];
      const float o2 = dot_stored(offset, offset);
      if (o2 == 0 || std::isnan(o2)) across(f + 6, offset);
    }
    f[0] = -offset[0], f[1] = -offset[1], f[2] = -offset[2];
  }
  unit3(f);
  cross_rows(f);
  pick_drift(f, static_cast<float>(spread), dist);
}

// Pirate's Moon 1.0 @ 0x004bb580: new here, the manager's has_target off against a target whose
// +0x6a8's +0x0c is 11, on otherwise (what 11 is isn't known); then MW3's 0x004b77e0.
void AimPm::lead(float dt, zgame::Handle plan, zgame::Handle w, zgame::Handle wm, Brain* b) {
  AimPmHooks& h = aim_pm_hooks();
  const zgame::Handle e = h.contact_entity(h.plan_contact(plan));
  h.wm_has_target(wm, !(e && h.entity_pilot_kind(e) == 0xb));
  if (!active) {
    h.wm_aim_at(wm, h.plan_position(plan));
    return;
  }
  for (int i = 0; i < 3; ++i) offset[i] = static_cast<float>(double{dt} * drift[i] + offset[i]);
  const float* pos = h.plan_position(plan);
  float at[3];
  for (int i = 0; i < 3; ++i) at[i] = pos[i] + offset[i];
  if (limit2 < dot_stored(offset, offset)) repick(plan, w, b, at);
  h.wm_aim_at(wm, at);
}

namespace {

constexpr int32_t kMostCandidates = 8;     // [0x0060b708]
Contact* g_no_contact = nullptr;           // 0x0062f8f0
Contact** const kNoSlot = &g_no_contact;   // [0x0062f8b8]
constexpr float kTwoThirds = 0.6666666865348816f;  // 0x005e28ac
// The parts on PM's mech.
enum : int32_t { kLeg0 = 0x39c, kLeg1 = 0x3a0, kArm0 = 0x3a4, kArm1 = 0x3a8, kTorso = 0x3ac, kHead = 0x3b4 };

// 0x004bbf20: who's filtered, counted up to `limit`.
struct Filter {
  PickFiring& f;
  int32_t limit, count = 0;
  bool open;
  Filter(PickFiring& fr, int32_t n) : f(fr), limit(n) {
    const uint8_t a = fr.pilot.filter[0], b = fr.pilot.filter[1];
    open = (a & 4) && (a & 0x20) && (a & 0x40) && (a & 0x80) && (b & 1);
  }
  bool skip(Contact* c);  // 0x004ba190
};

bool Filter::skip(Contact* c) {
  PickPmHooks& h = pick_pm_hooks();
  if (!(count < limit)) return true;
  const zgame::Handle e = h.contact_entity(c);
  if (!e || !h.active(e)) return true;
  if (h.team(e) & h.allies(f.vehicle)) return true;
  const zgame::Handle g = h.group(e);
  const uint32_t marker = g ? h.group_marker(g) : 0;
  if (marker && marker != f.pilot.marker) return true;
  if (!(contact_distance(*c) <= f.reach)) return true;
  if (!open) {
    const zgame::Handle m = h.vehicle(e);
    const zgame::Handle driver = m ? h.vehicle_x58(m) : 0;
    const uint32_t x100 = m ? h.vehicle_x100(m) : 0;
    const zgame::Handle g2 = h.group(e);
    const uint32_t xe8 = g2 ? h.group_xe8(g2) : 0;
    const uint8_t a = f.pilot.filter[0];
    if (!marker && (!(a & 4) || !driver) && (!(a & 8) || driver != h.player()) && (!(a & 0x10) || !(h.flags(e) & 0x10)) &&
        (!(a & 0x20) || !m || driver || x100) && (!(a & 0x40) || !x100) && (!(a & 0x80) || !h.entity_x50(e)) &&
        (!(f.pilot.filter[1] & 1) || !xe8))
      return true;
  }
  ++count;
  return false;
}

// 0x004ba340 with its record 0x004bc230: lower is better; none past the first out of reach.
struct Scorer {
  PickFiring& f;
  float most2;
  bool going = true;
  explicit Scorer(PickFiring& fr) : f(fr) {
    const double r = fr.pilot.range;
    most2 = static_cast<float>(r * r);
  }
  void operator()(PickRec& r);
};

void Scorer::operator()(PickRec& r) {
  PickPmHooks& h = pick_pm_hooks();
  const auto none = [&] { r.slot = kNoSlot, r.score = 0, r.faces = 0, r.in_sight = 0; };
  Contact* c = *r.slot;
  const zgame::Handle e = going ? h.contact_entity(c) : 0;
  if (!e) return none();
  float s = c->dist2;
  if (s < 1.0f) return none();
  if (s > most2) {
    going = false;
    return none();
  }
  if (s < 1600.0f) s = 100.0f;
  contact_distance(*c);
  const float* dir = c->dir;
  if (!(std::fabs(dir[1]) <= 0.5f)) return none();
  if (f.aimed) {
    if (f.aimed_up) {
      const float* to = h.position(f.aimed);
      const float* from = h.position(f.vehicle);
      float line[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
      const float len = unit3(line);
      const float dot = dot_stored(line, dir);
      if (dot > 0) {
        if (len < 10.0f) return none();
        const double off = (1.0 - double{dot} * dot) / dot * len * len;
        if (off < 400.0) return none();
      }
    }
    f.aimed = 0, f.aimed_up = false;
  }
  const float* ahead = h.forward(f.vehicle);
  const float along = static_cast<float>(double{dir[0]} * ahead[0] + double{dir[2]} * ahead[2]);
  const double turn = (2.0 - along) * (0.25 / (0.25 - double{dir[1]} * dir[1]));
  s = static_cast<float>(turn * turn * s);
  const bool faces = h.faces(f.vehicle, dir);
  if (!faces) s = s + s;
  s = static_cast<float>((double{h.health(e)} + 0.5) * s * kTwoThirds);
  if (const zgame::Handle m = h.mech(e)) {
    const float heat = h.heat(m);
    const double k = heat > 1.5f ? 1.0 : double{heat} * kTwoThirds;
    s = static_cast<float>((1.5 - k) * s * kTwoThirds);
  }
  const bool sight = h.in_sight(f.vehicle, e);
  r.score = sight ? s : static_cast<float>(double{s} * 10.0);
  r.faces = faces, r.in_sight = sight;
}

// 0x004ba030: the vehicle takes it, or has it already; the slot its own then.
bool take(PickRec& r, zgame::Handle v) {
  PickPmHooks& h = pick_pm_hooks();
  Contact** own = h.target_slot(v);
  bool ok = true;
  if (!*own || h.contact_entity(*r.slot) != h.contact_entity(*own)) {
    ok = h.select(v, *r.slot);
    if (!ok) return false;
  }
  r.slot = own;
  return ok;
}

// 0x004bbe30: the entity's contact, when it passes and the vehicle takes it.
bool pick_entity(PickFiring& f, zgame::Handle e, PickRec* out) {
  PickPmHooks& h = pick_pm_hooks();
  if (!e) return false;
  std::vector<Contact*>& cs = h.contacts(f.vehicle);
  for (Contact*& c : cs) {
    if (h.contact_entity(c) != e) continue;
    Filter pass(f, 1);
    if (pass.skip(c)) return false;
    *out = PickRec{};
    out->slot = &c;
    Scorer score(f);
    score(*out);
    return out->slot != kNoSlot && take(*out, f.vehicle);
  }
  return false;
}

// 0x004bbfa0: the first eight that pass, the best (0x004bd760: a sort to one place by 0x004ba170)
// then the rest in that order, until the vehicle takes one.
bool pick_best(PickFiring& f, PickRec* out) {
  PickPmHooks& h = pick_pm_hooks();
  std::vector<Contact*>& cs = h.contacts(f.vehicle);
  const int32_t room = static_cast<int32_t>(cs.size()) < kMostCandidates ? static_cast<int32_t>(cs.size()) : kMostCandidates;
  Filter pass(f, room);
  std::vector<PickRec> recs;
  for (Contact*& c : cs) {
    if (pass.skip(c)) continue;
    PickRec r;
    r.slot = &c;
    recs.push_back(r);
  }
  if (recs.empty()) return false;
  Scorer score(f);
  for (PickRec& r : recs) score(r);
  std::vector<PickRec> kept;
  for (const PickRec& r : recs)
    if (r.slot != kNoSlot) kept.push_back(r);
  for (size_t i = 1; i < kept.size(); ++i)
    if (kept[i].score < kept[0].score) std::swap(kept[i], kept[0]);
  for (PickRec& r : kept) {
    if (take(r, f.vehicle)) {
      *out = r;
      return true;
    }
  }
  return false;
}

// The rest of 0x004bc2e0: which part to aim at. A leg's is its calf (0x0041e290).
void pick_part(PickFiring& f, PlanPm& p, zgame::Handle m) {
  PickPmHooks& h = pick_pm_hooks();
  const PickPilot& b = f.pilot;
  const auto alive = [&](int32_t at) { return h.part_health(h.part(m, at)) > 0; };
  const auto weighted = [&] {
    const double sum = ((double{b.weights[3]} + b.weights[2]) + b.weights[1]) + b.weights[0];
    if (sum == 0) return;
    const double first = h.rand() * static_cast<double>(static_cast<float>(sum)) * 3.0517578125e-05 - b.weights[0];
    float left = static_cast<float>(first);
    if (first < 0 && alive(kTorso)) {
      p.part = static_cast<uint32_t>(h.part(m, kTorso));
      return;
    }
    const auto less = [&](float w) {
      const double v = left - double{w} * 0.5;
      left = static_cast<float>(v);
      return v < 0;
    };
    if (less(b.weights[1]) && alive(kLeg0)) {
      p.part = h.part_child(m, 0);
      return;
    }
    if (less(b.weights[1]) && alive(kLeg1)) {
      p.part = h.part_child(m, 1);
      return;
    }
    if (less(b.weights[2]) && alive(kArm0)) {
      p.part = static_cast<uint32_t>(h.part(m, kArm0));
      return;
    }
    if (less(b.weights[2]) && alive(kArm1)) {
      p.part = static_cast<uint32_t>(h.part(m, kArm1));
      return;
    }
    p.part = static_cast<uint32_t>(h.part(m, alive(kHead) ? kHead : kTorso));
  };
  const auto extreme = [&](bool high) {
    const auto wins = [&](float v, float best) { return high ? best < v : best > v; };
    float best = h.part_health(h.part(m, kTorso));
    for (int32_t at : {kHead, kArm0, kArm1}) {
      const float v = h.part_health(h.part(m, at));
      if (wins(v, best)) best = v, p.part = static_cast<uint32_t>(h.part(m, at));
    }
    float v = h.part_health(h.part(m, kLeg0));
    if (wins(v, best)) best = v, p.part = h.part_child(m, 0);
    v = h.part_health(h.part(m, kLeg1));
    if (wins(v, best)) p.part = h.part_child(m, 1);
  };
  p.part = static_cast<uint32_t>(h.part(m, kTorso));
  switch (b.part_mode) {
    case 0: case 3: weighted(); break;
    case 1: extreme(false); break;
    case 2: extreme(true); break;
    default: break;
  }
}

}  // namespace

// Pirate's Moon 1.0 @ 0x004bc2e0: the plan held until its time, else a new one: the entity asked
// for (+0x8c, +0x90), the pilot's threat list, the best contact; then a part of a mech to aim at,
// which MW3 doesn't have. Nothing found: no target.
PlanPm* plan_pick(PickFiring& f) {
  PickPmHooks& h = pick_pm_hooks();
  PlanPm* made = nullptr;
  if (h.now() < f.until && f.plan && f.plan->slot && *f.plan->slot) {
    f.seen = *h.target_slot(f.vehicle);
    return f.plan;
  }
  f.until = 0;
  delete f.plan;
  f.plan = nullptr;
  PickRec rec;
  rec.slot = kNoSlot;
  const auto find = [&]() -> bool {
    if (h.contacts(f.vehicle).empty()) return false;
    if (pick_entity(f, f.prefer, &rec)) return true;
    if (f.prefer_next) {
      const zgame::Handle e = f.prefer_next;
      const bool found = e != f.prefer && pick_entity(f, e, &rec);
      f.prefer_next = 0;
      if (found) return true;
    }
    switch (f.pilot.pick_order) {
      case 0: case 1:
        if (pick_entity(f, h.threat(f.brain, 0), &rec)) return true;
        break;
      case 2: case 3:
        if (pick_entity(f, h.threat(f.brain, 1), &rec)) return true;
        break;
      case 4: case 5: break;
      default: return false;
    }
    return pick_best(f, &rec);
  };
  if (find()) {
    made = new PlanPm;
    made->slot = rec.slot, made->score = rec.score, made->faces = rec.faces, made->in_sight = rec.in_sight;
    made->vehicle = f.vehicle;
    if (const zgame::Handle m = h.contact_mech(*made->slot)) pick_part(f, *made, m);
    f.until = h.now() + f.pilot.repick;
  } else {
    h.select_none(f.vehicle);
  }
  f.seen = *h.target_slot(f.vehicle);
  return made;
}

namespace {

// A weapon weighed (0x18 bytes).
struct ShotRec {
  zgame::Handle w = 0;
  float cost = FLT_MAX, heat = FLT_MAX, value = 0;
  bool wanted = false, within = false, cool = false;  // +0x14, +0x15, +0x16
};

// 0x004bc930's record (0x80 bytes): what limits this volley and the running counts.
struct ShotCtx {
  float heat_room = FLT_MAX, rate_room = FLT_MAX;  // +0x0c, +0x10
  float ammo_keep = 0;                             // +0x14
  float value_k = 0, cost_k = 0;                   // +0x18, +0x1c
  float best_cost = FLT_MAX, best_heat = FLT_MAX;  // +0x20, +0x24
  ShotRec cheapest, coolest;                       // +0x28, +0x40
  bool any_ammo = false;                           // +0x58
  int32_t fit = 0, cool = 0, wanted = 0, ready = 0, fit_wanted = 0;  // +0x5c, +0x60, +0x64, +0x68, +0x6c
  float dist2 = 0;                                 // +0x08
  const float* at = nullptr;                       // +0x78
};

// 0x004bca70: the bin's fill; 1 without a bin, for no ammo count, or full.
double fill(zgame::Handle w) {
  ShotPmHooks& h = shot_pm_hooks();
  const int32_t n = h.ammo(w);
  const float a = static_cast<float>(n);
  int32_t full = 0;
  if (static_cast<double>(n) == -1.0 || !h.bin(w, &full)) return 1.0;
  return double{a} == full ? 1.0 : a / static_cast<double>(full);
}

// 0x004bcad0
ShotRec weigh(zgame::Handle w, ShotCtx& x) {
  ShotPmHooks& h = shot_pm_hooks();
  ShotRec r;
  r.w = w;
  if (!h.ready(w) || (x.at && !h.can_hit(w, x.at))) return r;
  if (!(h.reach_least(w) < x.dist2) || !(h.reach_most(w) > x.dist2)) return r;
  ++x.ready;
  r.wanted = x.any_ammo || fill(w) > x.ammo_keep;
  if (r.wanted) ++x.wanted;
  r.cost = static_cast<float>(double{x.cost_k} * h.cost(w));
  const double dps = h.dps(w);
  r.heat = static_cast<float>(dps);
  r.cool = dps < x.rate_room;
  r.within = r.cool && x.heat_room > r.cost;
  if (r.within && r.cost < x.best_cost) x.cheapest = r, x.best_cost = r.cost;
  if (r.heat < x.best_heat) x.coolest = r, x.best_heat = r.heat;
  if (r.cool) ++x.cool;
  if (r.within) {
    ++x.fit;
    if (r.wanted) ++x.fit_wanted;
  }
  r.value = static_cast<float>(double{h.value(w)} * x.value_k);
  return r;
}

}  // namespace

// Pirate's Moon 1.0 @ 0x004bcd40 (0x004bd920 is std::sort): each weapon weighed against the room
// above the shooter's heat. None fits: the lowest vt+0x1c alone; one: it; else those that fit, those
// with ammo to spare if some, best rating first while both sums stay in the room.
bool shot_pick(zgame::Handle wm, const Contact* c, const float* at, float k, float rate) {
  ShotPmHooks& h = shot_pm_hooks();
  if (!c) return false;
  const ShotShooter me = h.shooter(c);
  ShotCtx x;
  x.value_k = k, x.at = at, x.dist2 = c->dist2;
  if (me.pilot) x.cost_k = me.cost_least < k ? k : me.cost_least;
  if (me.vehicle) {
    x.heat_room = me.heat_most - me.heat;
    x.rate_room = me.rate_most - me.heat;
    if (me.health >= me.health_past) x.ammo_keep = me.ammo_keep;
  }
  x.any_ammo = !(x.ammo_keep > 0);
  const std::vector<zgame::Handle>& ws = h.weapons(wm);
  std::vector<ShotRec> recs;
  recs.reserve(ws.size());
  for (const zgame::Handle w : ws) recs.push_back(weigh(w, x));
  if (x.ready < 1 || x.cool < 1) return false;
  if (x.fit == 0 || x.fit == 1) {
    recs.assign(1, x.fit == 0 ? x.coolest : x.cheapest);
  } else {
    const size_t all = recs.size();
    const auto keep = [&](auto pass) {
      size_t n = 0;
      for (const ShotRec& r : recs)
        if (pass(r)) recs[n++] = r;
      recs.resize(n);
    };
    keep([](const ShotRec& r) { return r.within; });
    if (0 < x.fit_wanted && static_cast<size_t>(x.fit_wanted) < all) keep([](const ShotRec& r) { return r.wanted && r.within; });
    if (recs.size() > 1) {
      crt::msvc_sort(recs.begin(), recs.end(), [](const ShotRec& a, const ShotRec& b) { return a.value > b.value; });
      float cost = 0, heat = 0;
      size_t i = 0;
      for (; i < recs.size(); ++i) {
        cost = cost + recs[i].cost;
        const double sum = double{recs[i].heat} + heat;
        heat = static_cast<float>(sum);
        if (cost > x.heat_room || sum > x.rate_room) break;
      }
      recs.resize(i);
    }
  }
  if (recs.empty()) return false;
  h.group_clear(wm);
  for (const ShotRec& r : recs) h.group_add(wm, r.w, rate);
  h.set_mode(wm, 2);
  return true;
}

}  // namespace mech3::ai
