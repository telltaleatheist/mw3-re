// The SDL front end's pieces that run without a window (docs/program.md):
// the DirectInput stand-in and the GDI text on SDL3_ttf.
#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <chrono>
#include <csignal>
#include <thread>
#if !defined(_WIN32)
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#endif
#include <string>
#include <vector>

#include "check.h"
#include "frontend/sdl/gdi_font_size.h"
#include "frontend/sdl/sdl_fit.h"
#include "frontend/sdl/sdl_input.h"
#include "frontend/sdl/sdl_twod_host.h"
#include "gamez/zinterp/zinterp.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zutil/zfix.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zrender/zrndr_blit.h"
#include "gamez/zrender/zrndr_tint.h"
#include "gamez/zvideo/zvid_format.h"
#include "gamez/ztwod/twod_host.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zinput/zinput.h"
#include "gamez/zsound/zsnd.h"
#include "mech3/app.h"
#include "mech3/app_engine.h"
#include "mech3/app_hud.h"
#include "mech3/app_mech.h"
#include "mech3/app_states.h"
#include "mech3/mech_control.h"
#include "mech3/mission.h"
#include "mech3/options.h"
#include "mech3/pilot.h"
#include "platform/platform.h"
#include "platform/render3d.h"
#include "platform/silent_sound.h"
#include "mech3/hud.h"

#include <cstdio>
#include <map>

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

// GDI's font sizes and widths (twod.md section 6, Text widths): a cell
// height takes the largest VDMX size that fits (else the win metrics
// scaled), the ascent and descent its record's; the advances are the
// hinted ones the fonts' hdmx tables record for GDI, and nothing is kerned.
TEST(sdl_host_fonts_take_gdis_sizes_and_widths) {
  sdlfe::GdiFaceMetrics m;
  m.upem = 2048, m.win_ascent = 1854, m.win_descent = 434;  // Arial's
  m.vdmx = {{9, 9, -3}, {10, 10, -3}, {11, 11, -3}, {12, 12, -3}, {13, 13, -3}, {14, 13, -3}, {15, 14, -3}};
  auto size = [&](int h) { return sdlfe::gdi_font_size(m, h); };
  // 16: 13 and 14 both have a 16 cell; the first that matches (Wine's load_VDMX).
  EXPECT(size(16).ppem == 13 && size(16).ascent == 13 && size(16).descent == 3);
  EXPECT(size(12).ppem == 9 && size(12).ascent == 9 && size(12).descent == 3);  // as Windows (Wine's tests)
  EXPECT(size(-10).ppem == 10 && size(-10).ascent == 10 && size(-10).descent == 3);
  EXPECT(size(30).ppem == 27);  // past the table: scaled (30 x 2048 / 2288, rounded)
  EXPECT(size(-40).ppem == 40 && size(-40).ascent == 36 && size(-40).descent == 8);
  m.vdmx.clear();
  EXPECT(size(16).ppem == 14 && size(12).ppem == 11);  // 12: 10.74 rounds to 11, 11's cell 12.3 -> 12
  m.upem = 1000, m.win_ascent = 1150, m.win_descent = 350;
  EXPECT(size(7).ppem == 4);  // 4.67 rounds to 5, whose cell (7.5 -> 8) is over 7: one less
  if (!have_fonts()) return;
  Mounted mnt;
  zvid::set_pixel_format(5, 6, 5);
  sdlfe::SdlTwodHost h(nullptr, nullptr, nullptr);
  const zgame::Handle dc = h.CreateCompatibleDC(0);
  auto extent = [&](int32_t height, const char* face, const char* text, int32_t* cy = nullptr) {
    const zgame::Handle f = h.CreateFontA(height, 0, 0, 0, 400, 0, 0, 0, 0, 4, 0, 1, 0, face);
    h.SelectObject(dc, f);
    int32_t w = 0, hh = 0;
    h.GetTextExtentPoint32A(dc, text, static_cast<int32_t>(std::strlen(text)), &w, &hh);
    if (cy) *cy = hh;
    h.SelectObject(dc, 0);
    h.DeleteObject(f);
    return w;
  };
  // Arial's hdmx at 11, 13, 15 and 16 pixels: "Current Tonnage" 81, 95, 110, 116.
  EXPECT(extent(-11, "Arial", "Current Tonnage") == 81);
  EXPECT(extent(-13, "Arial", "Current Tonnage") == 95);
  EXPECT(extent(-15, "Arial", "Current Tonnage") == 110);
  EXPECT(extent(-16, "Arial", "Current Tonnage") == 116);
  EXPECT(extent(-16, "Arial", "AVATAR") == extent(-16, "Arial", "A") * 3 + extent(-16, "Arial", "V") +
                                              extent(-16, "Arial", "T") + extent(-16, "Arial", "R"));  // no kerning
  int32_t cy = 0;
  extent(16, "Arial", "x", &cy);
  EXPECT(cy == 16);  // the salvage screen's lab_large fonts: 13 pixels, a 16 cell
  extent(12, "Arial", "x", &cy);
  EXPECT(cy == 12);
  extent(12, "Lucida Console", "x", &cy);
  EXPECT(cy == 12);  // no VDMX: 1616 + 432 units scaled
  h.DeleteDC(dc);
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

// zImage's release (0x00517b10) as the host does it: a named image is shared
// and counted, a fresh copy read once the count is back to 0; a grab is
// freed on its release, and a second release of it is caught (FIX (bug))
// instead of freeing it twice.
TEST(sdl_host_image_release_counts_and_frees) {
  if (!std::filesystem::exists(std::string(MW3_GAME_DIR) + "/zbd/rimage.zbd")) return;
  zutl::Mounts saved = zutl::mounts();
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(std::string(MW3_GAME_DIR) + "/zbd");
  for (int l = 0; l < 2; ++l) zimg::library_list(l).clear();
  zimg::register_image_library(1);
  {
    sdlfe::SdlTwodHost h(nullptr, nullptr, nullptr);
    zimg::Texture* a = h.image_get("dmg_head_blue");
    zimg::Texture* b = h.image_get("dmg_head_blue");
    EXPECT(a && a == b);  // one image, counted twice
    EXPECT(h.image_release(a) == nullptr);
    EXPECT(h.image_get("dmg_head_blue") == a);  // still held once: the same
    h.image_release(a);
    h.image_release(a);  // the count at 0: freed
    zimg::Texture* c = h.image_get("dmg_head_blue");
    EXPECT(c && c->width > 0 && c->pixels);  // a fresh copy, readable
    h.image_release(c);

    std::vector<uint16_t> buf(8 * 4, 0x1234);
    zrndr::Target2D& t = zrndr::target2d();
    const zrndr::Target2D keep = t;
    t.base = reinterpret_cast<uint8_t*>(buf.data()), t.width = 8, t.height = 4, t.pitch = 16;
    zimg::Texture* g = h.screen_grab(1);
    EXPECT(g && g->width == 8 && static_cast<uint16_t*>(g->pixels)[5] == 0x1234);
    h.image_release(g);  // a grab: freed now
    h.image_release(g);  // again: reported, not freed twice
    t = keep;
  }
  for (int l = 0; l < 2; ++l) zimg::library_list(l).clear();
  zutl::mounts() = saved;
}

// Playtest #8: a HUD weapon line through the SDL host with the game's fonts.
// Half recharged, the bar's left half is the name in wepstring_green and the
// right half the name in wepstring_red (hud.md section 13).
TEST(sdl_host_hud_weapon_line_shows_the_recharge) {
  if (!have_fonts()) return;
  Mounted m;
  zvid::set_pixel_format(5, 6, 5);
  zrdr::init();
  EXPECT(zrdr::open_archive("zbd\\reader.zbd", true));  // fonts.zrd
  sdlfe::SdlTwodHost h(nullptr, nullptr, nullptr);
  ztwod::set_host(&h);
  {
    mech3::HudWeaponLine line;
    line.red = 0;
    EXPECT(line.set_look(true, false));  // live with ammo: the name red, the rest green
    line.name.set_text_plain("ER LARGE LASER");
    line.text2.set_text_plain("ER LARGE LASER");
    line.set_pos(100, 100);
    line.show_for(-1.0f);
    line.update(0.0f);
    EXPECT(line.fill != nullptr && line.empty != nullptr);
    std::vector<uint16_t> buf(640 * 480, 0);
    zrndr::Target2D& t = zrndr::target2d();
    const zrndr::Target2D saved = t;
    t.base = reinterpret_cast<uint8_t*>(buf.data()), t.width = 640, t.height = 480, t.pitch = 1280;
    if (line.fill && line.empty) {
      line.set_value(0.5f);
      line.draw();
      const int cut = 100 + line.fill_src.right;
      std::map<uint16_t, int> left, right;
      for (int y = 100; y < 100 + line.fill->height; ++y)
        for (int x = 100; x < 100 + line.empty->width; ++x)
          if (uint16_t p = buf[static_cast<size_t>(y) * 640 + x]) ++(x < cut ? left : right)[p];
      auto top = [](const std::map<uint16_t, int>& c) {
        uint16_t best = 0;
        int n = 0;
        for (auto [p, k] : c)
          if (k > n) best = p, n = k;
        return best;
      };
      std::printf("weapon line: fill %dx%d cut %d, left %04x x%zu, right %04x x%zu\n", line.fill->width,
                  line.fill->height, line.fill_src.right, top(left), left.size(), top(right), right.size());
      EXPECT(!left.empty() && !right.empty());
      EXPECT(top(left) != top(right));  // green filled, red still to fill
    }
    t = saved;
  }
  ztwod::set_host(nullptr);
  zrdr::close_archives(true);
}

namespace {
// The program's platform without a window (test_program.cpp's fakes), the
// presented 16-bit frame kept.
struct NoWindow : platform::Window {
  uint32_t create(const std::string&, int, int) override { return 0x1234; }
  void set_title(const std::string&) override {}
  void show() override {}
  void client_size(int* w, int* h) override { *w = 640, *h = 480; }
};
struct NoAlert : platform::Alert {
  void abort_message(const std::string&) override {}
};
struct StepClock : platform::Clock {
  uint32_t now = 0;
  uint32_t ticks_ms() override { return now += 16; }
};
// zFMV's clock: a second a call, so the Intro's SPLASH wait passes in a few frames.
struct MovieClock : platform::Clock {
  uint32_t now = 0;
  uint32_t ticks_ms() override { return now += 1000; }
};
platform::Clock& fast_movie_clock() {
  static MovieClock c;
  return c;
}
struct KeepScreen : platform::Screen2D {
  std::vector<uint16_t> last;  // the 2D frame shown, or the layer laid over the 3D frame
  void keep(const uint16_t* px, int w, int h, int pitch) {
    if (w != 640 || h != 480) return;
    last.resize(640 * 480);
    for (int y = 0; y < h; ++y) std::copy(px + y * (pitch / 2), px + y * (pitch / 2) + w, last.begin() + y * w);
  }
  void present_16(const uint16_t* px, int w, int h, int pitch, bool) override { keep(px, w, h, pitch); }
  void overlay_16(const uint16_t* px, int w, int h, int pitch, bool, uint16_t, const uint8_t*,
                  const uint16_t*) override {
    keep(px, w, h, pitch);
  }
};
// A Direct3D device that draws nothing (test_program's pause test).
struct NoGpu : platform::Render3D {
  int32_t create_texture(int, int, const uint8_t*, uint8_t) override { return 1; }
  void release_texture(int32_t) override {}
  void update_texture(int32_t, int, int, const uint8_t*) override {}
  void set_viewport(int, int, int, int) override {}
  void clear(bool, uint8_t, uint8_t, uint8_t) override {}
  void begin_scene() override {}
  void end_scene() override {}
  void draw(platform::Primitive, const platform::TLVertex*, int, const platform::DrawState&) override {}
  void set_fog_color(uint8_t, uint8_t, uint8_t) override {}
  void present() override {}
  bool read_back(std::vector<uint32_t>* argb, int* w, int* h) override {
    argb->assign(320 * 240, 0xff000000u);
    *w = 320, *h = 240;
    return true;
  }
};
struct Passes : platform::MessagePump {
  int passes = 0, limit = 200;
  bool peek() override { return ++passes > limit; }
  bool pump() override { return mech3::missions().cleanup(false), false; }
  void wait(uint32_t) override {}
  void post_quit() override {}
};
struct FireKey : platform::Input {
  std::vector<platform::KeyEvent> queue;
  uint32_t t = 0;
  void key(uint32_t dik, bool down) { queue.push_back({dik, down ? 0x80u : 0u, ++t, t}); }
  bool open(Device d) override { return d == kKeyboard; }
  void close(Device) override {}
  int32_t acquire(Device) override { return 0; }
  int32_t unacquire(Device) override { return 0; }
  int32_t keyboard_data(platform::KeyEvent* out, uint32_t* count) override {
    const uint32_t n = std::min<uint32_t>(*count, static_cast<uint32_t>(queue.size()));
    std::copy(queue.begin(), queue.begin() + n, out);
    queue.erase(queue.begin(), queue.begin() + n);
    *count = n;
    return 0;
  }
  int32_t mouse_state(platform::MouseState*) override { return kNotAcquired; }
  int32_t joystick_state(platform::JoyState*) override { return kNotAcquired; }
};
}  // namespace

// Playtest #8 end to end: c1m1 with the SDL host's 2D (no window; the
// software path, or a Direct3D device that draws nothing with the layer laid
// over it), fire, and the weapon list shows red (the bars still to refill)
// for a while, then all green again.
struct BarRun {
  int max_red = 0, red_frames = 0, green_before = 0, red_before = 0, red_end = -1;
};
void weapon_list_run(bool d3d, BarRun& out) {
  Mounted m;
  zutl::set_write_dir(std::string(MW3_TEST_TMP) + "/sdl_bars_write");
  mech3::reset_states();
  if (!zsnd::state().open) zsnd::state() = zsnd::State{};
  for (int l = 0; l < 2; ++l) zimg::library_list(l).clear();
  mech3::damage_set() = mech3::DamageSet{};
  mech3::weapon_set() = mech3::WeaponSet{};  // its fonts are the host's handles: a new host each run
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  FireKey keys;
  NoWindow window;
  NoAlert alert;
  StepClock clock;
  KeepScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.movie_clock = &fast_movie_clock(), p.screen = &screen, p.sound = &sound, p.files = &files;
  p.input = &keys;
  NoGpu gpu;
  if (d3d) p.render = &gpu;
  int& max_red = out.max_red;
  int& red_frames = out.red_frames;
  int& green_before = out.green_before;
  int& red_before = out.red_before;
  int& red_end = out.red_end;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    sdlfe::SdlTwodHost host(&engine, nullptr, &window);
    ztwod::set_host(&host);
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    const uint16_t red = zvid::pack_bytes(235, 0, 0), green = zvid::pack_bytes(0, 235, 0);
    int frames = 0;
    uint32_t fire = 0;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core || screen.last.empty()) return;
      ++frames;
      int r = 0, g = 0;
      for (int y = 28; y < 130; ++y)  // the weapon list's ten lines: (530, 30), 10 pixels a line
        for (int x = 500; x < 640; ++x) {
          const uint16_t px = screen.last[static_cast<size_t>(y) * 640 + x];
          r += px == red, g += px == green;
        }
      if (frames == 9) green_before = g, red_before = r;
      if (frames == 10) fire = zin::key1_of(0x3d) & 0xff, keys.key(fire, true);
      if (frames == 13) keys.key(fire, false);
      if (frames > 10) max_red = std::max(max_red, r), red_frames += r > 0;
      if (frames == 120) red_end = r;
    };
    Passes pump;
    pump.limit = 300;
    app.run(pump);
    std::printf("weapon list (%s): before %d green %d red; after firing red up to %d px over %d frames; at the end "
                "%d red\n",
                d3d ? "Direct3D" : "software", green_before, red_before, max_red, red_frames, red_end);
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
    ztwod::set_host(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  mech3::options() = mech3::Options{};
  // The interp's scroll list, which the world's unload (0x005964c0(1)) would
  // empty: the next run's set-up resets its nodes (0x005964c0(0)).
  zint::scroll_list_init();
}

TEST(sdl_program_weapon_list_shows_the_recharge) {
  if (!have_fonts()) return;
  for (bool d3d : {false, true}) {
    BarRun r;
    weapon_list_run(d3d, r);
    EXPECT(r.green_before > 100 && r.red_before == 0);  // full bars: green
    EXPECT(r.max_red > 20 && r.red_frames >= 5);        // firing: the bars empty to red and refill
    EXPECT(r.red_end == 0);                             // full again
  }
}

// Playtest #23/#24 reopened, through the SDL mouse: c1m1's Mad Cat at full
// throttle (THROTTLE0's key), its medium lasers selected and charged, the
// left button clicked as a player does (down, then up three frames later, as
// SDL events through SdlInput, read by zInput's mouse at each frame's poll).
// Logged per frame: the manager's trigger, the beam's state, how far the
// beam's origin is from the firepoint's place now. The laser fires on the
// press's frame; the beam fires on after the release until its charge is
// spent, and with the fix (bugs) it leaves from the arm all the while; the
// original's path leaves it at the last held frame's muzzle, behind.
struct ClickRun {
  int press_frame = -1, fire_frame = -1, firing_frames = 0;
  float max_stale = 0;  // the beam's origin off the firepoint while the beam shows
  std::string log;
};
static ClickRun click_run(int hold_frames, bool bugs) {
  ClickRun out;
  Mounted m;
  zutl::set_write_dir(std::string(MW3_TEST_TMP) + "/sdl_click_write");
  mech3::reset_states();
  if (!zsnd::state().open) zsnd::state() = zsnd::State{};
  for (int l = 0; l < 2; ++l) zimg::library_list(l).clear();
  mech3::damage_set() = mech3::DamageSet{};
  mech3::weapon_set() = mech3::WeaponSet{};
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  const zfix::Options keep_fix = zfix::options();
  sdlfe::SdlInput input;
  NoWindow window;
  NoAlert alert;
  StepClock clock;
  KeepScreen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.movie_clock = &fast_movie_clock(), p.screen = &screen, p.sound = &sound, p.files = &files;
  p.input = &input;
  auto key = [&](uint8_t dik, bool down) {
    for (int sc = 0; sc < SDL_SCANCODE_COUNT; ++sc)
      if (sdlfe::dik_from_scancode(static_cast<SDL_Scancode>(sc)) == dik) {
        SDL_Event e{};
        e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        e.key.scancode = static_cast<SDL_Scancode>(sc);
        e.key.down = down;
        input.on_event(e);
        return;
      }
  };
  auto button = [&](bool down) {
    SDL_Event e{};
    e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    e.button.button = SDL_BUTTON_LEFT;
    e.button.down = down;
    input.on_event(e);
  };
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    sdlfe::SdlTwodHost host(&engine, nullptr, &window);
    ztwod::set_host(&host);
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1m1, the Mad Cat
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    int frames = 0, group = -1;
    void* player = nullptr;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      ++frames;
      if (frames == 3) {
        // The original's path for the beam only once the mission is up (its
        // load needs the HUD constructor's fix, hud.md section 3).
        zfix::options().bugs = bugs;
        player = engine.object_of(engine.field32(engine.pilot(), 0x14));
        for (int32_t g = 0; g < 5 && group < 0; ++g)
          if (mech3::mech_group_names(player, g).find("ERLaser_Medium") != std::string::npos) group = g;
        if (group >= 0) mech3::mech_select_group(player, group);
        key(static_cast<uint8_t>(zin::key1_of(0x24) & 0xff), true);  // THROTTLE0: 100%
      }
      if (frames == 5) key(static_cast<uint8_t>(zin::key1_of(0x24) & 0xff), false);
      if (!player || group < 0) return;
      // The click, as the frame's events (seen by the next frame's poll).
      constexpr int kClick = 150;  // at speed
      if (frames == kClick) button(true), out.press_frame = frames + 1;
      if (frames == kClick + hold_frames) button(false);
      if (frames > kClick && frames <= kClick + 40) {
        mech3::BeamProbe b;
        mech3::mech_group_beam(player, group, &b);
        if (b.firing && out.fire_frame < 0) out.fire_frame = frames;
        out.firing_frames += b.firing;
        float off = 0;
        if (b.beam_active)
          off = std::sqrt((b.origin[0] - b.live[0]) * (b.origin[0] - b.live[0]) +
                          (b.origin[1] - b.live[1]) * (b.origin[1] - b.live[1]) +
                          (b.origin[2] - b.live[2]) * (b.origin[2] - b.live[2]));
        if (b.beam_active) out.max_stale = std::max(out.max_stale, off);
        char line[160];
        std::snprintf(line, sizeof line, "    %d: trigger %d/%d state %d, firing %d beam %d, ready %.2f, origin off %.2f m\n",
                      frames, b.trigger_down, b.trigger_was, b.state, b.firing, b.beam_active, b.ready, off);
        if (std::getenv("MW3_CLICK_LOG")) out.log += line;
      }
    };
    Passes pump;
    pump.limit = 600;
    app.run(pump);
    zfix::options() = keep_fix;
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
    ztwod::set_host(nullptr);
  }
  zfix::options() = keep_fix;
  zrdr::close_archives(true);
  mech3::reset_states();
  mech3::options() = mech3::Options{};
  zint::scroll_list_init();  // as the weapon list's run
  return out;
}
TEST(sdl_program_mouse_click_fires_the_laser) {
  if (!have_fonts()) return;
  const ClickRun r = click_run(3, true), o = click_run(3, false);
  for (const ClickRun* x : {&r, &o})
    std::printf("click (3 frames, %s): pressed for frame %d, fired on %d, %d frames firing, the beam's origin up to "
                "%.2f m off the firepoint\n%s",
                x == &r ? "fixed" : "the original", x->press_frame, x->fire_frame, x->firing_frames, x->max_stale,
                x->log.c_str());
  EXPECT(r.fire_frame == r.press_frame && o.fire_frame == o.press_frame);
  EXPECT(r.firing_frames > 3 && o.firing_frames > 3);  // on after the release
  EXPECT(r.max_stale < 0.05f);  // from the arm
  EXPECT(o.max_stale > 0.3f);   // the original's: left behind
}

#if !defined(_WIN32)  // posix_spawn (ours, not on Windows)
extern char** environ;

// --frames through the pause (mission-plan item 22): a launched mission's
// frames counted only those that drew the scene, so Escape's pause screen
// stopped the count and the run never ended. The program itself, Escape at
// frame 900 (the Core state's: the timed fade has taken 99 to 576) and --frames 1200,
// must quit on its own (90 s allowed).
TEST(sdl_program_frames_end_a_paused_mission) {
  if (!have_fonts()) return;
  const std::string dir = std::string(MW3_TEST_TMP) + "/sdl_frames_pause_write";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::string out = dir + "/mw3.log";
  std::vector<std::string> args = {MW3_BIN, "--mission", "c1m1", "--tap", "0x01@900", "--frames", "1200",
                                   "--verbose", "--nosound", "--hidden", "--write-dir", dir};
  std::vector<char*> argv;
  for (std::string& a : args) argv.push_back(a.data());
  argv.push_back(nullptr);
  posix_spawn_file_actions_t fa;
  posix_spawn_file_actions_init(&fa);
  posix_spawn_file_actions_addopen(&fa, 1, out.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  posix_spawn_file_actions_adddup2(&fa, 1, 2);
  pid_t pid = 0;
  const int err = posix_spawn(&pid, MW3_BIN, &fa, nullptr, argv.data(), environ);
  posix_spawn_file_actions_destroy(&fa);
  EXPECT(err == 0);
  if (err) return;
  int status = 0;
  bool quit = false;
  const auto start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - start < std::chrono::seconds(90)) {
    if (waitpid(pid, &status, WNOHANG) == pid) {
      quit = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  if (!quit) kill(pid, SIGKILL), waitpid(pid, &status, 0);
  std::string log;
  if (FILE* f = std::fopen(out.c_str(), "r")) {
    char buf[4096];
    for (size_t n; (n = std::fread(buf, 1, sizeof buf, f)) > 0;) log.append(buf, n);
    std::fclose(f);
  }
  const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  std::printf("mw3 --mission c1m1 --tap 0x01@900 --frames 1200: %s after %.0f s, exit %d\n", quit ? "quit" : "killed",
              secs, quit && WIFEXITED(status) ? WEXITSTATUS(status) : -1);
  EXPECT(quit && WIFEXITED(status) && WEXITSTATUS(status) == 0);
  EXPECT(log.find("Pause State") != std::string::npos);  // Escape reached the pause
}
#endif

// The window's shape (resolution.md step 2, any_resolution): the frame keeps
// its aspect in the window, pillarboxed where they differ, and the cursor
// and the relative mouse map through that rect, not through 640.
TEST(sdl_window_shape_maps_the_mouse_through_the_frame) {
  auto near = [](float a, float b) { return std::fabs(a - b) < 0.01f; };
  float x = 0, y = 0;
  // 4:3 in 4:3: the whole window, as without the fix.
  const sdlfe::FrameShape v640{640, 480, true};
  const sdlfe::FitRect r43 = sdlfe::fit_rect(1280, 960, v640);
  EXPECT(r43.x == 0 && r43.y == 0 && r43.w == 1280 && r43.h == 960);
  sdlfe::window_to_frame(640, 480, 1280, 960, v640, &x, &y);
  EXPECT(near(x, 320) && near(y, 240));
  EXPECT(near(sdlfe::mickey_scale(1280, 960, v640), 0.5f));
  // 16:9 in 16:9 (mode 11, 1280x720, in a 1920x1080 window).
  const sdlfe::FrameShape v720{1280, 720, true};
  const sdlfe::FitRect r169 = sdlfe::fit_rect(1920, 1080, v720);
  EXPECT(r169.x == 0 && r169.y == 0 && r169.w == 1920 && r169.h == 1080);
  sdlfe::window_to_frame(960, 540, 1920, 1080, v720, &x, &y);
  EXPECT(near(x, 640) && near(y, 360));
  EXPECT(near(sdlfe::mickey_scale(1920, 1080, v720), 1280.0f / 1920.0f));
  // 4:3 in 16:9: pillarboxed, 960x720 at x 160; the bars map outside the frame.
  const sdlfe::FitRect rp = sdlfe::fit_rect(1280, 720, v640);
  EXPECT(rp.x == 160 && rp.y == 0 && rp.w == 960 && rp.h == 720);
  sdlfe::window_to_frame(160, 0, 1280, 720, v640, &x, &y);
  EXPECT(near(x, 0) && near(y, 0));
  sdlfe::window_to_frame(1120, 720, 1280, 720, v640, &x, &y);
  EXPECT(near(x, 640) && near(y, 480));
  sdlfe::window_to_frame(100, 360, 1280, 720, v640, &x, &y);
  EXPECT(x < 0);
  sdlfe::frame_to_window(320, 240, 1280, 720, v640, &x, &y);
  EXPECT(near(x, 640) && near(y, 360));
  EXPECT(near(sdlfe::mickey_scale(1280, 720, v640), 640.0f / 960.0f));
  // 16:9 in 4:3: letterboxed.
  const sdlfe::FitRect rl = sdlfe::fit_rect(1280, 960, v720);
  EXPECT(rl.x == 0 && rl.y == 120 && rl.w == 1280 && rl.h == 720);
  // Without the fix: stretched over the window, the old 640 / width.
  const sdlfe::FrameShape off{640, 480, false};
  const sdlfe::FitRect ro = sdlfe::fit_rect(1280, 720, off);
  EXPECT(ro.x == 0 && ro.w == 1280 && ro.h == 720);
  EXPECT(near(sdlfe::mickey_scale(1280, 720, off), 0.5f));
  // The windows a frame opens: 640x480 at 1280x960 (as before), 1280x720 itself.
  int w = 0, h = 0;
  sdlfe::default_window(v640, &w, &h);
  EXPECT(w == 1280 && h == 960);
  sdlfe::default_window(v720, &w, &h);
  EXPECT(w == 1280 && h == 720);

  // Through the program's own pieces, in a hidden 1280x720 window: zInput's
  // client mapping (SdlZinHost) and the relative mouse (SdlInput).
  if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) return;
  SDL_Window* win = SDL_CreateWindow("mw3 shape test", 1280, 720, SDL_WINDOW_HIDDEN);
  EXPECT(win != nullptr);
  if (!win) return;
  int ww = 0, wh = 0;
  SDL_GetWindowSize(win, &ww, &wh);
  if (ww == 1280 && wh == 720) {
    sdlfe::FrameShape shape{640, 480, true};
    sdlfe::SdlInput input;
    input.window = win;
    input.shape = &shape;
    sdlfe::SdlZinHost host(&input);
    int32_t cx = 1120, cy = 720;
    host.screen_to_client(&cx, &cy);
    EXPECT(cx == 640 && cy == 480);
    cx = 0, cy = 0;
    host.client_to_screen(&cx, &cy);
    EXPECT(cx == 160 && cy == 0);
    int32_t fw = 0, fh = 0;
    host.client_size(&fw, &fh);
    EXPECT(fw == 640 && fh == 480);
    EXPECT(input.open(platform::Input::kMouse));
    input.acquire(platform::Input::kMouse);
    SDL_Event e{};
    e.type = SDL_EVENT_MOUSE_MOTION;
    e.motion.xrel = 96, e.motion.yrel = 48;
    input.on_event(e);
    platform::MouseState ms;
    input.mouse_state(&ms);
    std::printf("window shape: 96 x 48 window points of motion are %d x %d mickeys (pillarboxed 960 wide)\n", ms.x, ms.y);
    EXPECT(ms.x == 64 && ms.y == 32);  // 640 / 960 of the motion
    input.unacquire(platform::Input::kMouse);
    input.close(platform::Input::kMouse);
    shape.fit = false;  // the fix off: 640 stretched over the window, the old mapping
    cx = 1280, cy = 720;
    host.screen_to_client(&cx, &cy);
    EXPECT(cx == 640 && cy == 480);
  }
  SDL_DestroyWindow(win);
}

