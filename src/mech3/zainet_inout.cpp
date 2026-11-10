#include "pm/gamez/zmodel/gmod_const.h"
#include "mech3/zainet_inout.h"
#include "mech3/zainet_inout_view.h"

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

#include "mech3/zai_steer_pm.h"
#include "mech3/zai_steps.h"

#include "gamez/zclass/class.h"
#include "gamez/znetwork/znet_object.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/cls_util.h"
#include "gamez/zclass/list.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmodel/gmod_const.h"
#include "crt/strcase.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3::ai {

namespace {
constexpr const char* kFile = "C:\\zipper\\Mech3\\ZAiNetMechInOut.cpp";
constexpr float kWidth = 3.0f;  // 0x005e8a18
// 0x005e8a5c (1) and 0x0060b394 / 0x005e8a1c (0 / 1) are constants in the
// exe: the corners go on the ground, red and yellow keep the model's
// winding, blue and cyan are reversed.
constexpr bool kGroundCorners = true;
constexpr bool kReverse[4] = {false, false, true, true};

InOutHooks g_hooks;
InOut g_inout;
std::unordered_map<uintptr_t, InOutLast> g_met_last;  // the vehicles' +0xd8 records

// x87 compares: true when below or unordered (C0).
bool lt(float a, float b) { return !(a >= b); }
// 0x00537ae0: in x and z.
float dist_xz_of(const float* a, const float* b) {
  const float dx = a[0] - b[0], dz = a[2] - b[2];
  return static_cast<float>(std::sqrt(double{dz} * dz + double{dx} * dx));
}
// 0x004c2430. These follow the exe at 53 bits: docs/pm-map.md, "PM's AI at 53 bits".
float dot3(const float* a, const float* b) {
  return static_cast<float>((double{a[0]} * b[0] + double{a[1]} * b[1]) + double{a[2]} * b[2]);
}
constexpr float kOut = -0.5f;  // 0x0060b8e0

// Pirate's Moon 1.0 @ 0x004c9600: the corners' summed distance picks the size class; then the
// passes each walk the corners with the two before, from (n-2, n-1).
void make_sides(InOutPolygon& p) {
  const size_t n = p.verts.size();
  float around = 0;
  const InOutVertex* cur0 = &p.verts[n - 1];
  for (const InOutVertex& v : p.verts) {  // 0x004c8be0
    around = dist_xz_of(v.pos, cur0->pos) + around;
    cur0 = &v;
  }
  if (!(around > 35000.0f)) {
    if (around > 20000.0f) p.near_limit = 500, p.x3c = 10, p.x44 = 1, p.width = 12, p.far_limit = 3500, p.in_limit = 4;
  } else {
    p.near_limit = 500, p.x3c = 10, p.x44 = 1, p.width = 12, p.far_limit = 10000, p.in_limit = 4;
  }
  InOutVertex* cur = &p.verts[n - 1];
  for (InOutVertex& v : p.verts) {  // 0x004c85b0
    std::memcpy(cur->offset, cur->pos, 12);
    cur->dir[0] = v.pos[0] - cur->pos[0], cur->dir[1] = 0.0f, cur->dir[2] = v.pos[2] - cur->pos[2];
    const double root = std::sqrt((double{cur->dir[1]} * cur->dir[1] + double{cur->dir[2]} * cur->dir[2]) + double{cur->dir[0]} * cur->dir[0]);
    const float len = static_cast<float>(root);
    if (std::fabs(len) != 0.0f) {
      const double k = 1.0 / root;
      for (int i = 0; i < 3; ++i) cur->dir[i] = static_cast<float>(k * cur->dir[i]);
    }
    cur->normal[0] = cur->dir[2], cur->normal[1] = cur->dir[1], cur->normal[2] = -cur->dir[0];
    cur = &v;
  }
  InOutVertex* prev = &p.verts[n - 2];
  cur = &p.verts[n - 1];
  for (InOutVertex& v : p.verts) {  // 0x004c8720: the corner's miter
    const float d = dot3(prev->dir, cur->dir);
    float m = static_cast<float>(std::sqrt((1.0 - d) / (1.0 + d)));
    if (m > 0.0f && lt(((prev->normal[0] * cur->dir[0] + prev->normal[1] * cur->dir[1]) + prev->normal[2] * cur->dir[2]), 0.0f)) m = -m;
    const float x = cur->normal[0] + m * cur->dir[0], y = cur->normal[1] + m * cur->dir[1], z = cur->normal[2] + m * cur->dir[2];
    const float w = p.width, w2 = p.width - kOut;
    const float px = cur->pos[0], py = cur->pos[1], pz = cur->pos[2];
    cur->offset[0] = px - x * w, cur->offset[1] = py - y * w, cur->offset[2] = pz - z * w;
    cur->inner[0] = px - x * w2, cur->inner[1] = py - y * w2, cur->inner[2] = pz - z * w2;
    prev = cur;
    cur = &v;
  }
  float sum = 0;
  cur = &p.verts[n - 1];
  for (InOutVertex& v : p.verts) {  // 0x004c8990
    cur->along = sum;
    cur->length = dist_xz_of(v.offset, cur->offset);
    sum = sum + cur->length;
    for (int k = 0; k < 3; ++k) {
      if (!lt(p.min[k], v.offset[k])) p.min[k] = v.offset[k];  // a NaN keeps what's there
      if (!lt(v.offset[k], p.max[k])) p.max[k] = v.offset[k];
    }
    cur = &v;
  }
  p.perimeter = sum;
}

double dist_xz(const float* a, const float* b) {  // 0x004c80f0: the root left in the register
  const float dx = b[0] - a[0], dz = b[2] - a[2];
  return std::sqrt(double{dx} * dx + double{dz} * dz);
}
// 0x004c0fe0 (ECX out, EDX a): a + (b - a) t in x and z, a's height.
void lerp(float out[3], const float* a, float t, const float* b) {
  out[0] = static_cast<float>((double{b[0]} - a[0]) * t + a[0]);
  out[1] = a[1];
  out[2] = static_cast<float>((double{b[2]} - a[2]) * t + a[2]);
}
// The side of `p` from the line a -> b (0x004c1410 / 0x004c1770's sums).
float side(const float* a, const float* b, const float* p) {
  const float dz = a[2] - b[2], dx = a[0] - b[0];  // stored
  return static_cast<float>(((double{dz} * p[0] - double{dx} * p[2]) + double{b[2]} * a[0]) - double{b[0]} * a[2]);
}
bool opposite(float s1, float s2) { return (lt(s1, 0.0f) && lt(0.0f, s2)) || (s1 > 0.0f && 0.0f > s2); }
// From the side line p -> v: whether the ray from `a` through `b` meets it,
// and the ray from `b` back through `a`.
bool ray_from_a(float sa, float sb) { return (lt(sa, 0.0f) && lt(sa, sb)) || (sa > 0.0f && sa > sb); }
bool ray_from_b(float sa, float sb) { return (lt(sa, sb) && lt(0.0f, sb)) || (sa > sb && 0.0f > sb); }

// Pirate's Moon 1.0 @ 0x004c92f0 (ECX {a, b, prev}): segment a -> b against the side prev -> v.
bool crosses(const float* a, const float* b, const float* p, const float* v) {
  if (!opposite(side(a, b, p), side(a, b, v))) return false;
  const float sa = side(p, v, a), sb = side(p, v, b);
  return ray_from_a(sa, sb) && ray_from_b(sa, sb);
}

// Pirate's Moon 1.0 @ 0x004c8c30 (ECX {query, prev}): the side prev -> v nearest `from` kept, then
// the query's segment against it, a crossing's place along it kept when nearest or farthest.
void edge_count(InOutQuery& c, const InOutVertex* prev, const InOutVertex* v) {
  const float* a = c.from;
  const float* b = c.to;
  const float u[3] = {a[0] - prev->offset[0], a[1] - prev->offset[1], a[2] - prev->offset[2]};
  const float perp = wide_sum(u[2], prev->normal[2], u[0], prev->normal[0]);
  float along = 0, d = std::fabs(perp);
  bool have_along = false;
  if (lt(d, c.dist)) {
    along = wide_sum(u[2], prev->dir[2], u[0], prev->dir[0]);
    have_along = true;
    if (lt(along, 0.0f)) d = static_cast<float>(std::sqrt(double{u[2]} * u[2] + double{u[0]} * u[0]));
    else if (lt(prev->length, along)) d = dist_xz_of(a, v->offset);
    if (lt(d, c.dist)) c.nearest = prev, c.dist = d, c.along = along, c.perp = perp;
  }
  if (!opposite(side(a, b, prev->offset), side(a, b, v->offset))) return;
  const float sa = side(prev->offset, v->offset, a), sb = side(prev->offset, v->offset, b);
  const bool ra = ray_from_a(sa, sb), rb = ray_from_b(sa, sb);
  if (ra) ++c.from_rays;
  if (rb) ++c.to_rays;
  if (!(ra && rb)) return;
  ++c.crossings;
  const float t = static_cast<float>(-double{sa} / (double{sb} - sa));
  if (!have_along) along = wide_sum(u[2], prev->dir[2], u[0], prev->dir[0]), have_along = true;
  if (lt(t, c.t_near)) c.t_near = t, c.near_a = prev, c.near_b = v, c.near_along = along;
  if (lt(c.t_far, t)) c.t_far = t, c.far_a = prev, c.far_b = v, c.far_along = along;
}

// 0x004c7e90: a point `along` the side and `out` off it.
void on_side(float out[3], const InOutVertex* c, float along, float out_by) {
  for (int k = 0; k < 3; ++k) out[k] = c->offset[k] + along * c->dir[k];
  for (int k = 0; k < 3; ++k) out[k] = out[k] + out_by * c->normal[k];
}
float clamp_along(float a, const InOutVertex* c) {  // the query's two clamps
  if (lt(a, 0.0f)) return 0.0f;
  return c->length < a ? c->length : a;
}
// 0x004c82b0: a side's middle, half a unit out.
void middle(float out[3], const InOutVertex* c) {
  const float h = c->length * 0.5f;
  for (int k = 0; k < 3; ++k) out[k] = h * c->dir[k];
  for (int k = 0; k < 3; ++k) out[k] = c->inner[k] + out[k];
}
// The two ways round from one place on the outline to another (the bodies of 0x004cb8b0,
// 0x004cf730, 0x004cc1f6): the forward way, negative wrapped; `*shorter` when it is the shorter.
float round_way(float to, float from, float perimeter, bool* shorter) {
  float x = to - from;
  if (lt(x, 0.0f)) x = x + perimeter;
  const float other = perimeter - x;
  const bool fwd = !(x > other);
  if (shorter) *shorter = fwd;
  return fwd ? x : other;
}

bool colour_is(const zmod::Material* m, float r, float g, float b) { return m->r == r && m->g == g && m->b == b; }

// Pirate's Moon 1.0 @ 0x004cced0 (MW3's 0x004c32c0): polygons of three corners
// or more (PM's count is ten bits), not strips (0x2000; MW3 skipped 0x400), into
// the area.
void add_polygon(InOutArea& area, const char* name, const pm::zmod::Polygon& poly, const float* verts) {
  if ((poly.flags >> 13) & 1) return;
  if ((poly.flags & 0x3ff) <= 2) return;
  area.polygons.push_back(std::make_unique<InOutPolygon>(name, poly, verts, kWidth));
}

// Mech3.exe 1.2 @ 0x004c3380 (ECX area, the node): its model's vertices
// through the top of the matrix stack (0x004c3520), on the ground, then its
// polygons. The name kept is the node's past its first seven characters when
// it starts "inout" and is longer than that.
void read_model(InOutArea& area, zcls::Node* node) {
  pm::zmod::Model* m = node->pm_model;  // PM's world nodes hold theirs here (cls_node.h)
  const int32_t nv = m->count(0x14);
  if (nv <= 0) return;
  const char* name = node->name();
  if (std::strncmp(name, "inout", 5) == 0 && std::strlen(name) > 7) name += 7;
  std::vector<float> buf(static_cast<size_t>(nv) * 3);
  zmth::MatrixStack& st = zmth::matrix_stack();
  if (st.is_identity()) {
    std::memcpy(buf.data(), m->vertices, buf.size() * 4);
  } else {
    const float* t = st.matrix().m;
    for (int32_t i = 0; i < nv; ++i) {
      const float* s = m->vertices + 3 * i;
      float* d = buf.data() + 3 * i;
      d[0] = ((s[0] * t[0] + s[1] * t[3]) + s[2] * t[6]) + t[9];
      d[1] = ((s[0] * t[1] + s[1] * t[4]) + s[2] * t[7]) + t[10];
      d[2] = ((s[0] * t[2] + s[1] * t[5]) + s[2] * t[8]) + t[11];
    }
  }
  if (kGroundCorners && g_hooks.ground)
    for (int32_t i = 0; i < nv; ++i) g_hooks.ground(buf.data() + 3 * i);  // 0x004c5440 over 0x004c4240
  const int32_t np = m->count(0x10);
  for (int32_t i = 0; i < np; ++i) add_polygon(area, name, m->polygons[i], buf.data());  // 0x004c5470
}

void walk(zcls::Node* n, InOutArea& area);

void walk_children(zcls::Node* n, InOutArea& area) {  // 0x004c3f70
  for (int32_t i = 0; i < n->child_count(); ++i) walk(n->children[i], area);
}

// Mech3.exe 1.2 @ 0x004c3ed0: object3ds (5) through their matrices, LODs (6)
// in range, the other grouping classes their children only.
void walk(zcls::Node* n, InOutArea& area) {
  switch (n->cls()) {
    case 1: case 7: case 8: case 9: case 10: case 11:
      walk_children(n, area);
      break;
    case 5: {  // 0x004c3fb0
      if (!(n->flags() & 4)) return;
      n->set_flags(n->flags() & 0xfdffffff);
      auto* d = static_cast<uint8_t*>(n->data);
      uint32_t df;
      std::memcpy(&df, d, 4);
      auto* local = reinterpret_cast<zmth::Mat34*>(d + 0x30);
      auto* world = reinterpret_cast<zmth::Mat34*>(d + 0x60);
      const bool pushed = (df & 8) == 0;
      if (pushed) {
        if (!(n->flags() & 0x80000)) {
          zmth::push_copy(world);
          zmth::multiply(*local, 3);
        } else if (df & 0x20) {
          zmth::push_copy(world);
          zmth::multiply(*local, 3);
          df &= ~0x20u;
          std::memcpy(d, &df, 4);
        } else {
          zmth::push(world);
        }
      }
      if (n->pm_model) read_model(area, n);  // 0x004c3600 through [0x0060b39c]
      walk_children(n, area);
      if (pushed) zmth::pop();
      break;
    }
    case 6: {  // 0x004c40e0
      float range;
      std::memcpy(&range, static_cast<uint8_t*>(n->data) + 4, 4);
      if (!(n->flags() & 4) || !(range <= 0.0f)) return;
      if (n->pm_model) read_model(area, n);
      walk_children(n, area);
      break;
    }
    default:
      zerr::reportf(0x100, kFile, 0x845, "Unknown class type %d for node %s", n->cls(), n->name());
  }
}
}  // namespace

InOutHooks& inout_hooks() { return g_hooks; }
InOut& inout() { return g_inout; }

// Pirate's Moon 1.0 @ 0x004c9af0 (MW3's 0x004c1ab0)
InOutPolygon::InOutPolygon(const char* name_, const pm::zmod::Polygon& poly, const float* v, float width_)
    : width(width_) {
  min[0] = min[1] = min[2] = FLT_MAX;     // 0x0060b3b8
  max[0] = max[1] = max[2] = -FLT_MAX;    // 0x0060b3a8
  if (name_) name = strdup(name_);
  const zmod::Material* m = poly.material_count > 0 && poly.materials ? poly.materials[0] : nullptr;
  if (!m) zerr::reportf(0x200, kFile, 0x5a5, "Net without color");
  else if (colour_is(m, 0, 0, 255)) kind = 2;         // 0x0060b958
  else if (colour_is(m, 0, 255, 255)) kind = 3;  // 0x0060b978
  else if (colour_is(m, 255, 0, 0)) kind = 0;    // 0x0060b968
  else if (colour_is(m, 255, 255, 0)) kind = 1;  // 0x0060b988
  tested = kind == 0 || kind == 2;
  inverted = kind >= 2;
  const uint32_t n = poly.flags & 0x3ff;  // PM's 0x004c9af0
  verts.reserve(n);
  for (uint32_t k = 0; k < n; ++k) {
    const uint32_t i = kReverse[kind] ? n - 1 - k : k;
    const float* p = v + 3 * poly.vertex_indices[i];
    InOutVertex c;  // 0x004c8230
    std::memcpy(c.pos, p, 12);
    std::memcpy(c.offset, p, 12);
    verts.push_back(c);
  }
  make_sides(*this);
}

InOutPolygon::~InOutPolygon() { std::free(name); }

const InOutVertex* InOutPolygon::next(const InOutVertex* v) const {
  return ++v == verts.data() + verts.size() ? verts.data() : v;
}

// Pirate's Moon 1.0 @ 0x004cb370 (the query's template copied)
InOutArea::InOutArea() {
  min[0] = min[1] = min[2] = FLT_MAX;
  max[0] = max[1] = max[2] = -FLT_MAX;
}

// Pirate's Moon 1.0 @ 0x004ca690: skipped when the near crossing is a corner's turn hardly past
// the side's end.
void InOutPolygon::cross(InOutArea& area) const {
  if (!tested) return;
  InOutQuery c;
  c.from = area.q.from, c.to = area.q.to;
  c.poly = const_cast<InOutPolygon*>(this);
  c.length = area.q.length;
  c.back = area.q.back;
  const InOutVertex* prev = &verts.back();
  for (const InOutVertex& v : verts) {
    edge_count(c, prev, &v);
    prev = &v;
  }
  bool skip = false;
  if (c.near_a) {
    lerp(c.near_point, c.from, c.t_near - c.back, c.to);
    const float u[3] = {c.near_point[0] - c.near_a->offset[0], 0.0f, c.near_point[2] - c.near_a->offset[2]};
    c.near_along = wide_sum(u[2], c.near_a->dir[2], u[0], c.near_a->dir[0]);
    skip = (c.near_b == c.far_a && lt(c.near_a->length - c.near_along, 1.0f)) || (c.near_a == c.far_b && lt(c.near_along, 1.0f));
  }
  if (skip) return;
  InOutQuery& q = area.q;
  q.crossings += c.crossings;
  q.from_rays += c.from_rays;
  q.to_rays += c.to_rays;
  const bool from_in = ((c.from_rays & 1) != 0) != (inverted != 0);
  const bool to_in = ((c.to_rays & 1) != 0) != (inverted != 0);
  if ((c.crossings == 0 && from_in && to_in) || (c.crossings > 0 && lt(c.t_near, q.t_near))) {
    q.poly = c.poly;
    q.near_a = c.near_a, q.near_b = c.near_b, q.far_a = c.far_a, q.far_b = c.far_b;
    q.t_near = c.t_near, q.t_far = c.t_far;
    area.from_in = from_in, area.to_in = to_in;
    q.nearest = c.nearest, q.dist = c.dist, q.along = c.along, q.perp = c.perp;
    q.near_along = c.near_along, q.far_along = c.far_along;
    std::memcpy(q.near_point, c.near_point, 12);
  }
}

// Pirate's Moon 1.0 @ 0x004cb2d0 (over 0x004c92f0, from the last corner)
bool InOutPolygon::blocks(const float* from, const float* p) const {
  const InOutVertex* prev = &verts.back();
  for (const InOutVertex& v : verts) {
    if (crosses(from, p, prev->offset, v.offset)) return true;
    prev = &v;
  }
  return false;
}

// Pirate's Moon 1.0 @ 0x004caef0 / 0x004cb0e0 over 0x004cab20 (and 0x004cae70's limit), the sides
// from (last, first) on.
InOutPolygon::Near InOutPolygon::nearest(const float* from, const InOutVertex* ref, float limit) const {
  Near best;
  const InOutVertex* cur = &verts.back();
  for (const InOutVertex& v : verts) {
    bool look = true;
    if (ref) {
      float behind = ref->along - cur->along;
      if (lt(behind, 0.0f)) behind = behind + perimeter;
      look = lt(behind, limit);
    }
    if (look) {
      const float u[2] = {from[0] - cur->offset[0], from[2] - cur->offset[2]};
      const float perp = wide_sum(u[1], cur->normal[2], u[0], cur->normal[0]);
      float d = std::fabs(perp);
      if (lt(d, best.dist)) {
        const float along = wide_sum(u[1], cur->dir[2], u[0], cur->dir[0]);
        if (lt(along, 0.0f)) d = static_cast<float>(std::sqrt(double{u[1]} * u[1] + double{u[0]} * u[0]));
        else if (lt(cur->length, along)) d = dist_xz_of(from, v.offset);
        if (lt(d, best.dist)) best.corner = cur, best.dist = d, best.along = along, best.perp = perp;
      }
    }
    cur = &v;
  }
  return best;
}

// Pirate's Moon 1.0 @ 0x004cf350: a via node within 100 of a side, on the polygon and on the
// corners within 100 along the outline of that side.
void InOutPolygon::add_via(const InOutVia& via) {
  const Near a = nearest(via.pos);
  const Near b = nearest(via.other_pos);
  if (!a.corner || !lt(a.dist, 100.0f)) return;
  InOutVia w = via;
  w.corner = a.corner, w.other_corner = b.corner;
  vias.push_back(w);
  const InOutVertex* first = verts.data();
  const InOutVertex* end = first + verts.size();
  const InOutVertex* lo = a.corner;
  const InOutVertex* hi = a.corner;
  float at = a.corner->along - 100.0f;
  while (at < lo->along) {  // back to the corner before the span
    if (lo == first) {
      lo = end;
      at = at + perimeter;
    }
    --lo;
  }
  at = a.corner->along + 100.0f;
  while (!(hi->along > at)) {  // on to the corner past it
    if (++hi == end) {
      hi = first;
      at = at - perimeter;
    }
  }
  for (const InOutVertex* c = lo; c != hi;) {
    auto* m = const_cast<InOutVertex*>(c);
    bool have = false;
    for (const InOutVia& o : m->vias) have = have || (o.net == w.net && o.other == w.other);
    if (!have) m->vias.push_back(w);
    if (++c == end) c = first;
  }
}

bool InOutArea::clear(const float* from, const float* p) const {
  for (const auto& poly : polygons)
    if (poly->tested && poly->blocks(from, p)) return false;
  return true;
}

// Pirate's Moon 1.0 @ 0x004cca00: which way round is shorter, near side to far side.
const InOutVertex* InOutArea::shorter_way() {
  const float d1 = static_cast<float>(dist_xz(q.near_point, q.near_b->offset));  // 0x004c80f0
  const float both = static_cast<float>(dist_xz(q.far_point, q.far_a->offset) + d1);
  float c = static_cast<float>((double{q.far_a->along} - q.near_b->along) + both);
  if (lt(c, 0.0f)) c = c + q.poly->perimeter;
  const float other = q.poly->perimeter - c;
  if (lt(c, other)) {
    q.arc = c;
    return q.near_b;
  }
  q.arc = other;
  return q.near_a;
}

// Pirate's Moon 1.0 @ 0x004cb8b0: one side on from `from_side`, toward `side_` the shorter way:
// its middle when that's in a clear line, else the corner.
const float* InOutArea::round_to(const InOutVertex* from_side, const InOutVertex* side_) {
  const InOutPolygon& poly = *q.poly;
  float x = side_->along - from_side->along;
  if (lt(x, 0.0f)) x = x + poly.perimeter;
  const InOutVertex* corner;
  const InOutVertex* mid;
  if (lt(poly.perimeter - x, x)) {
    outcome += 3;
    corner = from_side;
    mid = (from_side == poly.verts.data() ? poly.verts.data() + poly.verts.size() : from_side) - 1;
  } else {
    outcome += 1;
    corner = mid = poly.next(from_side);
  }
  middle(q.scratch, mid);
  if (clear(q.from, q.scratch)) return q.scratch;
  outcome += 1;
  return corner->inner;
}

// Pirate's Moon 1.0 @ 0x004cba50 over 0x004c8370, 0x004cf730: for a vehicle that may use them, the
// cheapest via near this corner, by the way round to it, its length, and the way on.
const float* InOutArea::way_through(bool ways, const InOutVertex* corner) {
  if (!ways || corner->vias.empty()) return nullptr;
  float best = q.arc;
  if (q.poly->far_limit < best) best = q.poly->far_limit;
  const InOutVia* got = nullptr;
  const InOutVertex* first = q.poly->verts.data();
  const float around = q.poly->perimeter;
  for (const InOutVia& v : corner->vias) {
    // 0x004cf730: its own side's place; the exe's next corner runs off the vector for the last
    // side, only for a distance it then doesn't use unless NaN.
    const InOutVertex* c8 = v.other_corner;
    float along = 0;
    if (c8) {
      const float u[2] = {v.pos[0] - c8->offset[0], v.pos[2] - c8->offset[2]};
      along = clamp_along(wide_sum(u[1], c8->dir[2], u[0], c8->dir[0]), c8);
    }
    const float m1 = round_way(q.far_a->along, (c8 ? c8 : first)->along + along, around, nullptr);
    const float n = clamp_along(q.near_along, q.near_a);
    const float m2 = round_way(v.corner->along, q.near_a->along + n, around, nullptr);
    const float cost = (m1 + dist_xz_of(v.pos, v.other_pos)) + m2;
    if (lt(cost, best)) best = cost, got = &v;
  }
  q.via = got;
  if (!got) return nullptr;
  if (got->corner == corner) {
    outcome += 5;
    return got->pos;
  }
  return round_to(corner, got->corner);
}

// Pirate's Moon 1.0 @ 0x004cbbb0. From inside a polygon, close to a side: the nearest place half a
// unit past it, and the way goes on from there. Inside and deep in: that place is the answer.
// `to` inside: its nearest place, as the last crossing. Then by distances: far from the side,
// short of the first crossing; a long way round, a via or the middle of a side nearer `to`;
// else the corners round the shorter way, the last in a clear line taken.
const float* InOutArea::resolve(bool ways) {
  const float* r = nullptr;
  bool near_set = false, far_set = false;
  if (from_in && lt(q.dist, q.poly->in_limit)) {
    from_in = 0;
    q.near_a = q.nearest;
    q.near_b = q.poly->next(q.near_a);
    on_side(q.near_point, q.nearest, clamp_along(q.along, q.nearest), kOut);
    q.from = q.near_point;
    near_set = true;
    ++q.crossings;
  }
  if (to_in) {
    const InOutPolygon::Near n = q.poly->nearest(q.to);
    if (n.corner) {  // FIX (A): NaN distances keep none; the exe reads through null
      for (int k = 0; k < 3; ++k) q.far_point[k] = n.along > 0.0f ? n.corner->offset[k] + n.along * n.corner->dir[k] : n.corner->offset[k];
      for (int k = 0; k < 3; ++k) q.far_point[k] = q.far_point[k] + kOut * n.corner->normal[k];
      q.far_a = n.corner;
      q.far_b = q.poly->next(n.corner);
      far_set = true;
      ++q.crossings;
    }
  }
  outcome = 0;
  if (!(q.crossings > 0 || (from_in && to_in))) return nullptr;
  if (from_in) {
    outcome = 1;
    on_side(q.near_point, q.nearest, clamp_along(q.along, q.nearest), kOut);
    q.from = q.near_point;
    return q.near_point;
  }
  if (!q.near_a || !q.far_a) return nullptr;
  outcome = 2;
  if (!near_set) {
    lerp(q.near_point, q.from, q.t_near, q.to);
    for (int k = 0; k < 3; ++k) q.near_point[k] = q.near_point[k] + kOut * q.near_a->normal[k];
  }
  if (!far_set) {
    lerp(q.far_point, q.from, q.t_far, q.to);
    for (int k = 0; k < 3; ++k) q.far_point[k] = q.far_point[k] + kOut * q.far_a->normal[k];
  }
  const InOutVertex* way = shorter_way();
  const InOutPolygon& poly = *q.poly;
  if (lt(poly.near_limit, q.dist)) {
    outcome = 3;
    return q.near_point;
  }
  if (lt(poly.far_limit, q.arc)) {
    outcome = 4;
    r = way_through(ways, q.nearest);
    if (!r && lt(q.t_near * q.length, poly.near_limit)) r = way_through(ways, q.near_a);
    if (r) return r;
    const InOutPolygon::Near n = poly.nearest(q.to, q.near_a, poly.far_limit);
    if (!n.corner) return q.near_point;  // FIX (A): as above
    if (n.corner == q.near_a) {
      outcome = 0xb;
      middle(q.scratch, q.near_a);
      return q.scratch;
    }
    outcome = 0xc;
    if (lt(q.dist, 1.5f)) {
      bool s1, s2;
      const float m1 = round_way(n.corner->along, q.near_a->along, poly.perimeter, &s1);
      const float m2 = round_way(n.corner->along, q.nearest->along, poly.perimeter, &s2);
      if (s1 == s2 && lt(m2, m1)) return round_to(q.nearest, n.corner);
    }
    return round_to(q.near_a, n.corner);
  }
  outcome = 0x13;
  r = way_through(ways, q.near_a);
  if (!r) {
    outcome = 0x14;
    const InOutVertex* first = poly.verts.data();
    const InOutVertex* end = first + poly.verts.size();
    if (way == q.near_a) {
      const InOutVertex* cur = q.far_a;
      while (!r && cur != q.near_a) {
        if (++cur == end) cur = first;
        if (clear(q.from, cur->inner)) r = cur->inner;
      }
    } else {
      const InOutVertex* cur = q.far_b;
      while (!r && cur != q.near_b) {
        if (cur == first) cur = end;
        --cur;
        if (clear(q.from, cur->inner)) r = cur->inner;
      }
    }
  }
  return r ? r : q.near_point;
}

// Pirate's Moon 1.0 @ 0x004cc7f0: the polygon met last on its own first, the rest only when it
// changes nothing.
const float* InOutArea::query(const float* from, const float* to, InOutPolygon* first, bool ways) {
  const float* got = to;
  q = InOutQuery{};
  q.from = from, q.to = to;
  from_in = to_in = 0;
  const float dx = to[0] - from[0], dz = to[2] - from[2];
  q.length = static_cast<float>(std::sqrt(double{dz} * dz + double{dx} * dx));
  q.back = 0.1f / q.length;
  q.via = nullptr;
  if (first) {
    first->cross(*this);
    if (const float* r = resolve(ways)) got = r;
  }
  if (got == to) {
    for (const auto& poly : polygons)
      if (poly.get() != first) poly->cross(*this);
    if (const float* r = resolve(ways)) got = r;
  }
  return got;
}

// Pirate's Moon 1.0 @ 0x004cdb30 (MW3's 0x004c36c0): a campaign's and a
// standalone mission's area is the mission's, of ten (0x00435db0).
InOutVariant inout_variant() {
  InOutVariant v;
  if (!g_hooks.have_game || !g_hooks.have_game()) return v;
  const char* suffix = nullptr;
  v.game = g_hooks.game_type ? g_hooks.game_type() : -1;
  if (v.game == 1 || v.game == 5) {
    v.numbered = 1;
    v.index = (g_hooks.mission_number ? g_hooks.mission_number() : 0) - 1;
    v.count = 10;
    suffix = v.game == 1 ? "m" : "sa";  // 0x0060ba5c, 0x0060ba58
  } else if (v.game == 2) {
    v.count = 1;
    suffix = "ia";  // 0x0060ba54
  } else if (v.game == 4) {
    v.count = 1;
    suffix = "t";  // 0x0060ba50
  }
  if (suffix) {
    v.valid = 1;
    v.prefix = std::snprintf(v.name, sizeof v.name, "%s%s", "inout", suffix);
  }
  return v;
}

// Pirate's Moon 1.0 @ 0x004cdce0 (MW3's 0x004c3850)
int32_t InOutVariant::match(const char* child) const {
  long n = 0;
  if (valid) {
    if (!numbered) {
      if (strcasecmp(name, child) == 0) n = 1;
    } else {
      char* end = nullptr;
      if (strncasecmp(name, child, static_cast<size_t>(prefix)) == 0) n = std::strtol(child + prefix, &end, 0);
      if (!end || *end != 0) n = 0;
    }
  }
  return static_cast<int32_t>(n - 1);
}

// Mech3.exe 1.2 @ 0x004c3e50 (ECX node, EDX area, the callback 0x004c3600):
// from the node's place in the world (0x004ed280 mode 3 on a fresh top).
void inout_read_tree(zcls::Node* node, InOutArea& area) {
  if (!node) return;
  zmth::Mat34 top;
  zmth::push(&top);
  zmth::load_identity();
  if (zcls::node_to_world(node, 3) == 0) walk(node, area);
  zmth::pop();
}

// Pirate's Moon 1.0 @ 0x004cddb0 (MW3's 0x004c3930)
void inout_mission_load() {
  g_inout.areas.reset();
  zcls::Node* root = zcls::list_find_by_name(6, "inout");
  if (!root) return;
  const InOutVariant v = inout_variant();
  if (g_hooks.have_game && g_hooks.have_game() && v.valid) {
    // 0x004c3630: count copies of a fresh area (a negative count is none).
    g_inout.areas = std::make_unique<std::vector<InOutArea>>(static_cast<size_t>(v.count > 0 ? v.count : 0));
    for (int32_t i = 0; i < root->child_count(); ++i) {
      zcls::Node* c = root->children[i];
      const int32_t at = v.match(c->name());
      // Past the vector: skipped, as PM's own check (0x004cddb0).
      if (at >= 0 && static_cast<size_t>(at) < g_inout.areas->size()) inout_read_tree(c, (*g_inout.areas)[size_t(at)]);
    }
  }
  while (root->parent_count() > 0) zcls::node_remove_child(root->parents[0], root);  // 0x004ebb40
  zcls::tree_destroy(root);  // 0x004f9050
}

// Pirate's Moon 1.0 @ 0x004ce290 (MW3's 0x004c3b80)
void inout_select() {
  if (!g_inout.areas) return;
  const int32_t i = inout_variant().index;
  // FIX (bug): unchecked in the exe; a mission past ten would point past the vector.
  g_met_last.clear();
  if (i >= 0 && static_cast<size_t>(i) < g_inout.areas->size()) g_inout.current = &(*g_inout.areas)[size_t(i)];
}

// Pirate's Moon 1.0 @ 0x004ccaf0
bool inout_pull(uintptr_t vehicle, const float pos[3], const float in[3], float out[3], bool ways) {
  InOutArea* a = g_inout.current;
  if (!a) {
    g_met_last.erase(vehicle);
    return false;
  }
  // Vehicle 0 is ours, the way finder's grid (zai_nav.cpp): no vehicle, nothing kept.
  const float* r = a->query(pos, in, vehicle ? g_met_last[vehicle].poly : nullptr, ways);
  if (vehicle) g_met_last[vehicle] = InOutLast{a->q.poly, a->q.via, a->q.via && a->q.via->pos == r};
  if (r == in) return false;
  out[0] = r[0], out[1] = r[1], out[2] = r[2];
  out[1] = in[1];  // after the copy: with `in` as `out`, the pulled point's own height
  return true;
}

InOutLast inout_last(uintptr_t vehicle) {
  const auto it = g_met_last.find(vehicle);
  return it == g_met_last.end() ? InOutLast{} : it->second;
}

void inout_add_via(const void* net, const void* node, const float* pos, const void* other, const float* other_pos) {
  if (!g_inout.current) return;
  InOutVia v;
  v.net = net, v.node = node, v.pos = pos, v.other = other, v.other_pos = other_pos;
  for (const auto& p : g_inout.current->polygons) p->add_via(v);
}

// Pirate's Moon 1.0 @ 0x004ce2f0, 0x004c7d60, 0x004c7c00 and 0x004c7b20 (each step's vt+0x60, +0x74, +0xa0: a via node;
// its place and its +0x30's, and the net, to 0x004cf350 on each polygon; the exe's log line of it goes nowhere).
void for_each_via(void (*fn)(void* ctx, AiNet* net, const ViaNode* node), void* ctx) {
  for (znet::Object* x : znet::registry().list) {
    auto* net = dynamic_cast<CompiledNet*>(x);  // 0x004c7c00: vt+0x28, then vt+0x34
    if (!net) continue;
    for (znet::Element* e : net->elems) {  // 0x004c7b20: vt+0x60, +0x74, +0xa0
      Behaviour* b = e ? static_cast<AiElem*>(e)->as_behaviour() : nullptr;
      if (b && b->kind == StepKind::ViaNode) fn(ctx, net, static_cast<const ViaNode*>(b));
    }
  }
}

int32_t inout_register_vias() {
  if (!g_inout.current) return 0;
  int32_t n = 0;
  for_each_via(
      [](void* ctx, AiNet* net, const ViaNode* v) {
        const Behaviour* other = v->next_step;  // its +0x30
        inout_add_via(net, v, v->position(), other, other ? other->position() : nullptr);
        ++*static_cast<int32_t*>(ctx);
      },
      &n);
  return n;
}

int32_t inout_via_count() {
  if (!g_inout.current) return 0;
  size_t n = 0;
  for (const auto& p : g_inout.current->polygons) n += p->vias.size();
  return static_cast<int32_t>(n);
}

// Mech3.exe 1.2 @ 0x004c3bd0 -> 0x004c2a30 over 0x004c29a0. Any other
// message tests the red ones (kind 0).
namespace {
int32_t g_inout_changes = 0;
}
int32_t inout_changes() { return g_inout_changes; }

void inout_set_state(const char* name, int32_t message) {
  if (!name || !g_inout.current) return;
  ++g_inout_changes;
  int32_t kind = 0;
  switch (message) {
    case 0x39: kind = 1; break;
    case 0x3a: kind = 0; break;
    case 0x3b: kind = 3; break;
    case 0x3c: kind = 2; break;
  }
  for (const auto& p : g_inout.current->polygons)
    if (p->name && std::strcmp(name, p->name) == 0) p->tested = p->kind == kind;
}

// Mech3.exe 1.2 @ 0x004c3dd0
void inout_mission_end() {
  g_met_last.clear();
  g_inout.areas.reset();
  ++g_inout_changes;
  // FIX (bug): the exe leaves [0x0060b344] pointing at the freed area; a next
  // mission without one (multiplayer, or a world without `inout`) then pulls
  // the AI's destinations with freed memory (docs/ai.md section 11).
  g_inout.current = nullptr;
  // FIX (bug): the boundary's drawing ([0x0060b334], under the world) is
  // kept by the exe past the world's end, so the next Show InOut takes it as
  // still drawn and its taking down reaches freed nodes; forgotten here.
  inout_view() = nullptr;
}

}  // namespace mech3::ai
