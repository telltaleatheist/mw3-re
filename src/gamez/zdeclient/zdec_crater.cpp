#include "gamez/zdeclient/zdec_crater.h"

#include <cmath>
#include <cstring>

#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/cls_world.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zgeometry/zgeo_model.h"
#include "gamez/zmodel/gmod_const.h"

#pragma STDC FP_CONTRACT OFF

namespace zdec {
namespace {
const char kFile[] = "C:\\zipper\\GameZMech3\\zDEClient\\zdec_crater.cpp";
const char kFeature[] = "ZDEC_FEATURE";  // 0x005eb9f0, the crater nodes' name
// 0x005eb910: four up normals, the crater polygons' (one a point).
const float kUp[12] = {0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0};
constexpr float kGrey = 0.8f;  // 0x3f4ccccd, the colour where no cut ground is near
CraterState g_craters;
CraterHooks g_hooks;

zcls::World* world_data() { return static_cast<zcls::World*>(state().world); }
}  // namespace

CraterState& craters() { return g_craters; }
CraterHooks& crater_hooks() { return g_hooks; }

// Mech3.exe 1.2 @ 0x005020b0
zcls::Node* world_node() { return state().world_node; }

// Mech3.exe 1.2 @ 0x00502090
zcls::Partition* cell_at(int32_t x, int32_t z) {
  zcls::World* w = world_data();
  if (!w) return nullptr;
  return w->rows[z] + x;
}

// Mech3.exe 1.2 @ 0x004ffe80. The texture entry is the first after the
// defaults whose ground is the order's surface, else the defaults.
CraterRecord* record_new(const zwep::CraterOrder* order) {
  CraterRecord* r = new CraterRecord;
  r->order = *order;
  r->ring.assign(3 * static_cast<size_t>(order->points), 0.0f);
  r->set = zgeo::clip_set_new();
  if (r->order.x00 & 0x1008) {
    State& s = state();
    r->texture = s.textures;
    for (int32_t i = 1; i < s.texture_count; ++i) {
      if (s.textures[i].ground == static_cast<zmod::Material*>(r->order.material)) {
        r->texture = s.textures + i;
        break;
      }
    }
  }
  return r;
}

// Mech3.exe 1.2 @ 0x004ff8f0
void record_free(CraterRecord* r) {
  if (!r) return;
  if (r->set) zgeo::clip_set_free(r->set);
  delete r;
}

// Mech3.exe 1.2 @ 0x004ffad0. The centre is pushed back inside its cell
// (by the radius and 1 m), then the ring is laid out round it; a crater
// within 5 m (by box) of another crater node in the cell is refused.
CraterRecord* crater_build(const zwep::CraterOrder* order) {
  CraterRecord* r = record_new(order);
  zcls::Node* world = world_node();
  zcls::World* w = static_cast<zcls::World*>(world->data);
  int32_t cx, cz, x, z, inside;
  if (!w || zcls::world_cell_wrapped(world, &cx, order->at.x, order->at.z, &cz, &x, &z, &inside) != 0 || !inside) {
    record_free(r);
    return nullptr;
  }
  zcls::Partition* cell = cell_at(x, z);
  r->cell = cell;
  if (!cell || cell->raw[0x39] >= w->raw[0x4c]) {
    record_free(r);
    return nullptr;
  }
  zwep::CraterOrder& o = r->order;
  const float dx = o.at.x - cell->get<float>(8);
  const float dz = o.at.z - cell->get<float>(0xc);
  const float size_x = w->get<float>(0x64), low_z = w->get<float>(0x68);
  const float right = dx + o.radius;
  if (right > size_x) o.at.x = o.at.x - ((right - size_x) - -1.0f);
  else if (!(dx - o.radius >= 0.0f)) o.at.x = ((o.radius - dx) + o.at.x) - -1.0f;
  const float bottom = dz - o.radius;
  if (!(bottom >= low_z)) {
    o.at.z = ((low_z - bottom) + o.at.z) - -1.0f;
  } else if (0.0f < dz + o.radius) {
    o.at.z = o.at.z - ((dz + o.radius) - -1.0f);
  }
  const int32_t n = order->points;
  const float step = static_cast<float>(6.28318530718 / static_cast<double>(n));
  float angle = 0.0f;
  for (int32_t i = 0; i < n; ++i) {
    float* p = &r->ring[3 * static_cast<size_t>(i)];
    p[0] = static_cast<float>(std::sin(static_cast<double>(angle)));
    p[2] = static_cast<float>(std::cos(static_cast<double>(angle)));
    p[0] = p[0] * o.radius;
    p[2] = o.radius * p[2];
    p[0] = p[0] + o.at.x;
    p[1] = o.at.y;
    p[2] = o.at.z + p[2];
    angle = step + angle;
  }
  const float* p = r->ring.data();
  r->box[0] = r->box[2] = p[0];
  r->box[1] = r->box[3] = p[2];
  for (int32_t i = 1; i < n; ++i) {
    const float* q = p + 3 * i;
    if (r->box[0] > q[0]) r->box[0] = q[0];
    if (r->box[2] < q[0]) r->box[2] = q[0];
    if (r->box[1] > q[2]) r->box[1] = q[2];
    if (r->box[3] < q[2]) r->box[3] = q[2];
  }
  if (cell->raw[0x39] != 0) {
    const int16_t count = cell->get<int16_t>(0x3a);
    for (int16_t i = 0; i < count; ++i) {
      zcls::Node* node = cell->nodes[i];
      if (std::strcmp(node->name(), kFeature) != 0 || !node->owner) continue;
      const Feature& f = *static_cast<const Feature*>(node->owner);
      if (f.type != 1 && f.type != 3) continue;
      if (r->box[0] < f.box[2] - -5.0f && f.box[0] - 5.0f < r->box[2] && r->box[1] < f.box[3] - -5.0f &&
          f.box[1] - 5.0f < r->box[3]) {
        record_free(r);
        return nullptr;
      }
    }
  }
  return r;
}

// Mech3.exe 1.2 @ 0x004fff20. The ring becomes the outline as the clip
// left it (snapped, with the cut points).
int32_t crater_clip(CraterRecord* r) {
  const int32_t changed = zgeo::clip_cell(r->ring.data(), r->order.points, r->cell, *r->set);
  if (changed < 1) return changed < 0 ? 0 : changed;
  if (r->set->points.empty()) return 0;
  r->ring = r->set->points;
  r->order.points = r->set->point_count;
  return changed;
}

// Mech3.exe 1.2 @ 0x004fff80. A new node and model in the cell's place:
// for each ring point, a slope quad from it and the next to the points
// half way to the floor centre (a depth lower), and a floor triangle fan
// from the centre (two depths below the hit). Textured craters map the
// crater material over the radius and take the cut ground's colours.
int32_t crater_tessellate(CraterRecord* r) {
  zcls::Node* node = nullptr;
  zmod::Model* m = zgeo::crater_node(r->set->cells[0], world_node(), &node);
  if (node) {
    zcls::node_set_name(node, kFeature);
    g_craters.features.push_back(Feature{r->type, {r->box[0], r->box[1], r->box[2], r->box[3]}});
    node->owner = &g_craters.features.back();
  }
  if (!m) return -1;
  zwep::CraterOrder& o = r->order;
  const int32_t n = o.points;
  const bool textured = o.x00 & 0x1008;
  float radius = 0.0f, inv = 0.0f;
  zmod::Material* material = nullptr;
  if (textured) {
    radius = o.radius;
    material = r->texture->crater;
    inv = 1.0f / (radius + radius);
  }
  o.at.y = o.at.y - o.depth;
  const float* ring = r->ring.data();
  std::vector<float> uv(textured ? 4 * static_cast<size_t>(n) + 2 : 0);
  std::vector<float> mid(3 * static_cast<size_t>(n));
  const float cx = o.at.x, cz = o.at.z, cy = o.at.y - o.depth;
  if (textured) uv[4 * static_cast<size_t>(n)] = inv * radius, uv[4 * static_cast<size_t>(n) + 1] = inv * radius;
  // FIX (bug): the exe writes the centre's UV through a null pointer for an
  // untextured crater (none in the shipped worlds).
  for (int32_t k = 0; k < n; ++k) {
    const float* p = ring + 3 * k;
    float* q = &mid[3 * static_cast<size_t>(k)];
    const float down = o.at.y - p[1];
    q[0] = (cx - p[0]) * 0.5f + p[0];
    q[1] = (cy - p[1]) * 0.5f + p[1];
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
  float color[12];
  auto color_of = [&](int slot, const float* at) {
    if (!zgeo::colors_find(at, color + 3 * slot)) color[3 * slot] = color[3 * slot + 1] = color[3 * slot + 2] = kGrey;
  };
  for (int32_t k = 0; k < n; ++k) {
    const int32_t k1 = (k + 1) % n;
    float quad[12];
    std::memcpy(quad, ring + 3 * k, 12);
    std::memcpy(quad + 3, ring + 3 * k1, 12);
    std::memcpy(quad + 6, &mid[3 * static_cast<size_t>(k1)], 12);
    std::memcpy(quad + 9, &mid[3 * static_cast<size_t>(k)], 12);
    if (!textured) {
      zgeo::add_polygon(m, 4, quad, nullptr, nullptr, kUp, nullptr);
      continue;
    }
    float quv[8];
    std::memcpy(quv, &uv[2 * static_cast<size_t>(k)], 8);
    std::memcpy(quv + 2, &uv[2 * static_cast<size_t>(k1)], 8);
    std::memcpy(quv + 4, &uv[2 * static_cast<size_t>(n + k1)], 8);
    std::memcpy(quv + 6, &uv[2 * static_cast<size_t>(n + k)], 8);
    for (int i = 0; i < 4; ++i) color_of(i, quad + 3 * i);
    zgeo::add_polygon(m, 4, quad, material, quv, kUp, color);
  }
  float tri[9] = {cx, cy, cz};
  color_of(0, tri);
  for (int32_t k = 0; k < n; ++k) {
    const int32_t k1 = (k + 1) % n;
    std::memcpy(tri + 3, &mid[3 * static_cast<size_t>(k)], 12);
    std::memcpy(tri + 6, &mid[3 * static_cast<size_t>(k1)], 12);
    if (!textured) {
      zgeo::add_polygon(m, 3, tri, nullptr, nullptr, kUp, nullptr);
      continue;
    }
    float tuv[6];
    std::memcpy(tuv, &uv[4 * static_cast<size_t>(n)], 8);
    std::memcpy(tuv + 2, &uv[2 * static_cast<size_t>(n + k)], 8);
    std::memcpy(tuv + 4, &uv[2 * static_cast<size_t>(n + k1)], 8);
    color_of(1, tri + 3);
    color_of(2, tri + 6);
    zgeo::add_polygon(m, 3, tri, material, tuv, kUp, color);
  }
  zgeo::hooks().node_set_model(node, m);
  return 0;
}

void order_words(const zwep::CraterOrder& o, uint32_t out[10]) {
  auto bits = [](float f) {
    uint32_t b;
    std::memcpy(&b, &f, 4);
    return b;
  };
  out[0] = o.x00, out[1] = static_cast<uint32_t>(o.points);
  out[2] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(o.material));
  out[3] = o.x0c, out[4] = bits(o.depth), out[5] = bits(o.radius);
  out[6] = bits(o.at.x), out[7] = bits(o.at.y), out[8] = bits(o.at.z);
  out[9] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(o.owner));
}

// Mech3.exe 1.2 @ 0x00500850. A crater's entry leaves its 12th word as
// the exe's stack had it; here 0.
int32_t history_push(int32_t type, const uint32_t* words) {
  int count;
  if (type == 1) count = 10;
  else if (type == 3) count = 11;
  else return 0;
  HistoryEntry e;
  e.type = type;
  std::memcpy(e.words, words, 4 * static_cast<size_t>(count));
  g_craters.history.push_back(e);
  return 0;
}

// Mech3.exe 1.2 @ 0x00500600
void note_touched(const zgeo::ClipSet& set) {
  for (const zgeo::CellClip& c : set.cells)
    for (const zgeo::NodeModel& nm : c.nodes) g_craters.touched.insert(nm.node);
}

// Mech3.exe 1.2 @ 0x004ff940. The vertex merge distance is 0.005 while the
// crater is made.
//
// FIX (bug): the exe frees the record at the end, though the crater node
// keeps pointing at it (+0x40) for the next crater's overlap test, which
// then reads freed memory (whatever the heap put there). The port keeps
// each crater node's type and box (craters().features).
int32_t crater_instance(const zwep::CraterOrder* order, int dust) {
  const float precision = zmod::vertex_precision();
  zmod::set_vertex_precision(0.005f);
  CraterRecord* r = crater_build(order);
  zgeo::colors_begin(order->at.x, order->at.y, order->at.z, order->radius);
  if (!r) {
    zerr::report(0x100, kFile, 0x93, "Failed to instance crater: Build Failed");
    zmod::set_vertex_precision(precision);
    return -1;
  }
  if (!crater_clip(r)) {
    record_free(r);
    zerr::report(0x100, kFile, 0xcb, "Failed to instance crater: Clip Failed");
    zmod::set_vertex_precision(precision);
    return -1;
  }
  if (crater_tessellate(r) != 0) {
    record_free(r);
    zerr::report(0x100, kFile, 0xda, "Failed to instance crater: Tesselation Failed");
    zmod::set_vertex_precision(precision);
    return -1;
  }
  uint32_t words[10];
  order_words(*order, words);
  history_push(1, words);
  note_touched(*r->set);
  zgeo::commit(*r->set);
  zmod::set_vertex_precision(precision);
  // An untextured crater has no entry; the exe reads through null here.
  if (dust && r->texture && r->texture->anim && g_hooks.play_anim) {
    const float at[3] = {r->order.at.x, r->order.at.y, r->order.at.z};
    g_hooks.play_anim(r->texture->anim, at);
  }
  record_free(r);
  return 0;
}

// Mech3.exe 1.2 @ 0x004ffaa0
int32_t crater_make(const zwep::CraterOrder* order) {
  if (g_craters.veto && !g_craters.veto(order)) return -1;
  return crater_instance(order, 1);
}

}  // namespace zdec
