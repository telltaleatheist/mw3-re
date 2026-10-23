#include "mech3/veh_move.h"

#include <cmath>
#include <cstring>

#include "gamez/zclass/cls_di.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmath/zmth_vector.h"
#include "gamez/zmodel/gmod_cull.h"
#include "mech3/veh_basic.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

namespace {
VehBasicHooks& hk() { return veh_basic_hooks(); }
float sign1(float x) { return x >= 0.0f ? 1.0f : -1.0f; }  // the exe's: 1 at 0, -1 for NaN
}  // namespace

// Mech3.exe 1.2 @ 0x004a2a00: the turn's own (turns[0]), damped to 0 without one.
void VehicleControls::turn_step(const VehicleMotion& m, float dt) {
  const float t = turn;  // IControls vt+0x24
  if (t == 0.0f || std::isnan(t)) {
    x50.y = zmth::exp_neg(m.turn_damping * dt) * x50.y;
    return;
  }
  if ((t > 0.0f && !(x50.y >= 0.0f)) || (t < 0.0f && x50.y > 0.0f)) x50.y = 0.0f;
  const float r = (m.turns[0] * t) * dt + x50.y;
  x50.y = r;
  if (r > top_turn) x50.y = top_turn;
  else if (!(r >= -top_turn)) x50.y = -top_turn;
}

// Mech3.exe 1.2 @ 0x004a28c0
void vehicle_turn_to_heading(BasicVehicle& v) {
  VehicleControls& c = *v.controls;
  if (!c.x760) return;
  if (c.turn != 0.0f && !std::isnan(c.turn)) c.x760 = 0;  // IControls vt+0x24 (a NaN counts as none)
  if (!c.x760) return;
  const float dt = hk().frame_dt();
  const float step = static_cast<float>(std::cos(double{dt * c.motion.turns[1]}));
  const float dot = v.forward.z * c.heading.z + v.forward.x * c.heading.x;
  const float cross = c.heading.x * v.forward.z - v.forward.x * c.heading.z;
  if (step > dot) {  // further than a frame's turn
    const float s = cross < 0.0f || std::isnan(cross) ? -1.0f : 1.0f;
    c.turn = s;  // IControls vt+0x28
    c.x50.y = s * c.motion.turns[1];
    return;
  }
  c.x760 = 0;
  c.turn = 0.0f;
  c.angles.y = static_cast<float>(std::atan2(-double{c.heading.x}, -double{c.heading.z}));
  c.x50.y = 0.0f;
}

// Mech3.exe 1.2 @ 0x004a2890
void vehicle_up_refresh(BasicVehicle& v) {
  hk().refresh(v);
  const float* m = v.transform;
  v.controls->up = {m[3], m[4], m[5]};
}

// Mech3.exe 1.2 @ 0x004a2b10
void vehicle_speed_step(BasicVehicle& v) {
  VehicleControls& c = *v.controls;
  if (c.sliding) return vehicle_slide(v);
  const VehicleMotion& m = c.motion;
  const float dt = hk().frame_dt();
  const float t = c.throttle;  // IControls vt+0x1c, asked each time
  if (t == 0.0f || std::isnan(t)) {
    c.x70.z = zmth::exp_neg(m.rate_damping[1] * dt) * c.x70.z;
  } else {
    // "Below" tests pass unordered values (the x87's C0), "above" ones do not.
    if ((t > 0.0f && c.x70.z > 0.0f) || (t < 0.0f && !(c.x70.z >= 0.0f)))
      c.x70.z = zmth::exp_neg(m.rate_damping[1] * dt) * c.x70.z;
    c.x70.z = c.x70.z - (t * m.rates[0]) * dt;
    const float cap = std::fabs(t) * c.top_speed;
    if (c.x70.z > cap) c.x70.z = cap;
    else if (!(c.x70.z >= -cap)) c.x70.z = -cap;
  }
  if (c.x70.x != 0.0f && !std::isnan(c.x70.x)) c.x70.x = zmth::exp_neg(m.rate_damping[0] * dt) * c.x70.x;
  if (!(std::fabs(c.x70.x) >= 0.01f)) c.x70.x = 0.0f;
}

// Mech3.exe 1.2 @ 0x004a2dd0
void vehicle_slide(BasicVehicle& v) {
  VehicleControls& c = *v.controls;
  const float dt = hk().frame_dt();
  const float fx = v.forward.x, fz = v.forward.z;
  c.x70.x = -fz * c.velocity.x - -fx * c.velocity.z;
  c.x70.z = -fz * c.velocity.z + -fx * c.velocity.x;
  const float top = c.top_speed;
  const float z = c.x70.z - (c.throttle * c.motion.rates[0]) * dt;
  c.x70.z = z;
  if (z > top) c.x70.z = top;
  else if (!(z >= -top)) c.x70.z = -top;
  float s = vehicle_side_force(v) * dt + c.x70.x;
  if (c.x70.x != 0.0f && !std::isnan(c.x70.x) && sign1(c.x70.x) != sign1(s)) {
    s = 0.0f;
    c.sliding = 0;
  }
  c.x70.x = s;
}

// Mech3.exe 1.2 @ 0x004a2c90
float vehicle_side_force(BasicVehicle& v) {
  const float dt = hk().frame_dt();
  if (!(double{dt} >= 1e-07)) return 0.0f;  // a NaN too
  VehicleControls& c = *v.controls;
  const float f = v.transform[1] * -60.0f +
                  -c.x70.z * (v.forward.x * c.x314.z - v.forward.z * c.x314.x) * hk().frame_inv_dt();
  if (c.x70.x == 0.0f || std::isnan(c.x70.x)) {  // the exe's equal test passes a NaN
    if (c.motion.friction[0] < std::fabs(f)) {
      c.sliding = 1;
      return f - sign1(f) * c.motion.friction[0];
    }
    return 0.0f;
  }
  return f - sign1(c.x70.x) * c.motion.friction[1];
}

// ---------------------------------------------------------------------------
// The suspension (2c2)

// Mech3.exe 1.2 @ 0x004a2820
void vehicle_support_step(float* y, float* vs, float ground, int32_t* air, float g, float landing) {
  const float dt = hk().frame_dt();
  const float v = *vs - g * dt;
  *vs = v;
  const float next = v * dt + *y;
  if (ground < next) {
    *air = 1;
    *y = next;
    return;
  }
  if (*air) {
    *vs = landing;
    *y = ground;
    *air = 0;
    return;
  }
  *vs = (ground - *y) * hk().frame_inv_dt();
  *y = ground;
  *air = 0;
}

// Mech3.exe 1.2 @ 0x004a23f0. Each support's sum of downhill pushes on steep
// polygons (0x004a2770) ends in a local the exe never reads: left out.
bool vehicle_support_ground(BasicVehicle& v, float* heights, float* top, float* slopes, int32_t* counts) {
  VehBasicHooks& h = hk();
  VehicleControls& c = *v.controls;
  VehicleMotion& m = c.motion;
  for (int i = 0; i < 100; ++i) counts[i] = 0;
  const int32_t flag = h.node_flag8(v.node);
  h.node_set_flag8(v.node, 0);
  const float* t = v.transform;
  const int32_t n = m.support_count;
  for (int32_t i = 0; i < n; ++i) {
    const Vec3c& s = m.support[i];
    m.support[8 + i] = {((s.x * t[0] + s.y * t[3]) + s.z * t[6]) + t[9], ((s.x * t[1] + s.y * t[4]) + s.z * t[7]) + t[10],
                        ((s.x * t[2] + s.y * t[5]) + s.z * t[8]) + t[11]};
  }
  static zcls::GroundHits recs[8];  // 4 on the exe's stack (every vehicle has 4 supports)
  for (auto& r : recs) r = zcls::GroundHits{};
  h.ground_many(&m.support[8].x, n, 500.0f, recs);
  h.node_set_flag8(v.node, flag);
  bool missing = false;
  for (int32_t i = 0; i < n; ++i)
    if (recs[i].count == 0) {
      missing = true;
      break;
    }
  *top = -200.0f;
  if (missing) return false;
  for (int32_t i = 0; i < n; ++i) {
    int32_t best = 0, water = 0;
    float surface = 0;
    heights[i] = ground_pick(recs[i], m.support[8 + i].y, 4.0f, &best, 1, &water, &surface);
    const zcls::GroundHit& g = recs[i].hit[best];
    *top = *top > surface ? *top : surface;
    slopes[i] = -(c.velocity.x * g.normal[0] + c.velocity.z * g.normal[2]);
    if (i == 0 && recs[0].count > 0) {
      uint8_t rec[4];
      std::memcpy(rec, &g.zone[0], 4);
      const uint8_t* cam = h.camera_zones();
      int32_t zone = cam ? zmod::zone_common(cam, rec) : 0xff;
      if (zone == -1) zone = rec[1];
      v.x64 = rec[0];  // 0x005a80d0
      std::memcpy(v.zones, rec + 1, 3);
      h.node_set_zone(v.node, static_cast<uint8_t>(zone));
    }
    ++counts[water];
    // The exe's quicksand (3) and lava (4) supports sit 0.3 under the
    // surface; the pick's flag is only ever 0 or 1.
    if ((water == 3 || water == 4) && !(heights[i] > *top)) heights[i] = *top - 0.3f;
  }
  return true;
}

// Mech3.exe 1.2 @ 0x004abf70
void height_on_plane(float pos[3], const float at[3], const float n[3]) {
  if (n[1] == 0.0f || std::isnan(n[1])) {
    pos[1] = at[1];
    return;
  }
  const float r = 1.0f / n[1];
  pos[1] = ((pos[0] - at[0]) * -(r * n[0]) + -(n[2] * r) * (pos[2] - at[2])) + at[1];
}

// Mech3.exe 1.2 @ 0x004a3e40
void vehicle_forward_from_up(BasicVehicle& v) {
  const Vec3c& u = v.controls->up;
  if (u.y == 0.0f || std::isnan(u.y)) return;
  float f[3] = {v.forward.x, -((u.x * v.forward.x + v.forward.z * u.z) / u.y), v.forward.z};
  vec_normalise(f);
  v.x18 = {f[0], f[1], f[2]};
}

// Mech3.exe 1.2 @ 0x004a3ed0
void vehicle_matrix_build(BasicVehicle& v) {
  float* t = v.transform;
  const Vec3& f = v.x18;
  const Vec3c& u = v.controls->up;
  t[0] = f.y * u.z - f.z * u.y;
  t[1] = f.z * u.x - f.x * u.z;
  t[2] = f.x * u.y - f.y * u.x;
  t[3] = u.x, t[4] = u.y, t[5] = u.z;
  t[6] = -f.x, t[7] = -f.y, t[8] = -f.z;
  const float* p = hk().entity_position(v);
  t[9] = p[0], t[10] = p[1], t[11] = p[2];
}

// Mech3.exe 1.2 @ 0x004a1760
int32_t vehicle_suspension(BasicVehicle& v) {
  VehBasicHooks& h = hk();
  VehicleControls& c = *v.controls;
  VehicleMotion& m = c.motion;
  float heights[8] = {}, slopes[8] = {}, top = 0;
  int32_t counts[100];
  if (!vehicle_support_ground(v, heights, &top, slopes, counts)) return 0;
  if (counts[3] > 1) {
    c.top_speed = m.rates[1] * m.quicksand_slowdown;
    c.top_turn = m.turns[1] * m.quicksand_slowdown;
  } else if (counts[4] > 1) {
    c.top_speed = m.rates[1] * m.lava_slowdown;
    c.top_turn = m.turns[1] * m.lava_slowdown;
  } else {
    c.top_speed = m.rates[1];
    c.top_turn = m.turns[1];
  }
  // The supports' vertical speeds (support_word[0..], floats) and in-air
  // flags ([8..]), worked on here and stored back.
  float vs[8];
  int32_t air[8];
  std::memcpy(vs, m.support_word, sizeof vs);
  std::memcpy(air, m.support_word + 8, sizeof air);
  const int32_t n = m.support_count;
  float old[8];
  int32_t lowest = 0;  // the exe's is unset when no height is under 5000
  float low = 5000.0f;
  for (int32_t i = 0; i < n; ++i) {
    old[i] = vs[i];
    if (low > heights[i]) low = heights[i], lowest = i;
  }
  int32_t others[3] = {};
  int32_t k = 0;
  bool all_air = true;
  for (int32_t i = 0; i < n; ++i) {
    if (i == lowest) continue;
    if (k < 3) others[k++] = i;
    vehicle_support_step(&m.support[8 + i].y, &old[i], heights[i], &air[i], c.x1c, slopes[i]);
    vs[i] = old[i];
    all_air = all_air && air[i] != 0;
  }
  c.x20 = all_air ? 1 : 0;
  int32_t best = others[0];  // the exe's is unset when every speed is past 10000
  float least = 10000.0f;
  for (int32_t o : others) {
    const float a = std::fabs(vs[o]);
    if (a < least) least = a, best = o;
  }
  int32_t ia = (best + 1) % 4;
  if (ia == lowest) ia = (lowest + 1) % 4;
  int32_t ib = (ia + 1) % 4;
  if (ib == lowest) ib = (lowest + 1) % 4;
  const float v0 = vs[best];
  c.velocity.y = v0;
  c.x70.y = v0;
  const float d1 = vs[ia] - v0, d2 = vs[ib] - v0;
  const int32_t tri[3] = {ia, ib, lowest};
  Vec3c e[3];
  for (int j = 0; j < 3; ++j) {
    const Vec3c& p = m.support[tri[j]];
    const Vec3c& q = m.support[best];
    e[j] = {p.x - q.x, p.y - q.y, p.z - q.z};
  }
  float px, pz;
  if (c.x20) {
    px = (c.angles.x - -0.5236f) * -0.7f;
    if (!(px > -0.7f)) px = -0.7f;
    pz = c.angles.z * -0.7f;
  } else {
    const float inv = 1.0f / (e[1].z * e[0].x - e[0].z * e[1].x);
    px = (d1 * e[1].x - d2 * e[0].x) * inv;
    pz = (d1 * e[1].z - d2 * e[0].z) * inv;
  }
  const float kd = zmth::exp_neg(m.a_damping * h.frame_dt());
  const float rest = 1.0f - kd;
  c.x50.x = kd * c.x50.x + px * rest;
  c.x50.z = kd * c.x50.z + pz * rest;
  for (int j = 0; j < 3; ++j) vs[tri[j]] = (e[j].x * c.x50.z - e[j].z * c.x50.x) + v0;
  float nrm[3];
  zmth::poly_normal(&m.support[8 + others[0]].x, &m.support[8 + others[2]].x, &m.support[8 + others[1]].x, nrm);
  const float* p = h.entity_position(v);
  float pos[3] = {p[0], p[1], p[2]};
  height_on_plane(pos, &m.support[8 + others[0]].x, nrm);
  h.set_position(v, pos);  // vt+0x18
  c.up = {nrm[0], nrm[1], nrm[2]};
  vehicle_forward_from_up(v);
  vehicle_matrix_build(v);
  zmth::rotate_y(nrm, nrm, -c.angles.y);
  c.angles.x = static_cast<float>(std::asin(double{nrm[2]}));
  c.angles.z = static_cast<float>(std::asin(-double{nrm[0]}));
  if (c.x20) {
    auto settle = [&](float& angle, float& rate) {
      if (angle > 0.5236f) {
        angle = 0.5236f, rate = 0.0f;
        for (int i = 0; i < 4; ++i) if (vs[i] > 0.0f) vs[i] = 0.0f;
      }
      if (!(angle >= -0.5236f)) {
        angle = -0.5236f, rate = 0.0f;
        for (int i = 0; i < 4; ++i) if (vs[i] > 0.0f) vs[i] = 0.0f;
      }
    };
    settle(c.angles.x, c.x50.x);
    settle(c.angles.z, c.x50.z);
  }
  std::memcpy(m.support_word, vs, sizeof vs);
  std::memcpy(m.support_word + 8, air, sizeof air);
  return 1;
}

}  // namespace mech3

