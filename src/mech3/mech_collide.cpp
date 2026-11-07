// Mech collision (docs/movement-collision.md). Original file unknown: the code
// sits between mech_anim.cpp and mech_data.cpp (named mech_collide after the
// link order).
#include "mech3/mech_collide.h"

#include <cmath>
#include <cstring>

#include "crt/msvc_math.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zgeometry/zgeo_x87.h"
#include "gamez/zmath/zmth_vector.h"
#include "gamez/zmodel/gmod_cull.h"
#include "gamez/zutil/zfix.h"
#include "mech3/mech.h"
#include "mech3/mech_move.h"
#include "mech3/mission_world.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

namespace {
using zgeo::X;
CollideCalls g_calls;

constexpr bool kSkiProbes = true;      // byte 0x005e24d4: probe z from the skis' length
constexpr float kSkiReach = 1.1f;      // 0x005e24d8
constexpr float kBumpRadius = 9.0f;    // 0x005e24dc
constexpr float kCeilingNy = -0.3f;    // 0x005e24e0
constexpr float kWallNy = 0.78f;       // 0x005e24e4
constexpr float kCrushVolume = 180.0f; // 0x005e24e8
constexpr float kTurnProbes = 3.3f;    // 0x005bd380
constexpr int kTopProbe = 10, kSoundBump = 0xd;

const Vec3* position_of(Mech& m) { return skis_calls().position ? skis_calls().position(m) : nullptr; }

void set_position(Mech& m, const Vec3& p) {
  if (g_calls.set_position) g_calls.set_position(m, &p);
}

// 0x0051e280: v reflected off the plane of n (n.v == 0 gives -v).
Vec3 reflect(const Vec3& n, const Vec3& v) {
  const float d = ((X(n.x) * v.x + X(n.y) * v.y) + X(n.z) * v.z).f();
  if (d == 0.0f) return {v.x * -1.0f, v.y * -1.0f, v.z * -1.0f};  // 0x005ce028
  const X k = -X(d);
  const Vec3 s{(k * n.x).f(), (k * n.y).f(), (k * n.z).f()};
  const Vec3 t{(X(v.x) + s.x).f(), (X(v.y) + s.y).f(), (X(v.z) + s.z).f()};
  return {(X(s.x) + t.x).f(), (X(s.y) + t.y).f(), (X(s.z) + t.z).f()};
}

// 0x004020d0 at the x87's precision.
void normalize(Vec3& v) {
  const X len = sqrt((X(v.x) * v.x + X(v.z) * v.z) + X(v.y) * v.y);
  if (len.f() == 0.0f) return;
  const X inv = X(1.0f) / len;
  v = {(inv * v.x).f(), (inv * v.y).f(), (inv * v.z).f()};
}

// 0x0051e210 at the x87's precision; v.y as it was.
void flat_normalize(Vec3& v) {
  const float len = sqrt((X(v.x) * v.x + X(0.0f) * 0.0f) + X(v.z) * v.z).f();
  const X inv = len == 0.0f ? X(len) : X(1.0f) / len;
  v.x = (inv * v.x).f();
  v.z = (X(v.z) * inv).f();
}
}  // namespace

CollideCalls& collide_calls() { return g_calls; }

// Mech3.exe 1.2 @ 0x00422c60
void Collider::clear_lists() {
  walls.clear();
  vehicles.clear();
  mechs.clear();
  ceilings.clear();
  touches.clear();
}

// Mech3.exe 1.2 @ 0x00422500
void Collider::place(const float m[12]) {
  for (int32_t i = 0; i < count; ++i) {
    const Vec3& l = local[i];
    world[i] = {((l.x * m[0] + l.y * m[3]) + l.z * m[6]) + m[9], ((l.x * m[1] + l.y * m[4]) + l.z * m[7]) + m[10],
                ((l.x * m[2] + l.y * m[5]) + l.z * m[8]) + m[11]};
    last[i] = world[i];
  }
}

// Mech3.exe 1.2 @ 0x00418f70: the turn rate at the current throttle.
float Mech::turn_rate_now() const {
  return (1.0f - std::fabs(controls->throttle) * 0.8f) * turn_rate;  // 0x005bcdf8, 0x005e1fcc
}

// Mech3.exe 1.2 @ 0x00422360. Rows of three at 0.4 of the box's height at
// the skis' ends (the box's, doubled, without skis), four at the feet and
// one at the top.
void Mech::setup_collider() {
  if (g_calls.refresh_box) g_calls.refresh_box(node);
  static const float kNone[6] = {};
  const float* b = g_calls.node_box ? g_calls.node_box(node) : kNone;
  const float cx = (b[0] + b[3]) * 0.5f, cy = (b[1] + b[4]) * 0.4f, cz = (b[2] + b[5]) * 0.5f;
  const float zf = kSkiProbes && skis ? -(skis->half * kSkiReach) : b[2] + b[2];
  const float zb = kSkiProbes && skis ? skis->half * kSkiReach : b[5] + b[5];
  Vec3* p = collider.local;
  p[0] = {b[0], cy, zf}, p[1] = {cx, cy, zf}, p[2] = {b[3], cy, zf};
  p[3] = {b[0], cy, zb}, p[4] = {cx, cy, zb}, p[5] = {b[3], cy, zb};
  const float fx = foot(0)->translation->x;
  p[6] = {fx, 0.75f, b[2]};
  p[7] = {-fx, 0.75f, b[2]};
  p[8] = {fx, 0.75f, -b[2]};
  p[9] = {-fx, 0.75f, -b[2]};
  p[10] = {cx, b[4], cz};
}

// Mech3.exe 1.2 @ 0x00423010: each crossing of one segment into its list.
void Mech::sort_hits(const zcls::SegmentHits& rec, const Vec3& from, const Vec3& to, int32_t probe) {
  for (int32_t j = 0; j < rec.count; ++j) {
    const zcls::SegmentHit& h = rec.hit[j];
    const float ny = h.normal[1];
    Handle n = h.node;
    std::vector<CollisionHit>* list = nullptr;
    if (g_calls.node_mech && g_calls.node_mech(n)) list = &collider.mechs;
    else if (g_calls.node_entity && g_calls.node_entity(n)) list = &collider.vehicles;
    else if (probe == kTopProbe && ny < kCeilingNy) list = &collider.ceilings;
    else if (n && g_calls.node_touchable && g_calls.node_touchable(n)) list = &collider.touches;
    else if ((probe < 6 && ny < kWallNy && -kWallNy < ny) || (airborne() && probe < 10 && ny < kWallNy))
      list = &collider.walls;
    if (!list) continue;
    CollisionHit e;
    e.hit = h;
    e.from = from;
    e.to = to;
    e.probe = probe;
    list->push_back(e);
  }
}

// Mech3.exe 1.2 @ 0x00422ef0. The mech's own node is taken out of the query
// and put back as intersecting, whatever it was before.
int32_t Mech::collide_query(const float* list, int32_t points) {
  float segs[24 * 3];
  for (int32_t i = 0; i < points; ++i) std::memcpy(segs + 3 * i, list + 4 * i, 12);
  static zcls::SegmentHits recs[12];  // on the exe's stack
  auto intersect = [&](int on) {
    if (g_calls.node_intersect) {
      g_calls.node_intersect(node, on);
    } else if (auto* n = static_cast<zcls::Node*>(node)) {
      n->set_flags(on ? n->flags() | 0x10 : n->flags() & ~0x10u);
    }
  };
  intersect(0);
  // FIX (bug), W58: an AI mech out of the camera's zones has their walls too (Skis::update).
  const int32_t zone_mode = zmod::zones().mode;
  if (zfix::options().bugs && this != world().mission_player) zmod::zones().mode = 0;
  if (g_calls.segments) g_calls.segments(segs, points, recs);
  else if (zcls::Node* w = world_hooks().world) zcls::segment_query(w, segs, points, recs);
  zmod::zones().mode = zone_mode;
  intersect(1);
  for (int32_t i = 0; i < points; i += 2) {
    int32_t probe;
    std::memcpy(&probe, list + 4 * i + 3, 4);
    const float* s = segs + 3 * i;
    sort_hits(recs[i >> 1], {s[0], s[1], s[2]}, {s[3], s[4], s[5]}, probe);
  }
  return collider.any() ? 0 : 1;
}

// Mech3.exe 1.2 @ 0x00422d20. Turning more than moving: the four corners;
// moving: the row ahead or behind, and only for the player the two foot
// probes on that side; the top probe always, from probe 0's height.
void Mech::collide_segments() {
  Collider& c = collider;
  const Vec3& v = vec_3c;
  const X vsq = (X(v.x) * v.x + X(v.y) * v.y) + X(v.z) * v.z;
  const X psq = (X(push.x) * push.x + X(push.y) * push.y) + X(push.z) * push.z;
  const X moving = psq + vsq;
  c.clear = 1;
  bool use[11] = {};
  if (abs(X(v.z)) < abs(X(vec_58.y) * kTurnProbes)) use[0] = use[2] = use[3] = use[5] = true;
  const bool player = this == world().mission_player;
  if (moving > X(0.0f)) {
    if (v.z < 0.0f) {
      use[0] = use[1] = use[2] = true;
      if (player) use[6] = use[7] = true;
    } else {
      use[3] = use[4] = use[5] = true;
      if (player) use[8] = use[9] = true;
    }
  }
  use[kTopProbe] = true;
  c.last[kTopProbe] = c.world[kTopProbe];
  c.last[kTopProbe].y = c.world[0].y;
  float list[22 * 4];
  int32_t n = 0;
  for (int32_t i = 0; i < 11; ++i) {
    if (!use[i]) continue;
    const Vec3* ends[2] = {&c.last[i], &c.world[i]};
    for (const Vec3* e : ends) {
      float* o = list + 4 * n++;
      o[0] = e->x, o[1] = e->y, o[2] = e->z;
      std::memcpy(o + 3, &i, 4);
    }
  }
  if (n) c.clear = collide_query(list, n);
}

// Mech3.exe 1.2 @ 0x004231c0. The mech is moved out by the probe's
// penetration reflected off the wall, its velocity's into-wall part kept at
// the restitution; the throttle drops to a tenth (an AI's flips), and a yaw
// kick turns the mech off the wall.
void Mech::wall_response(const CollisionHit& h) {
  Vec3 n{h.hit.normal[0], h.hit.normal[1], h.hit.normal[2]};
  if (n.x == 0.0f && n.z == 0.0f) return;
  const Vec3& lv = vec_3c;
  const float speed = sqrt((X(lv.x) * lv.x + X(lv.y) * lv.y) + X(lv.z) * lv.z).f();
  position_of(*this);
  const Vec3 p{h.hit.point[0], 0.0f, h.hit.point[2]};
  const Vec3 t{h.to.x, 0.0f, h.to.z};
  n.y = 0.0f;
  const Vec3 d{(X(t.x) - p.x).f(), 0.0f - 0.0f, (X(t.z) - p.z).f()};
  flat_normalize(n);
  Vec3 r = reflect(n, d);
  const Vec3 p2{(X(p.x) + r.x).f(), 0.0f + r.y, (X(p.z) + r.z).f()};
  const Vec3 delta{(X(p2.x) - t.x).f(), p2.y - 0.0f, (X(p2.z) - t.z).f()};
  if (const Vec3* at = position_of(*this))
    set_position(*this, {(X(at->x) + delta.x).f(), (X(at->y) + delta.y).f(), (X(at->z) + delta.z).f()});
  for (int32_t i = 0; i < collider.count; ++i) {
    Vec3& w = collider.world[i];
    w = {(X(w.x) + delta.x).f(), (X(w.y) + delta.y).f(), (X(w.z) + delta.z).f()};
  }
  const float rest = collider.restitution;
  flat_normalize(r);
  const Vec3 g{(X(r.x) - n.x).f(), r.y - 0.0f, (X(r.z) - n.z).f()};
  const Vec3 rs{(X(speed) * r.x).f(), (X(speed) * r.y).f(), (X(speed) * r.z).f()};
  const float a = (X(rs.x) * g.x + X(rs.z) * g.z).f();
  const Vec3 along{(X(a) * g.x).f(), (X(a) * g.y).f(), (X(a) * g.z).f()};
  const X b = X((X(rs.x) * n.x + X(rs.z) * n.z).f()) * rest;
  const Vec3 into{(b * n.x).f(), (b * 0.0f).f(), (b * n.z).f()};
  const Vec3 nv{(X(into.x) + along.x).f(), (X(into.y) + along.y).f(), (X(into.z) + along.z).f()};
  push = {(X(nv.x) - velocity.x).f(), 0.0f, (X(nv.z) - velocity.z).f()};
  const int32_t flips = g_calls.ai_reverses ? g_calls.ai_reverses(*this) : 0;
  controls->throttle = flips ? -controls->throttle : crt::x87_mul_f24(controls->throttle, 0.1);  // 0x005bd378
  const float sign = (X(d.z) * rs.x - X(d.x) * rs.z) < X(0.0f) ? -1.0f : 1.0f;
  vec_58.y = (X(sign) * speed * collider.yaw_kick + vec_58.y).f();
  if (this == world().mission_player && world().force_feedback && g_calls.ff_jolt) {
    const float nf[3] = {n.x, n.y, n.z};
    g_calls.ff_jolt(nf, speed / max_speed);
  }
}

// Mech3.exe 1.2 @ 0x00423600. Airborne: bounce down off it when rising, and a
// knock on the head. On foot: damage by the volume of the box hit, and a crouch.
void Mech::ceiling_response(const CollisionHit& h) {
  if (airborne()) {
    if (vec_3c.y > 0.0) {
      const Vec3 v = vec_3c;
      impulse = {0, 0, 0};
      const float vy = (X(v.y) * -0.66f).f();  // 0x005bd390
      skis->set_vy(vy);
      vec_3c = {v.x, vy, v.z};
      if (const Vec3* at = position_of(*this)) {
        Vec3 q = *at;
        q.y = ((X(h.hit.point[1]) - collider.local[kTopProbe].y) - 0.11f).f();  // 0x005bd394
        set_position(*this, q);
      }
    }
    head->apply_damage(0.002f, 0);  // 0x3b03126f
    return;
  }
  const float* b = g_calls.hit_box ? g_calls.hit_box(h.hit.node) : nullptr;
  if (!b) return;
  const float v = ((X(b[4]) - b[1]) * (X(b[5]) - b[2]) * (X(b[3]) - b[0]) / kCrushVolume * 0.25f).f();
  if (v == 0.0f) return;
  head->apply_damage(crt::x87_mul_f24(v, 0.1), 0);
  torso->apply_damage(crt::x87_mul_f24(v, 0.4), 0);  // 0x005bd3a0
  const float side = crt::x87_mul_f24(v, 0.25);      // 0x005bd3a8
  torso->apply_damage(side, 1);
  torso->apply_damage(side, 2);
  if (g_calls.crouch) g_calls.crouch(*hip, v);
  else hip->duck(v);
}

// Mech3.exe 1.2 @ 0x004237c0. Each vehicle hit: a non-allied one takes damage
// by the speed (killing it ends the response unblocked); every live one is
// shoved. Dead units are ignored by the foot probes.
void Mech::vehicle_response() {
  bool hit = false;
  // The list is walked from its head (the last sorted) as the exe's.
  for (auto it = collider.vehicles.rbegin(); it != collider.vehicles.rend(); ++it) {
    const CollisionHit& e = *it;
    Handle ent = g_calls.node_entity ? g_calls.node_entity(e.hit.node) : nullptr;
    if (!ent) continue;
    const bool dead = g_calls.entity_dead && g_calls.entity_dead(ent);
    if (dead && e.probe >= 6 && e.probe <= 9) continue;
    Handle target = g_calls.entity_target ? g_calls.entity_target(ent) : nullptr;
    if (!target) continue;
    Handle body = g_calls.entity_body ? g_calls.entity_body(ent) : nullptr;
    if (!body) continue;
    hit = true;
    const Vec3 v = velocity;
    const uint32_t team = g_calls.entity_team ? g_calls.entity_team(ent) : 0;
    if (!(team & allies)) {
      const float dmg = ((X(v.x) * v.x + X(v.z) * v.z) / (X(max_speed) * max_speed) * 20.0f).f();  // 0x005bd3b0
      if (g_calls.entity_damage && g_calls.entity_damage(target, dmg)) return;
    }
    float* bv = g_calls.body_velocity ? g_calls.body_velocity(body) : nullptr;
    float* spin = g_calls.body_spin ? g_calls.body_spin(body) : nullptr;
    if (!bv || !spin) continue;
    const X s = X(tonnage()) * (g_calls.body_mass ? g_calls.body_mass(body) : 0.0f) * 0.1f;  // 0x005bd3b4
    const Vec3 sv{(s * v.x).f(), (s * v.y).f(), (s * v.z).f()};
    const float nv[3] = {(X(bv[0]) + sv.x).f(), (X(bv[1]) + sv.y).f(), (X(bv[2]) + sv.z).f()};
    std::memcpy(bv, nv, 12);
    std::memcpy(spin, nv, 12);
    if (const float* m = g_calls.entity_matrix ? g_calls.entity_matrix(ent) : nullptr) {
      const float x = spin[0], y = spin[1], zz = spin[2];
      spin[2] = ((X(x) * m[6] + X(y) * m[7]) + X(zz) * m[8]).f();
      spin[1] = ((X(x) * m[3] + X(y) * m[4]) + X(zz) * m[5]).f();
      spin[0] = ((X(x) * m[0] + X(y) * m[1]) + X(zz) * m[2]).f();
    }
  }
  blocked[0] = hit;
  if (hit) wall_response(collider.vehicles.back());
}

// Mech3.exe 1.2 @ 0x00423a20. The head hit only: unless allied, each mech
// takes the other's share of the speed's damage, the other gets a part of
// this one's velocity; then the wall response.
void Mech::mech_response() {
  const CollisionHit& e = collider.mechs.back();
  Mech* other = g_calls.node_mech ? g_calls.node_mech(e.hit.node) : nullptr;
  if (other && !(other->team & allies)) {
    const Vec3 v = velocity;
    const float r = ((X(v.x) * v.x + X(v.z) * v.z) / (X(max_speed) * max_speed) * 0.02f).f();  // 0x005bd3b8
    const float t_other = other->tonnage();
    const float sum = (X(tonnage()) + t_other).f();
    attacker = other;
    const int32_t probe = e.probe;
    if (probe < 6 || probe == kTopProbe) {
      const float d = (X(other->tonnage()) * r / sum).f();
      torso->apply_damage((X(d) * 0.5f).f(), 0);
      const float side = (X(d) * 0.25f).f();
      torso->apply_damage(side, 1);
      torso->apply_damage(side, 2);
    } else if (probe < 10) {
      legs[probe & 1]->apply_damage((X(other->tonnage()) * r / sum).f(), 0);
    }
    const float k = (X(tonnage()) / sum).f();
    const float s = (X(collider.restitution) * k).f();
    if (g_calls.remote && g_calls.remote(*other)) {
      if (g_calls.net_shove) g_calls.net_shove(*this, *other, (X(k) * r).f(), (X(s) * v.x).f(), (X(s) * v.z).f());
    } else {
      other->attacker = this;
      other->torso->apply_damage((X(k) * r).f(), 0);
      other->velocity.x = (X(s) * v.x + other->velocity.x).f();
      other->velocity.z = (X(s) * v.z + other->velocity.z).f();
    }
  }
  blocked[1] = true;
  wall_response(collider.mechs.back());
}

// Mech3.exe 1.2 @ 0x00423c90. 20 to the touched object; while it stands,
// it blocks like a wall.
bool Mech::touch_response() {
  const CollisionHit& e = collider.touches.back();
  const Vec3 from = e.from;
  // 20 a hit (0x41a00000): contact is a one-frame bounce whatever the frame rate.
  const float r = g_calls.touch ? g_calls.touch(e.hit.node, &from, e, 20.0f) : 0.0f;
  if (!(r > 0.0f)) return false;
  wall_response(e);
  return true;
}

// Mech3.exe 1.2 @ 0x004225c0. Returns 1 when the mech stays blocked: the
// resolutions left touch, ceiling or wall hits on the second query.
bool Mech::collide() {
  Collider& c = collider;
  blocked[0] = blocked[1] = blocked[2] = blocked[3] = false;
  const float* m = transform;
  for (int32_t i = 0; i < c.count; ++i) {
    const Vec3& p = c.local[i];
    c.world[i] = {(((X(p.x) * m[0] + X(p.y) * m[3]) + X(p.z) * m[6]) + m[9]).f(),
                  (((X(p.x) * m[1] + X(p.y) * m[4]) + X(p.z) * m[7]) + m[10]).f(),
                  (((X(p.x) * m[2] + X(p.y) * m[5]) + X(p.z) * m[8]) + m[11]).f()};
  }
  collide_segments();
  if (!c.clear) {
    if (!c.vehicles.empty()) vehicle_response();
    if (!c.mechs.empty()) mech_response();
    if (!c.touches.empty() && touch_response()) blocked[3] = true;
    if (!c.ceilings.empty()) {
      ceiling_response(c.ceilings.back());
      blocked[2] = true;
    }
    if (!c.walls.empty()) {
      wall_response(c.walls.back());
      blocked[3] = true;
    }
    c.clear_lists();
  }
  bool again = blocked[0] || blocked[1] || blocked[3];
  if (!blocked[0] && !blocked[1] && !blocked[3]) {
    // The bump: the nearest mech (or the Mobile HQ) among the contacts.
    const int32_t n = g_calls.contact_count ? g_calls.contact_count(*this) : 0;
    Handle found = nullptr;
    for (int32_t i = 0; i < n && !found; ++i) {
      Handle ct = g_calls.contact(*this, i);
      if (Mech* cm = g_calls.contact_mech ? g_calls.contact_mech(ct) : nullptr) {
        if (cm != this) found = ct;
      } else if (g_calls.contact_target && g_calls.contact_target(ct)) {
        found = ct;
      }
    }
    if (found && g_calls.contact_dist2(found) < kBumpRadius * kBumpRadius) {
      g_calls.contact_dist(found);
      const Vec3* dirp = g_calls.contact_dir(found);
      Vec3 away{-dirp->x, 0.0f, -dirp->z};
      normalize(away);
      const X amt = (X(kBumpRadius) - g_calls.contact_dist(found)) * 1.1f;  // 0x005bd360
      const Vec3 off{(amt * away.x).f(), (amt * away.y).f(), (amt * away.z).f()};
      if (const Vec3* at = position_of(*this))
        set_position(*this, {(X(at->x) + off.x).f(), (X(at->y) + off.y).f(), (X(at->z) + off.z).f()});
      const X k = abs(X(vec_3c.z)) * X(0.6000000238418579);  // 0x005bd368
      const Vec3 shove{(k * away.x).f(), (k * away.y).f(), (k * away.z).f()};
      push = {(X(push.x) + shove.x).f(), (X(push.y) + shove.y).f(), (X(push.z) + shove.z).f()};
      const float dt = frame_dt;
      const Vec3 step{(X(push.x) * dt).f(), (X(push.y) * dt).f(), (X(push.z) * dt).f()};
      const Vec3 mv{(X(off.x) + step.x).f(), (X(off.y) + step.y).f(), (X(off.z) + step.z).f()};
      for (int32_t i = 0; i < c.count; ++i) {
        Vec3& w = c.world[i];
        w = {(X(w.x) + mv.x).f(), (X(w.y) + mv.y).f(), (X(w.z) + mv.z).f()};
      }
      const Vec3& d = *dirp;
      const float cy = (X(d.z) * velocity.x - X(d.x) * velocity.z).f();
      const float sign = cy < 0.0f ? -1.0f : 1.0f;
      // The exe's sign picks the constant; turn_rate_now's value is still on the x87.
      const X rate = (X(1.0) - abs(X(controls->throttle)) * 0.8f) * turn_rate;
      vec_58.y = (rate * sign * 1.8f).f();  // 0x005bd370
      controls->throttle = crt::x87_mul_f24(controls->throttle, 0.1);
      blocked[1] = true;
      if (this == world().player && move_calls().pilot_sound)
        sounds[13] = move_calls().pilot_sound(*this, kSoundBump, 1.0f, -1.0f);
    }
    again = blocked[0] || blocked[1] || blocked[3];
    if (!again && net_collision) {
      // A collision the network reported, a few frames old at most.
      const NetCollision& r = *net_collision;
      const uint32_t f = zcls::frames().frame;
      if (r.on && r.active && r.frame == f && r.sent < f && f - 5 < r.sent) {
        CollisionHit e;
        std::memset(&e.hit, 0, sizeof e.hit);
        std::memcpy(e.hit.normal, &r.normal, 12);
        std::memcpy(e.hit.point, &r.point, 12);
        e.from = r.point;
        if (const Vec3* at = position_of(*this)) e.to = *at;
        e.probe = 0;
        wall_response(e);
        blocked[3] = true;
      }
    }
  }
  bool stuck = false;
  if (again) {
    collide_segments();
    if (!c.clear) {
      stuck = !c.touches.empty() || !c.ceilings.empty() || !c.walls.empty();
      c.clear_lists();
    }
  }
  if (!stuck)
    for (int32_t i = 0; i < c.count; ++i) c.last[i] = c.world[i];
  return stuck;
}

}  // namespace mech3
