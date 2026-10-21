// zGeometry's model side for tests/differential/test_zgeo_model.py: each
// function run on the test's inputs, with zModel's add-a-polygon, the
// material table, rand, the node calls and Weiler's free stubbed as the test
// stubs the original's; everything they are called with and everything the
// functions leave written out as one stream.
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zgeometry/zgeo_model.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_matl.h"

namespace {
std::vector<uint8_t> g_out;
const int32_t* g_rand = nullptr;
int g_rand_at = 0;
zmod::Material g_mats[16];
zmod::Model g_models[4];
zcls::Node g_nodes[16];
zcls::Partition g_cells[8];
int g_added = 0;

void u32(uint32_t v) {
  for (int i = 0; i < 4; ++i) g_out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
void f32(float v) {
  uint32_t b;
  std::memcpy(&b, &v, 4);
  u32(b);
}
void floats(const float* p, int n) {
  if (!p) return u32(0);
  u32(1);
  for (int i = 0; i < n; ++i) f32(p[i]);
}
// Ids: materials 0x100 + slot, models 0x200 + slot, nodes 0x300 + slot; the
// ones material_add makes 0x400 + n.
uint32_t mat_id(const zmod::Material* m) {
  if (!m) return 0;
  if (m >= g_mats && m < g_mats + 16) return 0x100 + static_cast<uint32_t>(m - g_mats);
  return 0x400 + m->texture_index;
}
uint32_t model_id(const zmod::Model* m) { return m ? 0x200 + static_cast<uint32_t>(m - g_models) : 0; }
uint32_t node_id(const zcls::Node* n) { return n ? 0x300 + static_cast<uint32_t>(n - g_nodes) : 0; }

std::vector<zmod::Material> g_made;
zmod::Material* material_add(zmod::Material& m) {
  u32(0x61), u32(m.alpha), u32(m.flags), u32(m.rgb), f32(m.r), f32(m.g), f32(m.b), u32(m.soil);
  static zmod::Material made[64];
  zmod::Material* r = &made[g_added % 64];
  r->texture_index = static_cast<uint32_t>(g_added++);
  return r;
}
int rand_stub() { return g_rand[g_rand_at++]; }
int model_add(zmod::Model* m, int32_t n, const float* points, const float* normals, const float* uvs, uint32_t a,
              uint32_t b, uint32_t c, zmod::Material* material, int32_t priority, uint32_t flag,
              const uint8_t zone[4], const float* colors) {
  u32(0x62), u32(model_id(m)), u32(static_cast<uint32_t>(n));
  floats(points, 3 * n), floats(normals, 3 * n), floats(uvs, 2 * n);
  u32(a), u32(b), u32(c), u32(mat_id(material)), u32(static_cast<uint32_t>(priority)), u32(flag);
  uint32_t z;
  std::memcpy(&z, zone, 4);
  u32(z);
  floats(colors, 3 * n);
  return n + 7;
}
zmod::Model* node_model(zcls::Node* node) {
  u32(0x63), u32(node_id(node));
  return node->model;
}
int node_set_model(zcls::Node* node, zmod::Model* model) {
  u32(0x64), u32(node_id(node)), u32(model_id(model));
  return 0;
}
int model_release(zmod::Model* m) {
  u32(0x65), u32(model_id(m));
  return 0;
}
void weiler_free(void* p) { u32(0x66), u32(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p))); }
void sink(int level, const char*, int line, const char* message) {
  u32(0x67), u32(static_cast<uint32_t>(level)), u32(static_cast<uint32_t>(line));
  const size_t n = std::strlen(message);
  u32(static_cast<uint32_t>(n));
  g_out.insert(g_out.end(), message, message + n);
}

void setup(const int32_t* rands) {
  g_out.clear();
  g_rand = rands, g_rand_at = 0, g_added = 0;
  zgeo::Hooks& h = zgeo::hooks();
  h.model_add = model_add, h.material_add = material_add, h.rand = rand_stub;
  h.node_model = node_model, h.node_set_model = node_set_model, h.model_release = model_release;
  h.weiler_free = weiler_free;
  zerr::set_sink(sink);
}
int finish(uint8_t* out, int cap) {
  const int n = static_cast<int>(g_out.size());
  if (n <= cap) std::memcpy(out, g_out.data(), g_out.size());
  return n;
}
void dump_colors() {
  const zgeo::ColorCache& c = zgeo::colors();
  u32(static_cast<uint32_t>(c.count)), f32(c.reach), floats(c.center, 3);
  for (int32_t i = 0; i < c.count; ++i) floats(c.entries[i].pos, 3), floats(c.entries[i].color, 3);
}

// A model with `nv` vertices and normals and polygon 0 as described.
struct Poly {
  zmod::Polygon p{};
  std::vector<uint32_t> vidx, nidx;
  std::vector<float> uvs, colors;
};
Poly g_poly;
void build(const float* verts, const float* normals, uint32_t flags, int32_t priority, const uint32_t* vidx,
           const uint32_t* nidx, const float* uvs, const float* colors, int32_t material, uint32_t zone) {
  zmod::Model& m = g_models[0];
  m.vertices = const_cast<float*>(verts), m.normals = const_cast<float*>(normals);
  const int n = static_cast<int>(flags & 0xff);
  g_poly.vidx.assign(vidx, vidx + n), g_poly.nidx.assign(nidx, nidx + n);
  g_poly.uvs.clear(), g_poly.colors.assign(colors, colors + 3 * n);
  if (uvs) g_poly.uvs.assign(uvs, uvs + 2 * n);
  zmod::Polygon& p = g_poly.p;
  p.flags = flags, p.priority = priority;
  p.vertex_indices = g_poly.vidx.data(), p.normal_indices = g_poly.nidx.data();
  p.uvs = uvs ? g_poly.uvs.data() : nullptr, p.colors = g_poly.colors.data();
  p.material = material ? &g_mats[material - 0x100] : nullptr;
  p.zone_set = zone;
}
}  // namespace

// The colour cache: records of 7 floats {op, x, y, z, a, b, c}: 0 begin at
// (x, y, z) radius a, 1 add (x, y, z) colour (a, b, c), 2 find (x, y, z).
extern "C" int probe_zgeo_colors(const float* ops, int n, uint8_t* out, int cap) {
  setup(nullptr);
  zgeo::colors() = zgeo::ColorCache{};
  for (int i = 0; i < n; ++i) {
    const float* o = ops + 7 * i;
    if (o[0] == 0) zgeo::colors_begin(o[1], o[2], o[3], o[4]);
    if (o[0] == 1) zgeo::colors_add(o + 1, o + 4);
    if (o[0] == 2) {
      float c[3] = {-1, -1, -1};
      u32(static_cast<uint32_t>(zgeo::colors_find(o + 1, c))), floats(c, 3);
    }
  }
  dump_colors();
  return finish(out, cap);
}

// 0 to_plane, 1 from_plane (n points), 2 boxes_touch (two boxes), 3 box_of
// (n points), 4 gradient (p0, p1, p2, s0, s1, s2).
extern "C" int probe_zgeo_geom(int op, const float* in, int n, uint8_t* out, int cap) {
  setup(nullptr);
  std::vector<float> v(in, in + (op == 2 ? 8 : op == 4 ? 12 : 3 * n));
  v.resize(v.size() + 3);  // never empty: a 0-point run logs a buffer, as the original's
  if (op == 0) zgeo::to_plane(v.data(), n), floats(v.data(), 3 * n);
  if (op == 1) zgeo::from_plane(v.data(), n), floats(v.data(), 3 * n);
  if (op == 2) u32(static_cast<uint32_t>(zgeo::boxes_touch(v.data(), v.data() + 4)));
  if (op == 3) {
    float box[4];
    zgeo::box_of(box, v.data(), n), floats(box, 4);
  }
  if (op == 4) {
    float g[2];
    zgeo::gradient(v.data(), v.data() + 3, v.data() + 6, v[9], v[10], v[11], g), floats(g, 2);
  }
  return finish(out, cap);
}

// Polygon 0 of a model: 0 uvs_at, 1 colors_at, 2 normals_at at the points;
// 3 polygon_points; 4 add_child, 5 copy_polygon (into model 1), the colour
// cache begun at `cache` (x, y, z, radius) first.
extern "C" int probe_zgeo_poly(int op, const float* points, int n, const float* verts, const float* normals,
                               uint32_t flags, int32_t priority, const uint32_t* vidx, const uint32_t* nidx,
                               const float* uvs, const float* colors, int32_t material, uint32_t zone,
                               const float* cache, const int32_t* rands, uint8_t* out, int cap) {
  setup(rands);
  build(verts, normals, flags, priority, vidx, nidx, uvs, colors, material, zone);
  const zmod::Model& m = g_models[0];
  const zmod::Polygon& p = g_poly.p;
  std::vector<float> r;
  if (op == 0) r = zgeo::uvs_at(points, n, m, p), floats(r.data(), 2 * n);
  if (op == 1) r = zgeo::colors_at(points, n, m, p), floats(r.data(), 3 * n);
  if (op == 2) r = zgeo::normals_at(points, n, m, p), floats(r.data(), 3 * n);
  if (op == 3) zgeo::polygon_points(m, p, r), floats(r.data(), static_cast<int>(3 * (flags & 0xff)));
  if (op == 4 || op == 5) {
    zgeo::colors() = zgeo::ColorCache{};
    zgeo::colors_begin(cache[0], cache[1], cache[2], cache[3]);
    const int ret = op == 4 ? zgeo::add_child(&g_models[1], n, points, m, p) : zgeo::copy_polygon(&g_models[1], m, p);
    u32(static_cast<uint32_t>(ret));
    dump_colors();
  }
  return finish(out, cap);
}

// add_polygon into model 1: material 0 or a slot id, uvs/normals/colors
// optional.
extern "C" int probe_zgeo_add(const float* points, int n, int32_t material, const float* uvs, const float* normals,
                              const float* colors, const int32_t* rands, uint8_t* out, int cap) {
  setup(rands);
  const int ret = zgeo::add_polygon(&g_models[1], n, points, material ? &g_mats[material - 0x100] : nullptr, uvs,
                                    normals, colors);
  u32(static_cast<uint32_t>(ret));
  return finish(out, cap);
}

// A clip polygon of n points: its box; then for each of the m points
// find_point, find_edge and insert_points of it alone; its points back out;
// then a fresh one with all m inserted at once; both freed.
extern "C" int probe_zgeo_clip(const float* points, int n, const float* ins, int m, uint8_t* out, int cap) {
  setup(nullptr);
  zgeo::ClipPolygon* c = zgeo::clip_polygon_new(points, n);
  c->weiler = reinterpret_cast<void*>(uintptr_t{0x5150});
  floats(c->box, 4), floats(c->points.data(), 3 * n);
  for (int i = 0; i < m; ++i) {
    u32(static_cast<uint32_t>(zgeo::find_point(*c, ins + 3 * i)));
    u32(static_cast<uint32_t>(zgeo::find_edge(*c, ins + 3 * i)));
    u32(static_cast<uint32_t>(zgeo::insert_points(*c, ins + 3 * i, 1)));
  }
  int32_t count = -1;
  std::vector<float> back(3, 7.0f);
  u32(static_cast<uint32_t>(zgeo::clip_polygon_out(*c, &count, back)));
  u32(static_cast<uint32_t>(count)), floats(back.data(), 3 * count);
  zgeo::clip_polygon_free(c);
  c = zgeo::clip_polygon_new(points, n);
  u32(static_cast<uint32_t>(zgeo::insert_points(*c, ins, m)));
  u32(static_cast<uint32_t>(c->count)), floats(c->points.data(), 3 * c->count);
  zgeo::clip_polygon_free(c);
  return finish(out, cap);
}

// commit: cells[i] has counts[i] (node, model) pairs from `pairs`
// (node slot, model slot or -1); nodes' current models from `current`;
// the cells' crater counts from `craters`.
extern "C" int probe_zgeo_commit(int ncells, const int32_t* counts, const int32_t* pairs, const int32_t* current,
                                 const uint8_t* craters, uint8_t* out, int cap) {
  setup(nullptr);
  for (int i = 0; i < 16; ++i) g_nodes[i].model = current[i] < 0 ? nullptr : &g_models[current[i]];
  zgeo::ClipSet* s = zgeo::clip_set_new();
  const int32_t* pr = pairs;
  for (int i = 0; i < ncells; ++i) {
    zgeo::CellClip cell;
    cell.cell = &g_cells[i];
    g_cells[i].raw[0x39] = craters[i];
    for (int k = 0; k < counts[i]; ++k, pr += 2)
      cell.nodes.push_back({&g_nodes[pr[0]], pr[1] < 0 ? nullptr : &g_models[pr[1]]});
    s->cells.push_back(cell);
  }
  u32(static_cast<uint32_t>(zgeo::commit(*s)));
  for (int i = 0; i < ncells; ++i) u32(g_cells[i].raw[0x39]), u32(static_cast<uint32_t>(s->cells[i].nodes.size()));
  zgeo::clip_set_free(s);
  return finish(out, cap);
}

// The two coloured materials from rand and from arguments.
extern "C" int probe_zgeo_materials(const float* rgb, const int32_t* rands, uint8_t* out, int cap) {
  setup(rands);
  u32(mat_id(zgeo::random_material()));
  u32(mat_id(zgeo::color_material(rgb[0], rgb[1], rgb[2])));
  u32(mat_id(zgeo::state().random)), u32(mat_id(zgeo::state().colored));
  return finish(out, cap);
}
