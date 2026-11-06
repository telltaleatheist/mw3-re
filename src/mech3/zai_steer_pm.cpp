#include "mech3/zai_steer_pm.h"

#include <cmath>

namespace mech3::ai {

// Pirate's Moon 1.0 @ 0x004dc0f0, its defaults the statics 0x0060bb78..0x0060bba8.
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
  float m = std::fabs(side);
  if (along < 0.0f) m = m - along;
  else if (length < along) m = (along - length) + m;
  return !(near_dist < m);
}

// Pirate's Moon 1.0 @ 0x004dc290 (MW3's 0x004ce6f0)
void SteerPm::carrot(float out[3]) const {
  out[0] = dir[0] * along_at + start[0];
  out[1] = 0.0f + start[1];
  out[2] = dir[1] * along_at + start[2];
  if (across != 0.0f) {
    out[0] = dir[1] * across + out[0];
    out[2] = out[2] - across * dir[0];
  }
}

// Pirate's Moon 1.0 @ 0x004dc320
void SteerPm::on_line(const float p[3], float out[2]) const {
  const float dx = p[0] - start[0], dz = p[2] - start[2];
  out[0] = dx * dir[0] + dz * dir[1];
  out[1] = dx * dir[1] - dz * dir[0];
}

// Pirate's Moon 1.0 @ 0x004dc980 (MW3's 0x004ceb00)
void SteerPm::project(const float p[3], float out[2]) const {
  on_line(p, out);
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
  r = r + across;
  r = r + l[1];
  if (r < -near_dist) r = -near_dist;
  else if (r > near_dist) r = near_dist;
  r = r - across;
  *ds = r;
  return r != 0.0f;
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
  const float len = std::sqrt(dir[1] * dir[1] + dir[0] * dir[0]);
  const float scale = len == 0.0f ? 0.0f : 1.0f / len;  // 0x004dbf10
  dir[0] = scale * dir[0];
  dir[1] = scale * dir[1];
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

// Pirate's Moon 1.0 @ 0x004dc9d0 (MW3's 0x004ceb50): nothing for a halted vehicle. The keep's reach
// shrinks with the distance to the side; the contacts until one has no target or is past the reach.
void SteerPm::measure(SteerPmHooks& h, zgame::Handle v) {
  if (h.halted(v)) return;
  float p[3], m[2];
  h.position(v, p);
  project(p, m);
  along = m[0];
  side = m[1];
  if (keep) {
    const float room = keep_near - std::fabs(side) * keep_slope;
    if (room > 0.0f) {
      const float r = room * keep_scale;
      if (r < std::fabs(along)) advance(along - std::copysign(r, along));
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
  const float n = (to - across) * ease * dt + across;
  across = n;
  if (std::fabs(to - n) < 0.05000000074505806) across = to;
  const float a = along_at - dt * rate;
  along_at = a;
  if (a < along_min) along_at = along_min;
  measure(h, v);
}

}  // namespace mech3::ai
