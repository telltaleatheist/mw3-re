// The mission boundary's reader (mech3/zainet_inout) for test_inout.py, on a
// world the port's loaders read (sw_load in probe_scenewalk.cpp).
#include <cstdint>
#include <cstring>

#include "gamez/zclass/camera.h"
#include "gamez/zclass/cls_di.h"
#include "gamez/zclass/list.h"
#include "gamez/zerror/zerr.h"
#include "mech3/veh_basic.h"
#include "mech3/zainet_inout.h"

namespace {

int32_t g_have, g_game, g_op, g_mission;
int32_t g_reports;

zcls::Node* world_node() { return zcls::camera_world(zcls::list_head(zcls::kCameras)->node); }

// 0x004c4240: from 100 above, the ground within 20 (0x0041eb90, 0x004abd90).
void ground(float p[3]) {
  static zcls::GroundHits hits;
  if (zcls::ground_point(world_node(), p[0], p[1] + 100.0f, p[2], &hits) == 0 && hits.count > 0) {
    int32_t best, in_water;
    float surface;
    p[1] = mech3::ground_pick(hits, p[1], 20.0f, &best, 1, &in_water, &surface);
  }
}

struct Out {
  uint8_t* p;
  int32_t n = 0, cap;
  void put(const void* v, int32_t k) {
    if (n + k <= cap) std::memcpy(p + n, v, size_t(k));
    n += k;
  }
  void u32(uint32_t v) { put(&v, 4); }
};

void stale(zcls::Node* n) {
  if (n->cls() == 5) {
    auto* d = static_cast<uint8_t*>(n->data);
    uint32_t f;
    std::memcpy(&f, d, 4);
    f |= 0x20;
    std::memcpy(d, &f, 4);
  }
  for (int32_t i = 0; i < n->child_count(); ++i) stale(n->children[i]);
}

// A vertex pointer as (polygon << 16 | corner) + 1, 0 for none.
uint32_t vertex_id(const mech3::ai::InOutArea& a, const mech3::ai::InOutVertex* v) {
  if (!v) return 0;
  for (size_t i = 0; i < a.polygons.size(); ++i) {
    const auto& vs = a.polygons[i]->verts;
    if (v >= vs.data() && v < vs.data() + vs.size()) return uint32_t(i << 16 | size_t(v - vs.data())) + 1;
  }
  return 0xffffffffu;
}
uint32_t poly_id(const mech3::ai::InOutArea& a, const mech3::ai::InOutPolygon* p) {
  if (!p) return 0;
  for (size_t i = 0; i < a.polygons.size(); ++i)
    if (a.polygons[i].get() == p) return uint32_t(i) + 1;
  return 0xffffffffu;
}
// The area's query state, its pointers as ids (from 1, to 2), then the
// flags and the normal.
void put_query(Out& o, const mech3::ai::InOutArea& a, const float* from, const float* to) {
  const mech3::ai::InOutQuery& q = a.q;
  auto pt = [&](const float* p) -> uint32_t { return !p ? 0 : p == from ? 1 : p == to ? 2 : 0xffffffffu; };
  o.u32(pt(q.from)), o.u32(pt(q.to)), o.u32(poly_id(a, q.poly));
  o.u32(vertex_id(a, q.near_a)), o.u32(vertex_id(a, q.near_b)), o.u32(vertex_id(a, q.far_a)), o.u32(vertex_id(a, q.far_b));
  o.put(&q.length, 4), o.put(&q.t_near, 4), o.put(&q.t_far, 4);
  o.put(q.near_point, 12), o.put(q.far_point, 12);
  o.u32(uint32_t(q.crossings)), o.u32(uint32_t(q.from_rays)), o.u32(uint32_t(q.to_rays));
}

}  // namespace

extern "C" {

// 0x004c2b00 on the current area: where the answer points (0 `to`, 1 the
// near point, 2 the far point, else a corner's id + 2), its value, the state.
int32_t inout_query(const float* from, const float* to, uint8_t* buf, int32_t cap) {
  using namespace mech3::ai;
  InOutArea* a = inout().current;
  Out o{buf, 0, cap};
  if (!a) return 0;
  const float* r = a->query(from, to);
  uint32_t kind = r == to ? 0 : r == a->q.near_point ? 1 : r == a->q.far_point ? 2 : 0xffffffffu;
  if (kind == 0xffffffffu)
    for (size_t i = 0; i < a->polygons.size(); ++i)
      for (const InOutVertex& v : a->polygons[i]->verts)
        if (r == v.offset) kind = vertex_id(*a, &v) + 2;
  o.u32(kind);
  o.put(r, 12);
  put_query(o, *a, from, to);
  o.u32(a->from_in | a->to_in << 8);
  o.put(a->normal, 12);
  return o.n;
}

// 0x004c3000: the answer and `out` (in and out the same buffer when `same`).
int32_t inout_pull_probe(const float* pos, float* in, float* out, int32_t same) {
  return mech3::ai::inout_pull(pos, in, same ? in : out) ? 1 : 0;
}

// 0x004c3bd0 with the node's name.
void inout_message(const char* name, int32_t message) { mech3::ai::inout_set_state(name, message); }


// `inout` moved as a modded world might: its local matrix set, every
// object3d under it marked for its world matrix to be redone (data flag 0x20),
// the root's and its object3d children's flag 8 (no matrix of its own)
// cleared, the children given the second matrix.
int32_t inout_move(const float* m) {
  zcls::Node* r = zcls::list_find_by_name(6, "inout");
  if (!r || r->cls() != 5) return 0;
  auto* d = static_cast<uint8_t*>(r->data);
  std::memcpy(d + 0x30, m, 48);
  stale(r);
  uint32_t f;
  std::memcpy(&f, d, 4);
  f &= ~8u;  // a matrix of its own
  std::memcpy(d, &f, 4);
  for (int32_t i = 0; i < r->child_count(); ++i) {  // each child shifted too
    zcls::Node* c = r->children[i];
    if (c->cls() != 5) continue;
    auto* cd = static_cast<uint8_t*>(c->data);
    std::memcpy(cd + 0x30, m + 12, 48);
    std::memcpy(&f, cd, 4);
    f &= ~8u;
    std::memcpy(cd, &f, 4);
    if (i & 1) c->set_flags(c->flags() & ~0x80000u);  // every other one off the tree-valid path
  }
  return 1;
}

// The reader with the pilot's game as given (have 0: no [0x0060861c]) and the
// ground on or off, then the selection; the areas, the current one's index
// (-1 none), whether `inout` is still found and the error reports.
int32_t inout_run(int32_t have, int32_t game, int32_t op, int32_t mission, int32_t on_ground, uint8_t* buf,
                  int32_t cap) {
  using namespace mech3::ai;
  g_have = have, g_game = game, g_op = op, g_mission = mission, g_reports = 0;
  InOutHooks& h = inout_hooks();
  h.have_game = [] { return g_have != 0; };
  h.game_type = [] { return g_game; };
  h.operation = [] { return g_op; };
  h.mission_number = [] { return g_mission; };
  h.ground = on_ground ? ground : nullptr;
  zerr::set_sink([](int, const char*, int, const char*) { ++g_reports; });
  inout() = InOut{};
  inout_mission_load();
  inout_select();
  zerr::set_sink(nullptr);
  Out o{buf, 0, cap};
  const auto* areas = inout().areas.get();
  o.u32(areas ? uint32_t(areas->size()) : 0xffffffffu);
  int32_t current = -1;
  if (areas)
    for (const InOutArea& a : *areas) {
      if (&a == inout().current) current = int32_t(&a - areas->data());
      o.u32(uint32_t(a.polygons.size()));
      for (const auto& p : a.polygons) {
        o.u32(uint32_t(p->kind));
        o.u32(p->inverted | p->tested << 8);
        o.put(&p->perimeter, 4);
        o.put(&p->width, 4);
        o.put(p->min, 12);
        o.put(p->max, 12);
        const char* nm = p->name ? p->name : "";
        o.u32(uint32_t(std::strlen(nm)));
        o.put(nm, int32_t(std::strlen(nm)));
        o.u32(uint32_t(p->verts.size()));
        for (const InOutVertex& v : p->verts) o.put(&v, 0x1c);
      }
      put_query(o, a, nullptr, nullptr);
      o.put(a.min, 12);
      o.put(a.max, 12);
    }
  o.u32(uint32_t(current));
  o.u32(zcls::list_find_by_name(6, "inout") ? 1 : 0);
  o.u32(uint32_t(g_reports));
  return o.n;
}

}  // extern "C"
