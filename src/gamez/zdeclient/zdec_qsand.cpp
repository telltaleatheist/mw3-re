#include <cstring>

#include "gamez/zclass/cls_node.h"
#include "gamez/zdeclient/zdec_crater.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zgeometry/zgeo_model.h"
#include "gamez/zmodel/gmod_const.h"

#pragma STDC FP_CONTRACT OFF

// zDEClient's quicksand pits (zdec_qsand.cpp; docs/craters.md, section 3).
namespace zdec {
namespace {
const char kFile[] = "C:\\zipper\\GameZMech3\\zDEClient\\zdec_qsand.cpp";
}  // namespace

void quicksand_words(const zwep::QuicksandOrder& o, uint32_t out[11]) {
  auto bits = [](float f) {
    uint32_t b;
    std::memcpy(&b, &f, 4);
    return b;
  };
  out[0] = o.x00, out[1] = static_cast<uint32_t>(o.points);
  out[2] = material_word(o.cycle), out[3] = material_word(o.still), out[4] = o.x10;
  out[5] = bits(o.depth), out[6] = bits(o.radius);
  out[7] = bits(o.at.x), out[8] = bits(o.at.y), out[9] = bits(o.at.z);
  out[10] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(o.owner));
}

// Mech3.exe 1.2 @ 0x005025e0. With texture flags and no material of its
// own, zDEClient's animated quicksand and its still frame.
QuicksandRecord* quicksand_record_new(const zwep::QuicksandOrder* order) {
  QuicksandRecord* r = new QuicksandRecord;
  r->order = *order;
  r->ring.assign(3 * static_cast<size_t>(order->points), 0.0f);
  r->set = zgeo::clip_set_new();
  r->cycle = static_cast<zmod::Material*>(r->order.cycle);
  r->still = static_cast<zmod::Material*>(r->order.still);
  if ((r->order.x00 & 0x1008) && !r->order.cycle) {
    r->cycle = state().quicksand_cycle;
    r->still = state().quicksand_still;
  }
  return r;
}

// Mech3.exe 1.2 @ 0x005020c0
void quicksand_record_free(QuicksandRecord* r) {
  if (!r) return;
  if (r->set) zgeo::clip_set_free(r->set);
  delete r;
}

// Mech3.exe 1.2 @ 0x00502230: 0x004ffad0's placement on this record.
QuicksandRecord* quicksand_build(const zwep::QuicksandOrder* order) {
  QuicksandRecord* r = quicksand_record_new(order);
  if (!place_ring(order->points, r->order.radius, r->order.at, r->ring, r->box, r->cell)) {
    quicksand_record_free(r);
    return nullptr;
  }
  return r;
}

// Mech3.exe 1.2 @ 0x00502660: 0x004fff20 on this record.
int32_t quicksand_clip(QuicksandRecord* r) {
  const int32_t changed = zgeo::clip_cell(r->ring.data(), r->order.points, r->cell, *r->set);
  if (changed < 1) return changed < 0 ? 0 : changed;
  if (r->set->points.empty()) return 0;
  r->ring = r->set->points;
  r->order.points = r->set->point_count;
  return changed;
}

// Mech3.exe 1.2 @ 0x005026c0. Two nodes: the pit (slope quads to the
// points half way in, one depth lower, and the floor fan from the centre
// two depths below the hit) in the still frame, then a lid at the hit's
// height (a fan from its centre to the ring) in the animated material, so
// the ground looks whole and moving. The texture runs 4.5 times over the
// radius; no colours or normals. The lid's UVs are the inner points' (a
// quirk kept: its texture is squeezed toward the middle).
int32_t quicksand_tessellate(QuicksandRecord* r) {
  zcls::Node* world = world_node();
  zcls::Node* node = nullptr;
  zmod::Model* m = zgeo::crater_node(r->set->cells[0], world, &node);
  if (!m || !node) {
    if (m) zgeo::hooks().model_release(m);
    if (node) zcls::node_release(node);
    return -1;
  }
  mark_feature(node, r->type, r->box);
  zwep::QuicksandOrder& o = r->order;
  const int32_t n = o.points;
  const bool textured = o.x00 & 0x1008;
  float radius = 0.0f, inv = 0.0f;
  if (textured) {
    radius = o.radius;
    inv = 4.5f / (radius + radius);
  }
  o.at.y = o.at.y - o.depth;
  const float* ring = r->ring.data();
  std::vector<float> uv(textured ? 4 * static_cast<size_t>(n) + 2 : 0);
  std::vector<float> mid(3 * static_cast<size_t>(n));
  const float cx = o.at.x, cz = o.at.z;
  const float floor_y = o.at.y - o.depth, top_y = o.at.y + o.depth;
  // FIX (bug, fixes.md "Untextured crater"): as the crater's, the exe writes
  // the centre's UV through null when untextured. (The port's buffers free
  // themselves on the failure returns below, where the exe's leak.)
  if (textured) uv[4 * static_cast<size_t>(n)] = inv * radius, uv[4 * static_cast<size_t>(n) + 1] = inv * radius;
  for (int32_t k = 0; k < n; ++k) {
    const float* p = ring + 3 * k;
    float* q = &mid[3 * static_cast<size_t>(k)];
    const float down = o.at.y - p[1];
    q[0] = (cx - p[0]) * 0.5f + p[0];
    q[1] = (floor_y - p[1]) * 0.5f + p[1];
    q[2] = (cz - p[2]) * 0.5f + p[2];
    q[1] = (down * 0.5f + p[1]) - o.depth;
    if (textured) {
      float* a = &uv[2 * static_cast<size_t>(k)];
      float* b = &uv[2 * static_cast<size_t>(n + k)];
      a[0] = ((p[0] - cx) + radius) * inv;
      a[1] = ((p[2] - cz) + radius) * inv;
      b[0] = ((q[0] - cx) + radius) * inv;
      b[1] = ((q[2] - cz) + radius) * inv;
    }
  }
  auto uvp = [&](size_t i) { return &uv[2 * i]; };
  for (int32_t k = 0; k < n; ++k) {
    const int32_t k1 = (k + 1) % n;
    float quad[12];
    std::memcpy(quad, ring + 3 * k, 12);
    std::memcpy(quad + 3, ring + 3 * k1, 12);
    std::memcpy(quad + 6, &mid[3 * static_cast<size_t>(k1)], 12);
    std::memcpy(quad + 9, &mid[3 * static_cast<size_t>(k)], 12);
    if (!textured) {
      zgeo::add_polygon(m, 4, quad, nullptr, nullptr, nullptr, nullptr);
      continue;
    }
    float quv[8];
    std::memcpy(quv, uvp(static_cast<size_t>(k)), 8);
    std::memcpy(quv + 2, uvp(static_cast<size_t>(k1)), 8);
    std::memcpy(quv + 4, uvp(static_cast<size_t>(n + k1)), 8);
    std::memcpy(quv + 6, uvp(static_cast<size_t>(n + k)), 8);
    zgeo::add_polygon(m, 4, quad, r->still, quv, nullptr, nullptr);
  }
  float tri[9] = {cx, floor_y, cz};
  for (int32_t k = 0; k < n; ++k) {
    const int32_t k1 = (k + 1) % n;
    std::memcpy(tri + 3, &mid[3 * static_cast<size_t>(k)], 12);
    std::memcpy(tri + 6, &mid[3 * static_cast<size_t>(k1)], 12);
    if (!textured) {
      zgeo::add_polygon(m, 3, tri, nullptr, nullptr, nullptr, nullptr);
      continue;
    }
    float tuv[6];
    std::memcpy(tuv, uvp(2 * static_cast<size_t>(n)), 8);
    std::memcpy(tuv + 2, uvp(static_cast<size_t>(n + k)), 8);
    std::memcpy(tuv + 4, uvp(static_cast<size_t>(n + k1)), 8);
    zgeo::add_polygon(m, 3, tri, r->still, tuv, nullptr, nullptr);
  }
  zcls::Node* lid = nullptr;
  zmod::Model* lm = zgeo::crater_node(r->set->cells[0], world, &lid);
  if (!lm || !lid) {
    if (lm) zgeo::hooks().model_release(lm);
    if (lid) zcls::node_release(lid);
    return -1;
  }
  mark_feature(lid, r->type, r->box);
  tri[1] = top_y;
  for (int32_t k = 0; k < n; ++k) {
    const int32_t k1 = (k + 1) % n;
    std::memcpy(tri + 3, ring + 3 * k, 12);
    std::memcpy(tri + 6, ring + 3 * k1, 12);
    if (!textured) {
      zgeo::add_polygon(lm, 3, tri, nullptr, nullptr, nullptr, nullptr);
      continue;
    }
    float tuv[6];
    std::memcpy(tuv, uvp(2 * static_cast<size_t>(n)), 8);
    std::memcpy(tuv + 2, uvp(static_cast<size_t>(n + k)), 8);
    std::memcpy(tuv + 4, uvp(static_cast<size_t>(n + k1)), 8);
    zgeo::add_polygon(lm, 3, tri, r->cycle, tuv, nullptr, nullptr);
  }
  return 0;
}

// Mech3.exe 1.2 @ 0x00502110. The crater's steps without the colour cache
// or the dust.
int32_t quicksand_make(const zwep::QuicksandOrder* order) {
  CraterState& c = craters();
  if (c.quicksand_veto && !c.quicksand_veto(order)) return -1;
  if (!state().quicksand_on) return -1;
  const float precision = zmod::vertex_precision();
  zmod::set_vertex_precision(0.005f);
  QuicksandRecord* r = quicksand_build(order);
  if (!r) {
    zerr::report(0x100, kFile, 0x81, "Failed to instance quick sand: Build Failed");
    zmod::set_vertex_precision(precision);
    return -1;
  }
  if (!quicksand_clip(r)) {
    quicksand_record_free(r);
    zerr::report(0x100, kFile, 0x92, "Failed to instance quick sand: Clip Failed");
    zmod::set_vertex_precision(precision);
    return -1;
  }
  if (quicksand_tessellate(r) != 0) {
    quicksand_record_free(r);
    zerr::report(0x100, kFile, 0xa1, "Failed to instance quick sand: Tesselation Failed");
    zmod::set_vertex_precision(precision);
    return -1;
  }
  uint32_t words[11];
  quicksand_words(*order, words);
  history_push(3, words);
  note_touched(*r->set);
  zgeo::commit(*r->set);
  zmod::set_vertex_precision(precision);
  // The pit's nodes keep their Feature copies (FIX, fixes.md "Crater record").
  quicksand_record_free(r);
  return 0;
}

}  // namespace zdec
