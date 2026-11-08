// zEffect's animation events with no differential of their own (the shipped worlds never reach them).
#include <cstdint>
#include <cstring>
#include <vector>

#include "check.h"
#include "gamez/zeffect/zeff_anim_events.h"
#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zeffect/zeff_init.h"

// EFFECT (0x005031b0): the def's effect +0x0c spawned at +0x10 from node +0x0e's place; -200 with no start node
// still adds the start's offset, as the exe loads it before testing the node; ref 0, no node, the offset alone.
TEST(anim_effect_event_spawns_at_its_place) {
  static int32_t spawned = -9;
  static float at[3];
  zeff::effect_hooks().spawn_asked = [](int32_t i, const float* p) { spawned = i, std::memcpy(at, p, 12); };
  std::vector<zeff::EffectRef> effects(3);
  effects[2].put<int32_t>(0x20, 7);
  zeff::AnimDef d{};
  d.effects = effects.data();
  const float start[3] = {100, 200, 300};
  std::memcpy(d.raw + 0x78, start, 12);
  uint8_t e[0x1c] = {};
  const int16_t which = 2, from = -200, none = 0;
  const float off[3] = {1, 2, 3};
  std::memcpy(e + 0xc, &which, 2), std::memcpy(e + 0xe, &from, 2), std::memcpy(e + 0x10, off, 12);
  EXPECT(zeff::ev_effect(d, e) == 2);
  EXPECT(spawned == 7 && at[0] == 101.0f && at[1] == 202.0f && at[2] == 303.0f);
  std::memcpy(e + 0xe, &none, 2);
  zeff::ev_effect(d, e);
  EXPECT(spawned == 7 && at[0] == 1.0f && at[1] == 2.0f && at[2] == 3.0f);
  zeff::effect_hooks().spawn_asked = nullptr;
}
