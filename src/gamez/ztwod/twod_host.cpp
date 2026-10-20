// The default Host: every call reports itself as unported (docs/twod.md, Hooks).
#include "gamez/ztwod/twod_host.h"

#include <cstring>

#include <cstdio>
#include <cstdlib>

#include "gamez/zimage/zimg_library.h"
#include "gamez/zinput/zinput.h"
#include "gamez/zrender/zrndr_blit.h"
#include "gamez/zrender/zrndr_prim2d.h"
#include "gamez/zrender/zrndr_span.h"
#include "gamez/zmodel/gmod_clip3d.h"
#include "gamez/zmodel/gmod_draw.h"

namespace ztwod {

namespace {
Host g_default;
Host* g_host = &g_default;
inline Handle ptr(const void* p) { return static_cast<Handle>(reinterpret_cast<uintptr_t>(p)); }
}  // namespace

Host& host() { return *g_host; }
void set_host(Host* h) { g_host = h ? h : &g_default; }

zimg::Texture* Host::image_get(const char* name) {
  unported("0x005179d0", {name});
  return nullptr;
}
zimg::Texture* Host::image_release(zimg::Texture* image) {
  unported("0x00517b10", {ptr(image)});
  return nullptr;
}
void Host::image_path(const char* path) { unported("0x00516f50", {path}); }
Handle Host::font_find(const char* name) { return unported("0x00517680", {name}); }
NamedFont Host::font_fields(Handle) { return NamedFont{}; }
void Host::font_addref(Handle font) { unported("0x005172e0", {font}); }
void Host::font_release(Handle font) { unported("0x00517300", {font}); }
void Host::texture_free(zimg::Texture* t) { zimg::free_texture(t); }
void Host::font_measure(const char* text, Handle font, int32_t* w, int32_t* h) {
  unported("0x0051a0d0", {text, font});
  *w = 0;
  *h = 0;
}
void Host::font_draw(const char* text, int32_t x, int32_t y, Handle font) {
  unported("0x005ae170", {text, x, y, font});
}
void Host::video_lock() { unported("0x00576de0", {}); }
void Host::video_unlock() { unported("0x00576e70", {}); }
zimg::Texture* Host::screen_grab(int32_t buffer) {
  unported("0x00579500", {buffer});
  return nullptr;
}
zimg::Texture* Host::screen_grab_rect(int32_t buffer, Rect* r, zimg::Texture* into) {
  unported("0x00579660", {buffer, r->left, r->top, r->right, r->bottom, ptr(into)});
  return nullptr;
}
// Mech3.exe 1.2 @ 0x0055b1f0
// FIX (A): the original Blts with DirectDraw (0x00576f90) when the image has a
// surface and the target is the locked buffer; here images have none, so it is
// always the software blit of the 16-bit target ([0x00800824] = 0x0055b3c0).
void Host::blit(zimg::Texture* image, int32_t x, int32_t y, int32_t flags, const Rect* src) {
  if (image) zrndr::blit16(image, x, y, static_cast<uint16_t>(flags), reinterpret_cast<const zrndr::BlitRect*>(src));
}
Handle Host::image_dc(zimg::Texture* image) { return unported("[0x0072f394]", {ptr(image)}); }
void Host::image_dc_release(zimg::Texture* image, Handle dc) { unported("[0x0072f398]", {ptr(image), dc}); }
uint32_t Host::format_message(char* buf, uint32_t size, uint32_t id, va_list) {
  if (size) buf[0] = 0;
  return unported("0x00575ff0", {size, id});
}
int32_t Host::video_mode() { return static_cast<int32_t>(unported("peek32", {0x72f348})); }
void Host::video_double() { unported("[0x0072f358]", {0, 0}); }
uint32_t Host::show_cursor(uint32_t on) { return unported("0x00579840", {on}); }
Rect Host::screen_rect() {
  unported("0x00579880", {});
  return Rect{};
}
Handle Host::render_target(int32_t* w, int32_t* h, int32_t* depth, int32_t* pitch) {
  *w = *h = *depth = *pitch = 0;
  return unported("0x00558080", {});
}
void Host::render_set_target(Handle base, const Rect* r, Handle depth, Handle pitch) {
  unported("0x00557f90", {base, r->left, r->top, r->right, r->bottom, depth, pitch});
}
void Host::ripple(int32_t x, int32_t y, int32_t inner, int32_t outer, int32_t amplitude, float wavelength,
                  float phase, const Rect* clip) {
  unported("0x00558dd0", {x, y, inner, outer, amplitude, zgame::bits(wavelength), zgame::bits(phase), ptr(clip)});
}
Handle Host::video_get(uint32_t addr) {
  char key[16];
  std::snprintf(key, sizeof key, "0x%08x", addr);
  return unported(key, {});
}
void Host::input_poll(uint8_t a) { unported("0x0051b360", {a}); }
void Host::timer_update() { unported("0x00575a00", {}); }
float Host::frame_dt() { return zgame::from_bits(unported("peek32", {0x72c52c})); }
void Host::video_flip(const Rect* a, const Rect* b, int32_t c, int32_t d) {
  unported("0x00576ea0", {a->left, a->top, a->right, a->bottom, b->left, b->top, b->right, b->bottom, c, d});
}
void Host::crt_free(void* p) { std::free(p); }
int32_t Host::sound_find(const char* name) { return static_cast<int32_t>(unported("0x0056f6f0", {name})); }
int32_t Host::sound_play(int32_t sound, float volume) {
  return static_cast<int32_t>(unported("0x0056e3d0", {sound, zgame::bits(volume)}));
}
void Host::sound_stop(int32_t handle) { unported("0x0056e9f0", {handle}); }
int32_t Host::cd_find(const char* name) { return static_cast<int32_t>(unported("0x00572a30", {name})); }
int32_t Host::cd_play(int32_t track, int32_t mode) {
  return static_cast<int32_t>(unported("0x00572af0", {track, mode}));
}
void Host::cd_stop() { unported("0x00572c30", {}); }
const char* Host::message(const char* key) {
  unported("0x00575fb0", {key});
  return key;
}
MouseState Host::mouse() {
  unported("0x0051b7b0", {});
  return MouseState{};
}
void Host::input_reset() { unported("0x0051b1c0", {}); }
void Host::set_char_callback(void (*cb)(int32_t, int32_t, uintptr_t), uintptr_t ctx) { zin::set_char_callback(cb, ctx); }
void Host::line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t pixel) {
  zrndr::line16(x0, y0, x1, y1, static_cast<uint16_t>(pixel));
}
namespace {
const int32_t* ints(const Rect* r) { return r ? &r->left : nullptr; }
}  // namespace
void Host::line_dashed(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t pixel, int32_t dash) {
  zrndr::line16_dashed(x0, y0, x1, y1, static_cast<uint16_t>(pixel), dash);
}
void Host::line_list(const int32_t* points, int32_t count, const Rect* clip, uint32_t pixel) {
  zrndr::line16_list(points, count, ints(clip), static_cast<uint16_t>(pixel));
}
void Host::circle(int32_t cx, int32_t cy, int32_t r, uint32_t pixel, const Rect* clip) {
  zrndr::circle16(cx, cy, r, static_cast<uint16_t>(pixel), ints(clip));
}
void Host::circle_noisy(int32_t cx, int32_t cy, int32_t r, uint32_t pixel, float noise, const Rect* clip) {
  zrndr::circle16_noisy(cx, cy, r, static_cast<uint16_t>(pixel), noise, ints(clip));
}
void Host::line_noisy(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t pixel, float noise, const Rect* clip) {
  zrndr::line16_noisy(x0, y0, x1, y1, static_cast<uint16_t>(pixel), noise, ints(clip));
}
void Host::render_clip(float mn[3], float mx[3]) {
  const zmod::ClipPlanes& c = zmod::draw_state().clip;
  auto f = [](uint32_t v) {
    float r;
    std::memcpy(&r, &v, 4);
    return r;
  };
  mn[0] = f(c.side_04), mn[1] = f(c.side_08), mn[2] = c.near_z;
  mx[0] = f(c.unknown_10), mx[1] = f(c.unknown_14), mx[2] = c.far_z;
}
// The fill at full precision, as in play (CLAUDE.md, FPU precision).
void Host::polygon(const float* vertices, int32_t count, uint32_t color, int32_t winding) {
  zrndr::soft_target().winding = winding;
  zrndr::fill_polygon(vertices, count, color, zrndr::X87::k53);
}

Handle Host::GetStockObject(int32_t which) { return unported("GetStockObject", {which}); }
Handle Host::CreateFontA(int32_t height, int32_t width, int32_t escapement, int32_t orientation, int32_t weight,
                         uint32_t italic, uint32_t underline, uint32_t strike, uint32_t charset,
                         uint32_t out_precision, uint32_t clip_precision, uint32_t quality,
                         uint32_t pitch_family, const char* face) {
  return unported("CreateFontA", {height, width, escapement, orientation, weight, italic, underline, strike,
                                  charset, out_precision, clip_precision, quality, pitch_family, face});
}
int32_t Host::GetObjectA(Handle font) { return static_cast<int32_t>(unported("GetObjectA", {font})); }
Handle Host::CreateFontIndirectA(Handle copied_from) { return unported("CreateFontIndirectA", {copied_from}); }
void Host::DeleteObject(Handle object) { unported("DeleteObject", {object}); }
Handle Host::CreateCompatibleDC(Handle dc) { return unported("CreateCompatibleDC", {dc}); }
Handle Host::SelectObject(Handle dc, Handle object) { return unported("SelectObject", {dc, object}); }
void Host::DeleteDC(Handle dc) { unported("DeleteDC", {dc}); }
int32_t Host::GetTextExtentPoint32A(Handle dc, const char* text, int32_t length, int32_t* cx, int32_t* cy) {
  *cx = 0;
  *cy = 0;
  return static_cast<int32_t>(unported("GetTextExtentPoint32A", {dc, text, length}));
}
int32_t Host::DrawTextA(Handle dc, const char* text, int32_t length, Rect* rect, uint32_t format) {
  return static_cast<int32_t>(unported(
      "DrawTextA", {dc, text, length, rect->left, rect->top, rect->right, rect->bottom, format}));
}
int32_t Host::GetTextMetricsA(Handle dc, TextMetrics* out) {
  *out = TextMetrics{};
  return static_cast<int32_t>(unported("GetTextMetricsA", {dc}));
}
void Host::SetTextColor(Handle dc, uint32_t color) { unported("SetTextColor", {dc, color}); }
void Host::SetBkColor(Handle dc, uint32_t color) { unported("SetBkColor", {dc, color}); }
void Host::SetBkMode(Handle dc, int32_t mode) { unported("SetBkMode", {dc, mode}); }
uint32_t Host::GetLastError() { return unported("GetLastError", {}); }

}  // namespace ztwod
