// Probes for test_puffers.py (docs/puffers.md): the puffer system (gmod_puff),
// its sprite (zrndr_draw), the animation event that drives it
// (zeff_anim_puffer), zmth::exp_neg / euler_to_quat and the node world
// helpers. Self-contained: no shared helpers.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "crt/msvc_rand.h"
#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zeffect/zeff_anim_puffer.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zimage/zimg_texture.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmodel/gmod_clip2d.h"
#include "gamez/zmodel/gmod_draw.h"
#include "gamez/zmodel/gmod_init.h"
#include "gamez/zmodel/gmod_puff.h"
#include "gamez/zrender/zrndr_fog.h"
#include "gamez/zutil/ztimer.h"

namespace {

// The log, int32 words: kind, then its fields.
//   1 create node            2 priority: node, value    3 callback: node, set?
//   4 destroy: node          5 zerr: line, length, chars
//   6 draw: entry (0 [0x72f3c0], 1 [0x72f3bc]), count, record, alpha bits,
//     overlay, 4 x (sx, sy, sz, tu, tv), then for entry 0 4 x rgb
//   7 node euler: node       8 node position: node
std::vector<int32_t> g_log;
std::vector<int32_t> g_out;

constexpr uintptr_t kNodeBase = 0x10000;  // fake node n (1..) at kNodeBase * n
std::vector<float> g_euler, g_pos;         // per fake node
bool g_real_nodes = false;                 // phase B: nodes from the world pool

zimg::TextureEntry* g_textures = nullptr;
int32_t g_ntex = 0;
zcls::Camera g_camera{};
zeff::AnimDef g_def{};

int32_t bits(float f) {
  int32_t b;
  std::memcpy(&b, &f, 4);
  return b;
}
float flt(uint32_t b) {
  float f;
  std::memcpy(&f, &b, 4);
  return f;
}

int32_t node_id(const zcls::Node* n) {
  if (!n) return 0;
  if (g_real_nodes) {
    const zcls::NodePool& p = zcls::nodes();
    for (size_t b = 0; b < p.blocks.size(); ++b) {
      const zcls::Node* base = p.blocks[b].get();
      if (n >= base && n < base + p.block_size) return 0x100000 + static_cast<int32_t>(b * p.block_size + (n - base));
    }
  }
  return static_cast<int32_t>(reinterpret_cast<uintptr_t>(n) / kNodeBase);
}
zcls::Node* node_of(int32_t id) {
  if (id == 0) return nullptr;
  if (id >= 0x100000) return zcls::node_at(id - 0x100000);
  return reinterpret_cast<zcls::Node*>(kNodeBase * static_cast<uintptr_t>(id));
}

int32_t tex_id(const zimg::TextureEntry* t) {
  if (!t) return -2;
  if (t == zimg::default_entry()) return -1;
  return static_cast<int32_t>(t - g_textures);
}
int32_t record_id(const void* r) {
  if (!r) return -2;
  const auto v = reinterpret_cast<uintptr_t>(r);
  return v == 0x6fff0000 ? -1 : static_cast<int32_t>((v - 0x70000000) / 16);
}

void zerr_sink(int, const char*, int line, const char* message) {
  const int32_t n = static_cast<int32_t>(std::strlen(message));
  g_log.insert(g_log.end(), {5, line, n});
  for (int32_t i = 0; i < n; ++i) g_log.push_back(static_cast<uint8_t>(message[i]));
}

void record_draw(int32_t entry, int32_t count, void* texture, float alpha, int32_t overlay) {
  g_log.insert(g_log.end(), {6, entry, count, record_id(texture), bits(alpha), overlay});
  const zmod::Clip2D& c = zmod::clip2d();
  for (int i = 0; i < 4; ++i) {
    const zmod::TLVertex& v = c.tl[i];
    g_log.insert(g_log.end(), {bits(v.sx), bits(v.sy), bits(v.sz), bits(v.tu), bits(v.tv)});
  }
  if (entry == 0)
    for (int i = 0; i < 4; ++i)
      for (int k = 0; k < 3; ++k) g_log.push_back(bits(c.work[i].color[k]));
}

void put_f(const float* f, int n) {
  for (int i = 0; i < n; ++i) g_out.push_back(bits(f[i]));
}

void put_tracks(const std::vector<zmod::PuffTextureKey>& t, const std::vector<zmod::PuffColorKey>& c,
                const std::vector<zmod::PuffGrowthKey>& g) {
  g_out.push_back(static_cast<int32_t>(t.size()));
  for (const auto& k : t) g_out.insert(g_out.end(), {bits(k.time), tex_id(k.texture)});
  g_out.push_back(static_cast<int32_t>(c.size()));
  for (const auto& k : c) put_f(&k.r, 5);
  g_out.push_back(static_cast<int32_t>(g.size()));
  for (const auto& k : g) put_f(&k.time, 2);
}

zmod::Puffer* puffer(int32_t index) {
  auto it = zmod::puff_state().puffers.begin();
  std::advance(it, index);
  return &*it;
}

}  // namespace

extern "C" {

// A fresh puffer system: the texture table (ntex entries "tex<i>", records
// at 0x70000000 + 16 i), rand seeded, every hook recording.
void pf_reset(uint32_t seed, int32_t ntex, int32_t real_nodes) {
  zmod::puff_state() = zmod::PuffState{};
  crt::msvc_srand(seed);
  g_log.clear();
  g_real_nodes = real_nodes != 0;
  zimg::TextureTable& tt = zimg::textures();
  tt.count = ntex;
  g_textures = tt.entries;
  g_ntex = ntex;
  for (int32_t i = 0; i < ntex; ++i) {
    zimg::TextureEntry& e = tt.entries[i];
    e = zimg::TextureEntry{};
    std::snprintf(e.name, sizeof e.name, "tex%d", i);
    e.state = 1;
    e.surface = reinterpret_cast<void*>(static_cast<uintptr_t>(0x70000000 + 16 * i));
  }
  zimg::default_entry()->surface = reinterpret_cast<void*>(static_cast<uintptr_t>(0x6fff0000));
  zerr::set_sink(zerr_sink);
  zmod::PuffHooks& h = zmod::puff_hooks();
  h = zmod::PuffHooks{};
  h.create_node = [](void*) {
    g_log.push_back(1);
    return node_of(0x77);
  };
  h.set_priority = [](void*, zcls::Node* n, int32_t v) {
    g_log.insert(g_log.end(), {2, node_id(n), v});
    return 0;
  };
  h.set_callback = [](void*, zcls::Node* n, void (*cb)()) {
    g_log.insert(g_log.end(), {3, node_id(n), cb == zmod::puff_tick ? 1 : cb ? 2 : 0});
    return 0;
  };
  h.destroy_node = [](void*, zcls::Node* n) { g_log.insert(g_log.end(), {4, node_id(n)}); };
  if (!g_real_nodes) {
    h.node_euler = [](void*, zcls::Node* n, float out[3]) {
      const int32_t id = node_id(n);
      g_log.insert(g_log.end(), {7, id});
      std::memcpy(out, &g_euler[3 * (id - 1)], 12);
      return 0;
    };
    h.node_position = [](void*, zcls::Node* n, float out[3]) {
      const int32_t id = node_id(n);
      g_log.insert(g_log.end(), {8, id});
      std::memcpy(out, &g_pos[3 * (id - 1)], 12);
      return 0;
    };
  }
  zmod::DrawTable& t = zmod::draw_table();
  t = zmod::DrawTable{};
  t.textured = [](void*, int32_t count, void* tex, float opacity, int32_t overlay) {
    record_draw(0, count, tex, opacity, overlay);
  };
  t.textured_white = [](void*, int32_t count, void* tex, int32_t, float opacity, int32_t overlay) {
    record_draw(1, count, tex, opacity, overlay);
  };
  zeff::anim_hooks().create_puffer = [] { return static_cast<void*>(zmod::puffer_create()); };
}

// Fake nodes 1..n: their world euler angles and positions.
void pf_nodes(int32_t n, const float* euler, const float* pos) {
  g_euler.assign(euler, euler + 3 * n);
  g_pos.assign(pos, pos + 3 * n);
}

int32_t pf_create() {
  zmod::puffer_create();
  return static_cast<int32_t>(zmod::puff_state().puffers.size()) - 1;
}

// One setter on puffer `index`, args as raw dwords (test_puffers.py SETTERS).
void pf_set(int32_t op, int32_t index, const uint32_t* a) {
  zmod::Puffer* p = puffer(index);
  const float v[3] = {flt(a[0]), flt(a[1]), flt(a[2])};
  const auto i = static_cast<int32_t>(a[0]);
  switch (op) {
    case 0: zmod::puffer_set_active(p, i); break;
    case 1: zmod::puffer_set_node(p, node_of(i)); break;
    case 2: zmod::puffer_set_translate(p, v); break;
    case 3: zmod::puffer_set_local_velocity(p, v); break;
    case 4: zmod::puffer_set_world_velocity(p, v); break;
    case 5: zmod::puffer_set_min_random(p, v); break;
    case 6: zmod::puffer_set_max_random(p, v); break;
    case 7: zmod::puffer_set_accel(p, v); break;
    case 8: zmod::puffer_set_by_distance(p, i); break;
    case 9: zmod::puffer_set_interval(p, v[0]); break;
    case 10: zmod::puffer_set_size_min(p, v[0]); break;
    case 11: zmod::puffer_set_size_max(p, v[0]); break;
    case 12: zmod::puffer_set_life_min(p, v[0]); break;
    case 13: zmod::puffer_set_life_max(p, v[0]); break;
    case 14: zmod::puffer_set_age_min(p, v[0]); break;
    case 15: zmod::puffer_set_age_max(p, v[0]); break;
    case 16: zmod::puffer_set_deviation(p, v[0]); break;
    case 17: zmod::puffer_set_near_fade(p, v[0], v[1]); break;
    case 18: zmod::puffer_set_far_fade(p, v[0], v[1]); break;
    case 19: zmod::puffer_set_friction(p, v[0]); break;
    case 20: zmod::puffer_set_wind_factor(p, v[0]); break;
    case 21: zmod::puffer_set_number(p, i); break;
    case 22: zmod::puffer_clear_textures(p); break;
    case 23: zmod::puffer_add_texture(p, i < 0 ? zimg::default_entry() : &g_textures[i]); break;
    case 24: zmod::puffer_add_timed_texture(p, i < 0 ? zimg::default_entry() : &g_textures[i], flt(a[1])); break;
    case 25: zmod::puffer_clear_colors(p); break;
    case 26: zmod::puffer_add_color(p, v[0], v[1], v[2], flt(a[3]), flt(a[4])); break;
    case 27: zmod::puffer_clear_growth(p); break;
    case 28: zmod::puffer_add_growth(p, v[0], v[1]); break;
    default: break;
  }
}

// Wind setters: 0 velocity, 1 max speed, 2 random accel, 3 random angvel.
void pf_wind(int32_t op, const float* v) {
  switch (op) {
    case 0: zmod::wind_set_velocity(v); break;
    case 1: zmod::wind_set_max_speed(v[0]); break;
    case 2: zmod::wind_set_random_accel(v[0]); break;
    case 3: zmod::wind_set_random_angvel(v[0]); break;
    default: break;
  }
}

void pf_tick_reset() { zmod::puff_tick_reset(); }
void pf_shutdown() { zmod::puff_shutdown(); }
void pf_tick(float dt) {
  ztimer::state().dt = dt;
  zmod::puff_tick();
}

// The camera for a draw: view (12), projection {near, cx, cy, inv_near,
// fy_near, fx_near}, the camera data's cull points and normals {pos, near
// point, near normal, far point, far normal}, the screen rect {left, top,
// right, bottom}, Direct3D on.
void pf_camera(const float* view, const float* proj, const float* cam, const float* rect, int32_t hardware) {
  std::memcpy(zmth::view().view.m, view, 48);
  zmth::Projection& p = zmth::projection();
  p.near_z = proj[0], p.cx = proj[1], p.cy = proj[2], p.inv_near = proj[3], p.fy_near = proj[4], p.fx_near = proj[5];
  std::memcpy(g_camera.floats(0x2c), cam, 12);
  std::memcpy(g_camera.floats(0xb8), cam + 3, 12);
  std::memcpy(g_camera.floats(0x1b8), cam + 6, 12);
  std::memcpy(g_camera.floats(0xc4), cam + 9, 12);
  std::memcpy(g_camera.floats(0x1c4), cam + 12, 12);
  zmod::walk_state().camera = &g_camera;
  zmod::ClipPlanes& c = zmod::draw_state().clip;
  std::memcpy(&c.side_04, rect, 4), std::memcpy(&c.side_08, rect + 1, 4);
  std::memcpy(&c.side_1c, rect + 2, 4), std::memcpy(&c.side_20, rect + 3, 4);
  zrndr::device_fog().active = hardware;
}

void pf_draw() { zmod::puff_draw(); }

// The state: wind (12 dwords), tick node, puffers, particles (as
// test_puffers.py reads the original's). Returns the length in words.
int32_t pf_dump(int32_t* out, int32_t cap) {
  g_out.clear();
  const zmod::PuffState& s = zmod::puff_state();
  const zmod::Wind& w = s.wind;
  put_f(w.current, 3), put_f(w.base, 3);
  put_f(&w.angle, 1), put_f(&w.speed, 1), put_f(&w.max_speed, 1), put_f(&w.angular_velocity, 1), put_f(&w.accel, 1);
  g_out.push_back(node_id(s.tick_node));
  g_out.push_back(static_cast<int32_t>(s.puffers.size()));
  for (const zmod::Puffer& p : s.puffers) {
    g_out.insert(g_out.end(), {p.active, p.number, p.timed_textures});
    put_tracks(p.textures, p.colors, p.growth);
    g_out.push_back(p.by_distance);
    const float f[12] = {p.interval, p.inv_interval, p.accum, p.size_min, p.size_max, p.life_min,
                         p.life_max, p.age_min, p.age_max, p.deviation, p.friction, p.wind_factor};
    put_f(f, 12);
    g_out.push_back(node_id(p.node));
    put_f(p.prev, 3);
    g_out.push_back(p.has_prev);
    put_f(p.translate, 3), put_f(p.local_velocity, 3), put_f(p.world_velocity, 3);
    put_f(p.min_random, 3), put_f(p.max_random, 3), put_f(p.accel, 3);
    const float g[6] = {p.near_start, p.near_end, p.near_inv, p.far_start, p.far_end, p.far_inv};
    put_f(g, 6);
  }
  g_out.push_back(static_cast<int32_t>(s.particles.size()));
  for (const zmod::Particle& q : s.particles) {
    put_f(q.pos, 21);
    g_out.push_back(static_cast<int32_t>(q.texture_at));
    g_out.push_back(static_cast<int32_t>(q.color_at));
    g_out.push_back(static_cast<int32_t>(q.growth_at));
    put_tracks(q.textures, q.colors, q.growth);
  }
  const int32_t n = static_cast<int32_t>(g_out.size());
  std::memcpy(out, g_out.data(), sizeof(int32_t) * static_cast<size_t>(n < cap ? n : cap));
  return n;
}

int32_t pf_log(int32_t* out, int32_t cap) {
  const int32_t n = static_cast<int32_t>(g_log.size());
  std::memcpy(out, g_log.data(), sizeof(int32_t) * static_cast<size_t>(n < cap ? n : cap));
  g_log.clear();
  return n;
}

// The animation def for the events: nrefs puffer refs (names 36 bytes each,
// ref_puffer: the puffer's list index or -1 for none), node refs (node ids),
// the def's node (+0x74). Ref 0 is the list's unused entry, as in the files.
void pf_def(int32_t nrefs, const char* names, const int32_t* ref_puffer, int32_t nnodes, const int32_t* node_ids,
            int32_t count_byte) {
  delete[] g_def.puffers;
  delete[] g_def.nodes;
  g_def = zeff::AnimDef{};
  std::snprintf(reinterpret_cast<char*>(g_def.raw), 32, "puffanim");
  g_def.raw[0x10c] = static_cast<uint8_t>(count_byte);
  g_def.puffers = nrefs ? new zeff::CreatedRef[nrefs]{} : nullptr;
  for (int32_t i = 0; i < nrefs; ++i) {
    std::memcpy(g_def.puffers[i].raw, names + 36 * i, 36);
    g_def.puffers[i].made = ref_puffer[i] < 0 ? nullptr : puffer(ref_puffer[i]);
  }
  g_def.raw[0x10a] = static_cast<uint8_t>(nnodes);
  g_def.nodes = nnodes ? new zeff::NodeRef40[nnodes]{} : nullptr;
  for (int32_t i = 0; i < nnodes; ++i) g_def.nodes[i].node = node_of(node_ids[i]);
}

// The event (0x250 bytes, written back: the index is cached in it).
int32_t pf_event(uint8_t* ev, int32_t def_node) { return zeff::anim_puffer_state(g_def, ev, node_of(def_node)); }

// The def's puffer list: count, then per ref 36 name bytes (9 words) and the
// puffer's list index (-1 none).
int32_t pf_def_dump(int32_t* out) {
  const int32_t n = g_def.count(0x10c);
  int32_t k = 0;
  out[k++] = n;
  for (int32_t i = 0; i < n && g_def.puffers; ++i) {
    std::memcpy(out + k, g_def.puffers[i].raw, 36);
    k += 9;
    int32_t idx = -1, j = 0;
    for (const zmod::Puffer& p : zmod::puff_state().puffers) {
      if (&p == g_def.puffers[i].made) idx = j;
      ++j;
    }
    out[k++] = idx;
  }
  return k;
}

float pf_exp(float x) { return zmth::exp_neg(x); }
void pf_quat(float x, float y, float z, float* q) { zmth::euler_to_quat(x, y, z, q); }

// zcls::node_world_euler / _position on pool node `index` (phase B, a world
// read by sw_load): out = euler, position; returns both results.
int32_t pf_node_world(int32_t index, float* out) {
  zcls::Node* n = index < 0 ? nullptr : zcls::node_at(index);
  const int32_t a = zcls::node_world_euler(n, out);
  const int32_t b = zcls::node_world_position(n, out + 3);
  return a | b << 4;
}

}  // extern "C"
