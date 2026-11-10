// The boundary's walls for test_inout_view.py: InOutWalls (0x004c42e0,
// 0x004c43b0, 0x004c45f0) on polygons given as corners, the calls out
// recorded: each colour asked (0x0050de00) and each triangle added
// (0x0053c6c0), the adds answering from a list.
#include <cstdint>
#include <cstring>

#include "gamez/zmodel/gmod_const.h"
#include "mech3/zainet_inout.h"
#include "mech3/zainet_inout_view.h"

namespace {
struct Tri {
  int32_t n, material, flag;
  uint8_t zone[4];
  float pts[9];
};
struct Out {
  int32_t full[64];
  Tri tris[2048];
  int32_t n_tris;
  float colors[256][3];
  int32_t n_colors;
  int32_t triangles, vertices, last;
};
Out* g_out = nullptr;
const int32_t* g_answers = nullptr;
int32_t g_n_answers = 0, g_next = 0;

zmod::Material* color(float r, float g, float b) {
  Out& o = *g_out;
  if (o.n_colors < 256) o.colors[o.n_colors][0] = r, o.colors[o.n_colors][1] = g, o.colors[o.n_colors][2] = b;
  return reinterpret_cast<zmod::Material*>(static_cast<uintptr_t>(++o.n_colors));
}
int add(zmod::Model*, int32_t n, const zmod::PolygonAdd& a) {
  Out& o = *g_out;
  if (o.n_tris < 2048) {
    Tri& t = o.tris[o.n_tris];
    t.n = n, t.material = static_cast<int32_t>(reinterpret_cast<uintptr_t>(a.material)), t.flag = int32_t(a.flag);
    std::memcpy(t.zone, a.zone, 4);
    std::memcpy(t.pts, a.points, sizeof t.pts);
  }
  ++o.n_tris;
  return g_n_answers ? g_answers[g_next++ % g_n_answers] : 0;
}
}  // namespace

extern "C" void probe_inout_walls(int32_t start_vertices, int32_t n_polys, const int32_t* counts, const uint8_t* tested,
                                  const int32_t* kinds, const float* corners, const int32_t* answers, int32_t n_answers,
                                  Out* out) {
  using namespace mech3::ai;
  std::memset(out, 0, sizeof *out);
  g_out = out, g_answers = answers, g_n_answers = n_answers, g_next = 0;
  inout_walls_calls() = {color, add};
  InOutWalls w(nullptr);
  w.vertices = static_cast<uint32_t>(start_vertices);
  const float* c = corners;
  for (int32_t i = 0; i < n_polys; ++i) {
    static zmod::Material red{};
    red.r = 255.0f;
    static uint32_t idx[3] = {0, 1, 2};
    static const float three[9] = {0, 0, 0, 1, 0, 0, 0, 0, 1};
    zmod::Polygon q{};
    q.flags = 3, q.vertex_indices = idx, q.material = &red;
    InOutPolygon p("x", q, three, 0.0f);  // then its corners and class as given
    p.verts.resize(size_t(counts[i]));
    for (InOutVertex& v : p.verts) v.pos[0] = c[0], v.pos[1] = c[1], v.pos[2] = c[2], c += 3;
    p.tested = tested[i], p.kind = kinds[i];
    out->full[i] = w.add(p) ? 1 : 0;
  }
  out->triangles = w.triangles, out->vertices = static_cast<int32_t>(w.vertices), out->last = w.last;
  inout_walls_calls() = {};
  g_out = nullptr;
}
