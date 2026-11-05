// The foot's wake for tests/differential/test_pm_wake.py (from probe_sound_extra's).
#include <cstdint>
#include <cstring>
#include <vector>

#include "mech3/mech_wake.h"

namespace {
std::vector<uint8_t> g_out;
void u32(uint32_t v) {
  for (int i = 0; i < 4; ++i) g_out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
int finish(uint8_t* out, int cap) {
  const int n = static_cast<int>(g_out.size());
  if (n <= cap) std::memcpy(out, g_out.data(), g_out.size());
  return n;
}
}  // namespace

extern "C" {

// The wake: each op a throttle to start at (float bits) or 0x7fffffff to
// stop; the calls logged, then the level and whether it plays.
int probe_pm_wake(const int32_t* ops, int n, int32_t level, uint32_t playing, uint8_t* out, int cap) {
  g_out.clear();
  mech3::WakeHooks& h = mech3::wake_hooks();
  static int counter = 0;
  counter = 0;
  h.start = [](zeff::AnimDef* def, zcls::Node* node) {
    u32(0x61), u32(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(def))), u32(node != nullptr);
    return reinterpret_cast<zeff::AnimDef*>(static_cast<uintptr_t>(0x100 + ++counter));
  };
  h.stop = [](zeff::AnimDef* p) { u32(0x62), u32(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p))); };
  h.set_active = [](zcls::Node*, int on) { u32(0x63), u32(static_cast<uint32_t>(on)); };
  mech3::FootWake w;
  w.level = level;
  w.playing = reinterpret_cast<zeff::AnimDef*>(static_cast<uintptr_t>(playing));
  static int node_dummy;
  w.node = reinterpret_cast<zcls::Node*>(&node_dummy);
  for (int i = 0; i < 3; ++i) w.anims[i] = reinterpret_cast<zeff::AnimDef*>(static_cast<uintptr_t>(0x10 + i));
  for (int i = 0; i < n; ++i) {
    if (ops[i] == 0x7fffffff) {
      mech3::foot_wake_stop(&w);
    } else {
      float t;
      std::memcpy(&t, ops + i, 4);
      mech3::foot_wake_start(&w, t);
    }
    u32(static_cast<uint32_t>(w.level)), u32(mech3::foot_wake_playing(&w));
  }
  h = mech3::WakeHooks{};
  return finish(out, cap);
}

}  // extern "C"
