// Probes for test_screen_tint.py (docs/anim-run.md, The screen tint): the
// animations' event 36 0x005074b0 through the weather manager's tint
// (0x005a6730 -> 0x005a6620, show_for 0x0059ff10), then the tint's draw
// 0x005a5360 -> 0x005a53c0 with 0x00558540's Direct3D quad recorded.
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zeffect/zeff_anim_run.h"
#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zrender/zrndr_fog.h"
#include "gamez/zrender/zrndr_tint.h"
#include "gamez/ztwod/twod_weather.h"
#include "gamez/zvideo/zvid_dd.h"
#include "gamez/zvideo/zvid_format.h"

namespace {
uint32_t fb(float f) {
  uint32_t i;
  std::memcpy(&i, &f, 4);
  return i;
}
float bf(uint32_t i) {
  float f;
  std::memcpy(&f, &i, 4);
  return f;
}
ztwod::Rect g_rects[2];
std::vector<uint32_t> g_quads;  // per draw: colour, has rect, l t r b, the alpha's two dwords

// Event 37's answers and what it asked.
struct WaveCalls {
  float d2 = 0, moved[3] = {}, screen[2] = {};
  uint32_t code = 0;
  std::vector<uint32_t> asked;  // world_point: 1, the point; project: 2, the point
  std::vector<uint32_t> draws;  // per ripple: x y zero radius csin_x, y, z (bits), rect 1/2/0
} g_wave;
}  // namespace

extern "C" {

// One event: its 0x40 bytes, the sequence's time and the time left (bits),
// the tint's flags and timer before, which manager rects are set (bit 0, 1)
// and their 8 ints. out: the result, the time left after, the colour, the
// alpha's dwords, the flags and timer after, then the draw's quads (count
// first) and which rect +0x34 holds after (0 none, 1, 2).
int32_t probe_tint_run(const uint8_t* ev, uint32_t now, uint32_t left, uint32_t flags, uint32_t timer,
                       int32_t rects, const int32_t* r8, uint32_t* out) {
  zvid::set_pixel_format(5, 6, 5);
  zvid::text_target().pixels = nullptr;
  zrndr::device_fog().active = 1;
  zrndr::tint_hooks().quad = [](uint16_t colour, const int32_t* rect, double alpha) {
    uint64_t q;
    std::memcpy(&q, &alpha, sizeof q);
    g_quads.push_back(colour);
    g_quads.push_back(rect ? 1 : 0);
    for (int i = 0; i < 4; ++i) g_quads.push_back(rect ? static_cast<uint32_t>(rect[i]) : 0);
    g_quads.push_back(static_cast<uint32_t>(q));
    g_quads.push_back(static_cast<uint32_t>(q >> 32));
  };
  zeff::anim_run_hooks().screen_tint = [](void*, uint16_t colour, double alpha) { ztwod::weather_tint(colour, alpha); };
  ztwod::WeatherLayer& l = ztwod::weather_layer();
  for (int i = 0; i < 2; ++i) {
    g_rects[i] = {r8[4 * i], r8[4 * i + 1], r8[4 * i + 2], r8[4 * i + 3]};
    l.rects[i] = rects & (1 << i) ? &g_rects[i] : nullptr;
  }
  ztwod::ScreenTint& t = l.tint;
  t.flags = flags, t.timer = bf(timer), t.rect = nullptr;
  std::vector<uint8_t> e(ev, ev + 0x40);
  zeff::SeqDef seq{};
  *seq.floats(0x28) = bf(now);
  zeff::anim().time_left() = bf(left);
  out[0] = static_cast<uint32_t>(zeff::anim_fbfx_color(seq, e.data()));
  out[1] = fb(zeff::anim().time_left());
  uint64_t q;
  std::memcpy(&q, &t.alpha, sizeof q);
  out[2] = t.colour, out[3] = static_cast<uint32_t>(q), out[4] = static_cast<uint32_t>(q >> 32);
  out[5] = t.flags, out[6] = fb(t.timer);
  g_quads.clear();
  t.draw();
  out[7] = static_cast<uint32_t>(g_quads.size() / 8);
  for (size_t i = 0; i < g_quads.size(); ++i) out[8 + i] = g_quads[i];
  out[8 + g_quads.size()] = t.rect == nullptr ? 0 : t.rect == &g_rects[0] ? 1 : 2;
  zrndr::tint_hooks().quad = nullptr;
  zrndr::device_fog().active = 0;
  return 0;
}

// Event 37 (0x005076c0) then a draw of the slot it placed (0x005a5360 ->
// 0x005a5440, the ripple recorded). in: the event's 0x70 bytes, the sequence's
// time and state byte, the time left, whether its node ref is set, the
// distance², the focal x, the screen's size, the node's point moved (3), the
// projection's code and x, y, the wave count before, each slot's flags and
// timer (10), the rects (bits 0, 1) and their 8 ints. out: the
// result, the time left, +0x3c, +0x40, +0x44, the count after, each slot's x
// y zero radius csin_x csin_y csin_z flags timer (5 x 9), then the calls asked
// (count first) and the draws (count first).
int32_t probe_wave_run(const uint8_t* ev, uint32_t now, int32_t state, uint32_t left, int32_t has_node, uint32_t d2,
                       uint32_t fx, int32_t w, int32_t h, const uint32_t* moved, uint32_t code, const uint32_t* screen,
                       int32_t count, const uint32_t* slots, int32_t rects, const int32_t* r8, uint32_t* out) {
  g_wave = WaveCalls{};
  g_wave.d2 = bf(d2), g_wave.code = code;
  for (int i = 0; i < 3; ++i) g_wave.moved[i] = bf(moved[i]);
  g_wave.screen[0] = bf(screen[0]), g_wave.screen[1] = bf(screen[1]);
  zeff::AnimRunHooks& rh = zeff::anim_run_hooks();
  const zeff::AnimRunHooks saved = rh;
  rh.distance2 = [](void*, zcls::Node*) { return g_wave.d2; };
  rh.world_point = [](void*, zcls::Node*, float p[3]) -> int32_t {
    g_wave.asked.insert(g_wave.asked.end(), {1, fb(p[0]), fb(p[1]), fb(p[2])});
    for (int i = 0; i < 3; ++i) p[i] = g_wave.moved[i];
    return 0;
  };
  rh.project = [](void*, const float in[3], float o[3]) -> uint32_t {
    g_wave.asked.insert(g_wave.asked.end(), {2, fb(in[0]), fb(in[1]), fb(in[2])});
    o[0] = g_wave.screen[0], o[1] = g_wave.screen[1], o[2] = 0;
    return g_wave.code;
  };
  rh.screen_wave = [](void*, int32_t x, int32_t y, int32_t z, int32_t r, int32_t cx, float cy, float cz) {
    ztwod::weather_wave(x, y, z, r, cx, cy, cz);
  };
  ztwod::WeatherHooks& wh = ztwod::weather_hooks();
  const auto saved_wave = wh.wave;
  wh.wave = [](int32_t x, int32_t y, int32_t z, int32_t r, int32_t cx, float cy, float cz, const ztwod::Rect* rect) {
    const ztwod::WeatherLayer& l = ztwod::weather_layer();
    g_wave.draws.insert(g_wave.draws.end(), {static_cast<uint32_t>(x), static_cast<uint32_t>(y), static_cast<uint32_t>(z),
                                             static_cast<uint32_t>(r), static_cast<uint32_t>(cx), fb(cy), fb(cz),
                                             rect == l.rects[0] ? 1u : rect == l.rects[1] ? 2u : 0u});
  };
  zvid::text_target().pixels = nullptr;
  zmth::projection().fx = bf(fx);
  zvid::surfaces().back.width = w, zvid::surfaces().back.height = h;
  ztwod::WeatherLayer& l = ztwod::weather_layer();
  for (int i = 0; i < 2; ++i) {
    g_rects[i] = {r8[4 * i], r8[4 * i + 1], r8[4 * i + 2], r8[4 * i + 3]};
    l.rects[i] = rects & (1 << i) ? &g_rects[i] : nullptr;
  }
  l.wave_count = count;
  for (int i = 0; i < 5; ++i) {
    ztwod::ScreenWave& v = l.waves[i];
    v.flags = slots[2 * i], v.timer = bf(slots[2 * i + 1]);
    v.x = v.y = v.zero = v.radius = v.csin_x = 0, v.csin_y = v.csin_z = 0, v.rect = nullptr;
  }
  std::vector<uint8_t> e(ev, ev + 0x70);
  zeff::NodeRef40 refs[8]{};
  zcls::Node* node = reinterpret_cast<zcls::Node*>(&refs[7]);  // any non-null: the hook answers for it
  for (auto& r : refs) r.node = has_node ? node : nullptr;
  zeff::AnimDef def{};
  def.nodes = refs;
  def.root = node;
  zeff::SeqDef seq{};
  *seq.floats(0x28) = bf(now);
  seq.raw[0x20] = static_cast<uint8_t>(state);
  zeff::anim().time_left() = bf(left);
  int k = 0;
  out[k++] = static_cast<uint32_t>(zeff::anim_fbfx_wave(&def, &seq, e.data()));
  out[k++] = fb(zeff::anim().time_left());
  for (int off : {0x3c, 0x40, 0x44}) out[k++] = *reinterpret_cast<const uint32_t*>(e.data() + off);
  out[k++] = static_cast<uint32_t>(l.wave_count);
  for (const ztwod::ScreenWave& v : l.waves) {
    for (int32_t i : {v.x, v.y, v.zero, v.radius, v.csin_x}) out[k++] = static_cast<uint32_t>(i);
    out[k++] = fb(v.csin_y), out[k++] = fb(v.csin_z), out[k++] = v.flags, out[k++] = fb(v.timer);
  }
  // The draw of the slot this event's call went to (the count before, at most 4).
  l.waves[count < 4 ? count : 4].draw();
  out[k++] = static_cast<uint32_t>(g_wave.asked.size());
  for (uint32_t v : g_wave.asked) out[k++] = v;
  out[k++] = static_cast<uint32_t>(g_wave.draws.size());
  for (uint32_t v : g_wave.draws) out[k++] = v;
  rh = saved;
  wh.wave = saved_wave;
  return k;
}

}  // extern "C"
