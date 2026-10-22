#pragma once
#include <cstdint>

// The player's cockpit sounds (mech.cpp; docs/cockpit-sounds.md): 30 zSound
// objects found by name at mission load (0x004141a0, [0x00605598]) and the
// voice each last played ([0x00605520]). The fire modes, the hits, the ammo,
// the gyro, MASC and the HUD's channels play through them by index.
namespace zsnd {
struct Object;
struct VoiceBase;
}  // namespace zsnd

namespace mech3 {

constexpr int kCockpitSounds = 30;

struct CockpitSounds {
  zsnd::Object* sound[kCockpitSounds] = {};     // 0x00605598
  zsnd::VoiceBase* voice[kCockpitSounds] = {};  // 0x00605520
};
CockpitSounds& cockpit_sounds();

// zSound's calls, for the differential test; unset: zsnd's and sound_extra's.
struct CockpitSoundCalls {
  zsnd::Object* (*find)(const char* name) = nullptr;                    // 0x0056f6f0
  zsnd::VoiceBase* (*play)(zsnd::Object* s, float volume) = nullptr;    // 0x004e6d80(s, 0; volume, 0, -1)
  void (*stop)(zsnd::VoiceBase* v) = nullptr;                           // 0x0056e9f0
  zsnd::VoiceBase* (*fade)(zsnd::VoiceBase* v, int stop, float level) = nullptr;  // 0x00571d20
};
CockpitSoundCalls& cockpit_sound_calls();

const char* cockpit_sound_name(int index);         // the names 0x004141a0 finds
void cockpit_sounds_load();                        // 0x004141a0
void cockpit_sound_play(int index, float volume);  // 0x0041e770 (its second argument unread)
void cockpit_sound_stop(int index, int fade);      // 0x0041e7a0
void cockpit_sound_fade(int index, int stop, float level);  // 0x0041e7e0
zsnd::VoiceBase* cockpit_sound_voice(int index);   // 0x0041e800

}  // namespace mech3
