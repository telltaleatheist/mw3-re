#include <cstdint>
#include <cstring>

#include "gamez/zclass/cls_node.h"
#include "mech3/mech.h"
#include "mech3/weapon.h"

// The aim 0x0041c900 and the placement's block (test_pm_mech_vt.py). f: the point, the node's
// place. out: result, the torso asked, the direction, the aim set, the aim.
namespace {
float g_aim_dir[3];
int32_t g_aim_asked = 0, g_aim_had = 0, g_aim_answer = 0;
}  // namespace
extern "C" void probe_pm_aim(const float* f, int32_t has_at, int32_t check, int32_t answer, float* out) {
  using namespace mech3;
  static Mech m;
  m = Mech{};
  alignas(zcls::Node) static unsigned char raw_node[sizeof(zcls::Node)];
  std::memset(raw_node, 0, sizeof raw_node);
  auto* n = reinterpret_cast<zcls::Node*>(raw_node);
  alignas(8) static unsigned char data[0x90];
  std::memset(data, 0, sizeof data);
  std::memcpy(data + 0x84, f + 3, 12);
  n->put<int32_t>(0x34, 5);
  n->set_flags(0x80000);
  n->data = data;
  m.node = n;
  alignas(WeaponManager) static unsigned char wm_raw[sizeof(WeaponManager)];
  std::memset(wm_raw, 0, sizeof wm_raw);
  auto& wm = *reinterpret_cast<WeaponManager*>(wm_raw);
  m.ammo = &wm;
  wm.aim_valid = true;  // a stale aim, to see it turned off
  wm.aim = {-1.0f, -1.0f, -1.0f};
  const auto keep = aim_calls().torso_aim;
  g_aim_asked = g_aim_had = 0;
  g_aim_answer = answer;
  aim_calls().torso_aim = [](Mech&, const Vec3* d, void*, int) -> int {
    g_aim_asked = 1;
    g_aim_had = d != nullptr;
    if (d) g_aim_dir[0] = d->x, g_aim_dir[1] = d->y, g_aim_dir[2] = d->z;
    return g_aim_answer;
  };
  const Vec3 at{f[0], f[1], f[2]};
  const bool r = m.aim_at(has_at ? &at : nullptr, nullptr, check != 0);
  aim_calls().torso_aim = keep;
  m.node = nullptr, m.ammo = nullptr;
  out[0] = r, out[1] = g_aim_asked, out[2] = g_aim_had;
  out[3] = g_aim_had ? g_aim_dir[0] : 0, out[4] = g_aim_had ? g_aim_dir[1] : 0, out[5] = g_aim_had ? g_aim_dir[2] : 0;
  out[6] = wm.aim_valid, out[7] = wm.aim_valid ? wm.aim.x : 0, out[8] = wm.aim_valid ? wm.aim.y : 0, out[9] = wm.aim_valid ? wm.aim.z : 0;
}
extern "C" void probe_pm_pending(const float* q, const float* tr, float* out) {
  using namespace mech3;
  static Mech m;
  m = Mech{};
  float t[12];
  std::memcpy(t, tr, sizeof t);
  std::memcpy(m.orientation, q, 16);
  m.transform = t;
  m.velocity = {0, 0, 0};  // reset_at's, first
  m.pending_from_state();
  m.transform = nullptr;
  std::memcpy(out, m.pending_q, 16);
  out[4] = m.pending_velocity.x, out[5] = m.pending_velocity.y, out[6] = m.pending_velocity.z;
  std::memcpy(out + 7, m.pending_m, 48);
}
