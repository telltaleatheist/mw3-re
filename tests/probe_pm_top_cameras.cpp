// Probe for tests/differential/test_pm_top_cameras.py: Pirate's Moon's top-down cameras, the mech's god camera
// (0x004311e0, the port's mech3::make_cameras) and a vehicle's above camera (0x004b0790, vehicle_cameras_make),
// with the definition blocks absent (the defaults alone).
#include <cstdint>
#include <cstring>

#include "gamez/zgame/zgame_table.h"
#include "mech3/player_view.h"
#include "mech3/veh_basic_object.h"
#include "pm/pm_game.h"

namespace {
namespace m3 = mech3;
struct Fake : m3::ViewHooks {
  m3::Node3 find_node(int32_t, const char*) override { return nullptr; }
  uint32_t node_flags(m3::Node3) override { return 0; }
  const float* node_position(m3::Node3) override { return nullptr; }
  void node_world_position(m3::Node3, float out[3]) override { out[0] = out[1] = out[2] = 0; }
  m3::Node3 node_parent(m3::Node3) override { return nullptr; }
  m3::Node3 world_node() override { return nullptr; }
  int32_t segment_query(const float*, const float*, uint8_t*) override { return 0; }
  const float* camera_position(m3::Node3) override { return nullptr; }
  const float* mission_fog(bool) override { return nullptr; }
  void* dfn_reader(int32_t) override { return nullptr; }
  void* reader_find(void*, const char*) override { return nullptr; }
  bool reader_float(void*, const char*, float*) override { return false; }
  bool reader_vec3(void*, const char*, float*) override { return false; }
  bool view_fov(float*) override { return false; }
  bool ground_query(float, float, float) override { return false; }
  float water_query(float, float, int32_t*, uint8_t, int32_t*, float*) override { return 0; }
  uint8_t water_flag() override { return 0; }
  void camera_zone(m3::Node3, int32_t, m3::Vehicle*) override {}
  void world_fog_range(float, float) override {}
  void world_fog_color(float, float, float) override {}
  void world_fog_altitude(float, float) override {}
  bool has_weather() override { return false; }
  uint32_t weather_flags() override { return 0; }
  void world_segments(float*, int32_t, zcls::SegmentHits*) override {}
  const float* entity_position(m3::Vehicle&) override { return nullptr; }
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
void put(const m3::CameraMode* m, uint32_t* out) {
  std::memcpy(out, &m->dist, 4);
  std::memcpy(out + 1, m->delta_rot, 16);
}
}  // namespace

extern "C" {
// in: pm (1 PM's table, 0 MW3's). out: the god camera's dist and rotation (w, x, y, z), then the vehicle's above
// camera's. Returns 0, or 1 when one is missing.
int probe_pm_top_cameras(int pm, uint32_t* out) {
  zgame::set_game(pm ? pm::table() : zgame::mw3_table());
  static Fake fake;
  m3::set_view_hooks(&fake);
  m3::CameraList* list = m3::make_cameras(0);
  const m3::CameraMode* god = list ? list->find(3) : nullptr;
  if (god) put(god, out);
  delete list;
  m3::vehicle_cameras_reset();
  m3::vehicle_cameras_make();
  const m3::CameraMode* above = m3::vehicle_cameras().find(3);
  if (above) put(above, out + 5);
  m3::vehicle_cameras_reset();
  m3::set_view_hooks(nullptr);
  zgame::set_game(pm::table());
  return god && above ? 0 : 1;
}
}  // extern "C"
