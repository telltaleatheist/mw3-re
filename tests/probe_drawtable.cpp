// Probes for test_drawtable.py (docs/drafts/drawtable.md): the Direct3D draw
// table, the flushes, the frame calls and texture records, drawing into a
// recording platform::Render3D. Self-contained: no shared helpers.
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zerror/zerr.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zrender/zrndr_fog.h"
#include "gamez/zvideo/zvid_ddd3d.h"
#include "gamez/zvideo/zvid_format.h"

namespace {

// The log, int32 words: kind, then its fields.
//   1 draw: prim, count, 11 DrawState fields, count * 8 vertex dwords
//   2 texture: width, height, type (the RGBA in g_rgba)
//   3 clear: colour, r, g, b    4 begin    5 end    6 fog colour: r, g, b
//   7 present    8 zerr: level, line, length, one word per character
//   9 the dirty flags after begin_scene    10 present's return value
std::vector<int32_t> g_log;
std::vector<uint8_t> g_rgba;
int32_t g_next_handle = 0x7000;

void put_state(const platform::DrawState& s) {
  const int32_t f[11] = {s.texture, s.address_u, s.address_v, s.blend, s.multiply, s.depth_write,
                         s.depth_always, s.alpha_modulate, s.fog, s.flat, s.wireframe};
  g_log.insert(g_log.end(), f, f + 11);
}

class Recorder : public platform::Render3D {
 public:
  int32_t create_texture(int width, int height, const uint8_t* rgba, uint8_t type) override {
    g_log.insert(g_log.end(), {2, width, height, type});
    g_rgba.assign(rgba, rgba + static_cast<size_t>(width) * height * 4);
    return g_next_handle++;
  }
  void release_texture(int32_t) override {}
  void set_viewport(int, int, int, int) override {}
  void clear(bool color, uint8_t r, uint8_t g, uint8_t b) override { g_log.insert(g_log.end(), {3, color, r, g, b}); }
  void begin_scene() override { g_log.push_back(4); }
  void end_scene() override { g_log.push_back(5); }
  void draw(platform::Primitive type, const platform::TLVertex* v, int count, const platform::DrawState& s) override {
    g_log.insert(g_log.end(), {1, static_cast<int32_t>(type), count});
    put_state(s);
    const auto* w = reinterpret_cast<const int32_t*>(v);
    g_log.insert(g_log.end(), w, w + 8 * count);
  }
  void set_fog_color(uint8_t r, uint8_t g, uint8_t b) override { g_log.insert(g_log.end(), {6, r, g, b}); }
  void present() override { g_log.push_back(7); }
};
Recorder g_recorder;

void zerr_sink(int level, const char*, int line, const char* message) {
  const int32_t n = static_cast<int32_t>(std::strlen(message));
  g_log.insert(g_log.end(), {8, level, line, n});
  for (int32_t i = 0; i < n; ++i) g_log.push_back(static_cast<uint8_t>(message[i]));
}

zvid::D3DTexture g_textures[16];
zvid::TLVertex g_tl[64];
zvid::WorkRecord g_work[64];

int32_t texture_index(const zvid::D3DTexture* t) { return t ? static_cast<int32_t>(t - g_textures) + 1 : 0; }
const zvid::D3DTexture* texture_at(int32_t i) { return &g_textures[i]; }

float as_float(int32_t bits) {
  float f;
  std::memcpy(&f, &bits, 4);
  return f;
}

// The pattern stale vertex bytes start with: dword k of region r.
uint32_t pattern(uint32_t r, uint32_t k) { return (r << 28) ^ (k * 2654435761u); }

}  // namespace

extern "C" {

// textures: 16 x {handle, type, address_u, address_v}.
void probe_dt_init(const int32_t* textures) {
  for (int i = 0; i < 16; ++i) {
    const int32_t* t = textures + 4 * i;
    g_textures[i] = {t[0], t[1], t[2], t[3]};
  }
  zvid::set_render3d(&g_recorder);
  zvid::draw_buffers() = {g_tl, g_work};
  zerr::set_sink(zerr_sink);
  g_log.clear();
}

// Queue and overlay counts, and the stale pattern in the pool, the overlay
// vertices and the flat buffer.
void probe_dt_reset(uint32_t count, uint32_t vertex_count, int32_t overlay_count) {
  zvid::TransparentQueue& q = zvid::transparent_queue();
  q.count = count;
  q.vertex_count = vertex_count;
  for (auto& p : q.polys) p = {};
  for (uint32_t i = 0; i < count; ++i)  // already queued: one vertex each, untextured
    q.polys[i] = {6, 1, nullptr, &q.verts[i < vertex_count ? i : vertex_count ? vertex_count - 1 : 0]};
  auto* pool = reinterpret_cast<uint32_t*>(q.verts);
  for (uint32_t k = 0; k < zvid::TransparentQueue::kMaxVerts * 8; ++k) pool[k] = pattern(1, k);
  zvid::OverlayList& o = zvid::overlay_list();
  o.count = overlay_count;
  for (int32_t e = 0; e < zvid::OverlayList::kMax; ++e) {
    auto* v = reinterpret_cast<uint32_t*>(o.polys[e].verts);
    for (uint32_t k = 0; k < 64 * 8; ++k) v[k] = pattern(2, static_cast<uint32_t>(e) * 512 + k);
  }
  auto* f = reinterpret_cast<uint32_t*>(zvid::flat_buffer());
  for (uint32_t k = 0; k < 64 * 8; ++k) f[k] = pattern(3, k);
}

// cache: the 7 DeviceState ints; draw: the 11 DrawState fields; light: rgb
// 0..255 and the brightest index; format: r/g/b masks, r_shift, g_shift,
// b_shift, g_bits.
void probe_dt_set(const int32_t* cache, const int32_t* draw, const float* light, int32_t brightest,
                  const int32_t* format) {
  zvid::device_state() = {cache[0], cache[1], cache[2], cache[3], cache[4], cache[5], cache[6]};
  platform::DrawState& d = zvid::draw_state();
  d.texture = draw[0];
  d.address_u = static_cast<uint8_t>(draw[1]);
  d.address_v = static_cast<uint8_t>(draw[2]);
  d.blend = draw[3], d.multiply = draw[4], d.depth_write = draw[5], d.depth_always = draw[6];
  d.alpha_modulate = draw[7], d.fog = draw[8], d.flat = draw[9], d.wireframe = draw[10];
  zvid::PointLight& l = zvid::point_light();
  std::memcpy(l.color, light, 12);
  l.brightest = brightest;
  zvid::channel_masks() = {static_cast<uint32_t>(format[0]), static_cast<uint32_t>(format[1]),
                           static_cast<uint32_t>(format[2])};
  zvid::PixelFormat& f = zvid::pixel_format();
  f.r_shift = format[3], f.g_shift = format[4], f.b_shift_right = format[5], f.g_bits = format[6];
}

void probe_dt_get(int32_t* cache, int32_t* draw, float* light, int32_t* brightest) {
  const zvid::DeviceState& s = zvid::device_state();
  const int32_t c[7] = {s.primitive, s.alpha_blend, s.shade_mode, s.alpha_op, s.address_u, s.address_v, s.z_write};
  std::memcpy(cache, c, sizeof c);
  const platform::DrawState& d = zvid::draw_state();
  const int32_t f[11] = {d.texture, d.address_u, d.address_v, d.blend, d.multiply, d.depth_write,
                         d.depth_always, d.alpha_modulate, d.fog, d.flat, d.wireframe};
  std::memcpy(draw, f, sizeof f);
  std::memcpy(light, zvid::point_light().color, 12);
  *brightest = zvid::point_light().brightest;
}

// fn: 0 flat, 1 lit, 2 textured, 3 textured lit, 4 per-vertex lit (args as
// test_drawtable.py packs them), 5 flush transparent, 6 flush overlay,
// 7 begin scene, 8 end scene, 9 point light colour (rgb), 10 fog colour
// (rgb = the sent colour), 13 textured white. tl/work: 64 vertices in, tl out.
void probe_dt_call(int32_t fn, const int32_t* a, uint8_t* tl, const float* work, const float* rgb) {
  std::memcpy(g_tl, tl, sizeof g_tl);
  std::memcpy(g_work, work, sizeof g_work);
  switch (fn) {
    case 0: zvid::draw_flat(static_cast<uint32_t>(a[0]), a[1], a[2], a[3], a[4]); break;
    case 1: zvid::draw_lit(static_cast<uint32_t>(a[0]), rgb, static_cast<uint32_t>(a[1]), a[2], a[3], a[4], a[5]); break;
    case 2: zvid::draw_textured(a[0], texture_at(a[1]), as_float(a[2]), a[3]); break;
    case 3: zvid::draw_textured_lit(static_cast<uint32_t>(a[0]), a[1], texture_at(a[2]), a[3], as_float(a[4]), a[5]); break;
    case 4: zvid::draw_textured_vertex_lit(static_cast<uint32_t>(a[0]), a[1], texture_at(a[2]), as_float(a[3]), a[4]); break;
    case 13: zvid::draw_textured_white(a[0], texture_at(a[1]), a[2], as_float(a[3]), a[4]); break;
    case 5: zvid::flush_transparent(); break;
    case 6: zvid::flush_overlay(); break;
    case 7: {
      zvid::Display& d = zvid::display();
      d.dirty = static_cast<uint32_t>(a[0]), d.fill_mode = a[1], d.dither = a[2];
      zvid::begin_scene();
      g_log.insert(g_log.end(), {9, static_cast<int32_t>(d.dirty)});
      break;
    }
    case 8: zvid::end_scene(); break;
    case 9: zvid::display().fullscreen = a[0], zvid::set_point_light_color(rgb); break;
    case 10: std::memcpy(zrndr::device_fog().sent, rgb, 12), zvid::send_fog_color(); break;
    case 11: {
      zvid::display().clear_on = a[0], zvid::display().zbuffer = true;
      zvid::clear_color() = static_cast<uint32_t>(a[1]);
      zvid::frame_clear(true);
      break;
    }
    case 12: {
      zvid::display().primary = a[0] != 0;
      const int32_t r = zvid::present(true);
      g_log.insert(g_log.end(), {10, r});
      break;
    }
    default: break;
  }
  std::memcpy(tl, g_tl, sizeof g_tl);
}

// The queue: counts[2]; polys n x {prim, count, texture index (0 none), vertex
// offset}; pool: the first vertex_count vertices. Returns the count.
int32_t probe_dt_queue(int32_t* counts, int32_t* polys, uint8_t* pool) {
  const zvid::TransparentQueue& q = zvid::transparent_queue();
  counts[0] = static_cast<int32_t>(q.count);
  counts[1] = static_cast<int32_t>(q.vertex_count);
  for (uint32_t i = 0; i < q.count && i < zvid::TransparentQueue::kMaxPolys; ++i) {
    const zvid::TransparentPoly& p = q.polys[i];
    int32_t* r = polys + 4 * i;
    r[0] = p.primitive, r[1] = p.count, r[2] = texture_index(p.texture);
    r[3] = p.verts ? static_cast<int32_t>(p.verts - q.verts) : -1;
  }
  std::memcpy(pool, q.verts, sizeof(zvid::TLVertex) * q.vertex_count);
  return static_cast<int32_t>(q.count);
}

// The overlay list: n x {type, count, texture index} and 64 vertices each.
int32_t probe_dt_overlay(int32_t* entries, uint8_t* verts) {
  const zvid::OverlayList& o = zvid::overlay_list();
  for (int32_t i = 0; i < o.count; ++i) {
    entries[3 * i] = o.polys[i].type;
    entries[3 * i + 1] = o.polys[i].count;
    entries[3 * i + 2] = texture_index(o.polys[i].texture);
    std::memcpy(verts + 64 * 32 * i, o.polys[i].verts, 64 * 32);
  }
  return o.count;
}

// The queue's polygon and vertex counts and the overlay count.
void probe_dt_counts(int32_t* out) {
  out[0] = static_cast<int32_t>(zvid::transparent_queue().count);
  out[1] = static_cast<int32_t>(zvid::transparent_queue().vertex_count);
  out[2] = zvid::overlay_list().count;
}

void probe_dt_flat(uint8_t* out) { std::memcpy(out, zvid::flat_buffer(), 64 * 32); }

// Returns the log length (words); copies up to cap words, then clears it.
int32_t probe_dt_log(int32_t* out, int32_t cap) {
  const int32_t n = static_cast<int32_t>(g_log.size());
  std::memcpy(out, g_log.data(), sizeof(int32_t) * (n < cap ? n : cap));
  g_log.clear();
  return n;
}

// 0x0057b130 on an image. display: max_width, max_height, caps, alpha_4444,
// g_bits, then r/g/b masks; rec out: {default?, type, address_u, address_v,
// handle}. The RGBA uploaded goes to rgba (width * height * 4).
void probe_dt_texture(const char* name, int32_t width, int32_t height, const uint16_t* pixels,
                      const uint8_t* alpha, int32_t palette, int32_t alpha_arg, int32_t clamp_u,
                      int32_t clamp_v, const int32_t* display, int32_t* rec, uint8_t* rgba) {
  static zvid::D3DTexture fallback{0x55, 1, 1, 1};
  zvid::default_texture() = &fallback;
  zvid::Display& d = zvid::display();
  d.max_width = static_cast<uint32_t>(display[0]), d.max_height = static_cast<uint32_t>(display[1]);
  d.caps = static_cast<uint32_t>(display[2]), d.alpha_4444 = display[3];
  zvid::pixel_format().g_bits = display[4];
  zvid::channel_masks() = {static_cast<uint32_t>(display[5]), static_cast<uint32_t>(display[6]),
                           static_cast<uint32_t>(display[7])};
  static uint16_t pal[256];
  zimg::Texture image{};
  image.width = static_cast<int16_t>(width);
  image.height = static_cast<int16_t>(height);
  image.pixels = const_cast<uint16_t*>(pixels);
  image.alpha = const_cast<uint8_t*>(alpha);
  image.palette = palette ? pal : nullptr;
  g_rgba.clear();
  zvid::D3DTexture* t = zvid::create_texture(name, image, alpha_arg, clamp_u, clamp_v);
  rec[0] = t == &fallback;
  rec[1] = t->type, rec[2] = t->address_u, rec[3] = t->address_v, rec[4] = t->handle;
  if (!g_rgba.empty()) std::memcpy(rgba, g_rgba.data(), g_rgba.size());
  if (t != &fallback) delete t;
}

}  // extern "C"
