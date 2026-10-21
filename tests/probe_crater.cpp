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
// The material whose pointer's low 32 bits are `low` (the history keeps
// pointers as 32-bit words), as an index; -1 for 0.
int32_t material_low(uint32_t low) {
  if (!low) return -1;
  const zmod::MaterialTable& t = zmod::materials();
  for (int32_t i = 0; i < t.count; ++i)
    if (static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&t.at(i))) == low) return i;
  return -2;
}

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
  zdec::state().quicksand_on = 1;
  zdec::state().quicksand_cycle = &zmod::materials().at(material);
  zdec::state().quicksand_still = &zmod::materials().at(material + 1);
  zcls::frames().frame = frame;
  zmod::set_vertex_precision(0.001f);
}

}  // extern "C"

namespace {
void snapshot() {
  const int32_t n = node_count();
  g_before.assign(static_cast<size_t>(n), nullptr);
  g_live.assign(static_cast<size_t>(n), 0);
  for (int32_t i = 0; i < n; ++i) {
    zcls::Node* node = zcls::node_at(i);
    g_live[static_cast<size_t>(i)] = (node->link & zcls::kNodeLive) != 0;
    g_before[static_cast<size_t>(i)] = node->model;
  }
}

// Every cell's crater count, each node whose model changed (1, index, new
// model), each new node (2, index, name, flags, zone, model) and each node
// gone (3, index).
void dump_changes() {
  zcls::World* w = static_cast<zcls::World*>(world_node()->data);
  const uint32_t nx = w->get<uint32_t>(0x88), nz = w->get<uint32_t>(0x8c);
  for (uint32_t j = 0; j < nz; ++j)
    for (uint32_t i = 0; i < nx; ++i) g_out.push_back(w->rows[j][i].raw[0x39]);
  const int32_t n = static_cast<int32_t>(g_before.size());
  const int32_t after = node_count();
  for (int32_t i = 0; i < after; ++i) {
    zcls::Node* node = zcls::node_at(i);
    const bool live = (node->link & zcls::kNodeLive) != 0;
    if (i < n && g_live[static_cast<size_t>(i)]) {
      if (!live) u32(3), u32(static_cast<uint32_t>(i));
      else if (node->model != g_before[static_cast<size_t>(i)]) u32(1), u32(static_cast<uint32_t>(i)), dump_model(node->model);
    } else if (live) {
      u32(2), u32(static_cast<uint32_t>(i)), raw(node->raw, 0x24), u32(node->flags()), u32(node->raw[0x30]);
      dump_model(node->model);
    }
  }
}

int finish(uint8_t* out, int cap) {
  const int size = static_cast<int>(g_out.size());
  if (size <= cap) std::memcpy(out, g_out.data(), g_out.size());
  return size;
}

void history_out(size_t before) {
  if (zdec::craters().history.size() <= before) return;
  const zdec::HistoryEntry& e = zdec::craters().history.back();
  u32(static_cast<uint32_t>(e.type));
  for (int i = 0; i < (e.type == 3 ? 11 : 10); ++i) {
    if (i == 2 || (e.type == 3 && i == 3)) u32(e.words[i]);
    else if (i == (e.type == 3 ? 10 : 9)) u32(0);
    else u32(e.words[i]);
  }
}

std::vector<std::vector<uint32_t>> g_records;
}  // namespace

extern "C" {

// One crater (dust 0/1) or quicksand pit (dust 2): the code, the reports
// and dust, the history entry, then dump_changes.
int crater_run(uint32_t flags, int32_t points, int32_t ground, float depth, float radius, float x, float y, float z,
               int dust, uint8_t* out, int cap) {
  g_out.clear();
  snapshot();
  const size_t history = zdec::craters().history.size();
  if (dust < 2) {
    zwep::CraterOrder o;
    o.x00 = flags, o.points = points;
    o.material = ground >= 0 ? static_cast<zwep::Handle>(&zmod::materials().at(ground)) : nullptr;
    o.depth = depth, o.radius = radius, o.at = {x, y, z};
    u32(static_cast<uint32_t>(zdec::crater_instance(&o, dust)));
  } else {
    zwep::QuicksandOrder q;
    q.x00 = flags, q.points = points, q.depth = depth, q.radius = radius, q.at = {x, y, z};
    u32(static_cast<uint32_t>(zdec::quicksand_make(&q)));
  }
  u32(0xaaaa);
  history_out(history);
  u32(0xbbbb);
  dump_changes();
  return finish(out, cap);
}

// zEntity's save (0x00500b50): each record's name and words (the Dummy's
// reset flag only; a crater's surface as a material index, the shooter 0).
int crater_save(uint8_t* out, int cap) {
  g_out.clear();
  g_records.clear();
  zdec::crater_hooks().write_record = [](void*, const char* name, const void* record, int32_t size) {
    std::vector<uint32_t> r(13);
    std::memcpy(r.data(), record, 0x34);
    g_records.push_back(r);
    raw(name, std::strlen(name)), g_out.push_back(0), u32(static_cast<uint32_t>(size));
    if (r[12]) {
      u32(r[12]);
      return;
    }
    for (int i = 0; i < 12; ++i) {
      if (i == 3 || (r[0] == 3 && i == 4)) u32(r[i]);
      else if (i == (r[0] == 3 ? 11 : 10)) u32(0);
      else u32(r[i]);
    }
    u32(r[12]);
  };
  zdec::history_save(nullptr);
  return finish(out, cap);
}

// zEntity's restore (0x00500c20) of saved record i, then dump_changes.
int crater_restore(int i, uint8_t* out, int cap) {
  g_out.clear();
  snapshot();
  const size_t history = zdec::craters().history.size();
  zdec::history_restore(g_records[static_cast<size_t>(i)].data());
  u32(0xaaaa);
  history_out(history);
  u32(0xbbbb);
  u32(static_cast<uint32_t>(zdec::craters().history.size())), u32(static_cast<uint32_t>(zdec::craters().touched.size()));
  dump_changes();
  return finish(out, cap);
}

}  // extern "C"
