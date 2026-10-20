// Probes for test_action_lists.py (docs/action-lists.md): a world read by the
// port's loaders, then zClass's action lists (0x4f7d80) with scripted actions
// and the frame update (0x4f7f70) over moved nodes, sequences and animates.
// Self-contained: no shared helpers.
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zclass/camera.h"
#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/cls_world.h"
#include "gamez/zclass/cls_zbd.h"
#include "gamez/zclass/list.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_fog.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zrender/zrndr_fog.h"
#include "gamez/zutil/ztimer.h"
#include "gamez/zvideo/zvid_format.h"

namespace {
using zcls::Node;
std::vector<uint8_t> g_out;    // the dump
std::vector<int32_t> g_trace;  // actions run, zerr reports
void put(const void* p, size_t n) {
  const auto* b = static_cast<const uint8_t*>(p);
  g_out.insert(g_out.end(), b, b + n);
}
void u32(uint32_t v) { put(&v, 4); }

int32_t index_of(const Node* n) {
  if (!n) return -1;
  const zcls::NodePool& p = zcls::nodes();
  for (size_t b = 0; b < p.blocks.size(); ++b) {
    const Node* base = p.blocks[b].get();
    if (n >= base && n < base + p.block_size) return static_cast<int32_t>(b * p.block_size + (n - base));
  }
  return -2;
}
Node* node(int32_t i) { return i < 0 ? nullptr : zcls::node_at(i); }

void sink(int level, const char*, int line, const char*) {
  g_trace.push_back(100), g_trace.push_back(level), g_trace.push_back(line);
}

// The scripted actions: each logs (1, k, its node), then runs its ops.
constexpr int kActions = 8;
struct Op {
  int32_t op, a, b, c, d, e;
};
std::vector<Op> g_script[kActions];
zcls::NodeAction action_fn(int32_t k);
float f(int32_t bits) {
  float v;
  std::memcpy(&v, &bits, 4);
  return v;
}
void run_ops(int k) {
  for (const Op& o : g_script[k]) {
    switch (o.op) {
      case 1: zcls::node_set_action(node(o.a), action_fn(o.b)); break;
      case 2: zcls::node_set_action_back(node(o.a), action_fn(o.b)); break;
      case 3: zcls::node_set_priority(node(o.a), o.b); break;
      case 4: zcls::node_set_active(node(o.a), o.b); break;
      case 5: zcls::object3d_set_translation(node(o.a), f(o.b), f(o.c), f(o.d)); break;
      case 6: zcls::list_remove(o.a, node(o.b)); break;
      case 7: zcls::object3d_set_rotation(node(o.a), f(o.b), f(o.c), f(o.d)); break;
      case 8: {  // its world position: brings a changed chain up to date mid-run
        float p[3];
        zcls::node_world_position(node(o.a), p);
        break;
      }
      case 9: {  // what the node's flags say now
        Node* n = node(o.a);
        g_trace.push_back(2), g_trace.push_back(index_of(n));
        g_trace.push_back(static_cast<int32_t>(n->flags())), g_trace.push_back(n->get<int32_t>(0x2c));
        break;
      }
    }
  }
}
template <int K>
void act(Node* n) {
  g_trace.push_back(1), g_trace.push_back(K), g_trace.push_back(index_of(n));
  run_ops(K);
}
zcls::NodeAction action_fn(int32_t k) {
  static const zcls::NodeAction fns[kActions] = {act<0>, act<1>, act<2>, act<3>, act<4>, act<5>, act<6>, act<7>};
  return k >= 0 && k < kActions ? fns[k] : nullptr;
}

std::vector<std::vector<float>> g_keys;  // the animates' keys
}  // namespace

extern "C" {

int al_load(const char* path) {
  zimg::textures().count = 0;
  zimg::textures().group = 3;
  zmod::materials() = zmod::MaterialTable{};
  zmod::models() = zmod::ModelPool{};
  zvid::set_pixel_format(5, 6, 5);
  zcls::nodes() = zcls::NodePool{};
  zcls::nodes().free_head = -1;
  zcls::lists() = zcls::Lists{};
  zcls::frames() = zcls::Frames{};
  zmod::fog() = zmod::Fog{};
  zrndr::fog_color() = zrndr::FogColor{};
  zerr::set_sink(sink);
  zmth::reset_stack();
  zcls::update_hooks() = zcls::UpdateHooks{};
  zcls::set_area_update(nullptr);
  zcls::node_update_observer() = [](Node* n) { g_trace.push_back(3), g_trace.push_back(index_of(n)); };
  for (auto& s : g_script) s.clear();
  g_keys.clear();
  g_trace.clear();
  return zcls::read_zbd(path);
}

void al_globals(uint32_t frame, float dt) {
  zcls::frames().frame = frame;
  ztimer::state().dt = dt;
}

void al_frame_update() { zcls::frame_update(); }
void al_run_lists() { zcls::run_action_lists(); }

// Setters on one node: 1 translation, 2 rotation, 3 scale, 4 camera position,
// 5 camera rotation, 6 add translation.
int al_set(int32_t i, int32_t what, float x, float y, float z) {
  Node* n = node(i);
  switch (what) {
    case 1: return zcls::object3d_set_translation(n, x, y, z);
    case 2: return zcls::object3d_set_rotation(n, x, y, z);
    case 3: return zcls::object3d_set_scale(n, x, y, z);
    case 4: return zcls::camera_set_position(n, x, y, z);
    case 5: return zcls::camera_set_rotation(n, x, y, z);
    case 6: return zcls::object3d_add_translation(n, x, y, z);
  }
  return -1;
}

// object3d_set_matrix (0x4fc300): its own matrix from then on (USE_MATRIX).
int al_set_matrix(int32_t i, const float* m) { return zcls::object3d_set_matrix(node(i), m); }

int al_object3d() { return index_of(zcls::object3d_create()); }
int al_set_action(int32_t i, int32_t k, int back) {
  return back ? zcls::node_set_action_back(node(i), action_fn(k)) : zcls::node_set_action(node(i), action_fn(k));
}
int al_set_priority(int32_t i, int32_t p) { return zcls::node_set_priority(node(i), p); }
void al_set_flags(int32_t i, uint32_t flags) { node(i)->set_flags(flags); }
void al_or(int32_t i, int32_t off, uint32_t bits) { node(i)->put<uint32_t>(off, node(i)->get<uint32_t>(off) | bits); }
void al_script(int32_t k, const int32_t* ops, int32_t n) {
  g_script[k].clear();
  for (int32_t j = 0; j < n; ++j) {
    const int32_t* o = ops + 6 * j;
    g_script[k].push_back({o[0], o[1], o[2], o[3], o[4], o[5]});
  }
}

// A sequence (class 7) on list 11: the 0x20-byte header, then `count`
// entries (node index, time).
int al_seq(const uint8_t* header, const int32_t* nodes, const float* times, int32_t count) {
  Node* n = zcls::node_alloc();
  n->put<int32_t>(0x34, 7);
  auto* s = new zcls::Seq{};
  std::memcpy(s->raw, header, 0x20);
  s->entries = new zcls::Seq::Entry[count > 0 ? count : 1]{};
  for (int32_t i = 0; i < count; ++i) s->entries[i] = {node(nodes[i]), times[i]};
  n->data = s;
  zcls::push_front(11, n);
  return index_of(n);
}

// An animate (class 8) on list 12: its 0xd4 bytes, its keys.
int al_animate(const uint8_t* raw, const float* keys, int32_t nkeys, uint32_t flags) {
  Node* n = zcls::node_alloc();
  n->put<int32_t>(0x34, 8);
  n->set_flags(flags);
  auto* a = new zcls::Animate{};
  std::memcpy(a->raw, raw, 0xd4);
  g_keys.emplace_back(keys, keys + 9 * nkeys);
  a->keys = g_keys.back().data();
  n->data = a;
  zcls::push_front(12, n);
  return index_of(n);
}

// The state both sides compare: every node, its class data and its
// world's cells, the model boxes, the lists.
int al_dump(uint8_t* out, int32_t cap) {
  g_out.clear();
  // Every node is on list 6 (the file's and those made at run time alike).
  for (zcls::ListItem* item = zcls::lists().list[zcls::kAllNodes].head; item; item = item->next) {
    Node* n = item->node;
    if (!n) continue;
    u32(static_cast<uint32_t>(index_of(n))), u32(n->flags()), u32(n->get<uint32_t>(0x2c)), u32(n->get<uint32_t>(0x34)), u32(n->get<uint32_t>(0x44));
    u32(n->get<uint32_t>(0x4c)), u32(n->get<uint32_t>(0x50));
    u32(n->action ? 1 : 0);
    u32(static_cast<uint32_t>(n->parent_count()));
    for (int32_t k = 0; k < n->parent_count(); ++k) u32(static_cast<uint32_t>(index_of(n->parents[k])));
    u32(static_cast<uint32_t>(n->child_count()));
    for (int32_t k = 0; k < n->child_count(); ++k) u32(static_cast<uint32_t>(index_of(n->children[k])));
    put(n->raw + 0x64, 0x58);  // +0x64..+0xbb: the boxes
    u32(n->get<uint32_t>(0xc0)), u32(n->get<uint32_t>(0xc4));
    u32(n->changed ? static_cast<uint32_t>(n->changed->removable) : 0xffffffffu);
    if (n->model) put(n->model->header + 0x48, 0x10);
    auto* d = static_cast<uint8_t*>(n->data);
    if (!d) continue;
    switch (n->cls()) {
      case 1: put(d + 0x10, 0x20), put(d + 0x80, 0x30); break;
      case 5: put(d, 0x90); break;
      case 7: {
        auto* s = static_cast<zcls::Seq*>(n->data);
        put(s->raw, 0x20);
        break;
      }
      case 8: put(d, 0x6c), put(d + 0x70, 0xd4 - 0x70); break;
      case 2: {
        auto* w = static_cast<zcls::World*>(n->data);
        put(w->raw, 0xc), put(w->raw + 0x10, 0x80);
        const int32_t nx = w->get<int32_t>(0x88), nz = w->get<int32_t>(0x8c);
        for (int32_t z = 0; z < nz; ++z)
          for (int32_t x = 0; x < nx; ++x) {
            zcls::Partition& c = w->rows[z][x];
            put(c.raw, 0x3c);
            for (int32_t k = 0; k < c.get<int16_t>(0x3a); ++k) u32(static_cast<uint32_t>(index_of(c.nodes[k])));
          }
        break;
      }
      default: break;
    }
  }
  u32(0xffffffffu);
  for (int l = 0; l < 16; ++l) {
    for (zcls::ListItem* it = zcls::lists().list[l].head; it; it = it->next)
      u32(static_cast<uint32_t>(index_of(it->node))), u32(static_cast<uint32_t>(it->removable));
    u32(0xfffffffeu), u32(static_cast<uint32_t>(zcls::lists().list[l].needs_purge));
  }
  u32(static_cast<uint32_t>(zcls::lists().enabled));
  const int32_t n = static_cast<int32_t>(g_out.size());
  if (n <= cap) std::memcpy(out, g_out.data(), g_out.size());
  return n;
}

// The actions run and the reports since the last call.
int al_trace(int32_t* out, int32_t cap) {
  const int32_t n = static_cast<int32_t>(g_trace.size());
  if (n <= cap) std::memcpy(out, g_trace.data(), 4 * g_trace.size());
  g_trace.clear();
  return n;
}

}  // extern "C"
