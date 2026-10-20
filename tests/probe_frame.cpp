// Probes for test_d3d_frame.py (docs/render-frame.md): the screen tint
// 0x00558540, the software finish 0x00558610 and its rows, the finish target
// 0x00558290, and the Direct3D frame's lock 0x00576d00 / 0x00576d60 with the
// surface lock 0x00577590 / 0x00577600 under it.
#include <cstdint>
#include <cstring>

#include "gamez/zrender/zrndr_blit.h"
#include "gamez/zrender/zrndr_span.h"
#include "gamez/zrender/zrndr_tint.h"
#include "gamez/zvideo/zvid_dd.h"
#include "gamez/zvideo/zvid_ddd3d.h"
#include "gamez/zvideo/zvid_format.h"

namespace {
// What the test's DirectDraw Lock answers: 0 fills these, else fails.
int32_t g_lock_result = 0;
zvid::Surface g_lock_fill;
int32_t g_locks = 0, g_unlocks = 0;

int32_t lock(void*, zvid::Surface* s) {
  ++g_locks;
  if (g_lock_result) return g_lock_result;
  s->width = g_lock_fill.width, s->height = g_lock_fill.height;
  s->pitch = g_lock_fill.pitch, s->pixels = g_lock_fill.pixels;
  return 0;
}
int32_t unlock(void*, zvid::Surface*) {
  ++g_unlocks;
  return 0;
}
}  // namespace

extern "C" {

// The display format (masks and shifts as 0x005792c0 sets them for r/g/b bits).
void probe_frame_format(int32_t r_bits, int32_t g_bits, int32_t b_bits, int32_t mmx, int32_t format2d) {
  zvid::set_pixel_format(r_bits, g_bits, b_bits);
  const zvid::PixelFormat& f = zvid::pixel_format();
  zvid::ChannelMasks& m = zvid::channel_masks();
  m.r = f.r_mask << f.r_shift, m.g = f.g_mask << f.g_shift, m.b = f.b_mask >> f.b_shift_right;
  zrndr::finish_machine().mmx = mmx != 0;
  zrndr::finish_machine().format = format2d;
  zrndr::tint_hooks().quad = zvid::set_tint_quad;
  zvid::surface_hooks() = {lock, unlock, nullptr};
}

void probe_frame_finish_target(uint16_t* base, int32_t w, int32_t h, int32_t pitch) {
  zrndr::set_finish_target(base, w, h, pitch);
}

// 0x00558540; rect null when has_rect is 0. The alpha as its dwords.
void probe_frame_tint(uint32_t colour, int32_t has_rect, const int32_t* rect, uint32_t lo, uint32_t hi,
                      int32_t direct3d) {
  const uint64_t q = lo | static_cast<uint64_t>(hi) << 32;
  double a;
  std::memcpy(&a, &q, 8);
  zrndr::tint(static_cast<uint16_t>(colour), has_rect ? rect : nullptr, a, direct3d != 0);
}

// The software tint state: pending, left, top, right, bottom, colour, alpha's dwords.
void probe_frame_tint_get(uint32_t* out) {
  const zrndr::TintState& s = zrndr::tint_state();
  uint64_t q;
  std::memcpy(&q, &s.alpha, 8);
  const uint32_t v[8] = {static_cast<uint32_t>(s.pending), static_cast<uint32_t>(s.left), static_cast<uint32_t>(s.top),
                         static_cast<uint32_t>(s.right), static_cast<uint32_t>(s.bottom), s.colour,
                         static_cast<uint32_t>(q), static_cast<uint32_t>(q >> 32)};
  std::memcpy(out, v, sizeof v);
}
void probe_frame_tint_set(const uint32_t* v) {
  zrndr::TintState& s = zrndr::tint_state();
  s.pending = static_cast<int32_t>(v[0]), s.left = static_cast<int32_t>(v[1]), s.top = static_cast<int32_t>(v[2]);
  s.right = static_cast<int32_t>(v[3]), s.bottom = static_cast<int32_t>(v[4]), s.colour = v[5];
  const uint64_t q = v[6] | static_cast<uint64_t>(v[7]) << 32;
  std::memcpy(&s.alpha, &q, 8);
}

// The quad: pending and its 32 vertex dwords.
void probe_frame_quad(uint32_t* out) {
  const zvid::TintQuad& q = zvid::tint_quad();
  out[0] = static_cast<uint32_t>(q.pending);
  std::memcpy(out + 1, q.verts, 128);
}

// 0x00558610; the constants after: keep, red, blue, green, pair_hi, pair_lo, row.
uint32_t probe_frame_finish(uint32_t* k) {
  const uint32_t r = zrndr::finish();
  const zrndr::FinishConstants& c = zrndr::finish_constants();
  const uint32_t v[7] = {c.keep, c.red, c.blue, c.green, c.pair_hi, c.pair_lo, static_cast<uint32_t>(c.row)};
  std::memcpy(k, v, sizeof v);
  return r;
}

// The back buffer's record before a lock: width, height, pitch, valid,
// pixels (as an address), locked. What the Lock answers, and its result.
void probe_frame_back_set(const int32_t* rec, uint16_t* pixels, const int32_t* fill, uint16_t* fill_pixels,
                          int32_t result) {
  zvid::Surface& b = zvid::surfaces().back;
  b.width = rec[0], b.height = rec[1], b.pitch = rec[2], b.valid = rec[3], b.pixels = pixels, b.locked = rec[5];
  g_lock_fill.width = fill[0], g_lock_fill.height = fill[1], g_lock_fill.pitch = fill[2];
  g_lock_fill.pixels = fill_pixels;
  g_lock_result = result;
  g_locks = g_unlocks = 0;
}
// 0x00576d00 (which 0) or 0x00576d60 (1).
void probe_frame_lock(int32_t which) {
  if (which == 0) zvid::lock_3d();
  else zvid::unlock_3d();
}
// Everything a lock sets, pointers as given: the record (6), the fillers'
// target (base, width, height, pitch, bytes, screen_row), the blit's (base,
// pitch), the finish target (5), the text target (4), lock and unlock counts.
void probe_frame_lock_get(uint64_t* out) {
  const zvid::Surface& b = zvid::surfaces().back;
  const zrndr::SoftTarget& s = zrndr::soft_target();
  const zrndr::FinishTarget& f = zrndr::finish_target();
  const zvid::TextTarget& t = zvid::text_target();
  const uint64_t v[] = {static_cast<uint32_t>(b.width), static_cast<uint32_t>(b.height), static_cast<uint32_t>(b.pitch),
                        static_cast<uint32_t>(b.valid), reinterpret_cast<uintptr_t>(b.pixels),
                        static_cast<uint32_t>(b.locked), reinterpret_cast<uintptr_t>(s.base),
                        static_cast<uint32_t>(s.width), static_cast<uint32_t>(s.height), static_cast<uint32_t>(s.pitch),
                        static_cast<uint32_t>(s.bytes), static_cast<uint32_t>(s.screen_row),
                        reinterpret_cast<uintptr_t>(zrndr::target2d().base), zrndr::target2d().pitch,
                        reinterpret_cast<uintptr_t>(f.base), static_cast<uint32_t>(f.width),
                        static_cast<uint32_t>(f.height), static_cast<uint32_t>(f.pitch),
                        static_cast<uint32_t>(f.pitch_px), reinterpret_cast<uintptr_t>(t.pixels),
                        static_cast<uint32_t>(t.width), static_cast<uint32_t>(t.height), static_cast<uint32_t>(t.pitch),
                        static_cast<uint32_t>(g_locks), static_cast<uint32_t>(g_unlocks)};
  std::memcpy(out, v, sizeof v);
}
void probe_frame_soft_set(int32_t width, int32_t height, int32_t bytes, int32_t screen_w) {
  zrndr::SoftTarget& s = zrndr::soft_target();
  s.width = width, s.height = height, s.bytes = bytes, s.screen_w = screen_w;
}

}  // extern "C"
