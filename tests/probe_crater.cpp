// Craters end to end (part 5b) for tests/differential/test_crater.py, on a
// world the port's loaders read (sw_load in probe_scenewalk.cpp): zDEClient
// pointed at it, craters made one by one, and after each what changed.
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zclass/camera.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/list.h"
#include "gamez/zdeclient/zdec_crater.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zgeometry/zgeo_model.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zutil/zfix.h"

namespace {
std::vector<uint8_t> g_out;
std::vector<zmod::Model*> g_before;  // each node's model before a crater
std::vector<uint8_t> g_live;
zdec::CraterTexture g_texture;
zeff::AnimDef* const kDust = reinterpret_cast<zeff::AnimDef*>(0x1234);

void u32(uint32_t v) {
  for (int i = 0; i < 4; ++i) g_out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
void raw(const void* p, size_t n) {
  const uint8_t* b = static_cast<const uint8_t*>(p);
  g_out.insert(g_out.end(), b, b + n);
}
void sink(int level, const char*, int line, const char*) { u32(0x70), u32(static_cast<uint32_t>(level)), u32(static_cast<uint32_t>(line)); }

int32_t material_index(const zmod::Material* m) {
  if (!m) return -1;
  const int32_t i = zmod::materials().index_of(m);
  return i >= 0 ? i : -2;
}
int32_t model_index(const zmod::Model* m) {
  if (!m) return -1;
  const auto& s = zmod::models().slots;
  return m >= s.data() && m < s.data() + s.size() ? static_cast<int32_t>(m - s.data()) : -2;
}
zcls::Node* world_node() { return zcls::camera_world(zcls::list_head(zcls::kCameras)->node); }

int32_t node_count() {
  const zcls::NodePool& p = zcls::nodes();
  return static_cast<int32_t>(p.blocks.size()) * p.block_size;
}

void dump_model(const zmod::Model* m) {
  u32(static_cast<uint32_t>(model_index(m)));
  if (!m) return;
  raw(m->header, 0x34);
  raw(m->header + 0x48, 0x14);
  const uint32_t np = m->count(0x10), nv = m->count(0x14), nn = m->count(0x18);
  raw(m->vertices, 12 * nv);
  raw(m->normals, 12 * nn);
  for (uint32_t i = 0; i < np; ++i) {
    const zmod::Polygon& q = m->polygons[i];
    const uint32_t k = q.flags & 0xff;
    u32(q.flags), u32(static_cast<uint32_t>(q.priority));
    raw(q.vertex_indices, 4 * k);
    if (q.flags & 0x200) raw(q.normal_indices, 4 * k);
    u32(q.uvs != nullptr);
    if (q.uvs) raw(q.uvs, 8 * k);
    raw(q.colors, 12 * k);
    u32(static_cast<uint32_t>(material_index(q.material))), u32(q.zone_set);
  }
}
}  // namespace

extern "C" {

// zDEClient on the loaded world: one texture entry (the crater material at
// `material`, a stand-in dust animation), the frame counter, fresh state.
void crater_setup(int32_t material, uint32_t frame) {
  zfix::options().bugs = false;
  zerr::set_sink(sink);
  zgeo::hooks() = zgeo::Hooks{};
  zgeo::hooks().world_node = zdec::world_node;
  zgeo::colors() = zgeo::ColorCache{};
  zdec::set_world(world_node());
  g_texture = zdec::CraterTexture{nullptr, &zmod::materials().at(material), kDust};
  zdec::state().textures = &g_texture;
  zdec::state().texture_count = 1;
  zdec::craters() = zdec::CraterState{};
  zdec::crater_hooks().play_anim = [](zeff::AnimDef* a, const float at[3]) {
    u32(0x71), u32(a == kDust), raw(at, 12);
  };
  zcls::frames().frame = frame;
  zmod::set_vertex_precision(0.001f);
}

// One crater: the code, the reports and dust, the history entry, every
// cell's crater count, each node whose model changed (index, new model)
// and each new node (index, name, flags, zone, model).
int crater_run(uint32_t flags, int32_t points, int32_t ground, float depth, float radius, float x, float y, float z,
               int dust, uint8_t* out, int cap) {
  g_out.clear();
  const int32_t n = node_count();
  g_before.assign(static_cast<size_t>(n), nullptr);
  g_live.assign(static_cast<size_t>(n), 0);
  for (int32_t i = 0; i < n; ++i) {
    zcls::Node* node = zcls::node_at(i);
    g_live[static_cast<size_t>(i)] = (node->link & zcls::kNodeLive) != 0;
    g_before[static_cast<size_t>(i)] = node->model;
  }
  zwep::CraterOrder o;
  o.x00 = flags, o.points = points, o.material = ground >= 0 ? static_cast<zwep::Handle>(&zmod::materials().at(ground)) : nullptr;
  o.depth = depth, o.radius = radius, o.at = {x, y, z};
  const size_t history = zdec::craters().history.size();
  u32(static_cast<uint32_t>(zdec::crater_instance(&o, dust)));
  u32(0xaaaa);
  if (zdec::craters().history.size() > history) {
    const zdec::HistoryEntry& e = zdec::craters().history.back();
    u32(static_cast<uint32_t>(e.type));
    for (int i = 0; i < 10; ++i) {
      if (i == 2) u32(static_cast<uint32_t>(material_index(static_cast<zmod::Material*>(o.material))));
      else if (i == 9) u32(0);
      else u32(e.words[i]);
    }
  }
  u32(0xbbbb);
  zcls::World* w = static_cast<zcls::World*>(world_node()->data);
  const uint32_t nx = w->get<uint32_t>(0x88), nz = w->get<uint32_t>(0x8c);
  for (uint32_t j = 0; j < nz; ++j)
    for (uint32_t i = 0; i < nx; ++i) g_out.push_back(w->rows[j][i].raw[0x39]);
  const int32_t after = node_count();
  for (int32_t i = 0; i < after; ++i) {
    zcls::Node* node = zcls::node_at(i);
    const bool live = (node->link & zcls::kNodeLive) != 0;
    if (i < n && g_live[static_cast<size_t>(i)]) {
      if (live && node->model != g_before[static_cast<size_t>(i)]) u32(1), u32(static_cast<uint32_t>(i)), dump_model(node->model);
    } else if (live) {
      u32(2), u32(static_cast<uint32_t>(i)), raw(node->raw, 0x24), u32(node->flags()), u32(node->raw[0x30]);
      dump_model(node->model);
    }
  }
  const int size = static_cast<int>(g_out.size());
  if (size <= cap) std::memcpy(out, g_out.data(), g_out.size());
  return size;
}

}  // extern "C"
