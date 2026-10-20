// The menu screens (docs/menus.md) where the differential test cannot look:
// the background drawn with a source rectangle bigger than its image
// (FIX (B), fixes.md: the pause screen's Options above 960 lines).
#include <cstring>
#include <vector>

#include "check.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zrender/zrndr_blit.h"
#include "gamez/ztwod/twod_host.h"
#include "mech3/menu_widgets.h"

namespace {
struct MenuEngine : mech3::Engine {
  int32_t w = 1280, h = 1024;
  int32_t fast_menu() override { return 1; }  // still: drawn whole, with the view as its rectangle
  int32_t display_width() override { return w; }
  int32_t display_height() override { return h; }
};

// An image exactly its pixels' size, so a read past it reads the guard after.
struct Picture {
  std::vector<uint16_t> px;
  zimg::Texture t{};
  Picture(int w, int h) : px(static_cast<size_t>(w * h) + 64, 0xdead) {
    for (int i = 0; i < w * h; ++i) px[static_cast<size_t>(i)] = static_cast<uint16_t>(0x100 + i % 0x700);
    t.width = static_cast<int16_t>(w);
    t.height = static_cast<int16_t>(h);
    t.width_again = w;
    t.pixels = px.data();
  }
};

struct Target {
  std::vector<uint16_t> px;
  Target(int w, int h) : px(static_cast<size_t>(w * h), 0) {
    zrndr::Target2D& t = zrndr::target2d();
    t.base = reinterpret_cast<uint8_t*>(px.data());
    t.width = static_cast<uint32_t>(w);
    t.height = static_cast<uint32_t>(h);
    t.pitch = static_cast<uint32_t>(2 * w);
    t.format = 6;
  }
  ~Target() { zrndr::target2d() = zrndr::Target2D{}; }
};
}  // namespace

// The source rectangle is cut at the image: its pixels drawn once, nothing
// read past it (the guard's 0xdead never reaches the screen).
TEST(blit_cuts_the_source_at_the_image) {
  Picture p(40, 30);
  Target screen(64, 48);
  const zrndr::BlitRect big{-4, -2, 64, 48};
  zrndr::blit16(&p.t, 0, 0, 0, &big);
  int drawn = 0;
  for (int y = 0; y < 48; ++y) {
    for (int x = 0; x < 64; ++x) {
      const uint16_t v = screen.px[static_cast<size_t>(y * 64 + x)];
      EXPECT(v != 0xdead);
      const bool inside = x >= 4 && x < 44 && y >= 2 && y < 32;
      if (inside) {
        EXPECT(v == p.px[static_cast<size_t>((y - 2) * 40 + (x - 4))]);
        ++drawn;
      } else {
        EXPECT(v == 0);
      }
    }
  }
  EXPECT(drawn == 40 * 30);
}

// The menus' still background (a mission's Options, or FastMenu) on a screen
// taller than its image: the view is the screen, the image is drawn whole.
TEST(still_menu_background_on_a_tall_screen) {
  MenuEngine e;
  Picture p(32, 24);  // the 1280x960 background in small: the screen is bigger than it
  Target screen(40, 32);
  e.w = 40;
  e.h = 32;
  mech3::ScrollImage bg(e);
  bg.image = &p.t;
  bg.src = &bg.view;  // as borrow_background leaves it
  EXPECT(bg.view.right == 40 && bg.view.bottom == 32);
  bg.draw();
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 40; ++x) {
      const uint16_t v = screen.px[static_cast<size_t>(y * 40 + x)];
      EXPECT(v != 0xdead);
      if (x >= 32 || y >= 24) EXPECT(v == 0);
    }
  bg.image = nullptr;
  bg.src = nullptr;
}
