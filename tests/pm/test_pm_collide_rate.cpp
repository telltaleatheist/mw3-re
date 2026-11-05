// The collision's per-frame amounts at any frame rate (zfix per_second, docs/fixes.md):
// a contact of 1/30 s does the same as two of 1/60 s. test_pm_collide.py checks the exe's path.
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
  void step(int times, float dt, void (*f)(Pair&)) {
    a.frame_dt = dt;
    for (int i = 0; i < times; ++i) f(*this);
  }
};
bool near(float x, float y) { return std::fabs(x - y) <= 1e-4f * (std::fabs(x) + std::fabs(y) + 1e-3f); }
void wall(Pair& p) {
  CollisionHit e;
  e.hit.normal[0] = 1.0f, e.hit.point[0] = 1.0f, e.to = {0, 0, 0};
  p.a.vec_3c = {0, 0, -40.0f};  // what the frame's move gives it again
  p.a.wall_response(e);
}
void crash(Pair& p) { p.a.crash(p.b, 1); }
void leg_crash(Pair& p) { p.a.crash(p.b, 4); }
}  // namespace

TEST(pm_collide_throttle_and_kick_per_second) {
  Pair once, twice;
  zfix::options().per_second = true;
  once.step(1, 1 / 30.0f, wall);
  twice.step(2, 1 / 60.0f, wall);
  EXPECT(near(once.ctl.throttle, twice.ctl.throttle));
  EXPECT(near(once.ctl.throttle, 0.1f));
  EXPECT(near(once.a.vec_58.y, twice.a.vec_58.y));
  EXPECT(once.a.vec_58.y != 0.0f);
}

TEST(pm_collide_crash_damage_per_second) {
  for (auto* f : {crash, leg_crash}) {
    Pair once, twice;
    zfix::options().per_second = true;
    once.step(1, 1 / 30.0f, f);
    twice.step(2, 1 / 60.0f, f);
    EXPECT(once.damage_a > 0.0f && once.damage_b > 0.0f);
    EXPECT(near(once.damage_a, twice.damage_a));
    EXPECT(near(once.damage_b, twice.damage_b));
  }
}

// The other's update replaces its velocity, so the remote shove is no rate: the same at any dt.
TEST(pm_collide_remote_shove_is_not_scaled) {
  static float got_damage, got_x, got_z;
  CollideCalls& c = collide_calls();
  const CollideCalls saved = c;
  c.remote = [](Mech&) { return true; };
  c.net_shove = [](Mech&, Mech&, float d, float x, float z) { got_damage = d, got_x = x, got_z = z; };
  zfix::options().per_second = true;
  Pair slow, fast;
  slow.a.collider.restitution = fast.a.collider.restitution = 0.5f;
  slow.step(1, 1 / 30.0f, crash);
  const float d30 = got_damage, z30 = got_z;
  fast.step(1, 1 / 120.0f, crash);
  c = saved;
  EXPECT(z30 != 0.0f);
  EXPECT(near(got_z, z30));
  EXPECT(near(got_damage * 4.0f, d30));  // the damage is the rate
}

TEST(pm_collide_original_path_is_per_frame) {
  Pair once, twice;
  zfix::options().per_second = false;
  once.step(1, 1 / 30.0f, crash);
  twice.step(2, 1 / 60.0f, crash);
  EXPECT(near(2.0f * once.damage_a, twice.damage_a));
  zfix::options().per_second = true;
}

// Drowning (0x0041a100): the structure's quarter a frame goes by time.
namespace {
float g_structure = 1.0f;
struct DrownPart : Head {
  void set_internal(float v, int) override { g_structure = v; }
  void set_armor(float, int) override {}
};
struct DrownTorso : Torso {
  void set_internal(float, int) override {}
};
struct DrownLeg : Leg {
  void set_internal(float, int) override {}
};
struct DrownArm : Arm {
  void set_internal(float, int) override {}
};
float drown(int times, float dt) {
  static Mech m;
  static Skis skis;
  static DrownPart head;
  static DrownTorso torso;
  static DrownLeg legs[2];
  static DrownArm arms[2];
  static Foot foot;
  static float place[12];
  m = Mech{};
  skis = Skis{};
  skis.surface[1] = skis.surface[4] = 30.0f;  // 30 m over a mech at 0
  m.skis = &skis, m.head = &head, m.torso = &torso, m.transform = place;
  m.height = 10.0f;
  for (int i = 0; i < 2; ++i) legs[i].foot = &foot, m.legs[i] = &legs[i], m.arms[i] = &arms[i];
  HeatCalls& c = heat_calls();
  const HeatCalls saved = c;
  c = HeatCalls{};
  c.health = [](Mech&) { return g_structure; };
  g_structure = 1.0f;
  for (int i = 0; i < times; ++i) update_heat(m, dt);
  c = saved;
  return g_structure;
}
}  // namespace

TEST(pm_drowning_per_second) {
  zfix::options().per_second = true;
  const float once = drown(1, 1 / 30.0f), twice = drown(2, 1 / 60.0f);
  EXPECT(near(once, 0.75f));
  EXPECT(near(once, twice));
  zfix::options().per_second = false;
  EXPECT(near(drown(2, 1 / 60.0f), 0.5625f));
  zfix::options().per_second = true;
}
