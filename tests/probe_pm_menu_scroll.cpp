// Pirate's Moon's menu background (0x004885b0) and title marquee (0x004883a0), for
// test_pm_menu_scroll.py: one object, a run of frames; its state after each.
#include <cstdint>
#include <cstring>
#include <memory>

#include "gamez/zimage/zimg_library.h"
#include "mech3/app_hooks.h"
#include "mech3/menu_widgets.h"

namespace {
struct ScrollEngine : mech3::Engine {
  int32_t w = 640, h = 480, fast = 0;
  int32_t display_width() override { return w; }
  int32_t display_height() override { return h; }
  int32_t fast_menu() override { return fast; }
};

int32_t bits(float f) {
  int32_t v;
  std::memcpy(&v, &f, 4);
  return v;
}
}  // namespace

// The background: image iw x ih on a w x h screen, moving or not, the view and carries to start;
// n frames of dts. Out per frame, 34 ints: carry_x, carry_y (bits), the view (4), the pieces' flags
// (4), positions (8) and rects (16). The pieces' flags are cleared before each frame, as a draw does.
extern "C" void probe_pm_scroll(int32_t iw, int32_t ih, int32_t w, int32_t h, int32_t moving, const int32_t* view,
                                const float* carry, const float* dts, int32_t n, int32_t* out) {
  ScrollEngine e;
  e.w = w, e.h = h;
  zimg::Texture tex{};
  tex.width = static_cast<int16_t>(iw), tex.height = static_cast<int16_t>(ih);
  mech3::ScrollImage s(e);
  s.image = &tex;
  s.moving = static_cast<uint8_t>(moving);
  s.view = ztwod::Rect{view[0], view[1], view[2], view[3]};
  s.carry_x = carry[0], s.carry_y = carry[1];
  s.hide();  // Object2D::update would draw the pieces (and clear their flags); the test stubs it
  for (int32_t f = 0; f < n; ++f) {
    for (uint8_t& p : s.piece) p = 0;
    s.update(dts[f]);
    int32_t* o = out + f * 34;
    o[0] = bits(s.carry_x), o[1] = bits(s.carry_y);
    o[2] = s.view.left, o[3] = s.view.top, o[4] = s.view.right, o[5] = s.view.bottom;
    for (int i = 0; i < 4; ++i) {
      o[6 + i] = s.piece[i];
      o[10 + 2 * i] = s.piece_pos[i][0], o[11 + 2 * i] = s.piece_pos[i][1];
      const ztwod::Rect& r = s.piece_src[i];
      o[18 + 4 * i] = r.left, o[19 + 4 * i] = r.top, o[20 + 4 * i] = r.right, o[21 + 4 * i] = r.bottom;
    }
  }
  s.image = nullptr;
}

// The marquee: image iw wide at (x, y), FastMenu or not; n frames. Out per frame: x, y, carry (bits).
extern "C" void probe_pm_marquee(int32_t iw, int32_t w, int32_t x, int32_t y, float carry, int32_t fast,
                                 const float* dts, int32_t n, int32_t* out) {
  ScrollEngine e;
  e.w = w, e.fast = fast;
  zimg::Texture tex{};
  tex.width = static_cast<int16_t>(iw), tex.height = 20;
  mech3::Marquee m(e);
  m.image = &tex;
  m.set_pos(x, y);
  m.carry = carry;
  for (int32_t f = 0; f < n; ++f) {
    m.update(dts[f]);
    out[3 * f] = m.get_x(), out[3 * f + 1] = m.get_y(), out[3 * f + 2] = bits(m.carry);
  }
  m.image = nullptr;
}
