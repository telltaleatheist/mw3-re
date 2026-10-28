// zModel's polygon add 0x0053c6c0 (craters part 5a) for
// tests/differential/test_gmod_add.py: a model built up by a run of adds,
// dumped after each.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "gamez/zerror/zerr.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_draw.h"
#include "gamez/zvideo/zvid_ddd3d.h"
#include "gamez/zutil/zfix.h"

namespace {
std::vector<uint8_t> g_out;
zmod::Model g_model;
zmod::Material g_materials[2];

void u32(uint32_t v) {
  for (int i = 0; i < 4; ++i) g_out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
void floats(const float* p, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    uint32_t b;
    std::memcpy(&b, p + i, 4);
    u32(b);
  }
}
void sink(int level, const char*, int line, const char*) { u32(0x70), u32(static_cast<uint32_t>(level)), u32(static_cast<uint32_t>(line)); }
int32_t cnt(int off) { return static_cast<int32_t>(g_model.count(off)); }
}  // namespace

// A fresh model; materials 0 (untextured) and 1 (textured).
// `fixes`: the colinear drop's FIX (zfix bugs) on.
extern "C" void probe_gadd_begin(float precision, int fixes) {
  zfix::options().bugs = fixes != 0;
  zmod::tolerances_reset();
  zerr::set_sink(sink);
  std::memset(g_model.header, 0, sizeof g_model.header);
  g_model.polygons = nullptr, g_model.vertices = nullptr, g_model.normals = nullptr, g_model.morphs = nullptr;
  g_materials[0] = zmod::Material{};
  g_materials[1] = zmod::Material{};
  g_materials[1].flags = 1;
  zmod::set_vertex_precision(precision);
}

// The scripts' tolerances (0x00538070, 0x00538080) on the model being built.
extern "C" void probe_gadd_tolerances(float coplanar, float colinear) {
  zmod::set_coplanar_tolerance(coplanar);
  zmod::set_colinear_tolerance(colinear);
}

// One add; then the code, the reports and the model.
extern "C" int probe_gadd(int n, float* points, const float* normals, const float* uvs, int morph,
                          const float* morph_points, const float* morph_uvs, int material, int priority, uint32_t flag,
                          const uint8_t* zone, const float* colors, uint8_t* out, int cap) {
  g_out.clear();
  zmod::PolygonAdd a;
  a.points = points, a.normals = normals, a.uvs = uvs, a.morph = morph, a.morph_points = morph_points;
  a.morph_uvs = morph_uvs, a.material = &g_materials[material], a.priority = priority, a.flag = flag;
  a.zone = zone, a.colors = colors;
  u32(static_cast<uint32_t>(zmod::model_add_polygon(&g_model, n, a)));
  floats(points, 3 * static_cast<size_t>(n));
  u32(static_cast<uint32_t>(cnt(0x10))), u32(static_cast<uint32_t>(cnt(0x14))), u32(static_cast<uint32_t>(cnt(0x18))),
      u32(static_cast<uint32_t>(cnt(0x1c)));
  floats(g_model.vertices, 3 * static_cast<size_t>(cnt(0x14)));
  floats(g_model.normals, 3 * static_cast<size_t>(cnt(0x18)));
  floats(g_model.morphs, 3 * static_cast<size_t>(cnt(0x1c)));
  for (int32_t i = 0; i < cnt(0x10); ++i) {
    const zmod::Polygon& q = g_model.polygons[i];
    const int32_t k = static_cast<int32_t>(q.flags & 0xff);
    u32(q.flags), u32(static_cast<uint32_t>(q.priority));
    for (int32_t j = 0; j < k; ++j) u32(q.vertex_indices[j]);
    if (q.flags & 0x200) for (int32_t j = 0; j < k; ++j) u32(q.normal_indices[j]);
    u32(q.uvs != nullptr);
    if (q.uvs) floats(q.uvs, 2 * static_cast<size_t>(k));
    floats(q.colors, 3 * static_cast<size_t>(k));
    u32(q.material == &g_materials[1] ? 1u : q.material == &g_materials[0] ? 0u : 9u);
    u32(q.zone_set);
  }
  const int n2 = static_cast<int>(g_out.size());
  if (n2 <= cap) std::memcpy(out, g_out.data(), g_out.size());
  return n2;
}

// The scripts' render setters for test_script_setters.py: from the
// defaults, op 0 BFE (0x0053cef0) then its getter, 1 inverse z
// (0x00526df0), 2 vertex shading (0x00526a00), 3 dither (0x00579240);
// then the four globals.
extern "C" void probe_script_setter(int op, float f, int i, float out[4]) {
  zmod::draw_state() = zmod::DrawState{};
  zvid::display() = zvid::Display{};
  if (op == 0) zmod::set_bfe_tolerance(f), out[3] = zmod::bfe_tolerance();
  if (op == 1) zmod::set_inverse_z_tolerance(f);
  if (op == 2) zmod::set_vertex_shading(i);
  if (op == 3) zvid::set_dither(i);
  out[0] = zmod::draw_state().backface;
  out[1] = zmod::draw_state().priority_scale;
  int32_t vs = zmod::draw_state().vertex_shading, d = zvid::display().dither;
  std::memcpy(&out[2], op == 3 ? &d : &vs, 4);
  if (op != 0) out[3] = 0;
}
