// test_pm_script_cmds.py's model commands on PM's models (pm::zmod, 0x28-byte polygons).
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zclass/cls_di.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zmodel/gmod_shadows.h"
#include "pm/gamez/zmodel/gmod_const.h"
#include "pm/gamez/zvideo/zvid_tl.h"
#include "gamez/zvideo/zvid_ddd3d.h"

extern "C" {

// One model command on a PM model whose header is `header` (0x64 bytes, written back) with header +0x10 polygons:
// polygon i's flags poly_flags[i] (written back), its vertex indices from `indices` (flags & 0x3ff of them), its
// first material mats[poly_mat[i]] (0x20 bytes of the exe's layout, written back), UVs or none (has_uvs[i]).
// morphs: the vertex count's three floats each. which: 0 facade (a), 1 back face (a), 2 colour (a), 3 morph (fixed,
// a their count, f the value, b the minimum uses), 4 lighting (a).
void probe_pm_model_cmd(int32_t which, uint8_t* header, uint32_t* poly_flags, const uint32_t* indices,
                        const int32_t* poly_mat, const int32_t* has_uvs, uint8_t* mats, int32_t nmat,
                        const int32_t* fixed, int32_t a, int32_t b, float f, float* morphs) {
  pm::zmod::Model m{};
  std::memcpy(m.header, header, sizeof m.header);
  const int32_t npoly = m.count(0x10);
  std::vector<pm::zmod::Polygon> polys(npoly > 0 ? npoly : 0);
  std::vector<zmod::Material> mt(nmat);
  std::vector<zmod::Material*> first(polys.size());
  std::vector<std::vector<uint32_t>> vi(polys.size());
  static float* uv_dummy[1] = {nullptr};
  for (int32_t i = 0; i < nmat; ++i) {
    const uint8_t* r = mats + 0x20 * i;
    mt[i].alpha = r[0], mt[i].flags = r[1];
    std::memcpy(&mt[i].rgb, r + 2, 2);
    std::memcpy(&mt[i].r, r + 4, 12);
    std::memcpy(mt[i].unk_14, r + 0x14, 12);
  }
  size_t at = 0;
  for (int32_t i = 0; i < npoly; ++i) {
    pm::zmod::Polygon& q = polys[i];
    q.flags = poly_flags[i];
    vi[i].assign(indices + at, indices + at + (poly_flags[i] & 0x3ff));
    at += poly_flags[i] & 0x3ff;
    q.vertex_indices = vi[i].data();
    first[i] = &mt[poly_mat[i]];
    q.materials = &first[i];
    q.material_count = 1;
    q.uvs = has_uvs[i] ? uv_dummy : nullptr;
  }
  m.polygons = polys.data();
  switch (which) {
    case 0: pm::zmod::model_set_facade(&m, static_cast<uint32_t>(a)); break;
    case 1: pm::zmod::model_set_show_backface(&m, static_cast<uint32_t>(a)); break;
    case 2: pm::zmod::model_set_color(&m, a); break;
    case 3: pm::zmod::model_set_morph(&m, fixed, f, a, b); break;
    default: pm::zmod::model_set_lighting(&m, static_cast<uint32_t>(a)); break;
  }
  std::memcpy(header, m.header, sizeof m.header);
  for (int32_t i = 0; i < npoly; ++i) poly_flags[i] = polys[i].flags;
  for (int32_t i = 0; i < nmat; ++i) {
    uint8_t* r = mats + 0x20 * i;
    r[0] = mt[i].alpha, r[1] = mt[i].flags;
    std::memcpy(r + 2, &mt[i].rgb, 2);
    std::memcpy(r + 4, &mt[i].r, 12);
    std::memcpy(r + 0x14, mt[i].unk_14, 12);
  }
  if (which == 3 && m.morphs) std::memcpy(morphs, m.morphs, 12 * static_cast<size_t>(m.count(0x14)));
  std::free(m.morphs);
  m.morphs = nullptr;
  for (auto& q : polys) q.vertex_indices = nullptr, q.materials = nullptr, q.uvs = nullptr;
  m.polygons = nullptr;
}

// NodeSetLighting (0x0050db40) on a tree of nodes holding PM models (pm_model), as probe_lighting's.
int32_t probe_pm_node_lighting(const int32_t* parent, const int32_t* has_model, uint32_t* model_flags, int32_t count,
                          int32_t on) {
  std::vector<zcls::Node> nodes(count);
  std::vector<pm::zmod::Model> models(count);
  std::vector<std::vector<zcls::Node*>> kids(count);
  for (int32_t i = 1; i < count; ++i) kids[parent[i]].push_back(&nodes[i]);
  for (int32_t i = 0; i < count; ++i) {
    std::memcpy(models[i].header + 8, &model_flags[i], 4);
    if (has_model[i]) nodes[i].pm_model = &models[i];
    nodes[i].put<int32_t>(0x5c, static_cast<int32_t>(kids[i].size()));
    nodes[i].children = kids[i].data();
  }
  const int32_t r = zcls::node_set_lighting(&nodes[0], on);
  for (int32_t i = 0; i < count; ++i) std::memcpy(&model_flags[i], models[i].header + 8, 4);
  for (auto& n : nodes) n.children = nullptr, n.pm_model = nullptr;
  return r;
}

// PM's VideoSetWireFrame (0x005981f0): the GPU state's wireframe after, from `before`.
int32_t probe_pm_wireframe(int32_t on, int32_t before) {
  platform::DrawState& d = zvid::draw_state();
  const bool saved = d.wireframe;
  d.wireframe = before != 0;
  pm::zvid::set_wireframe(on);
  const int32_t r = d.wireframe ? 1 : 0;
  d.wireframe = saved;
  return r;
}

int probe_pm_gamez_stack(const char* paths, int rb, int gb, int bb, int with_models, uint8_t* out, int cap);

namespace {
// test_pm_make_shadows.py's stand-in for the segment query (test_make_shadows.py's): a point is shadowed by a hash of
// it; a running checksum of every segment asked (both ends) and the count.
uint32_t g_hash = 0, g_calls = 0;
int stand_in(zcls::Node*, const float from[3], const float to[3], zcls::LineHits* out) {
  uint32_t f[3], t[3];
  std::memcpy(f, from, 12), std::memcpy(t, to, 12);
  for (uint32_t v : {f[0], f[1], f[2], t[0], t[1], t[2]}) g_hash = (g_hash ^ v) * 0x01000193u;
  ++g_calls;
  const bool hit = ((f[0] ^ (f[1] * 31u) ^ (f[2] * 131u)) % 3u) == 0;
  out->count = hit ? 1 : 0;
  return hit ? 0 : 1;
}
}  // namespace

// A PM world loaded as probe_pm_gamez_stack does (nodes too), then MakeShadows (PM's 0x00563d20) with the stand-in
// query; out: every used model's polygons' colours in slot order; stats: the segments' count and checksum.
int probe_pm_make_shadows(const char* path, uint8_t* out, int cap, uint32_t* stats) {
  std::vector<uint8_t> scratch(64u << 20);
  if (probe_pm_gamez_stack(path, 5, 6, 5, 2, scratch.data(), static_cast<int>(scratch.size())) < 0) return -3;
  g_hash = 0x811c9dc5u, g_calls = 0;
  zmod::set_shadow_query(stand_in);
  zmod::make_shadows();
  zmod::set_shadow_query(nullptr);
  stats[0] = g_calls, stats[1] = g_hash;
  std::vector<uint8_t> b;
  const pm::zmod::ModelPool& pool = pm::zmod::models();
  for (int32_t i = 0; i < pool.capacity; ++i) {
    const pm::zmod::Model& m = pool.blocks[i / pool.block][i % pool.block];
    if (m.count(0xc) < 1 || m.count(0x10) <= 0) continue;
    for (int32_t k = 0; k < m.count(0x10); ++k) {
      const pm::zmod::Polygon& q = m.polygons[k];
      const auto* c = reinterpret_cast<const uint8_t*>(q.colors);
      if (c) b.insert(b.end(), c, c + 12 * (q.flags & 0x3ff));
    }
  }
  if (static_cast<int>(b.size()) > cap) return -1;
  std::memcpy(out, b.data(), b.size());
  return static_cast<int>(b.size());
}

}  // extern "C"
