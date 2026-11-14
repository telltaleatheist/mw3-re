// Probe for tests/differential/test_pm_view_ground.py: Pirate's Moon's camera ground and water check 0x00408b20 with
// set_underwater 0x00408d00 and the fog mode 0x005404f0 (the port's PlayerView::follow_ground, set_underwater and
// zmod::fog_set_on), every outside call answered from the case and logged, as the test's stubs answer the exe's.
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zmodel/gmod_fog.h"
#include "mech3/mech.h"
#include "mech3/player_view.h"

namespace {
namespace m3 = mech3;
std::vector<uint32_t> g_log;
const uint32_t* g_in = nullptr;
float g_fog[2][7];
float f(uint32_t b) { float x; std::memcpy(&x, &b, 4); return x; }
uint32_t b(float x) { uint32_t v; std::memcpy(&v, &x, 4); return v; }

struct Fake : m3::ViewHooks {
  m3::Node3 find_node(int32_t, const char*) override { return nullptr; }
  uint32_t node_flags(m3::Node3) override { return 0; }
  const float* node_position(m3::Node3) override { return nullptr; }
  void node_world_position(m3::Node3, float out[3]) override { out[0] = f(g_in[0]), out[1] = f(g_in[1]), out[2] = f(g_in[2]); }
  m3::Node3 node_parent(m3::Node3) override { return nullptr; }
  m3::Node3 world_node() override { return nullptr; }
  int32_t segment_query(const float*, const float*, uint8_t*) override { return 0; }
  const float* camera_position(m3::Node3) override { return nullptr; }
  const float* mission_fog(bool water) override { return g_fog[water ? 1 : 0]; }
  void* dfn_reader(int32_t) override { return nullptr; }
  void* reader_find(void*, const char*) override { return nullptr; }
  bool reader_float(void*, const char*, float*) override { return false; }
  bool reader_vec3(void*, const char*, float*) override { return false; }
  bool view_fov(float*) override { return false; }
  bool ground_query(float, float y, float) override {
    g_log.insert(g_log.end(), {1u, b(y)});
    return g_in[3] != 0;
  }
  float water_query(float y, float height, int32_t* index, uint8_t flag, int32_t* kind, float* level) override {
    g_log.insert(g_log.end(), {2u, b(y), b(height), flag});
    *index = static_cast<int32_t>(g_in[6]), *kind = static_cast<int32_t>(g_in[7]), *level = f(g_in[8]);
    return f(g_in[9]);
  }
  uint8_t water_flag() override { return static_cast<uint8_t>(g_in[10]); }
  void camera_zone(m3::Node3, int32_t index, m3::Vehicle*) override { g_log.insert(g_log.end(), {6u, static_cast<uint32_t>(index)}); }
  void world_fog_range(float a, float c) override { g_log.insert(g_log.end(), {3u, b(a), b(c)}); }
  void world_fog_color(float r, float g, float bl) override { g_log.insert(g_log.end(), {4u, b(r), b(g), b(bl)}); }
  void world_fog_altitude(float a, float c) override { g_log.insert(g_log.end(), {5u, b(a), b(c)}); }
  void camera_add_position(m3::Node3, float x, float y, float z) override { g_log.insert(g_log.end(), {7u, b(x), b(y), b(z)}); }
  bool has_weather() override { return false; }
  uint32_t weather_flags() override { return 0; }
  void world_segments(float*, int32_t, zcls::SegmentHits*) override {}
  const float* entity_position(m3::Vehicle&) override {
    static float p[3];
    p[0] = 0, p[1] = f(g_in[5]), p[2] = 0;
    return p;
  }
  m3::CameraMode* target_camera(m3::Vehicle&, int32_t) override { return nullptr; }
  const float* torso_aim(m3::Mech&) override { return nullptr; }
  uint32_t controls_flags(m3::Mech&) override { return 0; }
  int32_t controls_kind(m3::Mech&) override { return 0; }
  uint32_t option_5f4d44() override { return 0; }
  uint8_t head_bob() override { return 0; }
  float head_bob_scale() override { return 0; }
  m3::Vehicle* player_target() override { return nullptr; }
  uint8_t game_byte_8e(uint32_t) override { return 0; }
};
}  // namespace

extern "C" {
// in: 0..2 the camera's place, 3 the ground probe hits, 4 a target's height (bits, 0 none), 5 its place's y,
// 6 the water's zone index, 7 its kind, 8 its level, 9 the ground under (bits), 10 the water flag, 11 under water
// before, 12 the fog mode before, 13 the mode's id (-1 none), 14..27 the air's and the water's fog (7 floats each).
// out: the answer, under water after, the fog mode after, then the calls.
int probe_pm_view_ground(const uint32_t* in, uint32_t* out) {
  g_in = in;
  g_log.clear();
  for (int k = 0; k < 7; ++k) g_fog[0][k] = f(in[14 + k]), g_fog[1][k] = f(in[21 + k]);
  static Fake fake;
  m3::set_view_hooks(&fake);
  m3::PlayerView v(reinterpret_cast<m3::Node3>(0x1000), nullptr, nullptr, 1);
  m3::Vehicle t{};
  if (in[4]) t.height = f(in[4]), v.target = &t;
  m3::CameraMode mode(static_cast<int32_t>(in[13]));
  v.mode = static_cast<int32_t>(in[13]) >= 0 ? &mode : nullptr;
  v.underwater = static_cast<uint8_t>(in[11]);
  zmod::fog().on = static_cast<int32_t>(in[12]);
  const bool r = v.follow_ground();
  int n = 0;
  out[n++] = r, out[n++] = v.underwater, out[n++] = static_cast<uint32_t>(zmod::fog().on);
  for (uint32_t x : g_log) out[n++] = x;
  v.target = nullptr, v.mode = nullptr;
  m3::set_view_hooks(nullptr);
  return n;
}
}  // extern "C"
