// The cockpit sounds (0x004141a0, 0x0041e770, 0x0041e7a0, 0x0041e7e0,
// 0x0041e800) for test_cockpit_sound.py: zSound's calls recorded as
// (kind, a, b, c) ints, the objects and voices as the test's numbers.
#include <cstdint>
#include <cstring>
#include <vector>

#include "mech3/cockpit_sound.h"

namespace {
std::vector<int32_t> g_log;
const char* const* g_found = nullptr;  // names the find answers, in order of their ids
int32_t g_found_n = 0;
int32_t g_next_voice = 0x1000;

uintptr_t u(const void* p) { return reinterpret_cast<uintptr_t>(p); }
template <typename T> T* h(uintptr_t v) { return reinterpret_cast<T*>(v); }
int32_t fbits(float f) {
  int32_t i;
  std::memcpy(&i, &f, 4);
  return i;
}
}  // namespace

extern "C" {
void cs_open(const char* const* found, int32_t n) {
  g_found = found, g_found_n = n, g_next_voice = 0x1000;
  g_log.clear();
  mech3::cockpit_sounds() = mech3::CockpitSounds{};
  auto& c = mech3::cockpit_sound_calls();
  c.find = [](const char* name) -> zsnd::Object* {
    for (int32_t i = 0; i < g_found_n; ++i)
      if (std::strcmp(g_found[i], name) == 0) return h<zsnd::Object>(0x100 + i);
    return nullptr;
  };
  c.play = [](zsnd::Object* s, float volume) {
    g_log.insert(g_log.end(), {1, static_cast<int32_t>(u(s)), fbits(volume), 0});
    return h<zsnd::VoiceBase>(g_next_voice++);
  };
  c.stop = [](zsnd::VoiceBase* v) { g_log.insert(g_log.end(), {2, static_cast<int32_t>(u(v)), 0, 0}); };
  c.fade = [](zsnd::VoiceBase* v, int stop, float level) {
    g_log.insert(g_log.end(), {3, static_cast<int32_t>(u(v)), stop, fbits(level)});
    return stop ? nullptr : v;
  };
}
void cs_close() { mech3::cockpit_sound_calls() = mech3::CockpitSoundCalls{}; }
void cs_load() { mech3::cockpit_sounds_load(); }
void cs_play(int32_t i, float volume) { mech3::cockpit_sound_play(i, volume); }
void cs_stop(int32_t i, int32_t fade) { mech3::cockpit_sound_stop(i, fade); }
void cs_fade(int32_t i, int32_t stop, float level) { mech3::cockpit_sound_fade(i, stop, level); }
int32_t cs_voice(int32_t i) { return static_cast<int32_t>(u(mech3::cockpit_sound_voice(i))); }
// 30 sounds, 30 voices, then the log.
int32_t cs_state(int32_t* out) {
  int32_t k = 0;
  for (auto* s : mech3::cockpit_sounds().sound) out[k++] = static_cast<int32_t>(u(s));
  for (auto* v : mech3::cockpit_sounds().voice) out[k++] = static_cast<int32_t>(u(v));
  for (int32_t x : g_log) out[k++] = x;
  return k;
}
}
