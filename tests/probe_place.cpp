// Mech placement (mech3/mech_place: 0x004161c0 and what it calls) for
// tests/differential/test_place.py, on a world the port's loaders read
// (sw_load in probe_scenewalk.cpp). The mech goes back and forth as an image
// laid out like the original's; every call out of the placement is logged.
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zclass/camera.h"
#include "gamez/zclass/cls_di.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/list.h"
#include "gamez/zmodel/gmod_cull.h"
#include "gamez/zmodel/gmod_hit.h"
#include "gamez/zmodel/gmod_matl.h"
#include "mech3/mech.h"
#include "mech3/mech_collide.h"
#include "mech3/mech_move.h"
#include "mech3/mech_place.h"
#include "mech3/mech_skis.h"
#include "mech3/mission_launch.h"
#include "mech3/veh_basic.h"
#include "mech3/weapon.h"

using namespace mech3;

namespace {

template <typename T> T rd(const uint8_t* b, int off) {
  T v;
  std::memcpy(&v, b + off, sizeof v);
  return v;
}
template <typename T> void wr(uint8_t* b, int off, T v) { std::memcpy(b + off, &v, sizeof v); }
int32_t bits(float f) {
  int32_t v;
  std::memcpy(&v, &f, 4);
  return v;
}
Handle h(uint32_t v) { return reinterpret_cast<Handle>(static_cast<uintptr_t>(v)); }
uint32_t u(Handle p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }

int32_t node_index(const zcls::Node* n) {
  if (!n) return -1;
  const zcls::NodePool& p = zcls::nodes();
  for (size_t b = 0; b < p.blocks.size(); ++b) {
    const zcls::Node* base = p.blocks[b].get();
    if (n >= base && n < base + p.block_size) return static_cast<int32_t>(b * p.block_size + (n - base));
  }
  return -2;
}

int32_t material_index(const zmod::Material* m) {
  if (!m) return -1;
  const auto& s = zmod::materials().slots;
  if (m >= s.data() && m < s.data() + s.size()) return static_cast<int32_t>(m - s.data());
  return -2;
}

zcls::Node* world_node() { return zcls::camera_world(zcls::list_head(zcls::kCameras)->node); }

// A record as di_ground writes it: count, then per slot normal, x, height,
// z, zone[2], material and node indices.
void put_record(int32_t* o, const zcls::GroundHits& g) {
  *o++ = g.count;
  for (const zcls::GroundHit& x : g.hit) {
    std::memcpy(o, x.normal, 12), o += 3;
    std::memcpy(o++, &x.x, 4);
    std::memcpy(o++, &x.height, 4);
    std::memcpy(o++, &x.z, 4);
    *o++ = static_cast<int32_t>(x.zone[0]);
    *o++ = static_cast<int32_t>(x.zone[1]);
    *o++ = material_index(x.material);
    *o++ = node_index(x.node);
  }
}

zcls::GroundHits g_point;  // kept between queries, as the test keeps the original's buffer

std::vector<int32_t> g_log;
void log(std::initializer_list<int32_t> v) { g_log.insert(g_log.end(), v); }

// The mech image's layout (the exe's offsets).
constexpr int kTransform = 0x400;  // the probe's own: the 12 floats +0x0c points at
constexpr int kControls = 0x440;   // the controls block +0x37c points at (0x60 bytes)
constexpr int kSkis = 0x800;       // the skis block +0x41c points at (0x300 bytes)
constexpr int kSkiMats = 0xb00;    // the two ski nodes' matrices (12 floats each)
constexpr int kImage = 0xb60;

float g_ski_mats[2][12];

void controls_io(MechControls& c, uint8_t* b, bool store) {
  float* f[] = {&c.throttle, &c.turn, &c.pitch, &c.twist, &c.jump, &c.jj_strafe, &c.jj_forward, &c.axis_2c,
                &c.axis_30};
  for (int i = 0; i < 9; ++i) store ? wr(b, 0x10 + 4 * i, *f[i]) : void(*f[i] = rd<float>(b, 0x10 + 4 * i));
  float* t[] = {&c.joy_throttle, &c.throttle_target, &c.turn_target, &c.jump_target};
  for (int i = 0; i < 4; ++i) store ? wr(b, 0x40 + 4 * i, *t[i]) : void(*t[i] = rd<float>(b, 0x40 + 4 * i));
}

void vec_io(Vec3& v, uint8_t* b, int off, bool store) {
  float* f[] = {&v.x, &v.y, &v.z};
  for (int i = 0; i < 3; ++i) store ? wr(b, off + 4 * i, *f[i]) : void(*f[i] = rd<float>(b, off + 4 * i));
}

void skis_io(Skis& s, uint8_t* b, bool store) {
  auto f = [&](int off, float& v) { store ? wr(b, off, v) : void(v = rd<float>(b, off)); };
  for (int k = 0; k < 2; ++k) {
    f(0x44 + 4 * k, s.sin_pitch[k]), f(0x4c + 4 * k, s.cos_pitch[k]);
    f(0x54 + 4 * k, s.a[k]), f(0x5c + 4 * k, s.b[k]), f(0x64 + 4 * k, s.c[k]);
  }
  f(0x78, s.half_track);
  for (int j = 0; j < 7; ++j) {
    vec_io(s.local[j], b, 0xd0 + 0xc * j, store);
    f(0x124 + 4 * j, s.ground[j]);
  }
  f(0x2ec, s.length), f(0x2f0, s.half), f(0x2f8, s.inv_half), f(0x2fc, s.inv_half2);
}

void mech_io(Mech& m, uint8_t* b, bool store) {
  vec_io(m.velocity, b, 0x30, store);
  vec_io(m.vec_3c, b, 0x3c, store);
  for (int i = 0; i < 4; ++i) store ? wr(b, 0x48 + 4 * i, m.orientation[i]) : void(m.orientation[i] = rd<float>(b, 0x48 + 4 * i));
  vec_io(m.vec_58, b, 0x58, store);
  Collider& c = m.collider;
  store ? wr(b, 0x120, c.count) : void(c.count = rd<int32_t>(b, 0x120));
  for (int i = 0; i < 15; ++i) {
    vec_io(c.local[i], b, 0x124 + 0xc * i, store);
    vec_io(c.world[i], b, 0x1d8 + 0xc * i, store);
    vec_io(c.last[i], b, 0x28c + 0xc * i, store);
  }
}

class Launch : public LaunchHooks {
 public:
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg>) override {
    log({-100, static_cast<int32_t>(std::strlen(key))});
    return 0;
  }
  uint8_t cheat = 0;
  int32_t option = 0;
  uint32_t pilot_80 = 0;
  uint8_t cheat_invincible() override { log({20}); return cheat; }
  int32_t option_invincible() override { log({21}); return option; }
  uint32_t field32(Handle, uint32_t off) override { return off == 0x80 ? pilot_80 : 0; }
  void mech_update(Mech& m, float dt) override { log({22, bits(dt), static_cast<int32_t>(m.detail)}); }
  int32_t mech_place(Mech&, const float pos[3], float yaw) override {
    log({23, bits(pos[0]), bits(pos[1]), bits(pos[2]), bits(yaw)});
    return 1;
  }
};

}  // namespace

extern "C" {

// One point query (0x004edab0) on the loaded world with zModel's zone mode;
// the record as di_ground's. Returns the query's result.
int32_t pl_point(float x, float y, float z, int32_t zone_mode, int32_t* out) {
  zmod::zones().mode = zone_mode;
  const int32_t r = zcls::ground_point(world_node(), x, y, z, &g_point);
  zmod::zones().mode = 1;
  put_record(out, g_point);
  return r;
}

void pl_reset() { std::memset(&g_point, 0, sizeof g_point); }

// Adds nodes (by pool index) to the world's children, as the test does to
// the original's (the camera and the sun: the walk's classes 1 and 9).
void pl_add_world_children(const int32_t* index, int32_t n) {
  zcls::Node* w = world_node();
  const int32_t had = w->child_count();
  auto** list = new zcls::Node*[static_cast<size_t>(had + n)];
  for (int32_t i = 0; i < had; ++i) list[i] = w->children[i];
  for (int32_t i = 0; i < n; ++i) list[had + i] = zcls::node_at(index[i]);
  w->children = list;  // the old list leaks, as the test's does
  w->put<int32_t>(0x5c, had + n);
}

// One polygon (0x0054d640): n vertices, the point's x and z; out: the
// result, then the slot's normal and height as bits (the slot starts as
// `slot`, 5 words).
void pl_polygon(const float* verts, int32_t n, float x, float z, const int32_t* slot, int32_t* out) {
  zcls::GroundHit g{};
  std::memcpy(g.normal, slot, 12);
  std::memcpy(&g.height, slot + 4, 4);
  out[0] = zmod::polygon_ground_point(&g, verts, x, z, n);
  std::memcpy(out + 1, g.normal, 12);
  std::memcpy(out + 4, &g.height, 4);
}

// The pick (0x004abd90) on a crafted record: heights and soils (-1: no
// material). out: the result's bits, best, in_water, the surface's bits.
void pl_pick(int32_t n, const float* heights, const int32_t* soils, float y, float step, int32_t water_not_ground,
             int32_t* out) {
  static zmod::Material mats[32];
  static zcls::GroundHits g;
  std::memset(&g, 0, sizeof g);
  g.count = static_cast<uint8_t>(n);
  for (int32_t i = 0; i < n && i < 32; ++i) {
    g.hit[i].height = heights[i];
    if (soils[i] >= 0) {
      mats[i].soil = static_cast<uint32_t>(soils[i]);
      g.hit[i].material = &mats[i];
    }
  }
  int32_t best = -7, in_water = -7;
  float surface = 0;
  const float r = ground_pick(g, y, step, &best, water_not_ground, &in_water, &surface);
  out[0] = bits(r), out[1] = best, out[2] = in_water, out[3] = bits(surface);
}

// The placement 0x004161c0 on the loaded world. `image` (kImage bytes)
// holds the mech's fields at the exe's offsets, the transform, controls,
// skis and ski matrices at the probe's; it comes back as the port left it.
// Returns the log's length (the result in *result). The log: 1 query (zone mode, x y z bits), 2 attach (node), 3 vtable
// +0x18 (x y z bits), 4 moved, 5 ski matrix set (side, 12 bits).
int32_t pl_place(const float* pos, float yaw, uint8_t* image, int32_t* log_out, int32_t log_cap, int32_t* result) {
  static Mech m;
  static MechControls ctl;
  static Skis skis;
  static float transform[12];
  g_log.clear();
  m.node = h(0x5150);
  m.controls = &ctl;
  m.skis = &skis;
  m.transform = transform;
  skis.ski[0] = h(0x5160), skis.ski[1] = h(0x5170);
  std::memcpy(transform, image + kTransform, sizeof transform);
  std::memcpy(g_ski_mats, image + kSkiMats, sizeof g_ski_mats);
  controls_io(ctl, image + kControls, false);
  skis_io(skis, image + kSkis, false);
  mech_io(m, image, false);

  PlaceCalls& pc = place_calls();
  pc.ground = [](float x, float y, float z, zcls::GroundHits* out) {
    log({1, zmod::zones().mode, bits(x), bits(y), bits(z)});
    return zcls::ground_point(world_node(), x, y, z, out);
  };
  pc.attach = [](Handle n) { log({2, static_cast<int32_t>(u(n))}); };
  collide_calls().set_position = [](Mech&, const Vec3* p) { log({3, bits(p->x), bits(p->y), bits(p->z)}); };
  update_calls().moved = [](Mech&) { log({4}); };
  SkisCalls& sc = skis_calls();
  sc.node_matrix = [](Handle n) { return g_ski_mats[n == h(0x5160) ? 0 : 1]; };
  sc.set_node_matrix = [](Handle n, const float* mat) {
    log({5, n == h(0x5160) ? 0 : 1});
    for (int i = 0; i < 12; ++i) g_log.push_back(bits(mat[i]));
  };
  zmod::zones().mode = 1;
  const int32_t r = m.place(pos, yaw);
  log({6, zmod::zones().mode});
  pc = PlaceCalls{};
  collide_calls().set_position = nullptr;
  update_calls().moved = nullptr;
  sc.node_matrix = nullptr, sc.set_node_matrix = nullptr;

  std::memcpy(image + kTransform, transform, sizeof transform);
  std::memcpy(image + kSkiMats, g_ski_mats, sizeof g_ski_mats);
  controls_io(ctl, image + kControls, true);
  skis_io(skis, image + kSkis, true);
  mech_io(m, image, true);
  const int32_t n = static_cast<int32_t>(g_log.size());
  std::memcpy(log_out, g_log.data(), 4 * static_cast<size_t>(n < log_cap ? n : log_cap));
  *result = r;
  return n;
}

// The pilot's spawn 0x004782f0 on a mech image: +0x630, +0x688, +0xdc,
// +0xde, +0x587 and the weapons' mode (its own word in the image at 0x700)
// in and out; the log as pl_place's with 20 [0x00608e40], 21 0x0040cf20,
// 22 the update (dt, detail), 23 the placement (pos, yaw).
int32_t pl_spawn(const float* pos, uint32_t yaw, uint8_t cheat, int32_t option, uint32_t pilot_80, int32_t has_weapons,
                 uint8_t* image, int32_t* log_out, int32_t log_cap, int32_t* result) {
  static Mech m;
  static WeaponManager w(nullptr);
  g_log.clear();
  m.invincible = rd<uint8_t>(image, 0x630) != 0;
  m.detail = rd<uint32_t>(image, 0x688);
  m.power_down_pending = rd<uint8_t>(image, 0xdc) != 0;
  m.powered_down = rd<uint8_t>(image, 0xde) != 0;
  m.hud_saved = rd<uint8_t>(image, 0x587);
  w.mode = rd<int32_t>(image, 0x700);
  m.ammo = has_weapons ? &w : nullptr;
  Launch l;
  l.cheat = cheat, l.option = option, l.pilot_80 = pilot_80;
  const int32_t r = place_pilot_mech(l, h(0x6000), m, pos, yaw);
  wr<uint8_t>(image, 0x630, m.invincible ? 1 : 0);
  wr<uint32_t>(image, 0x688, m.detail);
  wr<uint8_t>(image, 0xdc, m.power_down_pending ? 1 : 0);
  wr<uint8_t>(image, 0xde, m.powered_down ? 1 : 0);
  wr<uint8_t>(image, 0x587, m.hud_saved);
  wr<int32_t>(image, 0x700, w.mode);
  const int32_t n = static_cast<int32_t>(g_log.size());
  std::memcpy(log_out, g_log.data(), 4 * static_cast<size_t>(n < log_cap ? n : log_cap));
  *result = r;
  return n;
}

}  // extern "C"
