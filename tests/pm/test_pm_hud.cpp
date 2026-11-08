// Pirate's Moon's HUD at any size (resolution.md, hud.md).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>

#include "check.h"
#include "gamez/zutil/zfix.h"
#include "mech3/hud.h"
#include "mech3/options.h"

// Pirate's Moon's static (0x0043dfa0, hud.md section 21; pm-tests.md
// front-end item 13): its quad is drawn on the 3D frame, so a panel's canvas
// rect is put onto the frame as the panels' own quads are. At 16:9 (853x480
// on 1920x1080) the weapons' rect landed at 742..852 frame pixels, a small
// patch above the compass, and the MFD's in the middle of the view.
TEST(pm_static_on_the_frame_at_16_9) {
  const mech3::Options keep = mech3::options();
  const bool any = zfix::options().any_resolution;
  zfix::options().any_resolution = true;
  struct Hooks : mech3::HudHooks {
    float v[4][8] = {};
    int quads = 0;
    void static_quad(const StaticNoise&, const float q[4][8]) override { std::memcpy(v, q, sizeof v), ++quads; }
  } hooks;
  mech3::set_hud_hooks(&hooks);
  void* storage = ::operator new(sizeof(mech3::Hud));
  std::memset(storage, 0, sizeof(mech3::Hud));
  auto* hud = new (storage) mech3::Hud(nullptr);
  auto corners = [&](int32_t mode, const ztwod::Rect& r, float out[4]) {
    mech3::set_vmode(mode);
    hud->static_quad(0.5f, r);
    out[0] = std::min({hooks.v[0][0], hooks.v[1][0], hooks.v[2][0], hooks.v[3][0]});
    out[1] = std::min({hooks.v[0][1], hooks.v[1][1], hooks.v[2][1], hooks.v[3][1]});
    out[2] = std::max({hooks.v[0][0], hooks.v[1][0], hooks.v[2][0], hooks.v[3][0]});
    out[3] = std::max({hooks.v[0][1], hooks.v[1][1], hooks.v[2][1], hooks.v[3][1]});
  };
  float wide[4], old[4];
  corners(13, ztwod::Rect{742, 30, 852, 133}, wide);  // the weapons' panel at 16:9
  corners(5, ztwod::Rect{529, 30, 639, 133}, old);    // and at 640x480: as the exe
  std::printf("  static at mode 13: %.1f,%.1f..%.1f,%.1f; at mode 5: %.1f,%.1f..%.1f,%.1f\n", wide[0], wide[1], wide[2],
              wide[3], old[0], old[1], old[2], old[3]);
  EXPECT(hooks.quads == 2);
  // 742 x 1920/853 = 1670.2, 852 x 1920/853 = 1917.7; 30 x 2.25 = 67.5, 133 x 2.25 = 299.25 (less the half pixel)
  EXPECT(std::fabs(wide[0] - 1669.0f) < 2.0f && std::fabs(wide[2] - 1917.0f) < 2.0f);
  EXPECT(std::fabs(wide[1] - 67.0f) < 2.0f && std::fabs(wide[3] - 298.5f) < 2.0f);
  EXPECT(old[0] == 528.5f && old[1] == 29.5f && old[2] == 638.5f && old[3] == 132.5f);
  hud->~Hud();
  ::operator delete(storage);
  mech3::set_hud_hooks(nullptr);
  mech3::options() = keep;
  zfix::options().any_resolution = any;
}
