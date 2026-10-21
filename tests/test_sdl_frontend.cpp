// The SDL front end's pieces that run without a window (docs/program.md):
// the DirectInput stand-in and the GDI text on SDL3_ttf.
#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "check.h"
#include "frontend/sdl/sdl_input.h"
#include "frontend/sdl/sdl_twod_host.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zrender/zrndr_tint.h"
#include "gamez/zvideo/zvid_format.h"

TEST(sdl_input_scancodes_map_to_dik_codes) {
  using sdlfe::dik_from_scancode;
  EXPECT(dik_from_scancode(SDL_SCANCODE_ESCAPE) == 0x01);
  EXPECT(dik_from_scancode(SDL_SCANCODE_1) == 0x02 && dik_from_scancode(SDL_SCANCODE_0) == 0x0b);
  EXPECT(dik_from_scancode(SDL_SCANCODE_Q) == 0x10 && dik_from_scancode(SDL_SCANCODE_P) == 0x19);
  EXPECT(dik_from_scancode(SDL_SCANCODE_A) == 0x1e && dik_from_scancode(SDL_SCANCODE_L) == 0x26);
  EXPECT(dik_from_scancode(SDL_SCANCODE_Z) == 0x2c && dik_from_scancode(SDL_SCANCODE_M) == 0x32);
  EXPECT(dik_from_scancode(SDL_SCANCODE_W) == 0x11 && dik_from_scancode(SDL_SCANCODE_S) == 0x1f);
  EXPECT(dik_from_scancode(SDL_SCANCODE_F1) == 0x3b && dik_from_scancode(SDL_SCANCODE_F10) == 0x44);
  EXPECT(dik_from_scancode(SDL_SCANCODE_F11) == 0x57 && dik_from_scancode(SDL_SCANCODE_F12) == 0x58);
  EXPECT(dik_from_scancode(SDL_SCANCODE_SPACE) == 0x39 && dik_from_scancode(SDL_SCANCODE_RETURN) == 0x1c);
  EXPECT(dik_from_scancode(SDL_SCANCODE_LCTRL) == 0x1d && dik_from_scancode(SDL_SCANCODE_RCTRL) == 0x9d);
  EXPECT(dik_from_scancode(SDL_SCANCODE_LALT) == 0x38 && dik_from_scancode(SDL_SCANCODE_RALT) == 0xb8);
  EXPECT(dik_from_scancode(SDL_SCANCODE_LSHIFT) == 0x2a && dik_from_scancode(SDL_SCANCODE_RSHIFT) == 0x36);
  EXPECT(dik_from_scancode(SDL_SCANCODE_UP) == 0xc8 && dik_from_scancode(SDL_SCANCODE_DOWN) == 0xd0);
  EXPECT(dik_from_scancode(SDL_SCANCODE_LEFT) == 0xcb && dik_from_scancode(SDL_SCANCODE_RIGHT) == 0xcd);
  EXPECT(dik_from_scancode(SDL_SCANCODE_KP_ENTER) == 0x9c && dik_from_scancode(SDL_SCANCODE_KP_5) == 0x4c);
  EXPECT(dik_from_scancode(SDL_SCANCODE_PAGEUP) == 0xc9 && dik_from_scancode(SDL_SCANCODE_DELETE) == 0xd3);
  // No two keys share a code (bar the ISO # key, where the US backslash is).
  int seen[256] = {};
  for (int s = 0; s < SDL_SCANCODE_COUNT; ++s)
    if (const uint8_t d = dik_from_scancode(static_cast<SDL_Scancode>(s))) ++seen[d];
  int dup = 0;
  for (int d = 0; d < 256; ++d) dup += seen[d] > 1 && d != 0x2b;
  EXPECT(dup == 0);
  EXPECT(dik_from_scancode(SDL_SCANCODE_F20) == 0);
}

namespace {
SDL_Event key(SDL_Scancode s, bool down, bool repeat = false) {
  SDL_Event e{};
  e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
  e.key.scancode = s, e.key.down = down, e.key.repeat = repeat;
  return e;
}
}  // namespace

TEST(sdl_input_keyboard_buffers_like_getdevicedata) {
  sdlfe::SdlInput in;
  platform::KeyEvent ev[200];
  uint32_t n = 200;
  EXPECT(in.keyboard_data(ev, &n) == platform::Input::kNotAcquired && n == 0);
  EXPECT(in.open(platform::Input::kKeyboard) && in.acquire(platform::Input::kKeyboard) == 0);
  in.on_event(key(SDL_SCANCODE_W, true));
  in.on_event(key(SDL_SCANCODE_W, true, true));  // auto-repeat: not a new press
  in.on_event(key(SDL_SCANCODE_W, false));
  in.on_event(key(SDL_SCANCODE_F20, true));        // no DIK code: dropped
  n = 1;
  EXPECT(in.keyboard_data(ev, &n) == 0 && n == 1 && ev[0].ofs == 0x11 && ev[0].data == 0x80);
  n = 200;
  EXPECT(in.keyboard_data(ev, &n) == 0 && n == 1 && ev[0].ofs == 0x11 && ev[0].data == 0);
  // 0x80 events fit; the rest overflow, as with DIPROP_BUFFERSIZE 0x80.
  for (int i = 0; i < 70; ++i) in.on_event(key(SDL_SCANCODE_A, true)), in.on_event(key(SDL_SCANCODE_A, false));
  n = 200;
  EXPECT(in.keyboard_data(ev, &n) == platform::Input::kBufferOverflow && n == 0x80);
  EXPECT(ev[0].sequence < ev[1].sequence);
  in.unacquire(platform::Input::kKeyboard);
  in.on_event(key(SDL_SCANCODE_A, true));
  EXPECT(in.key_down(0x1e));  // the held state follows even when not acquired
  in.acquire(platform::Input::kKeyboard);
  n = 200;
  EXPECT(in.keyboard_data(ev, &n) == 0 && n == 0);
}

TEST(sdl_input_mouse_counts_relative_motion) {
  sdlfe::SdlInput in;
  EXPECT(in.open(platform::Input::kMouse) && in.acquire(platform::Input::kMouse) == 0);
  SDL_Event m{};
  m.type = SDL_EVENT_MOUSE_MOTION;
  m.motion.xrel = 2.5f, m.motion.yrel = -1.0f, m.motion.x = 100, m.motion.y = 50;
  in.on_event(m);
  in.on_event(m);
  SDL_Event b{};
  b.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
  b.button.button = SDL_BUTTON_RIGHT;
  in.on_event(b);
  platform::MouseState s;
  EXPECT(in.mouse_state(&s) == 0 && s.x == 5 && s.y == -2 && s.buttons[1] == 0x80 && s.buttons[0] == 0);
  EXPECT(in.mouse_state(&s) == 0 && s.x == 0 && s.y == 0);  // counts since the last read
  EXPECT(in.mouse_buttons == 2);
  in.unacquire(platform::Input::kMouse);
  EXPECT(in.mouse_state(&s) == platform::Input::kNotAcquired);
}

namespace {
struct Mounted {
  zutl::Mounts saved = zutl::mounts();
  Mounted() {
    zutl::mounts() = zutl::Mounts{};
    zutl::mount(MW3_GAME_DIR);
  }
  ~Mounted() { zutl::mounts() = saved; }
};
bool have_fonts() { return std::filesystem::exists(std::string(MW3_GAME_DIR) + "/arial.ttf"); }
}  // namespace

TEST(sdl_host_faces_use_the_games_ttf_files) {
  EXPECT(sdlfe::SdlTwodHost::font_file("Arial") == "arial.ttf");
  EXPECT(sdlfe::SdlTwodHost::font_file("Lucida Console") == "lucon.ttf");
  EXPECT(sdlfe::SdlTwodHost::font_file("Impact") == "impact.ttf");
  EXPECT(sdlfe::SdlTwodHost::font_file("Times New Roman") == "arial.ttf");
}

TEST(sdl_host_gdi_text_measures_and_draws) {
  if (!have_fonts()) return;
  Mounted m;
  zvid::set_pixel_format(5, 6, 5);
  sdlfe::SdlTwodHost h(nullptr, nullptr, nullptr);
  const zgame::Handle f = h.CreateFontA(-16, 0, 0, 0, 400, 0, 0, 0, 0, 4, 0, 1, 0, "Arial");
  EXPECT(f != 0);
  const zgame::Handle dc = h.CreateCompatibleDC(0);
  EXPECT(h.SelectObject(dc, f) == 0);
  int32_t w1 = 0, hh = 0, w2 = 0;
  EXPECT(h.GetTextExtentPoint32A(dc, "MECH", 4, &w1, &hh) == 1);
  EXPECT(h.GetTextExtentPoint32A(dc, "MECHMECH", 8, &w2, &hh) == 1);
  EXPECT(w1 > 20 && w2 > w1 * 3 / 2 && hh >= 16 && hh <= 22);
  ztwod::TextMetrics tm;
  EXPECT(h.GetTextMetricsA(dc, &tm) == 1 && tm.height == hh && tm.ascent > 0 && tm.ascent < tm.height);
  ztwod::Rect r{};
  EXPECT(h.DrawTextA(dc, "MECH\nWARRIOR", -1, &r, 0x400) == 2 * hh);  // DT_CALCRECT
  EXPECT(r.bottom == 2 * hh && r.right > w1);
  // Drawn into a 16-bit image in the text colour, nothing else touched.
  zimg::Texture img{};
  img.flags = zimg::kTwoBytes;
  img.width = static_cast<int16_t>(r.right), img.height = static_cast<int16_t>(r.bottom);
  std::vector<uint16_t> px(static_cast<size_t>(r.right) * r.bottom, 0);
  img.pixels = px.data();
  const zgame::Handle idc = h.image_dc(&img);
  h.SelectObject(idc, f);
  h.SetTextColor(idc, 0x0000ff);  // COLORREF red
  h.SetBkMode(idc, 1);            // TRANSPARENT
  h.DrawTextA(idc, "MECH\nWARRIOR", -1, &r, 0);
  const uint16_t red = zvid::pack_bytes(255, 0, 0);
  int lit = 0, other = 0, top = 0;
  for (size_t i = 0; i < px.size(); ++i) {
    lit += px[i] == red, other += px[i] != red && px[i] != 0;
    top += px[i] == red && i < px.size() / 2;
  }
  EXPECT(lit > 50 && other == 0);  // no antialiasing: the key-colour blit keeps clean edges
  EXPECT(top > 0 && top < lit);    // two lines
  h.image_dc_release(&img, idc);
  // Opaque mode fills the text's cells with the background colour first.
  std::fill(px.begin(), px.end(), uint16_t{0});
  const zgame::Handle idc2 = h.image_dc(&img);
  h.SelectObject(idc2, f);
  h.SetTextColor(idc2, 0x0000ff);
  h.SetBkMode(idc2, 2);
  h.SetBkColor(idc2, 0x00ff00);
  h.DrawTextA(idc2, "MECH", -1, &r, 0);
  const uint16_t green = zvid::pack_bytes(0, 255, 0);
  int greens = 0;
  for (uint16_t p : px) greens += p == green;
  EXPECT(greens > 50 && px[px.size() - 1] == 0);  // the first line's cells only
  h.DeleteDC(idc2);
  // A positive height is the cell height, as GDI's.
  const zgame::Handle cell = h.CreateFontA(20, 0, 0, 0, 400, 0, 0, 0, 0, 4, 0, 1, 0, "Arial");
  h.SelectObject(dc, cell);
  EXPECT(h.GetTextMetricsA(dc, &tm) == 1 && tm.height >= 19 && tm.height <= 21);
  h.DeleteDC(dc);
  h.DeleteObject(f);
  h.DeleteObject(cell);
}

TEST(sdl_host_word_break_wraps_in_the_box) {
  if (!have_fonts()) return;
  Mounted m;
  sdlfe::SdlTwodHost h(nullptr, nullptr, nullptr);
  const zgame::Handle f = h.CreateFontA(-12, 0, 0, 0, 400, 0, 0, 0, 0, 4, 0, 1, 0, "Arial");
  const zgame::Handle dc = h.CreateCompatibleDC(0);
  h.SelectObject(dc, f);
  ztwod::Rect r{0, 0, 80, 0};
  const int32_t one = h.DrawTextA(dc, "A", -1, &r, 0x400);
  r = {0, 0, 80, 0};
  const int32_t many = h.DrawTextA(dc, "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG", -1, &r, 0x410);
  EXPECT(many >= 3 * one && r.right <= 80);
  r = {0, 0, 0, 0};
  int32_t w_amp = 0, w_plain = 0, hh = 0;
  h.DrawTextA(dc, "Save &As", -1, &r, 0x400);
  w_amp = r.right;
  h.GetTextExtentPoint32A(dc, "Save As", 7, &w_plain, &hh);
  EXPECT(w_amp == w_plain);  // the & prefix is not drawn
  h.DeleteDC(dc);
  h.DeleteObject(f);
}

// The main menu's ripple (0x00558dd0, zrndr::ripple) through the SDL host:
// on the finish target the program's buffer stands for, the ring's pixels
// move, the inside and everything past the square stay, and the clip holds.
TEST(sdl_host_ripple_moves_the_ring_on_the_screen) {
  const int w = 640, h = 480;
  std::vector<uint16_t> screen(static_cast<size_t>(w) * h);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) screen[static_cast<size_t>(y) * w + x] = static_cast<uint16_t>(x * 7 + y * 13);
  const std::vector<uint16_t> before = screen;
  zrndr::set_finish_target(screen.data(), w, h, w * 2);
  sdlfe::SdlTwodHost host(nullptr, nullptr, nullptr);
  host.ripple(320, 240, 64, 100, 7, 12.0f, 0.0f, nullptr);  // a menu ring: 3 waves of 12 inside 100
  int moved = 0, inside_moved = 0, outside_moved = 0;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const size_t i = static_cast<size_t>(y) * w + x;
      const int d2 = (x - 320) * (x - 320) + (y - 240) * (y - 240);
      const bool changed = screen[i] != before[i];
      if (d2 < 60 * 60) inside_moved += changed;
      else if (std::abs(x - 320) > 100 || std::abs(y - 240) > 100) outside_moved += changed;
      else moved += changed;
    }
  EXPECT(moved > 5000);
  EXPECT(inside_moved == 0);
  EXPECT(outside_moved == 0);

  // Clipped to the left half: nothing right of the clip changes.
  screen = before;
  const ztwod::Rect clip{0, 0, 300, h - 1};
  host.ripple(320, 240, 64, 100, 7, 12.0f, 0.0f, &clip);
  int right = 0, left = 0;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const size_t i = static_cast<size_t>(y) * w + x;
      (x >= 300 ? right : left) += screen[i] != before[i];
    }
  EXPECT(right == 0);
  EXPECT(left > 1000);
  zrndr::set_finish_target(nullptr, 0, 0, 0);
}
