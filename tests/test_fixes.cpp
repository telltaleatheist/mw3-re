// The frame-rate fixes (zfix::Options::per_second, docs/fixes.md): the same
// second of play at 30 and 144 fps comes out the same, and the exe's
// per-frame path does not.
#include <cmath>

#include "check.h"
#include "gamez/zmodel/gmod_puff.h"
#include "gamez/zutil/zfix.h"
#include "gamez/zutil/ztimer.h"
#include "mech3/mech.h"
#include "mech3/mech_collide.h"
#include "mech3/mech_move.h"
#include "mech3/mission_world.h"

namespace {

float lava_heat(float fps, bool per_second) {
  zfix::options().per_second = per_second;
  mech3::heat_calls().heat_option = [] { return true; };
  mech3::Foot feet[2];
  mech3::Leg legs[2];
  mech3::Mech m{};
  for (int i = 0; i < 2; ++i) legs[i].foot = &feet[i], m.legs[i] = &legs[i];
  feet[0].soil = 8;
  const int frames = static_cast<int>(fps / 2);  // half a second, under the overheat line
  for (int i = 0; i < frames; ++i) mech3::update_heat(m, 1.0f / fps);
  mech3::heat_calls().heat_option = nullptr;
  zfix::options().per_second = true;
  return m.heat;
}

float smoke_speed(float fps, bool per_second) {
  zfix::options().per_second = per_second;
  zmod::PuffState& s = zmod::puff_state();
  s.particles.clear();
  s.wind = {};
  zmod::Particle p{};
  p.vel[0] = 10.0f, p.life = 100.0f, p.friction = 1.0f, p.wind_factor = 1.0f;
  s.particles.push_back(p);
  ztimer::state().dt = 1.0f / fps;
  for (int i = 0; i < static_cast<int>(fps); ++i) zmod::puff_tick();
  const float v = s.particles.front().vel[0];
  s.particles.clear();
  zfix::options().per_second = true;
  return v;
}

}  // namespace

TEST(fix_lava_heat_per_second) {
  EXPECT(std::fabs(lava_heat(30, true) - 0.75f) < 1e-3f);
  EXPECT(std::fabs(lava_heat(144, true) - 0.75f) < 1e-3f);
  EXPECT(std::fabs(lava_heat(30, false) - 0.75f) < 1e-3f);  // the exe at 30 fps
  EXPECT(lava_heat(144, false) > 3.5f * 0.75f / 2.0f);       // and nearly 5x at 144
}

TEST(fix_smoke_drag_per_second) {
  const float want = 10.0f * std::exp(-1.0f);
  EXPECT(std::fabs(smoke_speed(30, true) - want) < 0.05f);
  EXPECT(std::fabs(smoke_speed(144, true) - want) < 0.05f);
  EXPECT(smoke_speed(144, false) == 10.0f);  // the exe's table: no drag at all
}

// The movement steps (mech_move, mech_skis, mech_collide).
namespace {

mech3::Mech* bare_mech() {
  static mech3::Mech m;
  static mech3::MechControls c;
  static mech3::Torso torso;
  static mech3::Head head;
  static mech3::Hip hip;
  static mech3::Leg legs[2];
  static mech3::Foot feet[2];
  static mech3::Pilot pilot;
  static mech3::MechAnim anims[13];
  m = mech3::Mech{};
  c = mech3::MechControls{};
  c.owner = &m;
  m.controls = &c;
  m.torso = &torso, m.head = &head, m.hip = &hip, m.pilot = &pilot;
  for (int i = 0; i < 2; ++i) feet[i] = mech3::Foot{}, legs[i].foot = &feet[i], m.legs[i] = &legs[i];
  for (int i = 0; i < 13; ++i) anims[i] = mech3::MechAnim{}, anims[i].loop = 1.0f, m.anims[i] = &anims[i];
  m.remote = true;
  m.height = 10.0f;
  m.heat = 0.0f;
  return &m;
}

float carried_momentum(float fps, bool per_second) {
  zfix::options().per_second = per_second;
  mech3::Mech& m = *bare_mech();
  static mech3::MechAction act;
  act = mech3::MechAction{};
  act.loop = 10.0f;
  m.action_objects[5] = &act;
  m.actions.push_back(5);
  m.anim_state = 12;
  m.vec_3c = {10.0f, 0.0f, 0.0f};
  mech3::move_calls().skis_update = [](mech3::Mech&, float) { return false; };
  for (int i = 0; i < static_cast<int>(fps); ++i) m.update_action(1.0f / fps);
  mech3::move_calls().skis_update = nullptr;
  zfix::options().per_second = true;
  return m.momentum.x;
}

float blend_after_half_second(float fps, bool per_second) {
  zfix::options().per_second = per_second;
  mech3::Mech& m = *bare_mech();
  m.anim_state = 5;
  m.detail = 0x10;
  static float blend;
  blend = -1.0f;
  mech3::move_calls().skis_update = [](mech3::Mech&, float) { return false; };
  mech3::move_calls().gait_pose = [](mech3::Mech&, mech3::MechAnim&, float b) { blend = b; };
  const float dt = 1.0f / fps;
  for (int i = 0; i <= static_cast<int>(fps / 2); ++i) {
    m.move(dt, m.anims[4], 0.5f);
    ++m.gait_frames;
    m.gait_steps = m.gait_steps + zfix::frame_steps(dt);
  }
  mech3::move_calls() = mech3::MoveCalls{};
  zfix::options().per_second = true;
  return blend;
}

float jet_climb(float fps, bool per_second) {
  zfix::options().per_second = per_second;
  mech3::Mech& m = *bare_mech();
  m.equipment[11] = 1;
  m.controls->jump = 1.0f;
  const float g = mech3::environment().gravity;
  mech3::environment().gravity = 1000.0f;  // to reach the cap
  float sum = 0;
  for (int i = 0; i < static_cast<int>(fps); ++i) {
    m.impulse = {0, 0, 0};
    m.jet_fuel = 1.0f;
    m.jump_jets(1.0f / fps);
    sum += m.impulse.y;
  }
  mech3::environment().gravity = g;
  zfix::options().per_second = true;
  return sum;
}

float touch_damage(float fps, bool per_second) {
  zfix::options().per_second = per_second;
  mech3::Mech& m = *bare_mech();
  m.collider.touches.push_back(mech3::CollisionHit{});
  static float sum;
  sum = 0;
  mech3::collide_calls().touch = [](mech3::Handle, const mech3::Vec3*, const mech3::CollisionHit&, float d) {
    sum += d;
    return 0.0f;
  };
  m.frame_dt = 1.0f / fps;
  for (int i = 0; i < static_cast<int>(fps); ++i) m.touch_response();
  mech3::collide_calls() = mech3::CollideCalls{};
  zfix::options().per_second = true;
  return sum;
}

float wading_speed(float depth, bool fixed) {
  zfix::options().deep_water = fixed;
  mech3::Mech& m = *bare_mech();
  m.foot(0)->water_depth = depth, m.foot(1)->water_depth = depth;
  m.anim_state = 1;
  m.detail = 0x10;
  mech3::move_calls().move = [](mech3::Mech&, float, mech3::MechAnim*, float) {};
  m.select_gait(1.0f / 30);
  mech3::move_calls() = mech3::MoveCalls{};
  zfix::options().deep_water = true;
  return m.speed_target;
}

}  // namespace

TEST(fix_action_momentum_per_second) {
  const float want = 10.0f * std::pow(0.6f, 30.0f);
  EXPECT(std::fabs(carried_momentum(30, true) - want) < want * 1e-3f);
  EXPECT(std::fabs(carried_momentum(144, true) - want) < want * 1e-2f);
  EXPECT(carried_momentum(30, false) == carried_momentum(30, true));  // the exe's step at 30 fps
  EXPECT(carried_momentum(144, false) < want * 1e-6f);               // gone far sooner at 144
}

TEST(fix_gait_blend_in_per_second) {
  EXPECT(std::fabs(blend_after_half_second(30, true) - 0.75f) < 1e-5f);
  EXPECT(std::fabs(blend_after_half_second(144, true) - 0.75f) < 0.02f);
  EXPECT(blend_after_half_second(30, false) == blend_after_half_second(30, true));
  EXPECT(blend_after_half_second(144, false) == 1.0f);  // the exe: done after 20 frames, 0.14 s
}

TEST(fix_jet_cap_per_second) {
  EXPECT(std::fabs(jet_climb(30, true) - 180.0f) < 0.1f);  // 6 a frame at 30 fps
  EXPECT(std::fabs(jet_climb(144, true) - 180.0f) < 0.1f);
  EXPECT(jet_climb(30, false) == jet_climb(30, true));
  EXPECT(jet_climb(144, false) > 800.0f);  // the exe: 6 a frame at any rate
}

TEST(fix_touch_damage_per_second) {
  EXPECT(std::fabs(touch_damage(30, true) - 600.0f) < 0.1f);
  EXPECT(std::fabs(touch_damage(144, true) - 600.0f) < 0.1f);
  EXPECT(touch_damage(30, false) == touch_damage(30, true));
  EXPECT(touch_damage(144, false) == 20.0f * 144);
}

TEST(fix_deep_water_curve) {
  // Fixed: full speed at a tenth of the height down to 0.7 at full depth, no step.
  EXPECT(std::fabs(wading_speed(1.0f, true) - 1.0f) < 1e-5f);
  EXPECT(std::fabs(wading_speed(5.5f, true) - 0.85f) < 1e-5f);
  EXPECT(std::fabs(wading_speed(9.99f, true) - 0.7f) < 1e-3f);
  EXPECT(std::fabs(wading_speed(12.0f, true) - 0.7f) < 1e-5f);
  // The exe: near a standstill just under full depth, then back to 0.7.
  EXPECT(wading_speed(9.99f, false) < 0.01f);
  EXPECT(std::fabs(wading_speed(12.0f, false) - 0.7f) < 1e-5f);
}
