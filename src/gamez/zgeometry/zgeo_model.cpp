#include "gamez/zgeometry/zgeo_model.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "crt/msvc_rand.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zgeometry/zgeo_weiler.h"
#include "gamez/zmath/zmth_vector.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_cull.h"
#include "gamez/zmodel/gmod_matl.h"

#pragma STDC FP_CONTRACT OFF

namespace zgeo {
namespace {
const char kFile[] = "C:\\zipper\\GameZMech3\\zGeometry\\zgeo_model.cpp";
constexpr float kByteScale = 0.007782219909131527f;  // 0x005cdd18, 255 / 32767
constexpr float kNear = 0.01f;                       // 0x005cdd28 (a double of it)

zmod::Model* node_model(zcls::Node* node) { return node ? node->model : nullptr; }

void report_count(int level, int line, const char* format, int32_t n) {
  char text[96];
  std::snprintf(text, sizeof text, format, n);
  zerr::report(level, kFile, line, text);
}

// The 16-bit colour 0x0050dd20 and 0x0050de00 pack: each value's low bits
// (the 0..255 values not scaled down to 5, 6 and 5 bits).
uint16_t pack(uint16_t word, float r, float g, float b) {
  word = static_cast<uint16_t>((static_cast<int32_t>(r) << 11) | (word & 0x7ff));
  word = static_cast<uint16_t>(((static_cast<int32_t>(g) & 0x3f) << 5) | (word & 0xf81f));
  return static_cast<uint16_t>((word & ~0x1f) | (static_cast<int32_t>(b) & 0x1f));
}

const float* vertex(const zmod::Model& m, const zmod::Polygon& p, int i) {
  return m.vertices + p.vertex_indices[i] * 3;
}
}  // namespace

ColorCache& colors() {
  static ColorCache c;
  return c;
}
Hooks::Hooks()
    : material_add(zmod::material_add),
      rand(crt::msvc_rand),
      node_model(zgeo::node_model),
      node_set_model(zcls::node_set_model),
      model_release(zmod::model_release) {}
Hooks& hooks() {
  static Hooks h;
  return h;
}
State& state() {
  static State s;
  return s;
}

// Mech3.exe 1.2 @ 0x0050da90
void colors_begin(float x, float y, float z, float radius) {
  ColorCache& c = colors();
  c.count = 0;
  c.center[0] = x, c.center[1] = y, c.center[2] = z;
  c.reach = radius - -4.0f;
}

// Mech3.exe 1.2 @ 0x0050dad0
void colors_add(const float pos[3], const float color[3]) {
  ColorCache& c = colors();
  if (!(pos[0] >= c.center[0] - c.reach) || !(pos[0] <= c.reach + c.center[0])) return;
  if (!(pos[2] >= c.center[2] - c.reach) || !(pos[2] <= c.reach + c.center[2])) return;
  if (c.count == kColorEntries) return;
  for (int32_t i = 0; i < c.count; ++i)
    if (pos[0] == c.entries[i].pos[0] && pos[2] == c.entries[i].pos[2]) return;
  ColorEntry& e = c.entries[c.count++];
  std::memcpy(e.pos, pos, sizeof e.pos);
  std::memcpy(e.color, color, sizeof e.color);
}

// Mech3.exe 1.2 @ 0x0050dbc0
int colors_find(const float pos[3], float out[3]) {
  const ColorCache& c = colors();
  if (c.count <= 0) return 0;
  int32_t found = 0;
  for (int32_t i = 0; i < c.count; ++i)
    if (std::fabs(pos[0] - c.entries[i].pos[0]) < 0.5f && std::fabs(pos[2] - c.entries[i].pos[2]) < 0.5f) {
      found = i;
      break;
    }
  std::memcpy(out, c.entries[found].color, 3 * sizeof(float));
  return 1;
}

// Mech3.exe 1.2 @ 0x0050dc90
void to_plane(float* points, int32_t n) {
  for (float* p = points; n != 0; --n, p += 3) {
    const float y = p[1];
    p[1] = -p[2];
    p[2] = y;
  }
}

// Mech3.exe 1.2 @ 0x0050dc70
void from_plane(float* points, int32_t n) {
  for (float* p = points; n != 0; --n, p += 3) {
    const float z = p[2];
    p[2] = -p[1];
    p[1] = z;
  }
}

// Mech3.exe 1.2 @ 0x0050dcb0
int boxes_touch(const float a[4], const float b[4]) {
  if (b[2] - -1.0f < a[0]) return 0;
  if (a[2] < b[0] - 1.0f) return 0;
  if (b[3] - -1.0f < a[1]) return 0;
  if (a[3] < b[1] - 1.0f) return 0;
  return 1;
}

// Mech3.exe 1.2 @ 0x0050e3d0
void box_of(float box[4], const float* points, int32_t n) {
  box[0] = box[2] = points[0];
  box[1] = box[3] = points[1];
  for (int32_t i = 1; i < n; ++i) {
    const float* p = points + 3 * i;
    if (p[0] < box[0]) box[0] = p[0];
    if (box[2] < p[0]) box[2] = p[0];
    if (p[1] < box[1]) box[1] = p[1];
    if (box[3] < p[1]) box[3] = p[1];
  }
}

// Mech3.exe 1.2 @ 0x0050dd20
zmod::Material* random_material() {
  zmod::Material m{};
  zmod::material_init(m);
  Hooks& h = hooks();
  const float g = static_cast<float>(h.rand()) * kByteScale;
  const float r = static_cast<float>(h.rand()) * kByteScale;
  const float b = static_cast<float>(h.rand()) * kByteScale;
  m.r = r, m.g = g, m.b = b;
  m.rgb = pack(m.rgb, r, g, b);
  return state().random = h.material_add(m);
}

// Mech3.exe 1.2 @ 0x0050de00
zmod::Material* color_material(float r, float g, float b) {
  zmod::Material m{};
  zmod::material_init(m);
  m.r = r, m.g = g, m.b = b;
  m.rgb = pack(m.rgb, r, g, b);
  return state().colored = hooks().material_add(m);
}

// Mech3.exe 1.2 @ 0x0050e020
void gradient(const float p0[3], const float p1[3], const float p2[3], float s0, float s1, float s2,
              float out[2]) {
  const float ax = p2[0] - p1[0], az = p2[2] - p1[2];
  const float bx = p0[0] - p1[0], bz = p0[2] - p1[2];
  const float det = az * bx - ax * bz;
  const float d0 = s0 - s1, d2 = s2 - s1;
  if (det == 0.0f) {
    out[0] = out[1] = 0.0f;
    return;
  }
  const float inv = 1.0f / det;
  out[0] = -((d2 * bz - az * d0) * inv);
  out[1] = -((ax * d0 - d2 * bx) * inv);
}

// Mech3.exe 1.2 @ 0x0050df30. The UVs are (u, v) pairs, carried from vertex 1.
std::vector<float> uvs_at(const float* points, int32_t n, const zmod::Model& m, const zmod::Polygon& p) {
  const float *v0 = vertex(m, p, 0), *v1 = vertex(m, p, 1), *v2 = vertex(m, p, 2);
  const float* uv = p.uvs;
  float gu[2], gv[2];
  gradient(v0, v1, v2, uv[0], uv[2], uv[4], gu);
  gradient(v0, v1, v2, uv[1], uv[3], uv[5], gv);
  std::vector<float> out(2 * static_cast<size_t>(n > 0 ? n : 0));
  for (int32_t i = 0; i < n; ++i) {
    const float dx = points[3 * i] - v1[0], dz = points[3 * i + 2] - v1[2];
    out[2 * i] = dz * gu[1] + dx * gu[0] + uv[2];
    out[2 * i + 1] = dz * gv[1] + dx * gv[0] + uv[3];
  }
  return out;
}

// Mech3.exe 1.2 @ 0x0050e100. Colours are (r, g, b) per vertex.
std::vector<float> colors_at(const float* points, int32_t n, const zmod::Model& m, const zmod::Polygon& p) {
  const float *v0 = vertex(m, p, 0), *v1 = vertex(m, p, 1), *v2 = vertex(m, p, 2);
  const float* c = p.colors;
  float g[3][2];
  for (int k = 0; k < 3; ++k) gradient(v0, v1, v2, c[k], c[3 + k], c[6 + k], g[k]);
  std::vector<float> out(3 * static_cast<size_t>(n > 0 ? n : 0));
  for (int32_t i = 0; i < n; ++i) {
    const float dx = points[3 * i] - v1[0], dz = points[3 * i + 2] - v1[2];
    for (int k = 0; k < 3; ++k) out[3 * i + k] = dz * g[k][1] + dx * g[k][0] + c[3 + k];
  }
  return out;
}

// Mech3.exe 1.2 @ 0x0050e240
std::vector<float> normals_at(const float* points, int32_t n, const zmod::Model& m, const zmod::Polygon& p) {
  const float *v0 = vertex(m, p, 0), *v1 = vertex(m, p, 1), *v2 = vertex(m, p, 2);
  const float* n0 = m.normals + p.normal_indices[0] * 3;
  const float* n1 = m.normals + p.normal_indices[1] * 3;
  const float* n2 = m.normals + p.normal_indices[2] * 3;
  float g[3][2];
  for (int k = 0; k < 3; ++k) gradient(v0, v1, v2, n0[k], n1[k], n2[k], g[k]);
  std::vector<float> out(3 * static_cast<size_t>(n > 0 ? n : 0));
  for (int32_t i = 0; i < n; ++i) {
    const float dx = points[3 * i] - v1[0], dz = points[3 * i + 2] - v1[2];
    for (int k = 0; k < 3; ++k) out[3 * i + k] = dz * g[k][1] + dx * g[k][0] + n1[k];
    zmth::normalize(*reinterpret_cast<zmth::Vec3*>(&out[3 * i]));
  }
  return out;
}

// Mech3.exe 1.2 @ 0x0050f0a0
void polygon_points(const zmod::Model& m, const zmod::Polygon& p, std::vector<float>& out) {
  const uint32_t n = p.flags & 0xff;
  out.resize(3 * n);
  for (uint32_t i = 0; i < n; ++i) std::memcpy(&out[3 * i], vertex(m, p, static_cast<int>(i)), 12);
}

// Mech3.exe 1.2 @ 0x0050e450
ClipPolygon* clip_polygon_new(const float* points, int32_t n) {
  ClipPolygon* c = new ClipPolygon;
  c->points.assign(points, points + 3 * n);
  to_plane(c->points.data(), n);
  c->count = n;
  box_of(c->box, c->points.data(), n);
  return c;
}

// Mech3.exe 1.2 @ 0x0050e4c0
int clip_polygon_out(const ClipPolygon& c, int32_t* count, std::vector<float>& out) {
  *count = c.count;
  out.assign(c.points.begin(), c.points.begin() + 3 * c.count);
  from_plane(out.data(), *count);
  return 0;
}

// Mech3.exe 1.2 @ 0x0050e520
void clip_polygon_free(ClipPolygon* c) {
  if (hooks().weiler_free) hooks().weiler_free(c->weiler);
  delete c;
}

// Mech3.exe 1.2 @ 0x0050e550
int find_point(const ClipPolygon& c, const float point[3]) {
  for (int32_t i = 0; i < c.count; ++i)
    if (points_near(&c.points[3 * i], point, kNear)) return i;
  return -1;
}

// Mech3.exe 1.2 @ 0x0050e690. An edge along x or y takes the point within
// 0.01 of its line; others need both ratios to agree within 0.01.
int find_edge(const ClipPolygon& c, const float point[3]) {
  const int32_t n = c.count;
  for (int32_t i = 0; i < n; ++i) {
    const float* a = &c.points[3 * i];
    const float* b = &c.points[3 * ((i + 1) % n)];
    const float dx = b[0] - a[0], dy = b[1] - a[1];
    float t;
    if (std::fabs(dx) < kNear) {
      if (std::fabs(a[0] - point[0]) > kNear) continue;
      t = (point[1] - a[1]) / dy;
    } else if (std::fabs(dy) < kNear) {
      if (std::fabs(point[1] - a[1]) > kNear) continue;
      t = (point[0] - a[0]) / dx;
    } else {
      const float tx = (point[0] - a[0]) / dx, ty = (point[1] - a[1]) / dy;
      if (!(std::fabs(tx - ty) < kNear)) continue;
      if (!(ty > 0.0f) || !(ty < 1.0f) || !(tx > 0.0f) || !(tx < 1.0f)) continue;
      if (std::fabs(tx * dx + a[0] - point[0]) > kNear) continue;
      if (std::fabs(ty * dy + a[1] - point[1]) > kNear) continue;
      return i;
    }
    if (t > 0.0f && t < 1.0f) return i;
  }
  return -1;
}

// Mech3.exe 1.2 @ 0x0050e5a0
int insert_points(ClipPolygon& c, const float* points, int32_t n) {
  int changed = 0;
  for (const float* p = points; n > 0; --n, p += 3) {
    int32_t i = find_point(c, p);
    if (i != -1) {
      std::memcpy(&c.points[3 * i], p, 12);
    } else {
      i = find_edge(c, p);
      if (i == -1) continue;
      c.points.insert(c.points.begin() + 3 * (i + 1), p, p + 3);
      ++c.count;
    }
    changed = 1;
  }
  return changed;
}

// Mech3.exe 1.2 @ 0x0050e910
ClipSet* clip_set_new() { return new ClipSet; }

// Mech3.exe 1.2 @ 0x0050e930
void clip_set_free(ClipSet* s) { delete s; }

// Mech3.exe 1.2 @ 0x0050e850
int commit(ClipSet& s) {
  Hooks& h = hooks();
  for (CellClip& cell : s.cells) {
    for (NodeModel& nm : cell.nodes) {
      zmod::Model* old = h.node_model(nm.node);
      h.node_set_model(nm.node, nm.model);
      if (old) h.model_release(old);
    }
    ++cell.cell->raw[0x39];
    cell.nodes.clear();
    cell.nodes.shrink_to_fit();
  }
  return 0;
}

// Mech3.exe 1.2 @ 0x0050dea0
int add_polygon(zmod::Model* m, int32_t n, const float* points, zmod::Material* material, const float* uvs,
                const float* normals, const float* colors) {
  uint8_t zone[4];
  zmod::zone_reset(zone);
  if (n < 3) {
    report_count(0x800, 0xfc, "Attempting to generate polygon with (%d) vertices", n);
    return -1;
  }
  if (!uvs && !material) material = random_material();
  return hooks().model_add(m, n, points, normals, uvs, 0, 0, 0, material, 0, 0, zone, colors);
}

// Mech3.exe 1.2 @ 0x0050f4e0
int add_child(zmod::Model* m, int32_t n, const float* points, const zmod::Model& src, const zmod::Polygon& p) {
  if (n < 3) {
    report_count(0x800, 0x1c8, "Attempting to add child polygon with (%d) vertices", n);
    return -1;
  }
  std::vector<float> uvs, colors, normals;
  zmod::Material* material;
  if (p.uvs) {
    uvs = uvs_at(points, n, src, p);
    colors = colors_at(points, n, src, p);
    if (p.flags >> 9 & 1) normals = normals_at(points, n, src, p);
    for (int32_t i = 0; i < n; ++i) colors_add(points + 3 * i, &colors[3 * i]);
    material = p.material;
  } else {
    material = random_material();
  }
  auto ptr = [](const std::vector<float>& v) { return v.empty() ? nullptr : v.data(); };
  return hooks().model_add(m, n, points, ptr(normals), ptr(uvs), 0, 0, 0, material, p.priority, p.flags >> 8 & 1,
                           reinterpret_cast<const uint8_t*>(&p.zone_set), ptr(colors));
}

// Mech3.exe 1.2 @ 0x0050f640
int copy_polygon(zmod::Model* m, const zmod::Model& src, const zmod::Polygon& p) {
  const int32_t n = static_cast<int32_t>(p.flags & 0xff);
  std::vector<float> points, normals;
  polygon_points(src, p, points);
  if (p.flags >> 9 & 1) {
    normals.resize(3 * static_cast<size_t>(n));
    for (int32_t i = 0; i < n; ++i) std::memcpy(&normals[3 * i], src.normals + p.normal_indices[i] * 3, 12);
  }
  std::vector<float> colors(p.colors, p.colors + 3 * n);
  return hooks().model_add(m, n, points.empty() ? nullptr : points.data(), normals.empty() ? nullptr : normals.data(),
                           p.uvs, 0, 0, 0, p.material, p.priority, p.flags >> 8 & 1,
                           reinterpret_cast<const uint8_t*>(&p.zone_set), colors.empty() ? nullptr : colors.data());
}

}  // namespace zgeo
