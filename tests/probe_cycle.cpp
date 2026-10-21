// The model texture cycle for test_cycle.py: the interp's wrappers
// (0x0053bc30, 0x0053bcd0, 0x0053bcf0, 0x0053bd10) and the frame step
// 0x005376c0 on one model whose first polygon has a material of its own (or
// the default material), with `n` texture entries to map.
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zclass/cls_node.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_draw.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zutil/ztimer.h"

namespace {
zmod::Model g_model;
zmod::Polygon g_poly;
zmod::Material g_mat;
std::vector<zimg::TextureEntry> g_tex;
bool g_null = false;

zmod::Model* model() { return g_null ? nullptr : &g_model; }
zmod::Material& mat() { return *g_poly.material; }
int32_t tex_index(const zimg::TextureEntry* t) {
  if (!t) return -3;
  if (t == zimg::default_entry()) return -1;
  for (size_t i = 0; i < g_tex.size(); ++i)
    if (t == &g_tex[i]) return static_cast<int32_t>(i);
  return -2;
}
}  // namespace

extern "C" {
// flags: the material's, use_default: the default material, null_model.
void cy_reset(int32_t n, int32_t flags, int32_t use_default, int32_t null_model) {
  if (g_mat.cycle) {
    std::free(g_mat.cycle->textures);
    std::free(g_mat.cycle);
  }
  g_mat = zmod::Material{};
  g_mat.flags = static_cast<uint8_t>(flags);
  g_tex.assign(static_cast<size_t>(n), zimg::TextureEntry{});
  g_mat.texture = n ? &g_tex[0] : zimg::default_entry();
  g_poly = zmod::Polygon{};
  g_poly.material = use_default ? &zmod::default_material() : &g_mat;
  g_model.polygons = &g_poly;
  g_null = null_model != 0;
}
int32_t cy_on(int32_t count) { return zmod::cycle_set_on(model(), count); }
int32_t cy_map(int32_t k) { return zmod::cycle_set_map(model(), k < 0 ? zimg::default_entry() : &g_tex[k]); }
int32_t cy_loop(uint32_t v) { return zmod::cycle_set_looping(model(), v); }
int32_t cy_speed(float v) { return zmod::cycle_set_speed(model(), v); }
void cy_step(uint32_t frame, float dt) {
  zcls::frames().frame = frame;
  ztimer::state().dt = dt;
  zmod::material_cycle(mat());
}
void cy_frame(int32_t n) { zmod::cycle_set_frame(&g_model, n); }
void cy_restart() { zmod::cycle_reset(&g_model); }
// flags, texture, cycle?, looping, frame, index bits, speed bits, count, added, slots...
int32_t cy_state(int32_t* out) {
  zmod::Material& m = mat();
  int32_t k = 0;
  out[k++] = m.flags;
  out[k++] = tex_index(m.texture);
  const zmod::Cycle* c = m.cycle;
  out[k++] = c != nullptr;
  if (!c) return k;
  out[k++] = static_cast<int32_t>(c->looping);
  out[k++] = static_cast<int32_t>(c->frame);
  std::memcpy(&out[k++], &c->index, 4);
  std::memcpy(&out[k++], &c->speed, 4);
  out[k++] = static_cast<int32_t>(c->texture_count);
  out[k++] = static_cast<int32_t>(c->added);
  for (uint32_t i = 0; i < c->texture_count; ++i) out[k++] = tex_index(c->textures[i]);
  return k;
}
}
