#include "mech3/zainet_inout.h"

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/cls_util.h"
#include "gamez/zclass/list.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmodel/gmod_const.h"

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

// 0x004c1030 on (x, -, z): normalized in x and z unless of no length; the length.
float normalize_xz(float v[3]) {
  const float len = std::sqrt(v[0] * v[0] + v[2] * v[2]);  // 0x004c10e0
  if (len != 0.0f) {
    const float k = 1.0f / len;
    v[0] = v[0] * k;
    v[2] = v[2] * k;
  }
  return len;
}

// 0x004c1160 -> 0x004c5210 over 0x004c11f0: the corners walked in order with
// the two before each, from (n-2, n-1); each one's box, the corner before it
// given its pushed-out place and its side's length.
void make_sides(InOutPolygon& p) {
  const size_t n = p.verts.size();
  InOutVertex* prev = &p.verts[n - 2];
  InOutVertex* cur = &p.verts[n - 1];
  for (size_t i = 0; i < n; ++i) {
    InOutVertex* v = &p.verts[i];
    for (int k = 0; k < 3; ++k) {  // 0x004c51f0, 0x004c5200: the box keeps its value on a NaN
      if (!(p.min[k] < v->pos[k])) p.min[k] = v->pos[k];
      if (!(v->pos[k] < p.max[k])) p.max[k] = v->pos[k];
    }
    float a[3] = {prev->pos[2] - cur->pos[2], 0.0f, cur->pos[0] - prev->pos[0]};
    float b[3] = {cur->pos[2] - v->pos[2], 0.0f, v->pos[0] - cur->pos[0]};
    normalize_xz(a);
    cur->length = normalize_xz(b);
    p.perimeter = p.perimeter + cur->length;
    if (cur->length <= 0.0f) zerr::report(0x100, kFile, 0x1bc, "zero length segment in an inside/outside net");
    float c[3] = {a[0] + b[0], 0.0f, a[2] + b[2]};
    normalize_xz(c);
    c[0] = c[0] * p.width;
    c[2] = c[2] * p.width;
    cur->offset[0] = cur->pos[0] + c[0];
    cur->offset[2] = cur->pos[2] + c[2];
    cur->offset[1] = 0.0f;
    prev = cur;
    cur = v;
  }
}

bool colour_is(const zmod::Material* m, float r, float g, float b) { return m->r == r && m->g == g && m->b == b; }

// Mech3.exe 1.2 @ 0x004c32c0: polygons of three corners or more, not
// morphing (flag 0x400), into the area.
void add_polygon(InOutArea& area, const char* name, const zmod::Polygon& poly, const float* verts) {
  if ((poly.flags >> 10) & 1) return;
  if ((poly.flags & 0xff) <= 2) return;
  area.polygons.push_back(std::make_unique<InOutPolygon>(name, poly, verts, kWidth));
}

// Mech3.exe 1.2 @ 0x004c3380 (ECX area, the node): its model's vertices
// through the top of the matrix stack (0x004c3520), on the ground, then its
// polygons. The name kept is the node's past its first seven characters when
// it starts "inout" and is longer than that.
void read_model(InOutArea& area, zcls::Node* node) {
  zmod::Model* m = node->model;
  const int32_t nv = static_cast<int32_t>(m->count(0x14));
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
  const int32_t np = static_cast<int32_t>(m->count(0x10));
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
      if (n->model) read_model(area, n);  // 0x004c3600 through [0x0060b39c]
      walk_children(n, area);
      if (pushed) zmth::pop();
      break;
    }
    case 6: {  // 0x004c40e0
      float range;
      std::memcpy(&range, static_cast<uint8_t*>(n->data) + 4, 4);
      if (!(n->flags() & 4) || !(range <= 0.0f)) return;
      if (n->model) read_model(area, n);
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

// Mech3.exe 1.2 @ 0x004c1ab0
InOutPolygon::InOutPolygon(const char* name_, const zmod::Polygon& poly, const float* v, float width_)
    : width(width_) {
  min[0] = min[1] = min[2] = FLT_MAX;     // 0x0060b3b8
  max[0] = max[1] = max[2] = -FLT_MAX;    // 0x0060b3a8
  if (name_) name = strdup(name_);
  const zmod::Material* m = poly.material;
  if (colour_is(m, 0, 0, 255)) kind = 2;         // 0x005e8a20
  else if (colour_is(m, 0, 255, 255)) kind = 3;  // 0x005e8a40
  else if (colour_is(m, 255, 0, 0)) kind = 0;    // 0x005e8a30
  else if (colour_is(m, 255, 255, 0)) kind = 1;  // 0x005e8a50
  tested = kind == 0 || kind == 2;
  inverted = kind >= 2;
  const uint32_t n = poly.flags & 0xff;
  verts.reserve(n);
  for (uint32_t k = 0; k < n; ++k) {
    const uint32_t i = kReverse[kind] ? n - 1 - k : k;
    const float* p = v + 3 * poly.vertex_indices[i];
    InOutVertex c;  // 0x004c1110
    std::memcpy(c.pos, p, 12);
    std::memcpy(c.offset, p, 12);
    verts.push_back(c);
  }
  make_sides(*this);
}

InOutPolygon::~InOutPolygon() { std::free(name); }

// Mech3.exe 1.2 @ 0x004c26b0
InOutArea::InOutArea() {
  float big = FLT_MAX;
  std::memcpy(&query[(0x34 - 0x14) / 4], &big, 4);  // 0x004c1a00 at start-up: the template's +0x20
  min[0] = min[1] = min[2] = FLT_MAX;
  max[0] = max[1] = max[2] = -FLT_MAX;
}

// Mech3.exe 1.2 @ 0x004c36c0
InOutVariant inout_variant() {
  InOutVariant v;
  if (!g_hooks.have_game || !g_hooks.have_game()) return v;
  const char* suffix = nullptr;
  v.operation = g_hooks.operation ? g_hooks.operation() : 0;
  v.game = g_hooks.game_type ? g_hooks.game_type() : -1;
  if (v.game == 1) {
    v.numbered = 1;
    v.index = (g_hooks.mission_number ? g_hooks.mission_number() : 0) - 1;
    switch (v.operation) {  // 0x00484840
      case 1: case 2: v.count = 4; break;
      case 3: case 4: v.count = 6; break;
      default: v.count = 0;
    }
    suffix = "m";  // 0x005e8acc
  } else if (v.game == 2) {
    v.count = 1;
    suffix = "ia";  // 0x005e8ac8
  } else if (v.game == 4) {
    v.count = 1;
    suffix = "t";  // 0x005e8ac4
  }
  if (suffix) {
    v.valid = 1;
    v.prefix = std::snprintf(v.name, sizeof v.name, "%s%s", "inout", suffix);
  }
  return v;
}

// Mech3.exe 1.2 @ 0x004c3850
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

// Mech3.exe 1.2 @ 0x004c3930
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
      // FIX (bug): the exe writes past the vector for a child numbered beyond
      // the operation's missions; none is shipped (docs/ai.md section 11).
      if (at >= 0 && static_cast<size_t>(at) < g_inout.areas->size()) inout_read_tree(c, (*g_inout.areas)[size_t(at)]);
    }
  }
  while (root->parent_count() > 0) zcls::node_remove_child(root->parents[0], root);  // 0x004ebb40
  zcls::tree_destroy(root);  // 0x004f9050
}

// Mech3.exe 1.2 @ 0x004c3b80
void inout_select() {
  if (!g_inout.areas) return;
  const int32_t i = inout_variant().index;
  // FIX (bug): unchecked in the exe (0x004c50e0); a campaign mission past the
  // operation's count would point past the vector.
  if (i >= 0 && static_cast<size_t>(i) < g_inout.areas->size()) g_inout.current = &(*g_inout.areas)[size_t(i)];
}

// Mech3.exe 1.2 @ 0x004c3dd0
void inout_mission_end() {
  g_inout.areas.reset();
  // FIX (bug): the exe leaves [0x0060b344] pointing at the freed area; a next
  // mission without one (multiplayer, or a world without `inout`) then pulls
  // the AI's destinations with freed memory (docs/ai.md section 11).
  g_inout.current = nullptr;
}

}  // namespace mech3::ai
