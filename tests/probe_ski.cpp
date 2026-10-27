// The skis' debug polygon 0x004776d0 (its material 0x00477730) and the model
// priority 0x0053bb20, for tests/differential/test_ski_polygon.py: a model
// built by a run of adds, dumped after each.
#include <cstdint>
#include <cstring>
#include <vector>

#include "crt/msvc_rand.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zutil/zfix.h"
#include "mech3/mech_skis.h"

namespace {
std::vector<uint8_t> g_out;
zmod::Model g_model;

void u32(uint32_t v) {
  for (int i = 0; i < 4; ++i) g_out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
void f32(float f) {
  uint32_t b;
  std::memcpy(&b, &f, 4);
  u32(b);
}
int32_t cnt(int off) { return static_cast<int32_t>(g_model.count(off)); }
void sink(int level, const char*, int line, const char*) { u32(0x70), u32(static_cast<uint32_t>(level)), u32(static_cast<uint32_t>(line)); }

// What both sides dump: the model's counts, vertices and polygons (flags,
// priority, vertex indices, the material's colour), then the next rand().
void dump_model() {
  u32(static_cast<uint32_t>(cnt(0x10))), u32(static_cast<uint32_t>(cnt(0x14)));
  for (int32_t i = 0; i < 3 * cnt(0x14); ++i) f32(g_model.vertices[i]);
  for (int32_t i = 0; i < cnt(0x10); ++i) {
    const zmod::Polygon& q = g_model.polygons[i];
    u32(q.flags), u32(static_cast<uint32_t>(q.priority)), u32(q.zone_set);
    for (uint32_t j = 0; j < (q.flags & 0xff); ++j) u32(q.vertex_indices[j]);
    const zmod::Material* m = q.material;
    u32(m ? 1u : 0u);
    // Flag bits 3..7 are the exe's stack garbage (material_init keeps them): bits 0..2.
    if (m) u32(m->alpha), u32(m->flags & 7u), u32(m->rgb), f32(m->r), f32(m->g), f32(m->b);
  }
}

int finish(uint8_t* out, int cap) {
  const int n = static_cast<int>(g_out.size());
  if (n <= cap) std::memcpy(out, g_out.data(), g_out.size());
  return n;
}
}  // namespace

extern "C" void probe_ski_begin(uint32_t seed) {
  zfix::options().bugs = false;  // the original path (the colinear drop's FIX)
  zerr::set_sink(sink);
  zmod::materials() = zmod::MaterialTable{};
  std::memset(g_model.header, 0, sizeof g_model.header);
  g_model.polygons = nullptr, g_model.vertices = nullptr, g_model.normals = nullptr, g_model.morphs = nullptr;
  crt::msvc_srand(seed);
}

// One add: the answer, the caller's points after it, the model, the next rand().
extern "C" int probe_ski_add(int n, float* points, uint8_t* out, int cap) {
  g_out.clear();
  u32(mech3::ski_polygon(&g_model, n, points) ? 1u : 0u);
  for (int i = 0; i < 3 * n; ++i) f32(points[i]);
  dump_model();
  u32(static_cast<uint32_t>(crt::msvc_rand()));
  return finish(out, cap);
}

// The priority on the model (or on none): every polygon's after it.
extern "C" int probe_ski_priority(int priority, int null_model, uint8_t* out, int cap) {
  g_out.clear();
  zmod::model_set_priority(null_model ? nullptr : &g_model, priority);
  for (int32_t i = 0; i < cnt(0x10); ++i) u32(static_cast<uint32_t>(g_model.polygons[i].priority));
  return finish(out, cap);
}
