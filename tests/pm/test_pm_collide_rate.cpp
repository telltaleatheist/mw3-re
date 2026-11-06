// Collision's amounts are per hit, not per second (dev 2's measure, 2026-11-05: contacts
// rise far less than the frame rate): one hit does the same at any frame rate, per_second on.
#include <cmath>

#include "gamez/zclass/cls_node.h"
#include "gamez/zutil/zfix.h"
#include "mech3/mech.h"
#include "mech3/mech_collide.h"
#include "mech3/mech_move.h"
#include "mech3/mech_skis.h"
#include "check.h"

using namespace mech3;

namespace {
struct CountTorso : Torso {
  float* total = nullptr;
  void apply_damage(float d, int) override { *total += d; }
};
struct CountLeg : Leg {
  float* total = nullptr;
  void apply_damage(float d, int) override { *total += d; }
};
struct Pair {
  Mech a, b;
  MechControls ctl;
  MechData pa, pb;
  CountTorso ta, tb;
  CountLeg la, lb;
  float damage_a = 0, damage_b = 0;
  Pair() {
    pa.roster = 13, pb.roster = 4;
    a.pilot = &pa, b.pilot = &pb;
    a.controls = &ctl, ctl.owner = &a, ctl.throttle = 1.0f;
    ta.total = &damage_a, tb.total = &damage_b, la.total = &damage_a, lb.total = &damage_b;
    a.torso = &ta, b.torso = &tb, a.legs[0] = a.legs[1] = &la, b.legs[0] = b.legs[1] = &lb;
    a.max_speed = b.max_speed = 80.0f;
    a.turn_rate = 1.0f;
    a.pending_velocity = {0, 0, -40.0f};
    a.vec_3c = {0, 0, -40.0f};
    b.velocity = {0, 0, 20.0f};
  }
  void step(float dt, void (*f)(Pair&)) {
    a.frame_dt = dt;
    f(*this);
  }
};
bool near(float x, float y) { return std::fabs(x - y) <= 1e-5f * (std::fabs(x) + std::fabs(y) + 1e-3f); }
void wall(Pair& p) {
  CollisionHit e;
  e.hit.normal[0] = 1.0f, e.hit.point[0] = 1.0f, e.to = {0, 0, 0};
  p.a.wall_response(e);
}
void crash(Pair& p) { p.a.crash(p.b, 1); }
void leg_crash(Pair& p) { p.a.crash(p.b, 4); }
}  // namespace

TEST(pm_collide_wall_hit_at_any_rate) {
  Pair slow, fast;
  zfix::options().per_second = true;
  slow.step(1 / 30.0f, wall);
  fast.step(1 / 240.0f, wall);
  EXPECT(near(slow.ctl.throttle, 0.1f));
  EXPECT(near(fast.ctl.throttle, 0.1f));
  EXPECT(slow.a.vec_58.y != 0.0f);
  EXPECT(near(slow.a.vec_58.y, fast.a.vec_58.y));
}

// 0x4255f0: [0x0060463c], 20 a hit, whatever the frame.
TEST(pm_collide_touch_hit_at_any_rate) {
  static float got[2];
  static int at;
  const CollideCalls saved = collide_calls();
  collide_calls().touch = [](Handle, const Vec3*, const CollisionHit&, float d) { return got[at] = d, 0.0f; };
  zfix::options().per_second = true;
  Pair slow, fast;
  slow.a.collider.touches.emplace_back(), fast.a.collider.touches.emplace_back();
  at = 0;
  slow.a.frame_dt = 1 / 30.0f;
  EXPECT(!slow.a.touch_response());
  at = 1;
  fast.a.frame_dt = 1 / 240.0f;
  EXPECT(!fast.a.touch_response());
  collide_calls() = saved;
  EXPECT(got[0] == 20.0f && got[1] == 20.0f);
}

TEST(pm_collide_crash_hit_at_any_rate) {
  for (auto* f : {crash, leg_crash}) {
    Pair slow, fast;
    zfix::options().per_second = true;
    slow.step(1 / 30.0f, f);
    fast.step(1 / 240.0f, f);
    EXPECT(slow.damage_a > 0.0f && slow.damage_b > 0.0f);
    EXPECT(near(slow.damage_a, fast.damage_a));
    EXPECT(near(slow.damage_b, fast.damage_b));
  }
}

TEST(pm_collide_remote_crash_at_any_rate) {
  static float got_damage, got_x, got_z;
  CollideCalls& c = collide_calls();
  const CollideCalls saved = c;
  c.remote = [](Mech&) { return true; };
  c.net_shove = [](Mech&, Mech&, float d, float x, float z) { got_damage = d, got_x = x, got_z = z; };
  zfix::options().per_second = true;
  Pair slow, fast;
  slow.a.collider.restitution = fast.a.collider.restitution = 0.5f;
  slow.a.pending_velocity.x = fast.a.pending_velocity.x = 7.0f;
  slow.step(1 / 30.0f, crash);
  const float d30 = got_damage, x30 = got_x, z30 = got_z;
  fast.step(1 / 120.0f, crash);
  c = saved;
  EXPECT(z30 != 0.0f);
  EXPECT(x30 != 0.0f);
  EXPECT(near(got_x, x30));
  EXPECT(near(got_z, z30));
  EXPECT(near(got_damage, d30));
}
