// PM's mission boundary, for test_pm_ai_boundary.py: an area built from polygons the test makes,
// its queries and via nodes, each as text.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "gamez/zmodel/gmod_const.h"
#include "mech3/zainet_inout.h"
#include "pm/gamez/zmodel/gmod_const.h"

namespace {
using namespace mech3::ai;
std::unique_ptr<InOutArea> g_area;
std::vector<std::unique_ptr<float[]>> g_keep;

uint32_t fb(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}
int poly_id(const InOutPolygon* p) {
  for (size_t i = 0; i < g_area->polygons.size(); ++i)
    if (g_area->polygons[i].get() == p) return static_cast<int>(i);
  return p ? -2 : -1;
}
int corner_id(const InOutVertex* v) {
  if (!v) return -1;
  for (size_t i = 0; i < g_area->polygons.size(); ++i) {
    const auto& c = g_area->polygons[i]->verts;
    if (!c.empty() && v >= c.data() && v < c.data() + c.size()) return static_cast<int>(i * 1000 + (v - c.data()));
  }
  return -2;
}
void add(std::string& o, uint32_t v) {
  char b[16];
  std::snprintf(b, sizeof b, "%x ", v);
  o += b;
}
void add_i(std::string& o, int v) {
  o += std::to_string(v) + ' ';
}
void addf(std::string& o, const float* p, int n) {
  for (int i = 0; i < n; ++i) add(o, fb(p[i]));
}
int finish(const std::string& o, char* out, int size) {
  if (static_cast<int>(o.size()) >= size) return -1;
  std::memcpy(out, o.c_str(), o.size() + 1);
  return static_cast<int>(o.size());
}
}  // namespace

extern "C" void probe_pm_boundary_begin() {
  g_area = std::make_unique<InOutArea>();
  g_keep.clear();
  inout().current = g_area.get();
}

extern "C" void probe_pm_boundary_end() {
  inout().current = nullptr;
  inout_mission_end();
  g_area.reset();
  g_keep.clear();
}

extern "C" void probe_pm_boundary_polygon(float r, float g, float b, int n, const float* xyz, float width, const char* name) {
  zmod::Material m{};
  m.r = r, m.g = g, m.b = b;
  zmod::Material* mats[1] = {&m};
  std::vector<uint32_t> idx(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) idx[static_cast<size_t>(i)] = static_cast<uint32_t>(i);
  pm::zmod::Polygon poly{};
  poly.flags = static_cast<uint32_t>(n);
  poly.vertex_indices = idx.data();
  poly.material_count = 1;
  poly.materials = mats;
  g_area->polygons.push_back(std::make_unique<InOutPolygon>(name, poly, xyz, width));
}

extern "C" int probe_pm_boundary_dump(char* out, int size) {
  std::string o;
  for (const auto& p : g_area->polygons) {
    add(o, fb(p->perimeter)), add(o, fb(p->width));
    addf(o, p->min, 3), addf(o, p->max, 3);
    add(o, fb(p->far_limit)), add(o, fb(p->near_limit)), add(o, fb(p->x3c)), add(o, fb(p->in_limit));
    add(o, p->x44), add(o, p->inverted), add(o, p->tested), add(o, static_cast<uint32_t>(p->kind));
    o += p->name ? p->name : "-";
    o += ' ';
    for (const InOutVia& v : p->vias) add_i(o, corner_id(v.corner)), add_i(o, corner_id(v.other_corner));
    o += "|\n";
    for (const InOutVertex& c : p->verts) {
      addf(o, c.offset, 3), addf(o, c.pos, 3), addf(o, c.inner, 3);
      add(o, fb(c.length)), add(o, fb(c.along));
      addf(o, c.normal, 3), addf(o, c.dir, 3);
      for (const InOutVia& v : c.vias) add_i(o, corner_id(v.corner)), add_i(o, corner_id(v.other_corner));
      o += '\n';
    }
  }
  return finish(o, out, size);
}

extern "C" void probe_pm_boundary_via(const float* pos, const float* other_pos, int id, int other) {
  auto* p = new float[3];
  auto* q = new float[3];
  std::memcpy(p, pos, 12), std::memcpy(q, other_pos, 12);
  g_keep.emplace_back(p), g_keep.emplace_back(q);
  inout_add_via(nullptr, reinterpret_cast<const void*>(static_cast<uintptr_t>(id + 1)), p,
                reinterpret_cast<const void*>(static_cast<uintptr_t>(other + 1)), q);
}

extern "C" void probe_pm_boundary_state(const char* name, int message) { inout_set_state(name, message); }

extern "C" int probe_pm_boundary_query(const float* from, const float* to, int first, int ways, char* out, int size) {
  InOutPolygon* met = first >= 0 ? g_area->polygons[static_cast<size_t>(first)].get() : nullptr;
  const float* r = g_area->query(from, to, met, ways != 0);
  const InOutQuery& q = g_area->q;
  std::string o;
  add_i(o, r == to ? 1 : 0);
  addf(o, r, 3);
  add_i(o, g_area->outcome);
  add_i(o, poly_id(q.poly));
  add_i(o, corner_id(q.near_a)), add_i(o, corner_id(q.near_b)), add_i(o, corner_id(q.far_a)), add_i(o, corner_id(q.far_b));
  add_i(o, corner_id(q.nearest));
  add(o, fb(q.length)), add(o, fb(q.t_near)), add(o, fb(q.t_far));
  addf(o, q.near_point, 3), addf(o, q.far_point, 3);
  add_i(o, q.crossings), add_i(o, q.from_rays), add_i(o, q.to_rays);
  add(o, fb(q.arc)), add(o, fb(q.dist)), add(o, fb(q.along)), add(o, fb(q.perp));
  add(o, fb(q.near_along)), add(o, fb(q.far_along)), add(o, fb(q.back));
  addf(o, q.scratch, 3);
  add(o, g_area->from_in), add(o, g_area->to_in);
  return finish(o, out, size);
}

// The clamp 0x004ccaf0: a vehicle by number, what it pulled to.
extern "C" int probe_pm_boundary_pull(int vehicle, const float* pos, const float* in, int ways, float* out) {
  const int got = inout_pull(static_cast<uintptr_t>(vehicle), pos, in, out, ways != 0) ? 1 : 0;
  const InOutLast l = inout_last(static_cast<uintptr_t>(vehicle));
  // Bit 1 on: the via's node number + 1 from bit 2, bit 1 whether the answer is its place.
  return got | (l.at_via ? 2 : 0) | static_cast<int>(l.via ? reinterpret_cast<uintptr_t>(l.via->node) << 2 : 0);
}

// For the test's hunt, after a query: which rare case of 0x004cc32c it was. 1 the side it's nearest
// taken, 2 just past the 1.5 that takes it, 3 nearer that way but the two ways disagree; else 0.
extern "C" int probe_pm_boundary_case() {
  const InOutQuery& q = g_area->q;
  if (!q.poly || !q.nearest || !q.near_a || q.nearest == q.near_a) return 0;
  const InOutPolygon::Near n = q.poly->nearest(q.to, q.near_a, q.poly->far_limit);
  if (!n.corner) return 0;
  const float around = q.poly->perimeter;
  auto way = [&](float from, bool* fwd) {
    float x = n.corner->along - from;
    if (x < 0.0f) x += around;
    *fwd = !(x > around - x);
    return *fwd ? x : around - x;
  };
  bool s1, s2;
  const float m1 = way(q.near_a->along, &s1), m2 = way(q.nearest->along, &s2);
  if (!(m2 < m1)) return 0;
  if (s1 != s2) return 3;
  return q.dist < 1.5f ? 1 : q.dist < 2.5f ? 2 : 0;
}
