// The main menu's pieces (mech3/menu_widgets, docs/mainmenu.md) where the
// differential test cannot look: the frame-rate fix (FIX (B), fixes.md).
#include "check.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zutil/zfix.h"
#include "mech3/menu_widgets.h"

namespace {
struct MenuEngine : mech3::Engine {
  int32_t fast_menu() override { return 0; }
  int32_t display_width() override { return 640; }
  int32_t display_height() override { return 480; }
};
struct MenuMotion {
  int32_t scrolled, slid;
  float ripple_seconds;
};
// One second of the background, the title and a ripple at `fps`, hidden.
MenuMotion menu_motion(float fps, bool per_second) {
  zfix::options().per_second = per_second;
  MenuEngine e;
  zimg::Texture big{};
  big.width = 2000, big.height = 2000;
  mech3::ScrollImage bg(e);
  bg.image = &big;
  bg.flags |= ztwod::kHidden;
  mech3::Marquee title(e);
  title.image = &big;
  title.x = 1000;
  title.flags |= ztwod::kHidden;
  mech3::SlideButton b(e, -1);
  b.flags |= ztwod::kHidden;
  b.init_ripple();
  b.max_radius = 1000;
  b.restart_ripple();
  const float dt = 1.0f / fps;
  float rippled = 0.0f;
  for (int i = 0; i < static_cast<int>(fps); ++i) {
    bg.update(dt);
    title.update(dt);
    if (b.rippling) rippled += dt;
    b.update(dt);
  }
  bg.image = title.image = nullptr;
  zfix::options().per_second = true;
  return {bg.view.left, 1000 - title.x, rippled};
}
}  // namespace

// The background crawls 30 pixels a second, MECH3TEXT 60 and a ripple lasts
// about 0.6 s (18 frames at 30 fps) whatever the frame rate; the exe's steps
// go by frames.
TEST(main_menu_moves_per_second) {
  for (float fps : {30.0f, 60.0f, 144.0f}) {
    const MenuMotion m = menu_motion(fps, true);
    EXPECT(m.scrolled >= 29 && m.scrolled <= 30);
    EXPECT(m.slid >= 58 && m.slid <= 60);
    EXPECT(m.ripple_seconds > 0.55f && m.ripple_seconds < 0.62f);  // 7 to 2 at 9 a second, the last frame drawn
  }
  const MenuMotion raw = menu_motion(144.0f, false);
  EXPECT(raw.scrolled == 144 && raw.slid == 288);
  EXPECT(raw.ripple_seconds > 17.5f / 144.0f && raw.ripple_seconds < 18.5f / 144.0f);
}
