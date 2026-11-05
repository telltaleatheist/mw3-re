// The collision's per-frame amounts at any frame rate (zfix per_second, docs/fixes.md):
// a contact of 1/30 s does the same as two of 1/60 s. test_pm_collide.py checks the exe's path.
#include <cmath>

#include "gamez/zclass/cls_node.h"
#include "gamez/zutil/zfix.h"
#include "mech3/mech.h"
#include "mech3/mech_collide.h"
#include "mech3/mech_move.h"
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

TEST(pm_collide_original_path_is_per_frame) {
  Pair once, twice;
  zfix::options().per_second = false;
  once.step(1, 1 / 30.0f, crash);
  twice.step(2, 1 / 60.0f, crash);
  EXPECT(near(2.0f * once.damage_a, twice.damage_a));
  zfix::options().per_second = true;
}
