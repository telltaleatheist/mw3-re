#include "mech3/sound_extra.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#include "gamez/zreader/zreader.h"
#include "gamez/zsound/zsnd.h"
#include "gamez/zutil/zutl_msg.h"
#include "mech3/mech.h"
#include "mech3/message_panel.h"

namespace mech3 {
namespace {
SoundExtra g_default;  // 0x0060b800
bool g_subtitles = false;  // 0x0060b878
// 0x0060b814 / 0x0060b818: the records made, for the clear.
std::vector<std::unique_ptr<SoundExtra>> g_extras;
SoundExtraHooks g_hooks;

void free_extra(SoundExtra* r) {
  std::free(r->subtitle);
  r->subtitle = nullptr;
}
}  // namespace

SoundExtra& default_extra() { return g_default; }
SoundExtraHooks& sound_extra_hooks() { return g_hooks; }
bool& subtitles_on() { return g_subtitles; }

// Mech3.exe 1.2 @ 0x004e6b00 (with 0x004e6aa0 / 0x004e6a60 and 0x0049d1c0):
// the zero template's copy, MINDELAY's first value, SUBTITLE's message
// (0x00575fb0, its name when unknown) copied.
void* sound_extra_load(zsnd::Object*, zrdr::Node* extra) {
  if (!extra) return nullptr;
  const char* subtitle = nullptr;
  float delay = 0.0f;
  bool any = false;
  if (zrdr::Node* v = zrdr::find(extra, "MINDELAY")) delay = v->list[1].f, any = true;
  if (zrdr::Node* v = zrdr::find(extra, "SUBTITLE")) subtitle = zutl::localize(v->list[1].s), any = true;
  if (!any) return nullptr;
  auto r = std::make_unique<SoundExtra>();
  r->subtitle = subtitle ? strdup(subtitle) : nullptr;
  r->min_delay = delay;
  r->next = 0.0f;
  SoundExtra* out = r.get();
  g_extras.push_back(std::move(r));
  return out;
}

// Mech3.exe 1.2 @ 0x004e6be0
void sound_extra_free(zsnd::Object*, void* user) {
  if (!user) return;
  for (auto it = g_extras.begin(); it != g_extras.end(); ++it) {
    if (it->get() != user) continue;
    free_extra(it->get());
    g_extras.erase(it);
    return;
  }
}

// Mech3.exe 1.2 @ 0x004e69a0
void sound_extras_clear() {
  for (auto& r : g_extras) free_extra(r.get());
  g_extras.clear();
}

// Mech3.exe 1.2 @ 0x004e6cd0. The game time [0x0060991c]; a delay of 0
// leaves the next time alone.
zsnd::VoiceBase* sound_extra_play(SoundExtra& r, zsnd::Object* obj, float volume, const float* pos, const float* vel,
                                  float fade_to) {
  const float now = world().time;
  if (r.next > now) return nullptr;  // fcomp, test ah, 0x41: NaN plays
  if (r.min_delay > 0.0f) r.next = now + r.min_delay;
  zsnd::VoiceBase* v = g_hooks.play_fading ? g_hooks.play_fading(obj, pos, volume, vel, fade_to, 0)
                                           : zsnd::play_fading(obj, pos, volume, vel, fade_to, 0);
  if (g_subtitles && r.subtitle) {
    if (g_hooks.subtitle) g_hooks.subtitle(r.subtitle);
    else message_panel_add(r.subtitle);
  }
  return v;
}

// Mech3.exe 1.2 @ 0x004e6d80
zsnd::VoiceBase* sound_play(zsnd::Object* obj, const float* pos, float volume, const float* vel, float fade_to) {
  if (!obj) return nullptr;
  auto* r = static_cast<SoundExtra*>(zsnd::user_data(obj));  // 0x0056f430
  return sound_extra_play(r ? *r : g_default, obj, volume, pos, vel, fade_to);
}

// Mech3.exe 1.2 @ 0x00432990
zsnd::VoiceBase* pilot_sound(Pilot& p, int32_t id, float volume, const float* pos, const float* vel) {
  return sound_play(static_cast<zsnd::Object*>(p.sounds[id]), pos, volume, vel, -1.0f);
}

}  // namespace mech3
