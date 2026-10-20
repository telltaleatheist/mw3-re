// Probes for test_modeldraw.py (docs/drafts/modeldraw.md): a world read by
// the port's loaders, the model draw run on its nodes with a recording draw
// table and recording hooks. Self-contained: no shared helpers.
#include <cstdint>
#include <algorithm>
#include <cstring>
#include <vector>

#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/cls_zbd.h"
#include "gamez/zclass/list.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zimage/zimg_library.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmodel/gmod_draw.h"
#include "gamez/zmodel/gmod_fog.h"
#include "gamez/zmodel/gmod_fogcalc.h"
#include "gamez/zmodel/gmod_light.h"
#include "gamez/zrender/zrndr_fog.h"
#include "gamez/zutil/ztimer.h"
#include "gamez/zvideo/zvid_ddd3d.h"
#include "gamez/zvideo/zvid_format.h"

namespace {

std::vector<uint8_t> g_out;
std::vector<zimg::Texture> g_images;  // fake images: only flags (+9) is read

void put(const void* p, size_t n) {
  const auto* b = static_cast<const uint8_t*>(p);
  g_out.insert(g_out.end(), b, b + n);
}
void u32(uint32_t v) { put(&v, 4); }
void f32(float v) { put(&v, 4); }
uint32_t ptr32(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }

void verts(int32_t count) {
  if (count < 0) count = 0;
  if (count > zmod::kClipVerts) count = zmod::kClipVerts;
  const zmod::Clip2D& d = zmod::clip2d();
  for (int32_t i = 0; i < count; ++i) {
    put(&d.tl[i], 0x20);
    put(&d.work[i], 0x18);
  }
}
void prim() { u32(static_cast<uint32_t>(zvid::device_state().primitive)); }

void flat(void*, uint32_t color, int32_t alpha, int32_t count, int32_t priority, int32_t overlay) {
  u32(0), u32(color), u32(alpha), u32(count), u32(priority), u32(overlay), prim(), verts(count);
}
void lit(void*, uint32_t color, const float* rgb, uint32_t fogged, int32_t alpha, int32_t count,
         int32_t priority, int32_t overlay) {
  u32(1), u32(color), put(rgb, 12), u32(fogged), u32(alpha), u32(count), u32(priority), u32(overlay);
  prim(), verts(count);
}
void textured(void*, int32_t count, void* tex, float opacity, int32_t overlay) {
  u32(2), u32(count), u32(ptr32(tex)), f32(opacity), u32(overlay), prim(), verts(count);
}
void textured_lit(void*, uint32_t fogged, int32_t count, void* tex, int32_t priority, float opacity,
                  int32_t overlay) {
  u32(3), u32(fogged), u32(count), u32(ptr32(tex)), u32(priority), f32(opacity), u32(overlay), prim();
  verts(count);
}
void textured_vertex(void*, uint32_t fogged, int32_t count, void* tex, float opacity, int32_t overlay) {
  u32(4), u32(fogged), u32(count), u32(ptr32(tex)), f32(opacity), u32(overlay), prim(), verts(count);
}
// The projected-shadow sets the shadowed draws read: count, the textures,
// then each set's UVs for the polygon's vertices.
void sets(int32_t count) {
  if (count < 0) count = 0;
  if (count > zmod::kShadowVerts) count = zmod::kShadowVerts;
  const zmod::ShadowSets& s = zmod::clip2d().shadow;
  u32(static_cast<uint32_t>(s.count));
  for (int32_t k = 0; k < s.count; ++k) u32(ptr32(s.texture[k]));
  for (int32_t k = 0; k < s.count; ++k) put(s.uv[k], 8 * count);
}
void shadowed(void*, int32_t count, void* tex) {
  u32(5), u32(count), u32(ptr32(tex)), prim(), verts(count), sets(count);
}
void shadowed_lit(void*, uint32_t fogged, int32_t count, void* tex) {
  u32(6), u32(fogged), u32(count), u32(ptr32(tex)), prim(), verts(count), sets(count);
}
void shadowed_vertex(void*, uint32_t fogged, int32_t count, void* tex) {
  u32(7), u32(fogged), u32(count), u32(ptr32(tex)), prim(), verts(count), sets(count);
}
void projected(void*, const float* light, uint32_t fogged, int32_t count, void* tex, int32_t priority,
               float opacity, int32_t overlay) {
  u32(11), u32(fogged), u32(count), u32(ptr32(tex)), u32(priority), f32(opacity), u32(overlay), prim();
  put(light, 12 * count), verts(count);
}
void projected_shadowed(void*, const float* light, uint32_t fogged, int32_t count, void* tex) {
  u32(12), u32(fogged), u32(count), u32(ptr32(tex)), prim(), put(light, 12 * count), verts(count), sets(count);
}
void point(void*, const float s[3], uint32_t color, int32_t alpha) {
  u32(8), put(s, 12), u32(color), u32(alpha);
}

void fog_send() { u32(10), put(zrndr::device_fog().sent, 12); }

void refresh_static(void*, zmod::Model*, zcls::Node*) { u32(13); }
void point_color(const float rgb[3]) { u32(15), put(rgb, 12); }
void send_mix_fog() { u32(16); }
void mix_fog(float rgb[3]) { u32(17), put(rgb, 12); }

void report(int, const char*, int line, const char*) { u32(14), u32(line); }

void install() {
  zmod::DrawTable& t = zmod::draw_table();
  t.flat = flat, t.lit = lit, t.textured = textured, t.textured_lit = textured_lit;
  t.textured_vertex = textured_vertex, t.shadowed = shadowed, t.shadowed_lit = shadowed_lit;
  t.shadowed_vertex = shadowed_vertex, t.point = point;
  t.projected = projected, t.projected_shadowed = projected_shadowed;
  zmod::DrawHooks& h = zmod::draw_hooks();
  h.refresh_static = refresh_static, h.model_light = nullptr, h.flare = [](void*, zmod::Light*) { u32(18); };
  zmod::LightHooks& lh = zmod::light_hooks();
  lh = zmod::LightHooks{};
  lh.point_color = point_color, lh.send_mix_fog = send_mix_fog, lh.mix_fog = mix_fog;
  zmod::lighting() = zmod::Lighting{};
  zmod::lighting().mask_base = 0xffffffffu;
  zmod::lighting().mask = &zmod::lighting().mask_base;
  zrndr::device_fog().send = fog_send;
}

}  // namespace

extern "C" {

// The world as probe_read_zbd reads it. Returns read_zbd's result.
int32_t probe_md_load(const char* path) {
  zimg::textures().count = 0;
  zimg::textures().group = 3;
  zmod::materials() = zmod::MaterialTable{};
  zmod::models() = zmod::ModelPool{};
  zvid::set_pixel_format(5, 6, 5);
  zcls::nodes() = zcls::NodePool{};
  zcls::nodes().free_head = -1;
  zcls::lists() = zcls::Lists{};
  zmod::fog() = zmod::Fog{};
  zrndr::fog_color() = zrndr::FogColor{};
  zerr::set_sink(nullptr);
  const int32_t r = zcls::read_zbd(path);
  install();
  for (zmod::Model& m : zmod::models().slots)  // the static-light bake (+0x18) starts at 0, as in the test's original
    for (uint32_t i = 0; i < m.count(0x10); ++i)
      if (m.polygons[i].scratch) std::memset(m.polygons[i].scratch, 0, 4 * (m.polygons[i].flags & 0xff));
  zmod::draw_buffers() = zmod::DrawBuffers{};
  zmod::clip2d() = zmod::Clip2D{};
  zmod::clip3d_buffers() = zmod::Clip3dBuffers{};
  return r;
}

int32_t probe_md_node_count() { return zcls::nodes().capacity; }

// Per node: live, class, model index (-1 none), model type, polygons.
void probe_md_node(int32_t i, int32_t* out) {
  zcls::Node* n = zcls::node_at(i);
  out[0] = (n->link & zcls::kNodeLive) != 0;
  out[1] = n->cls();
  out[2] = n->model ? static_cast<int32_t>(n->model - zmod::models().slots.data()) : -1;
  out[3] = n->model ? static_cast<int32_t>(n->model->count(0)) : -1;
  out[4] = n->model ? static_cast<int32_t>(n->model->count(0x10)) : 0;
}

// Fake texture records: image byte 9 = flags[i], surface = 0x50000000 + 0x10 * i.
void probe_md_textures(const uint8_t* flags, int32_t count) {
  g_images.assign(static_cast<size_t>(count), zimg::Texture{});
  for (int32_t i = 0; i < count && i < zimg::kMaxTextures; ++i) {
    g_images[i].flags = flags[i];
    zimg::textures().entries[i].image = &g_images[i];
    zimg::textures().entries[i].surface = reinterpret_cast<void*>(static_cast<uintptr_t>(0x50000000u + 0x10u * i));
  }
}

// cam: the camera's world matrix (12 floats); proj: x, y, half_w, half_h,
// cot_h, cot_v, near, far; clip: the 0x7402f8 struct (0x24 bytes) and the point
// rect (0x10); second: 0x740338 (0x24) and its scale/offset (0x10). views:
// when given, what the original's 0x520b00 left at 0x73fbc0 and 0x73fc00, and
// its 0x521c60 at 0x73fb80..0x73fba0 and 0x73fc30: the maths are an input here
// (test_matrix.py checks them), so both sides start from the same bits.
void probe_md_camera(const float* cam, const float* proj, const uint8_t* clip, const uint8_t* second,
                     const float* views) {
  zrndr::device_fog().active = 1;
  zmth::Mat34 w;
  std::memcpy(w.m, cam, 48);
  zmth::set_view(w);
  zmth::set_projection(proj[0], proj[1], proj[2], proj[3], proj[4], proj[5], proj[6], proj[7]);
  if (views) {
    std::memcpy(zmth::view().view.m, views, 48);
    std::memcpy(zmth::view().camera.m, views + 12, 48);
    zmth::Projection& p = zmth::projection();
    const float* q = views + 24;  // 0x73fb80 .. 0x73fba0, then 0x73fc30
    p.near_z = q[0], p.cx = q[2], p.cy = q[3], p.inv_near = q[4], p.far_z = q[5];
    p.fy_near = q[6], p.fx = q[7], p.fy = q[8], p.fx_near = q[9];
  }
  std::memcpy(zmod::fog_calc().camera_to_world, zmth::view().camera.m, 48);
  zmod::DrawState& s = zmod::draw_state();
  std::memcpy(&s.clip, clip, 0x24);
  std::memcpy(s.point_rect, clip + 0x24, 0x10);
  std::memcpy(&s.second, second, 0x24);
  std::memcpy(s.second_scale, second + 0x24, 8);
  std::memcpy(s.second_offset, second + 0x2c, 8);
}

// st: opacity, overlay, vertex_shading, second_pass, priority_scale, frame,
// dt, lights, camera yaw, shadows; fog: the 0x74a22c block (0x38 bytes).
void probe_md_state(const float* st, const uint8_t* fog) {
  zmod::DrawState& s = zmod::draw_state();
  s.opacity = st[0];
  s.overlay = static_cast<int32_t>(st[1]);
  s.vertex_shading = static_cast<int32_t>(st[2]);
  s.second_pass = static_cast<int32_t>(st[3]);
  s.priority_scale = st[4];
  zcls::frames().frame = static_cast<uint32_t>(st[5]);
  zcls::frames().update_frame = 1;
  ztimer::state().dt = st[6];
  // st[7]: the world's lights (its sun) updated for this view and listed, or none.
  zmod::light_list().count = 0, zmod::light_list().any = 0;
  if (st[7] != 0.0f) {
    for (int32_t i = 0; i < zcls::nodes().capacity; ++i) {
      zcls::Node* nd = zcls::node_at(i);
      if (!nd || !(nd->link & zcls::kNodeLive) || nd->cls() != 2) continue;
      zmth::MatrixStack saved = zmth::matrix_stack();
      static zmth::Mat34 base;
      zmth::matrix_stack() = zmth::MatrixStack{};
      zmth::push(&base);
      zmth::load_identity();
      zmod::world_update_lights(nd);
      zmod::world_build_lights(nd, 0);
      zmth::matrix_stack() = saved;
      break;
    }
  }
  static float camera_data[0x1e8 / 4];
  camera_data[0x3c / 4] = st[8];  // the camera's yaw, for facades
  s.camera_data = camera_data;
  zmod::Fog& f = zmod::fog();
  std::memcpy(&f, fog, 0x38);
  s.shadows = static_cast<int32_t>(st[9]);
}

// One draw of node i under the world matrix `world`; the events go to out.
// Returns the bytes written (or -1 if they do not fit).
int32_t probe_md_draw(int32_t i, const float* world, uint32_t mask, uint8_t* out, int32_t cap) {
  static zmth::Mat34 w;
  std::memcpy(w.m, world, 48);
  zmth::MatrixStack& st = zmth::matrix_stack();
  st.top = 0;
  st.slot[0] = &w;
  st.identity[0] = 0;
  g_out.clear();
  zerr::set_sink(report);
  zmod::draw_model(zcls::node_at(i), mask);
  zerr::set_sink(nullptr);
  if (static_cast<int32_t>(g_out.size()) > cap) return -1;
  std::memcpy(out, g_out.data(), g_out.size());
  return static_cast<int32_t>(g_out.size());
}

}  // extern "C"

extern "C" {
// The model's camera-space vertices after a draw (MW3_DUMP in the test).
void probe_md_cam(float* out, int32_t n) { std::memcpy(out, zmod::draw_buffers().cam, 12 * n); }
}


extern "C" {
// zmth::load_model_view (0x51fb30) on `below` and the current view: the test
// gives the original the port's model-view so both draw from the same matrix
// (test_matrix.py checks 0x51fb30 itself).
void probe_md_model_view(const float* below, float* out) {
  zmth::Mat34 b, t;
  std::memcpy(b.m, below, 48);
  zmth::MatrixStack& s = zmth::matrix_stack();
  const zmth::MatrixStack saved = s;
  s.top = 0, s.slot[0] = &b, s.identity[0] = 0;
  zmth::push(&t);
  zmth::load_model_view();
  std::memcpy(out, t.m, 48);
  s = saved;
}
}

extern "C" {
// A model header dword (surveys and test scenarios).
uint32_t probe_md_model_get(int32_t mi, int32_t off) { return zmod::models().slots[mi].count(off); }
}

extern "C" {
// The most vertices any polygon of model mi has.
int32_t probe_md_max_poly(int32_t mi) {
  const zmod::Model& m = zmod::models().slots[mi];
  int32_t most = 0;
  for (uint32_t i = 0; i < m.count(0x10); ++i)
    most = std::max<int32_t>(most, static_cast<int32_t>(m.polygons[i].flags & 0xff));
  return most;
}
}

extern "C" {
// Scenario edits: a header dword of model mi, polygon k's flags, the morph
// deltas (n * 3 floats, which the model then points at; kept until the next).
void probe_md_set_model(int32_t mi, int32_t off, uint32_t v) {
  std::memcpy(zmod::models().slots[mi].header + off, &v, 4);
}
void probe_md_set_poly(int32_t mi, int32_t k, uint32_t flags) { zmod::models().slots[mi].polygons[k].flags = flags; }
void probe_md_set_morphs(int32_t mi, const float* d, int32_t n) {
  static std::vector<float> g_morphs;
  g_morphs.assign(d, d + 3 * n);
  zmod::models().slots[mi].morphs = g_morphs.data();
}
}

extern "C" {
// More scenario edits: a dword of polygon k (+4 priority), of model light li's
// raw bytes, of node i, of the cycle of polygon k's material (null: off -1
// clears the pointer, -2 puts it back).
void probe_md_set_poly_word(int32_t mi, int32_t k, int32_t off, uint32_t v) {
  zmod::Polygon& p = zmod::models().slots[mi].polygons[k];
  if (off == 4) p.priority = static_cast<int32_t>(v);
}
void probe_md_set_light(int32_t mi, int32_t li, int32_t off, uint32_t v) {
  zmod::Light& l = zmod::models().slots[mi].lights[li];
  if (off == 0x28) l.color = static_cast<uint16_t>(v);
  else std::memcpy(l.raw + off, &v, 4);
}
void probe_md_set_node(int32_t i, int32_t off, uint32_t v) { zcls::node_at(i)->put<uint32_t>(off, v); }
void probe_md_set_cycle(int32_t mi, int32_t k, int32_t off, uint32_t v) {
  static zmod::Cycle* saved = nullptr;
  zmod::Material* m = zmod::models().slots[mi].polygons[k].material;
  if (off == -1) saved = m->cycle, m->cycle = nullptr;
  else if (off == -2) m->cycle = saved;
  else std::memcpy(reinterpret_cast<uint8_t*>(m->cycle) + off, &v, 4);
}
// The shadow switch [0x74a264] (no casters: the receiver code stops at [0x7402c4]).
void probe_md_shadows(int32_t on) { zmod::draw_state().shadows = on; }
}
