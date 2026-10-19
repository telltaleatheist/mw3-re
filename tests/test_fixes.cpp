// The frame-rate fixes (zfix::Options::per_second, docs/fixes.md): the same
// second of play at 30 and 144 fps comes out the same, and the exe's
// per-frame path does not.
#include <cmath>

#include "check.h"
#include "gamez/zmodel/gmod_puff.h"
#include "gamez/zutil/zfix.h"
#include "gamez/zutil/ztimer.h"
#include "mech3/mech.h"

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
