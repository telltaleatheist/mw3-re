#include "mech3/zai_steer_pm.h"

#include <cmath>

namespace mech3::ai {

// Pirate's Moon 1.0 @ 0x004dc0f0, its defaults the statics 0x0060bb78..0x0060bba8.
float unit_flat(float* x, float* z, bool root_stored) {
  const double root = std::sqrt(double{*x} * *x + double{*z} * *z);
  const float len = static_cast<float>(root);
  if (len != 0.0f) {
    const double k = 1.0 / (root_stored ? double{len} : root);
    *x = static_cast<float>(*x * k), *z = static_cast<float>(*z * k);
  }
  return len;
}

SteerPm::SteerPm() {
  start[0] = start[1] = start[2] = 0;
  dir[0] = dir[1] = 0;
  ease = 2.0f;
  along = side = 0;
  near_dist = 10.0f;
  x40 = static_cast<float>(std::cos(0.7853981633975001));  // [0x0062fb5c], 0x004dbf80
  keep_near = 30.0f;
  x4c = 1.0f;
  keep_slope = 2.0f;
  keep_scale = 1.0f;
  push_reach = 40.0f;
  push_band = 15.0f;
  cos_on = static_cast<float>(std::cos(0.17453292519944444));         // 0x004dbfc0
  cos_turnabout = static_cast<float>(std::cos(2.3561944901925003));
  gain_along = 0.05263158f;  // 0x3d579436
  gain_side = 0.012658228f;  // 0x3c4f6475
  cos_dead = static_cast<float>(std::cos(0.03490658503988));         // 0x004dc020
  sin_dead = static_cast<float>(std::sin(0.03490658503988));
  cos_behind = static_cast<float>(std::cos(3.0543261909895));
}

// Pirate's Moon 1.0 @ 0x004dc210: near the segment, for an order that steers by it; any other is near.
bool SteerPm::near(SteerPmHooks& h) const {
  if (!h.order_steers(brain)) return true;
  double m = std::fabs(side);
  if (along < 0.0f) m = m - along;
  else if (length < along) m = (double{along} - length) + m;
  return !(near_dist < m);
}

// Pirate's Moon 1.0 @ 0x004dc290 (MW3's 0x004ce6f0)
void SteerPm::carrot(float out[3]) const {
  out[0] = static_cast<float>(double{dir[0] * along_at} + start[0]);  // each product stored first
  out[1] = 0.0f + start[1];
  out[2] = static_cast<float>(double{dir[1] * along_at} + start[2]);
  if (across != 0.0f) {
    out[0] = static_cast<float>(double{dir[1]} * across + out[0]);
    out[2] = static_cast<float>(out[2] - double{across} * dir[0]);
  }
}

// Pirate's Moon 1.0 @ 0x004dc320
void SteerPm::on_line(const float p[3], float out[2]) const {
  const double dx = double{p[0]} - start[0], dz = double{p[2]} - start[2];
  out[0] = static_cast<float>(dx * dir[0] + dz * dir[1]);
  out[1] = static_cast<float>(dx * dir[1] - dz * dir[0]);
}

// Pirate's Moon 1.0 @ 0x004dc980 (MW3's 0x004ceb00)
void SteerPm::project(const float p[3], float out[2]) const {
  on_line(p, out);  // stored first in the exe too
  out[0] = out[0] - along_at;
  out[1] = out[1] - across;
}

// Pirate's Moon 1.0 @ 0x004dc400: MW3's 0x004ce800, the carrot kept on the segment.
void SteerPm::advance(float d) {
  float by = along_min - along_at;
  if (by <= d) {
    const float most = length - along_at;
    by = most < d ? most : d;
  }
  along_at = by + along_at;
  along = along - by;
}

// Pirate's Moon 1.0 @ 0x004dc450 (MW3's 0x004ce820)
void SteerPm::shift(float da, float ds) {
  along_at = da + along_at;
  along = along - da;
  across = ds + across;
  side = side - ds;
}

// Pirate's Moon 1.0 @ 0x004dc480, rewritten from MW3's 0x004ce860: only to the side (the amount
// along stays 0), by what keeps the carrot and the vehicle `push_reach` from the contact's span
// on the segment, within `near_dist` of the line.
bool SteerPm::push_amount(SteerPmHooks& h, const Contact* c, float* da, float* ds) {
  *da = 0.0f;
  *ds = 0.0f;
  const zgame::Handle e = h.contact_entity(c);
  if (!e) return false;
  float p[3], l[2];
  h.position(e, p);
  project(p, l);
  const float from_carrot = -l[0], from_vehicle = along - l[0];
  const float lo = from_vehicle <= from_carrot ? from_vehicle : from_carrot;
  const float hi = from_vehicle <= from_carrot ? from_carrot : from_vehicle;
  // The span's nearest to 0, 0 when it crosses.
  float gap = 0.0f;
  if (lo >= 0.0f) gap = lo;
  else if (hi <= 0.0f) gap = hi;
  auto away = [&](float off, bool* in) {
    *in = std::fabs(gap) + std::fabs(off) < push_reach && std::fabs(off) < push_band;
    if (!*in) return off;
    float r = push_reach - std::fabs(gap);
    if (r > push_band) r = push_band;
    return std::copysign(r, off);
  };
  bool by_carrot, by_vehicle;
  const float at_carrot = away(-l[1], &by_carrot);
  const float at_vehicle = away(side - l[1], &by_vehicle);
  float r = -l[1];
  if (by_vehicle) r = at_vehicle;
  else if (by_carrot) r = std::copysign(at_carrot, at_vehicle);
  double wide = double{r} + across;
  wide = wide + l[1];
  if (wide < -near_dist) wide = -near_dist;
  else if (wide > near_dist) wide = near_dist;
  *ds = static_cast<float>(wide - across);
  return *ds != 0.0f;
}

// Pirate's Moon 1.0 @ 0x004dc7a0: MW3's 0x004cea30, but a segment of no length points back along
// the vehicle, and the carrot stays where it was (where the vehicle is, the first time) if that's
// within 50 of the new segment; failing that, where the vehicle is if that is.
void SteerPm::set(SteerPmHooks& h, const float to[3], const float from[3], float r, zgame::Handle v) {
  const bool again = sets != 0;
  ++sets;
  float was[3];
  if (again) carrot(was);
  else h.position(v, was);
  dir[0] = to[0] - from[0];
  dir[1] = to[2] - from[2];
  const float len = unit_flat(&dir[0], &dir[1], true);  // 0x004dbf10
  along_at = len;
  length = len;
  if (len == 0.0f) {
    float fx, fz;
    h.forward(v, &fx, &fz);
    dir[0] = -fx;
    dir[1] = -fz;
  }
  along_min = 0.0f;
  across = 0.0f;
  across_to = 0.0f;
  rate = r;
  start[0] = from[0], start[1] = from[1], start[2] = from[2];
  constexpr float kAlong = 50.0f, kSide = 50.0f;  // 0x0060bbd4, 0x0060bbd8
  auto within = [&](const float l[2]) {
    return !(l[0] < -kAlong) && !(length + kAlong < l[0]) && !(kSide < std::fabs(l[1]));
  };
  float l[2];
  on_line(was, l);
  if (within(l)) {
    along_at = l[0];
    across = l[1];
  } else if (again) {
    float p[3];
    h.position(v, p);
    on_line(p, l);
    if (within(l)) {
      along_at = l[0];
      across = l[1];
    }
  }
  float p[3], m[2];
  h.position(v, p);
  project(p, m);
  along = m[0];
  side = m[1];
}

// Pirate's Moon 1.0 @ 0x004dc9d0 (MW3's 0x004ceb50): nothing for a vehicle in the air. The keep's reach
// shrinks with the distance to the side; the contacts until one has no target or is past the reach.
void SteerPm::measure(SteerPmHooks& h, zgame::Handle v) {
  if (h.halted(v)) return;
  float p[3], m[2];
  h.position(v, p);
  project(p, m);
  along = m[0];
  side = m[1];
  if (keep) {
    const double room = keep_near - double{std::fabs(side)} * keep_slope;
    if (room > 0.0) {
      const double r = room * keep_scale;
      if (r < std::fabs(along)) advance(static_cast<float>(along - std::copysign(r, double{along})));
    } else {
      advance(along);
    }
  }
  if (!push || !near(h)) return;
  const float a = along > 0.0f ? along + push_reach : push_reach;  // 0x004dc360
  const float limit = push_band * push_band + a * a;
  for (const Contact* c : h.contacts(v)) {  // 0x004dc3b0
    const zgame::Handle e = h.contact_entity(c);
    if (!e || !h.targetable(e) || !(h.contact_dist2(c) <= limit)) break;
    float da, ds;
    if (push_amount(h, c, &da, &ds)) shift(da, ds);  // 0x004dc760
  }
}

// Pirate's Moon 1.0 @ 0x004dcb30 (MW3's 0x004cec80)
void SteerPm::update(SteerPmHooks& h, float dt, zgame::Handle v) {
  const float to = across_to;
  const double n = (double{to} - across) * ease * dt + across;
  across = static_cast<float>(n);
  if (std::fabs(to - n) < 0.05000000074505806) across = to;
  const double a = along_at - double{dt} * rate;
  along_at = static_cast<float>(a);
  if (a < along_min) along_at = along_min;
  measure(h, v);
}

}  // namespace mech3::ai
