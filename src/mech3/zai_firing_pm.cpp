#include "mech3/zai_firing_pm.h"

#include <cfloat>
#include <cmath>
#include <cstring>

#include "mech3/zai_target.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3::ai {
namespace {

AimPmHooks* g_hooks = nullptr;

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

}  // namespace mech3::ai
