// Probes for test_shadows.py (docs/render-shadows.md): the projected-shadow
// and projected-light draws of the Direct3D table into a recording
// platform::Render3D that keeps draw_multi's interleaved vertices, the caster
// list functions, and the model-draw extras (casters, projected lights). The
// queue, overlay and model-draw probes of probe_drawtable.cpp and
// probe_modeldraw.cpp are reused from Python.
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmodel/gmod_clip2d.h"
#include "gamez/zmodel/gmod_cull.h"
#include "gamez/zmodel/gmod_draw.h"
#include "gamez/zmodel/gmod_light.h"
#include "gamez/zvideo/zvid_ddd3d.h"

namespace {

// The log, int32 words:
//   1 draw: prim, count, 13 DrawState fields, count * 8 vertex dwords
//   11 draw_multi: prim, uv sets, count, 13 fields, count * (6 + 2 * sets) dwords
//   8 zerr: level, line, length, one word per character
std::vector<int32_t> g_log;

void put_state(const platform::DrawState& s) {
  const int32_t f[13] = {s.texture, s.address_u, s.address_v, s.blend, s.multiply, s.depth_write, s.depth_always,
                         s.alpha_modulate, s.fog, s.flat, s.wireframe, s.color_select, s.texcoord};
  g_log.insert(g_log.end(), f, f + 13);
}

class Recorder : public platform::Render3D {
 public:
  int32_t create_texture(int, int, const uint8_t*, uint8_t) override { return 0; }
  void release_texture(int32_t) override {}
  void set_viewport(int, int, int, int) override {}
  void clear(bool, uint8_t, uint8_t, uint8_t) override {}
  void begin_scene() override {}
  void end_scene() override {}
  void draw(platform::Primitive type, const platform::TLVertex* v, int count, const platform::DrawState& s) override {
    g_log.insert(g_log.end(), {1, static_cast<int32_t>(type), count});
    put_state(s);
    const auto* w = reinterpret_cast<const int32_t*>(v);
    g_log.insert(g_log.end(), w, w + 8 * count);
  }
  void draw_multi(platform::Primitive type, const uint32_t* v, int sets, int count,
                  const platform::DrawState& s) override {
    g_log.insert(g_log.end(), {11, static_cast<int32_t>(type), sets, count});
    put_state(s);
    const auto* w = reinterpret_cast<const int32_t*>(v);
    g_log.insert(g_log.end(), w, w + (6 + 2 * sets) * count);
  }
  void set_fog_color(uint8_t, uint8_t, uint8_t) override {}
  void present() override {}
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
zmod::ShadowSets g_shadow;

float as_float(int32_t bits) {
  float f;
  std::memcpy(&f, &bits, 4);
  return f;
}

}  // namespace

extern "C" {

// textures: 16 x {handle, type, address_u, address_v}. Also what
// probe_dt_init does for the queue and overlay probes.
void probe_sh_init(const int32_t* textures) {
  for (int i = 0; i < 16; ++i) {
    const int32_t* t = textures + 4 * i;
    g_textures[i] = {t[0], t[1], t[2], t[3]};
  }
  zvid::set_render3d(&g_recorder);
  zvid::draw_buffers() = {g_tl, g_work, &g_shadow};
  zerr::set_sink(zerr_sink);
  g_log.clear();
}

// cache: the 7 DeviceState ints; draw: the 13 DrawState fields; light: rgb
// 0..255 and the brightest index.
void probe_sh_set(const int32_t* cache, const int32_t* draw, const float* light, int32_t brightest) {
  zvid::device_state() = {cache[0], cache[1], cache[2], cache[3], cache[4], cache[5], cache[6]};
  platform::DrawState& d = zvid::draw_state();
  d.texture = draw[0];
  d.address_u = static_cast<uint8_t>(draw[1]);
  d.address_v = static_cast<uint8_t>(draw[2]);
  d.blend = draw[3], d.multiply = draw[4], d.depth_write = draw[5], d.depth_always = draw[6];
  d.alpha_modulate = draw[7], d.fog = draw[8], d.flat = draw[9], d.wireframe = draw[10];
  d.color_select = draw[11], d.texcoord = static_cast<uint8_t>(draw[12]);
  zvid::PointLight& l = zvid::point_light();
  std::memcpy(l.color, light, 12);
  l.brightest = brightest;
}

void probe_sh_get(int32_t* cache, int32_t* draw) {
  const zvid::DeviceState& s = zvid::device_state();
  const int32_t c[7] = {s.primitive, s.alpha_blend, s.shade_mode, s.alpha_op, s.address_u, s.address_v, s.z_write};
  std::memcpy(cache, c, sizeof c);
  const platform::DrawState& d = zvid::draw_state();
  const int32_t f[13] = {d.texture, d.address_u, d.address_v, d.blend, d.multiply, d.depth_write, d.depth_always,
                         d.alpha_modulate, d.fog, d.flat, d.wireframe, d.color_select, d.texcoord};
  std::memcpy(draw, f, sizeof f);
}

// fn: 0 0x57eeb0, 1 0x57f2a0, 2 0x57f860, 3 0x57ea20, 4 0x57fe20. a: count,
// texture index, flags, opacity bits, overlay, priority, light (0 none);
// shadow: count, then 7 texture indices, then 7 x 64 UV pairs; light: 64 x 3.
void probe_sh_call(int32_t fn, const int32_t* a, uint8_t* tl, const float* work, const int32_t* shadow,
                   const float* light) {
  std::memcpy(g_tl, tl, sizeof g_tl);
  std::memcpy(g_work, work, sizeof g_work);
  g_shadow.count = shadow[0];
  for (int k = 0; k < zmod::kShadowSets; ++k) g_shadow.texture[k] = &g_textures[shadow[1 + k]];
  std::memcpy(g_shadow.uv, shadow + 8, sizeof g_shadow.uv);
  const int32_t count = a[0];
  const zvid::D3DTexture* t = &g_textures[a[1]];
  const uint32_t flags = static_cast<uint32_t>(a[2]);
  const float* l = a[6] ? light : nullptr;
  switch (fn) {
    case 0: zvid::draw_shadowed(count, t); break;
    case 1: zvid::draw_shadowed_lit(flags, count, t); break;
    case 2: zvid::draw_shadowed_vertex_lit(flags, count, t); break;
    case 3: zvid::draw_projected(l, flags, count, t, a[5], as_float(a[3]), a[4]); break;
    case 4: zvid::draw_projected_shadowed(l, flags, count, t); break;
    default: break;
  }
  std::memcpy(tl, g_tl, sizeof g_tl);
}

int32_t probe_sh_log(int32_t* out, int32_t cap) {
  const int32_t n = static_cast<int32_t>(g_log.size());
  std::memcpy(out, g_log.data(), sizeof(int32_t) * (n < cap ? n : cap));
  g_log.clear();
  return n;
}

static int32_t texture_index(const zvid::D3DTexture* t) { return t ? static_cast<int32_t>(t - g_textures) + 1 : 0; }

// The last queued polygon {prim, count, texture index, vertex offset}.
void probe_sh_queue_last(int32_t* r) {
  const zvid::TransparentQueue& q = zvid::transparent_queue();
  const zvid::TransparentPoly& p = q.polys[q.count - 1];
  r[0] = p.primitive, r[1] = p.count, r[2] = texture_index(p.texture);
  r[3] = static_cast<int32_t>(p.verts - q.verts);
}

// The last overlay entry {type, count, texture index} and its 64 vertices.
void probe_sh_overlay_last(int32_t* e, uint8_t* verts) {
  const zvid::OverlayList& o = zvid::overlay_list();
  const zvid::OverlayPoly& p = o.polys[o.count - 1];
  e[0] = p.type, e[1] = p.count, e[2] = texture_index(p.texture);
  std::memcpy(verts, p.verts, 64 * 32);
}

// --- The caster lists (part 2) ---
// n records linked in order, 9 dwords each: enabled, slope x, z, box min x,
// z, box max x, z (as floats), then the view (12 floats), the wrapped-cell
// offset (3) and a partition box (6). out: all count, ids, the slopes in
// camera space (3 floats each, by all[] order), cell count, ids.
void probe_sh_cast(const uint32_t* f, int32_t n, const float* view, const float* offset, const float* box,
                   uint32_t* out) {
  static std::vector<zmod::ShadowCaster> casters;
  casters.assign(static_cast<size_t>(n), zmod::ShadowCaster{});
  for (int32_t i = 0; i < n; ++i) {
    zmod::ShadowCaster& c = casters[i];
    const uint32_t* v = f + 9 * i;
    c.enabled = static_cast<int32_t>(v[0]);
    std::memcpy(c.slope, v + 1, 8);
    std::memcpy(&c.box_min[0], v + 3, 4), std::memcpy(&c.box_min[2], v + 4, 4);
    std::memcpy(&c.box_max[0], v + 5, 4), std::memcpy(&c.box_max[2], v + 6, 4);
    c.next = i + 1 < n ? &casters[i + 1] : nullptr;
  }
  zmod::caster_list() = n ? casters.data() : nullptr;
  std::memcpy(zmth::view().view.m, view, 48);
  std::memcpy(zmod::lighting().camera_offset, offset, 12);
  zmod::collect_casters();
  zmod::caster_directions();
  zmod::cell_casters(box);
  const zmod::ShadowCasters& s = zmod::shadow_casters();
  uint32_t* o = out;
  *o++ = static_cast<uint32_t>(s.all_count);
  for (int32_t i = 0; i < s.all_count; ++i) *o++ = static_cast<uint32_t>(s.all[i] - casters.data());
  for (int32_t i = 0; i < s.all_count; ++i) std::memcpy(o, s.all[i]->slope_cam, 12), o += 3;
  *o++ = static_cast<uint32_t>(s.cell_count);
  for (int32_t i = 0; i < s.cell_count; ++i) *o++ = static_cast<uint32_t>(s.cell[i] - casters.data());
}

// --- The model draw's extras (test_shadows.py part 3, with probe_md_*) ---

// The camera's wrapped-cell offset (0x73fbb0), 1 / fx and 1 / fy
// (0x63d758 / 0x63d75c) and the point-light colour (0x72f40c), as the
// original has them.
void probe_sh_md_setup(const float* offset, const float* inv_f, const float* point) {
  std::memcpy(zmod::lighting().camera_offset, offset, 12);
  zmth::projection().inv_fx = inv_f[0], zmth::projection().inv_fy = inv_f[1];
  std::memcpy(zvid::point_light().color, point, 12);
}

// The walk's caster list ([0x73fc4c], [0x7402c4]): per caster 24 dwords:
// position 3, ground_at 2, ground_y, slope 2, slope_len, box_min 3, box_max 3,
// scale 2, slope_cam 3, texture (a fake pointer value), colour, enabled.
void probe_sh_md_casters(const uint32_t* f, int32_t n) {
  static std::vector<zmod::ShadowCaster> casters;
  static zmod::ShadowCaster* list[200];
  casters.assign(static_cast<size_t>(n), zmod::ShadowCaster{});
  for (int32_t i = 0; i < n; ++i) {
    zmod::ShadowCaster& c = casters[i];
    const uint32_t* v = f + 24 * i;
    std::memcpy(c.position, v, 12);
    std::memcpy(c.ground_at, v + 3, 8);
    std::memcpy(&c.ground_y, v + 5, 4);
    std::memcpy(c.slope, v + 6, 8);
    std::memcpy(&c.slope_len, v + 8, 4);
    std::memcpy(c.box_min, v + 9, 12);
    std::memcpy(c.box_max, v + 12, 12);
    std::memcpy(c.scale, v + 15, 8);
    std::memcpy(c.slope_cam, v + 17, 12);
    c.texture = reinterpret_cast<void*>(static_cast<uintptr_t>(v[20]));
    c.color = v[21];
    c.enabled = static_cast<int32_t>(v[22]);
    list[i] = &c;
  }
  zmod::ShadowCasters& s = zmod::shadow_casters();
  s.list = list;
  s.count = n;
}

// Lights appended to the model draw's light list (0x749824): n records of
// 0xd0 bytes; any light makes the list non-empty.
void probe_sh_md_lights(const uint8_t* data, int32_t n) {
  static zcls::Light lights[16];
  zmod::LightList& l = zmod::light_list();
  for (int32_t i = 0; i < n && i < 16; ++i) {
    std::memcpy(lights[i].raw, data + 0xd0 * i, 0xd0);
    l.list[l.count++] = zmod::LightEntry{&lights[i], nullptr, 0, 0, 0};
  }
  if (l.count) l.any = 1;
}

// How far 0x547740 went on the last projected polygon: vertices, triangles.
void probe_sh_md_mesh(int32_t* out) {
  out[0] = zmod::projected_lights().vertex_count;
  out[1] = zmod::projected_lights().triangle_count;
}

// The interleaved buffer as the last shadowed draw left it (count vertices
// of 8 + 2 * sets dwords).
void probe_sh_multi(int32_t* out, int32_t words) { std::memcpy(out, zvid::multi_buffer(), 4 * words); }

}  // extern "C"
