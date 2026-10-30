// The lance orders' dispatcher (test_lance_dispatch.py): mech3/lance_orders
// on real Mechs, the brain and its orders stood in (logged).
#include <cstdint>
#include <cstring>
#include <vector>

#include "mech3/lance_orders.h"
#include "mech3/mech.h"
#include "mech3/mech_collide.h"
#include "mech3/mech_control.h"
#include "mech3/zai_brain.h"

using namespace mech3;

namespace {
// The emulator's addresses as handles: the player, two mates, the target.
uint32_t g_handle[4];
float g_pos[4][3];
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
  for (uint32_t i = 0; i < 4; ++i)
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
// (in[13]: the target a mech, else a vehicle that is none, 0x004e6ff0's 0)
// seen and entity as indices and its dist2 and delta)), its length.
int32_t probe_lance_dispatch(const int32_t* in, const float* pos, const uint32_t* handles, uint32_t* out, int32_t cap) {
  g_log.clear();
  g_target_mech = in[13] != 0;
  ai::set_brain_hooks(&g_hooks);
  Mech m[4];
  for (int i = 0; i < 4; ++i) {
    g_mech[i] = &m[i];
    g_handle[i] = handles[i];
    std::memcpy(g_pos[i], pos + 3 * i, 12);
  }
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
