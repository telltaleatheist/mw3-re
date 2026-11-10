#include "mech3/zainet_inout_view.h"

#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/cls_util.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zgeometry/zgeo_model.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_cull.h"
#include "mech3/game_editor.h"
#include "mech3/zainet_inout.h"

namespace mech3::ai {

namespace {
zcls::Node* g_view = nullptr;  // [0x0060b334]

constexpr float kHeight = 10.0f;      // 0x005e8a68
constexpr uint32_t kVertices = 1024;  // 0x005e8a6c: a model's most
constexpr bool kTwoSided = true;      // 0x005e8a64: each triangle again, the other way round
constexpr bool kKeepAdding = true;    // 0x005e8a60: on after an add fails
constexpr bool kSideFlag = false;     // [0x0060b3a0]: never set
InOutWallsCalls g_calls;

zmod::Material* color(float r, float g, float b) {
  return g_calls.color ? g_calls.color(r, g, b) : zgeo::color_material(r, g, b);  // 0x0050de00
}
}  // namespace

InOutWallsCalls& inout_walls_calls() { return g_calls; }

// Mech3.exe 1.2 @ 0x004c42e0
InOutWalls::InOutWalls(zmod::Model* m) : model(m) {
  front = color(255.0f, 0.0f, 0.0f);
  side = color(0.0f, 0.0f, 255.0f);
  zmod::zone_reset(zone);  // 0x0053cc40; the points the zero vector 0x0063d870
}

// Mech3.exe 1.2 @ 0x004c4520 (`front`, flag (0x005e8a64 == 0)) and
// 0x004c4590 (`side`, flag [0x0060b3a0]): the triangle a, b, c added.
void InOutWalls::triangle(zmod::Material* m, bool flag) {
  if (!kKeepAdding && last != 0) return;
  zmod::PolygonAdd a;
  a.points = pts, a.material = m, a.flag = flag ? 1u : 0u, a.zone = zone;
  last = g_calls.add ? g_calls.add(model, 3, a) : zmod::model_add_polygon(model, 3, a);  // 0x0053c6c0
}

namespace {
void copy3(float* to, const float* from) { to[0] = from[0], to[1] = from[1], to[2] = from[2]; }
void swap3(float* a, float* b) {  // 0x004c5500
  for (int i = 0; i < 3; ++i) {
    const float t = a[i];
    a[i] = b[i], b[i] = t;
  }
}
}  // namespace

// Mech3.exe 1.2 @ 0x004c45f0: the wall from the last corner (a, b its top)
// to this one, two triangles, each again the other way round in the
// polygon's colour; a and b move on to this corner.
void InOutWalls::corner(const InOutVertex& v) {
  float* a = pts;
  float* b = pts + 3;
  float* c = pts + 6;
  copy3(c, v.pos);
  if (kTwoSided) {
    triangle(front, !kTwoSided);
    swap3(a, c);
    triangle(side, kSideFlag);
    copy3(c, a);
    c[1] += kHeight;
    triangle(front, !kTwoSided);
    swap3(b, c);
    triangle(side, kSideFlag);
    vertices += 3;
    triangles += 2;
  } else {
    triangle(front, !kTwoSided);
    copy3(a, c);
    c[1] += kHeight;
    triangle(front, !kTwoSided);
    copy3(b, c);
    vertices += 3;
    triangles += 1;
  }
}

// Mech3.exe 1.2 @ 0x004c43b0: a tested polygon's walls, its colour first;
// true (and nothing added) when its corners would take the model past 1024
// vertices.
bool InOutWalls::add(const InOutPolygon& p) {
  if (!p.tested) return false;
  const uint32_t n = static_cast<uint32_t>(p.verts.size());  // 0x004c4d60
  if (vertices + n * 3 >= kVertices) return true;
  copy3(pts, p.verts[n - 1].pos);  // 0x004c4d90(n - 1)
  copy3(pts + 3, pts);
  pts[4] += kHeight;
  switch (p.kind) {
    case 0:
    case 2:
      side = color(0.0f, 0.0f, 255.0f);
      break;
    case 1:
    case 3:
      side = color(255.0f, 255.0f, 0.0f);
      break;
  }
  for (const InOutVertex& v : p.verts) corner(v);  // 0x004c54d0
  return false;
}

zcls::Node*& inout_view() { return g_view; }

// Mech3.exe 1.2 @ 0x004c4920 (with 0)
bool inout_view_empty() { return g_view == nullptr; }

// Mech3.exe 1.2 @ 0x004c4950
void inout_view_show(const char* name) {
  if (name)
    inout_view_build();
  else
    inout_view_free();
}

// Mech3.exe 1.2 @ 0x004c4730: with a world and the areas read, the current
// mission's (0x004c36c0's number) walls made, a model at a time, each on an
// object3d under one root, which joins the world (its vt+0, 0x0040bab0).
void inout_view_build() {
  auto* world = static_cast<zcls::Node*>(editor_hooks().world_node());  // [0x00608c84]
  if (g_view || !world) return;
  const InOutVariant v = inout_variant();
  if (!inout().areas) return;  // [0x0060b33c]
  const int32_t at = v.index;  // 0x004c3910
  // FIX (bug): unchecked in the exe (0x004c50e0), as in inout_select.
  if (at < 0 || static_cast<size_t>(at) >= inout().areas->size()) return;
  const InOutArea& area = (*inout().areas)[size_t(at)];
  zcls::Node* root = zcls::object3d_create();  // 0x004fb740
  if (!root) return;
  auto it = area.polygons.begin();
  const auto end = area.polygons.end();
  zcls::Node* node = nullptr;
  while (it != end) {
    node = zcls::object3d_create();
    if (!node) break;
    zmod::Model* m = zmod::model_alloc();  // 0x00538fe0
    if (!m) break;
    InOutWalls w(m);
    zcls::node_set_model(node, m);    // 0x004eb370
    zcls::node_add_child(root, node);  // 0x004eb9b0
    const auto from = it;
    while (it != end && !w.add(**it)) ++it;  // 0x004c5530
    // FIX (bug): a polygon of 342 corners or more fits no model, and the exe
    // makes empty ones for it for ever; it is left out.
    if (it == from && it != end) ++it;
  }
  if (it != end && node) zcls::node_delete(node);  // 0x004eaea0
  zcls::node_set_intersect(root, 0);                // 0x004eb6d0
  g_view = root;
  zcls::node_add_child(world, root);  // 0x0040bab0
}

// Mech3.exe 1.2 @ 0x004c41b0: the root off its parents (0x004ebb40) and
// deleted with its walls (0x004f9050).
void inout_view_free() {
  if (!g_view) return;
  zcls::Node* root = g_view;
  while (root->parent_count() > 0) zcls::node_remove_child(root->parents[0], root);
  zcls::tree_destroy(root);
  g_view = nullptr;
}

}  // namespace mech3::ai
