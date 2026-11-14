// PM's sensor range (0x004e7170) for tests/differential/test_pm_sensor_range.py.
#include <cstdint>
#include <cstring>

#include "mech3/zai_target.h"

namespace {
using zgame::Handle;
struct Hooks : mech3::ai::TargetHooks {
  uint32_t pilot = 0, player = 0, self_vehicle_v = 0, x58 = 0, radar = 0;
  int32_t type = 0;
  bool have_pilot() override { return pilot != 0; }
  Handle player_vehicle() override { return player; }
  Handle self_vehicle(Handle) override { return self_vehicle_v; }
  Handle self_x58(Handle) override { return x58; }
  int32_t game_type() override { return type; }
  bool mp_radar() override { return radar != 0; }
} g_hooks;
}  // namespace

extern "C" {
// in: pilot, player vehicle, self's vt+0x58 answer (its vehicle), vt+0x60 answer, game type, radar byte, range
// bits. out: the stored sensor range, range and range squared (bits).
void probe_pm_sensor_range(const uint32_t* in, uint32_t* out) {
  mech3::ai::TargetHooks* old = &mech3::ai::target_hooks();
  g_hooks.pilot = in[0], g_hooks.player = in[1], g_hooks.self_vehicle_v = in[2], g_hooks.x58 = in[3];
  g_hooks.type = static_cast<int32_t>(in[4]), g_hooks.radar = in[5];
  mech3::ai::set_target_hooks(&g_hooks);
  mech3::ai::AiTarget t{};
  t.set_sensor_range(0x1234, zgame::from_bits(in[6]));
  std::memcpy(&out[0], &t.sensor_range, 4);
  std::memcpy(&out[1], &t.range, 4);
  std::memcpy(&out[2], &t.range2, 4);
  mech3::ai::set_target_hooks(old);
}
}
