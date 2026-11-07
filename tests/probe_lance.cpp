// The lance orders' dispatcher (test_lance_dispatch.py): mech3/lance_orders
// on real Mechs, the brain and its orders stood in (logged).
#include <cstdint>
#include <cstring>
#include <vector>

#include "mech3/lance_orders.h"
#include "mech3/mech.h"
#include "mech3/mech_collide.h"
#include "mech3/mech_control.h"
#include "mech3/mech_pocket.h"
#include "mech3/sensors.h"
#include "mech3/zai_brain.h"

using namespace mech3;

namespace {
// The emulator's addresses as handles: the player, two mates, the target.
uint32_t g_handle[6];  // then each mate's own marker, Pirate's Moon's
float g_pos[6][3];
ai::AiTarget* g_own[2];
Mech* g_mech[4];
ai::Contact g_contact[4];  // 3 the player's contacts, then each mate's contact on the player (+0x98)
ai::Contact g_pc[2];
bool g_target_mech = true;
int32_t g_brain_of[2];      // a mate's brain (its token 1, 2) or none (0)
std::vector<uint32_t> g_log;

uint32_t fbits(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}
int index_of(const Mech* m) {
  for (int i = 0; i < 4; ++i)
    if (g_mech[i] == m) return i;
  return -1;
}
uint32_t handle_index(uint32_t h) {
  for (uint32_t i = 0; i < 6; ++i)
    if (g_handle[i] == h) return i + 1;
  return 0;
}

struct Hooks : ai::BrainHooks {
  zgame::Handle vehicle_handle(void* v) override {
    const int i = index_of(static_cast<Mech*>(v));
    return i >= 0 ? g_handle[i] : 0;
  }
  // 0x004e6fe0: what the contact sees (vt+0x2c); the made-up objects answer themselves.
  zgame::Handle contact_vehicle(const ai::Contact* c) override { return c->seen; }
  ai::AiTarget* targetable(zgame::Handle v) override {
    const uint32_t i = handle_index(v);
    return i >= 5 ? g_own[i - 5] : i ? static_cast<ai::AiTarget*>(g_mech[i - 1]) : nullptr;
  }
  void position(zgame::Handle v, float out[3]) override {
    const uint32_t i = handle_index(v);
    if (i) std::memcpy(out, g_pos[i - 1], 12);
  }
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg>) override {
    g_log.push_back(0xdead);
    g_log.push_back(static_cast<uint32_t>(std::strlen(key)));
    return 0;
  }
};
Hooks g_hooks;
}  // namespace

extern "C" {

// in: the order; the player's saved contact and ally contact (0 none, 1..3);
// his target contact (0..3); each contact's mech (0 none, 1 player, 2/3 a
// mate, 4 the target); each mate's +0xbc, brain (0/1) and contact on the
// player (0/1); the four positions (12 floats). out: the log (each order
// given: 0x1a, the order, the brain token, the contact (a player's contact
// 1..3, a mate's on the player 0x11 / 0x12, else the temporary one's owner,
// (in[13]: the target a mech, else a vehicle that is none, 0x004e6ff0's 0;
// in[14..16]: own markers, the target the marker; in[17]: the mates' contacts see the target)
// seen and entity as indices and its dist2 and delta)), its length.
int32_t probe_lance_dispatch(const int32_t* in, const float* pos, const uint32_t* handles, uint32_t* out, int32_t cap) {
  g_log.clear();
  g_target_mech = in[13] != 0;
  ai::set_brain_hooks(&g_hooks);
  Mech m[4];
  for (int i = 0; i < 4; ++i) g_mech[i] = &m[i];
  for (int i = 0; i < 6; ++i) {
    g_handle[i] = handles[i];
    std::memcpy(g_pos[i], pos + 3 * i, 12);
  }
  ai::AiTarget own[2];
  g_own[0] = &own[0], g_own[1] = &own[1];
  static int32_t has_own[2];
  has_own[0] = in[14], has_own[1] = in[15];
  const uint32_t saved_marker = pocket_globals().marker;
  pocket_globals().marker = in[16] ? handles[3] : 0xdead0000u;
  MechControls controls;
  controls.owner = &m[0];
  m[0].controls = &controls;
  for (int k = 0; k < 3; ++k) g_contact[k] = ai::Contact{}, g_contact[k].seen = in[4 + k] ? handles[in[4 + k] - 1] : 0;
  auto contact = [](int32_t k) -> ai::Contact* { return k ? &g_contact[k - 1] : nullptr; };
  m[0].saved_target = contact(in[1]);
  m[0].ally_target = contact(in[2]);
  m[0].target = contact(in[3]);
  for (int j = 0; j < 2; ++j) {
    Mech& mate = m[1 + j];
    mate.xbc = static_cast<uint8_t>(in[7 + 3 * j]);
    g_brain_of[j] = in[8 + 3 * j];
    mate.player_contact = in[9 + 3 * j] ? &g_pc[j] : nullptr;
    g_pc[j] = ai::Contact{}, g_pc[j].seen = in[17] ? handles[3] : handles[0];
  }
  CollideCalls saved_collide = collide_calls();
  collide_calls().contact_mech = [](Handle c) -> Mech* {
    const uint32_t i = handle_index(static_cast<ai::Contact*>(c)->seen);
    if (i == 4 && !g_target_mech) return nullptr;
    return i ? g_mech[i - 1] : nullptr;
  };
  LanceCalls& lc = lance_calls();
  lc.brain = [](Mech& mate) -> ai::Brain* {
    const int j = index_of(&mate) - 1;
    return j >= 0 && j < 2 && g_brain_of[j] ? reinterpret_cast<ai::Brain*>(static_cast<uintptr_t>(0x100 + j)) : nullptr;
  };
  lc.own_marker = [](ai::Brain& b) -> uint32_t {
    const int j = static_cast<int>(reinterpret_cast<uintptr_t>(&b) - 0x100);
    return has_own[j] ? g_handle[4 + j] : 0;
  };
  lc.own_marker_place = [](uint32_t own_h, uint32_t target) {
    g_log.push_back(0x51), g_log.push_back(handle_index(own_h)), g_log.push_back(handle_index(target));
  };
  lc.own_marker_show = [](uint32_t own_h, bool on) {
    g_log.push_back(0x52), g_log.push_back(handle_index(own_h)), g_log.push_back(on ? 1 : 0);
  };
  lc.order = [](ai::Brain& b, int32_t order, ai::Contact* c) -> int32_t {
    g_log.push_back(0x1a);
    g_log.push_back(static_cast<uint32_t>(order == 12 ? 7 : order));  // one wrapper, 0x004ea180
    g_log.push_back(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&b) - 0x100 + 1));
    int k = 0;
    for (int i = 0; i < 3; ++i)
      if (c == &g_contact[i]) k = i + 1;
    if (c == &g_pc[0]) k = 0x11;
    if (c == &g_pc[1]) k = 0x12;
    g_log.push_back(static_cast<uint32_t>(k));
    if (!k) {
      g_log.push_back(handle_index(c->owner)), g_log.push_back(handle_index(c->seen));
      g_log.push_back(handle_index(c->entity)), g_log.push_back(fbits(c->dist2));
      for (int i = 0; i < 3; ++i) g_log.push_back(fbits(c->delta[i]));
    }
    return 1;
  };
  MechControls* r = lance_dispatch(&controls, in[0]);
  g_log.push_back(0x2b);
  g_log.push_back(r == &controls ? 1u : 0u);
  for (int j = 0; j < 2; ++j)
    if (has_own[j]) g_log.push_back(own[j].team), g_log.push_back(own[j].allies), g_log.push_back(own[j].enemies);
  pocket_globals().marker = saved_marker;
  lc = LanceCalls{};
  collide_calls() = saved_collide;
  for (Mech& x : m) x.controls = nullptr, x.saved_target = x.ally_target = x.target = x.player_contact = nullptr;
  ai::set_brain_hooks(nullptr);
  const int32_t n = static_cast<int32_t>(g_log.size());
  if (n > cap) return -1;
  std::memcpy(out, g_log.data(), 4 * g_log.size());
  return n;
}

}  // extern "C"

// PM's lance flag 0x004e5f90 (test_pm_lance_flag.py). in: a mech, a controls
// part, a brain, the brain's +0xed, the vehicle's +0x58 (0 none, 1 the player,
// 2 another), its team, the player's allies, +0xbc, allies, enemies, an own
// marker there. out: +0xbc, allies, enemies, then the log (0x71 made, 0x72 freed).
namespace {
const int32_t* g_f;
ai::AiTarget g_fx, g_fp;
std::vector<uint32_t> g_flog;
enum { kX = 1, kMech, kCtl, kBrain, kPlayer, kOther };
struct FlagHooks : ai::SensorHooks {
  uint32_t vcall(zgame::Handle o, uint32_t slot, std::initializer_list<zgame::Arg>) override {
    if (o == kX && slot == 0x40) return g_f[0] ? kMech : 0;
    if (o == kMech && slot == 0x140) return g_f[1] ? kCtl : 0;
    if (o == kCtl && slot == 0x54) return g_f[2] ? kBrain : 0;
    if (o == kX && slot == 0x50) return g_f[4] == 1 ? kPlayer : g_f[4] == 2 ? kOther : 0;
    if (slot == 0x28) return o;
    return 0;
  }
  ai::AiTarget* fields(zgame::Handle x) override { return x == kX ? &g_fx : x == kPlayer ? &g_fp : nullptr; }
  uint8_t brain_lancemate(zgame::Handle) override { return static_cast<uint8_t>(g_f[3]); }
  zgame::Handle player() override { return kPlayer; }
  uint32_t team(zgame::Handle) override { return static_cast<uint32_t>(g_f[5]); }
  void brain_own_marker(zgame::Handle, bool on) override {
    if (on) g_flog.push_back(0x71);
    else if (g_f[10]) g_flog.push_back(0x72);
  }
};
FlagHooks g_flag_hooks;
}  // namespace

extern "C" int32_t probe_pm_lance_flag(const int32_t* in, uint32_t* out, int32_t cap) {
  g_f = in;
  g_flog.clear();
  ai::SensorHooks* saved = &ai::sensor_hooks();
  ai::set_sensor_hooks(&g_flag_hooks);
  g_fp.allies = static_cast<uint32_t>(in[6]);
  g_fx.xbc = static_cast<uint8_t>(in[7]);
  g_fx.allies = static_cast<uint32_t>(in[8]), g_fx.enemies = static_cast<uint32_t>(in[9]);
  ai::lance_flag(kX);
  ai::set_sensor_hooks(saved);
  g_flog.insert(g_flog.begin(), {g_fx.xbc, g_fx.allies, g_fx.enemies});
  const int32_t n = static_cast<int32_t>(g_flog.size());
  if (n > cap) return -1;
  std::memcpy(out, g_flog.data(), 4 * g_flog.size());
  return n;
}
