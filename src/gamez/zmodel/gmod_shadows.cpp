#include "gamez/zmodel/gmod_shadows.h"

#include <cstring>
#include <vector>

#pragma STDC FP_CONTRACT OFF

#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/list.h"
#include "gamez/zmath/zmth_matrix.h"

namespace zmod {

namespace {
BakeShadowState g_state;
ShadowQuery g_query = zcls::segment_list;

// The walk's test on a node: active (4) and intersecting (0x10).
bool walked(const zcls::Node* n) { return n && (n->flags() & 4) && (n->flags() & 0x10); }

// An object3d, or each child of a LoD by 0x00548600 (the grid's, the world's and an object's children alike).
void visit(zcls::Node* n) {
  if (!walked(n)) return;
  if (n->cls() == 5) {
    shadow_object(n);
  } else if (n->cls() == 6) {
    for (int32_t i = 0; i < n->child_count(); ++i) bake_shadow_node(n->children[i]);
  }
}

zmth::Mat34* matrix_at(zcls::Object3d* d, int off) { return reinterpret_cast<zmth::Mat34*>(d->raw + off); }
}  // namespace

BakeShadowState& bake_shadow_state() { return g_state; }
void set_shadow_query(ShadowQuery q) { g_query = q ? q : zcls::segment_list; }

// Mech3.exe 1.2 @ 0x00548250 (ECX the model): with +0 clear and polygons, its points (morphed by +0x24 when flag 8
// and a weight and morphs) through the stack's top; each point's segment to the point plus the offset, and a
// crossing darkens that point's colour on every polygon to a fifth (0x005ce4d0).
void shadow_model(Model& m) {
  uint32_t head, flags;
  std::memcpy(&head, m.header, 4);
  std::memcpy(&flags, m.header + 8, 4);
  if (head != 0 || m.count(0x10) == 0) return;
  const int32_t verts = static_cast<int32_t>(m.count(0x14));
  const int32_t n = verts > 0 ? verts : 0;
  float weight;
  std::memcpy(&weight, m.header + 0x24, 4);
  std::vector<zmth::Vec3> pts(n);
  if (n) std::memcpy(pts.data(), m.vertices, sizeof(zmth::Vec3) * n);
  // The exe morphs into its shared buffer ([0x0074a288]) and transforms from there; past the morphs that buffer
  // holds what an earlier model left (the reader keeps the morphs within the points): the points here.
  if ((flags & 8) && weight != 0.0f && m.count(0x1c) != 0)
    zmth::morph(pts.data(), reinterpret_cast<const zmth::Vec3*>(m.vertices),
                reinterpret_cast<const zmth::Vec3*>(m.morphs), m.count(0x1c), weight);
  zmth::transform_points(pts.data(), static_cast<uint32_t>(n));
  // FIX (A): the exe keeps the answers in 1,024 words on its stack, overrun by a model of more points.
  std::vector<uint8_t> dark(n);
  for (int32_t i = 0; i < n; ++i) {
    const float from[3] = {pts[i].x, pts[i].y, pts[i].z};
    const float to[3] = {pts[i].x + g_state.offset[0], pts[i].y + g_state.offset[1], pts[i].z + g_state.offset[2]};
    zcls::LineHits hits;
    hits.count = 0;
    dark[i] = g_query(g_state.world, from, to, &hits) == 0 && hits.count != 0;
  }
  for (uint32_t p = 0; p < m.count(0x10); ++p) {
    Polygon& q = m.polygons[p];
    for (uint32_t k = 0; k < (q.flags & 0xff); ++k) {
      if (!dark[q.vertex_indices[k]]) continue;
      for (int c = 0; c < 3; ++c) q.colors[3 * k + c] = q.colors[3 * k + c] * 0.2f;
    }
  }
}

// Mech3.exe 1.2 @ 0x005484c0 (ECX node): unless its data's flag 8, its matrix onto the stack (with node flag
// 0x80000 and no data flag 0x20, its world matrix +0x60 as it is; else +0x60 made from the top and its local
// +0x30, flag 0x20 cleared after unless flag 1); its model, then its children; the matrix off.
void shadow_object(zcls::Node* node) {
  auto* d = static_cast<zcls::Object3d*>(node->data);
  if (!d) return;
  bool pushed = false;
  if (!(d->get<uint32_t>(0) & 8)) {
    pushed = true;
    if (!(node->flags() & 0x80000)) {
      zmth::push_copy(matrix_at(d, 0x60));
      zmth::multiply(*matrix_at(d, 0x30), 1);
    } else if (!(d->get<uint32_t>(0) & 0x20)) {
      zmth::push(matrix_at(d, 0x60));
    } else {
      zmth::push_copy(matrix_at(d, 0x60));
      zmth::multiply(*matrix_at(d, 0x30), 1);
      if (!(d->get<uint32_t>(0) & 1)) d->put<uint32_t>(0, d->get<uint32_t>(0) & ~0x20u);
    }
  }
  if (node->model) shadow_model(*node->model);
  for (int32_t i = 0; i < node->child_count(); ++i) visit(node->children[i]);
  if (pushed) zmth::pop();
}

// Mech3.exe 1.2 @ 0x00548600 (ECX node): the walk's test, then an object3d, or a LoD's children again.
void bake_shadow_node(zcls::Node* node) { visit(node); }

// Mech3.exe 1.2 @ 0x005485b0 (ECX node): each walked child, an object3d or a LoD's children again. Nothing in the
// exe calls it (0x00548600 does the LoDs).
void shadow_lod_children(zcls::Node* node) {
  for (int32_t i = 0; i < node->child_count(); ++i) {
    zcls::Node* c = node->children[i];
    if (!walked(c)) continue;
    if (c->cls() == 5) shadow_object(c);
    else if (c->cls() == 6) shadow_lod_children(c);
  }
}

// Mech3.exe 1.2 @ 0x00548650 (ECX world): the changed nodes updated, an identity on the stack, first crossings only
// (0x004ed9c0); every cell's nodes, column by column, then the world's children.
void shadow_world(zcls::Node* world) {
  if (zcls::list_head(zcls::kChanged)) zcls::update_changed();
  auto* w = static_cast<zcls::World*>(world->data);
  zmth::Mat34 base;
  zmth::push(&base);
  zmth::load_identity();
  zcls::segment_options().first_hit = 1;
  for (int32_t x = 0; x < w->get<int32_t>(0x88); ++x)
    for (int32_t z = 0; z < w->get<int32_t>(0x8c); ++z) {
      zcls::Partition& p = w->rows[z][x];
      for (int16_t i = 0; i < p.get<int16_t>(0x3a); ++i) visit(p.nodes[i]);
    }
  for (int32_t i = 0; i < world->child_count(); ++i) visit(world->children[i]);
  zcls::segment_options().first_hit = 0;
  zmth::pop();
}

// Mech3.exe 1.2 @ 0x005487d0 (MakeShadows): the light "sunlight" on list 9 (its direction +0x84 times -1000 the
// segments' offset) and "world1" on list 13. The exe returns whatever EAX held; 0 here.
int make_shadows() {
  zcls::Node* light = zcls::list_find_by_name(zcls::kLights, "sunlight");
  if (!light || light->cls() != 9) return 0;
  const auto* l = static_cast<const zcls::Light*>(light->data);
  for (int i = 0; i < 3; ++i) g_state.offset[i] = l->get<float>(0x84 + 4 * i) * -1000.0f;  // 0x005ce4d4
  g_state.world = zcls::list_find_by_name(zcls::kWorlds, "world1");
  if (g_state.world) shadow_world(g_state.world);
  return 0;
}

}  // namespace zmod
