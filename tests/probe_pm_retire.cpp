// Probe for tests/differential/test_pm_retire.py: Pirate's Moon's shot retire 0x005a8390 (the port's zwep::retire),
// held a frame first: its answer, the stamp, and the free path's calls.
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zclass/cls_node.h"
#include "gamez/zweapon/zweapon.h"

namespace {
std::vector<uint32_t> g_log;
uint32_t id(zwep::Handle h) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(h)); }
}  // namespace

extern "C" {
// in: the frame count, the shot's stamp, tether-guided, two flyout animations (0 none). out: the answer, the stamp,
// speed bits, state, both animations, the owner, released (on the free list), then the calls.
int probe_pm_retire(uint32_t frame, uint32_t stamp, int tether, uint32_t a1, uint32_t a2, uint32_t* out) {
  zwep::RunHooks& h = zwep::run_hooks();
  const zwep::RunHooks kept = h;
  h = zwep::RunHooks{};
  h.anim_stop = [](zwep::Handle a) { g_log.push_back(1), g_log.push_back(id(a)); };
  h.remove_child = [](zwep::Handle parent, zwep::Handle child) { g_log.push_back(2), g_log.push_back(id(parent)), g_log.push_back(id(child)); };
  g_log.clear();
  zcls::frames().frame = frame;
  zwep::WeaponDef def{};
  def.flags = tether ? zwep::kTetherGuided : 0;
  zwep::Projectile p{};
  p.frame_stamp = stamp, p.speed = 5.0f, p.state = 0;
  p.owner = reinterpret_cast<zwep::Handle>(0x4040);
  p.node = reinterpret_cast<zwep::Handle>(0x5050);
  p.flyout_anim = reinterpret_cast<zwep::Handle>(static_cast<uintptr_t>(a1));
  p.flyout_attached = reinterpret_cast<zwep::Handle>(static_cast<uintptr_t>(a2));
  zwep::State& s = zwep::state();
  zwep::Projectile* free_before = s.free_list;
  zwep::Handle world_before = s.world;
  s.world = reinterpret_cast<zwep::Handle>(0x7070);
  const bool r = zwep::retire(&def, &p);
  const bool released = s.free_list == &p;
  s.free_list = free_before, s.world = world_before;
  h = kept;
  int n = 0;
  uint32_t sp;
  std::memcpy(&sp, &p.speed, 4);
  out[n++] = r, out[n++] = p.frame_stamp, out[n++] = sp, out[n++] = static_cast<uint32_t>(p.state);
  out[n++] = id(p.flyout_anim), out[n++] = id(p.flyout_attached), out[n++] = id(p.owner), out[n++] = released;
  for (uint32_t v : g_log) out[n++] = v;
  return n;
}
}  // extern "C"
