// The ground probes (mech3/mech_skis) for tests/differential/test_skis.py. The
// skis, the mech and its feet go back and forth as byte images laid out like
// the original's; the ground query answers with the records the original's
// query wrote in the emulator, and every call out is logged.
#include <cstdint>
#include <cstring>
#include <map>
#include <vector>

#include "gamez/zmath/zmth_matrix.h"
#include "mech3/mech.h"
#include "mech3/mech_skis.h"
#include "mech3/mission_world.h"

using namespace mech3;

namespace {
template <typename T> T rd(const uint8_t* b, int off) {
  T v;
  std::memcpy(&v, b + off, sizeof v);
  return v;
}
template <typename T> void wr(uint8_t* b, int off, T v) { std::memcpy(b + off, &v, sizeof v); }
Handle h(uint32_t v) { return reinterpret_cast<Handle>(static_cast<uintptr_t>(v)); }
uint32_t u(Handle p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }

struct Io {
  uint8_t* b;
  bool store;
  void f(int off, float& v) { store ? wr(b, off, v) : void(v = rd<float>(b, off)); }
  void i(int off, int32_t& v) { store ? wr(b, off, v) : void(v = rd<int32_t>(b, off)); }
  void bo(int off, bool& v) { store ? wr<uint8_t>(b, off, v) : void(v = rd<uint8_t>(b, off) != 0); }
  void hd(int off, Handle& v) { store ? wr<uint32_t>(b, off, u(v)) : void(v = h(rd<uint32_t>(b, off))); }
  void v3(int off, Vec3& v) { f(off, v.x), f(off + 4, v.y), f(off + 8, v.z); }
};

void hit_io(GroundHit& g, uint8_t* b, bool store) {
  Io io{b, store};
  io.v3(0, g.normal), io.f(0xc, g.x), io.f(0x10, g.height), io.f(0x14, g.z);
  store ? std::memcpy(b + 0x18, g.zone, 8) : std::memcpy(g.zone, b + 0x18, 8);
  io.hd(0x20, g.material), io.hd(0x24, g.node);
}

void skis_io(Skis& s, uint8_t* b, bool store) {
  Io io{b, store};
  io.bo(0x5, s.slid);
  for (int k = 0; k < 12; ++k) io.f(0x8 + 4 * k, s.matrix[k]);
  for (int k = 0; k < 2; ++k) {
    io.f(0x44 + 4 * k, s.sin_pitch[k]), io.f(0x4c + 4 * k, s.cos_pitch[k]);
    io.f(0x54 + 4 * k, s.a[k]), io.f(0x5c + 4 * k, s.b[k]), io.f(0x64 + 4 * k, s.c[k]);
    io.bo(0x6c + k, s.down[k]), io.f(0x70 + 4 * k, s.slope[k]);
  }
  io.f(0x78, s.half_track);
  for (int j = 0; j < 7; ++j) {
    io.v3(0x7c + 0xc * j, s.world[j]), io.v3(0xd0 + 0xc * j, s.local[j]);
    io.f(0x124 + 4 * j, s.ground[j]), io.f(0x140 + 4 * j, s.vy[j]), io.f(0x15c + 4 * j, s.surface[j]);
    io.i(0x178 + 4 * j, s.in_water[j]);
    hit_io(s.contact[j], b + 0x194 + 0x28 * j, store);
    io.f(0x2ac + 4 * j, s.clearance[j]), io.f(0x2c8 + 4 * j, s.y[j]), io.bo(0x2e4 + j, s.landed[j]);
  }
  io.f(0x2ec, s.length), io.f(0x2f0, s.half), io.f(0x2f4, s.mid_width);
  io.f(0x2f8, s.inv_half), io.f(0x2fc, s.inv_half2);
}

void mech_io(Mech& m, uint8_t* b, bool store) {
  Io io{b, store};
  io.bo(0x8, m.active);
  io.v3(0x24, m.forward), io.v3(0x3c, m.vec_3c);
  for (int k = 0; k < 4; ++k) io.f(0x48 + 4 * k, m.orientation[k]);
  io.f(0x68, m.height);
  for (int k = 0; k < 4; ++k) io.bo(0x374 + k, m.blocked[k]);
  io.v3(0x594, m.impulse), io.v3(0x5a0, m.push);
  int32_t d = static_cast<int32_t>(m.detail);
  io.i(0x688, d);
  m.detail = static_cast<uint32_t>(d);
}

struct State {
  Mech m;
  Skis s;
  Leg legs[2];
  Foot feet[2];
  Vec3 foot_pos[2];
  float transform[12];
  float ski_mats[2][12];
  Vec3 pos;
  const uint8_t* hits = nullptr;
  const float* after = nullptr;  // the probes as the original's query left them
  std::map<uint32_t, int> soils;
  std::vector<int32_t> log;
};
State* g = nullptr;

void log(std::initializer_list<int32_t> v) {
  g->log.push_back(static_cast<int32_t>(v.size()));
  g->log.insert(g->log.end(), v);
}
int32_t bits(float f) {
  int32_t i;
  std::memcpy(&i, &f, 4);
  return i;
}
}  // namespace

extern "C" {

// One MechSkis::Update (0x4765b0). `feet` is two 0x100 images (+0x48
// destroyed, the translation at +0xf0); `soils` pairs material addresses with
// their soil. Returns airborne; the log goes to `out` (length in *n).
int32_t probe_skis(uint8_t* skis, uint8_t* mech, float* transform, uint8_t* feet, float* ski_mats,
                   uint32_t ski0, uint32_t ski1, const uint8_t* hits, const float* after,
                   const uint32_t* soils, int32_t nsoils,
                   float pos_y, float gravity, float ceiling, float dt, int32_t* out, int32_t* n) {
  static State state;
  g = &state;
  Mech& m = g->m;
  m = Mech{};
  g->s = Skis{};
  mech_io(m, mech, false);
  skis_io(g->s, skis, false);
  std::memcpy(g->transform, transform, sizeof g->transform);
  m.transform = g->transform;
  std::memcpy(g->ski_mats, ski_mats, sizeof g->ski_mats);
  g->s.mech = &m;
  g->s.ski[0] = h(ski0), g->s.ski[1] = h(ski1);
  m.skis = &g->s;
  for (int i = 0; i < 2; ++i) {
    g->legs[i].foot = &g->feet[i];
    m.legs[i] = &g->legs[i];
    g->feet[i].destroyed = feet[0x100 * i + 0x48] != 0;
    g->foot_pos[i] = rd<Vec3>(feet + 0x100 * i, 0xf0);
    g->feet[i].translation = &g->foot_pos[i];
  }
  g->pos = {transform[9], pos_y, transform[11]};
  g->hits = hits;
  g->after = after;
  g->soils.clear();
  for (int k = 0; k < nsoils; ++k) g->soils[soils[2 * k]] = static_cast<int>(soils[2 * k + 1]);
  environment().gravity = gravity;
  environment().jet_ceiling = ceiling;
  g->log.clear();
  SkisCalls& c = skis_calls();
  c.query = [](Vec3* p, int count, float top, GroundHits* out) {
    const int start = static_cast<int>(p - g->s.world);
    log({1, start, count, bits(top)});
    for (int k = 0; k < count; ++k) {
      log({2, bits(p[k].x), bits(p[k].y), bits(p[k].z)});
      p[k] = {g->after[3 * (start + k)], g->after[3 * (start + k) + 1], g->after[3 * (start + k) + 2]};
      const uint8_t* r = g->hits + 0x504 * (start + k);
      out[k].count = r[0];
      for (int j = 0; j < 32; ++j) hit_io(out[k].hit[j], const_cast<uint8_t*>(r) + 4 + 0x28 * j, false);
    }
  };
  c.soil = [](Handle mat) {
    auto it = g->soils.find(u(mat));
    return it == g->soils.end() ? 0 : it->second;
  };
  c.node_matrix = [](Handle node) { return g->ski_mats[node == g->s.ski[1] ? 1 : 0]; };
  c.set_node_matrix = [](Handle node, const float* mat) {
    log({3, node == g->s.ski[1] ? 1 : 0});
    for (int k = 0; k < 12; ++k) log({4, bits(mat[k])});
  };
  c.position = [](Mech&) -> const Vec3* { return &g->pos; };
  c.zone = [](const uint8_t* z) { log({5, rd<int32_t>(z, 0), rd<int32_t>(z, 4)}); };
  zmth::reset_stack();
  zmth::Mat34 base{};
  zmth::push(&base);
  const bool air = g->s.update(dt);
  zmth::pop();
  mech_io(m, mech, true);
  skis_io(g->s, skis, true);
  std::memcpy(ski_mats, g->ski_mats, sizeof g->ski_mats);
  const int32_t len = static_cast<int32_t>(g->log.size());
  std::memcpy(out, g->log.data(), 4 * (len < *n ? len : *n));
  *n = len;
  return air;
}

}  // extern "C"
