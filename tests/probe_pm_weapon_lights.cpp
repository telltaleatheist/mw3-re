// Probe for tests/differential/test_pm_weapon_lights.py: Pirate's Moon's effect block reader 0x005a6700 (the port's
// zwep::read_effect) on a reader tree given as the reader's binary bytes, its lookups answered by a hash of the
// name, as the test's stubs answer the exe's.
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "gamez/zreader/zreader.h"
#include "gamez/zweapon/zwep_init.h"

namespace {
uint32_t fnv(const char* s, uint32_t salt) {
  uint32_t h = 2166136261u ^ salt;
  for (; *s; ++s) h = (h ^ static_cast<uint8_t>(*s)) * 16777619u;
  return h | 1u;  // never 0
}
zwep::Handle as_handle(uint32_t v) { return reinterpret_cast<zwep::Handle>(static_cast<uintptr_t>(v)); }
uint32_t of(zwep::Handle h) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(h)); }
}  // namespace

extern "C" {
// bytes: one node (a list of the block's key and value pairs); key: the block read ("FIRE", "FLYOUT", ...);
// out: flags, the two animations, attached, model animation, surface, min range, effect, model, the sound count
// and four sounds, the bounce count and four, the attach node. Returns the words, -1 on a bad tree.
int probe_pm_read_effect(const uint8_t* bytes, int len, const char* key, int fire_effects, uint32_t* out) {
  std::FILE* f = fmemopen(const_cast<uint8_t*>(bytes), static_cast<size_t>(len), "rb");
  if (!f) return -1;
  zrdr::Node root{};
  const uint32_t ok = zrdr::read_node(f, &root);
  std::fclose(f);
  (void)ok;
  zwep::InitHooks& h = zwep::init_hooks();
  const zwep::InitHooks kept = h;
  h.anim = [](const char* s) { return as_handle(fnv(s, 1)); };
  h.sound = [](const char* s) { return as_handle(fnv(s, 2)); };
  h.effect = [](const char* s) { return static_cast<int32_t>(fnv(s, 3)); };
  h.find_node = [](int32_t list, const char* s) { return as_handle(fnv(s, 4 + static_cast<uint32_t>(list))); };
  zwep::set_fire_effects(fire_effects != 0);
  zwep::Effect e{};
  e.attach_node = as_handle(0x5a5a5a5a);
  zwep::read_effect(&root, &e, key);
  zwep::set_fire_effects(true);
  h = kept;
  int n = 0;
  out[n++] = e.flags, out[n++] = of(e.animation), out[n++] = of(e.animation2), out[n++] = of(e.animation_attached);
  out[n++] = of(e.model_animation), out[n++] = of(e.surface_animation), out[n++] = of(e.animation_min_range);
  out[n++] = static_cast<uint32_t>(e.effect), out[n++] = of(e.model), out[n++] = static_cast<uint32_t>(e.sound_count);
  for (zwep::Handle s : e.sounds) out[n++] = of(s);
  out[n++] = static_cast<uint32_t>(e.bounce_sound_count);
  for (zwep::Handle s : e.bounce_sounds) out[n++] = of(s);
  out[n++] = of(e.attach_node);
  return n;
}
}  // extern "C"
